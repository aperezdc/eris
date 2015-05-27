/*
 * eris-module.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifdef ERIS_LIBDWARF_BUNDLED
# include "libdwarf.h"
# include "dwarf.h"
#else
# ifdef ERIS_WORKAROUND_DWARF_PUBTYPE_DIE_OFFSET
#  if ERIS_WORKAROUND_DWARF_PUBTYPE_DIE_OFFSET
#   define dwarf_pubtype_die_offset dwarf_pubtype_type_die_offset
#  endif
# endif /* ERIS_WORKAROUND_DWARF_PUBTYPE_DIE_OFFSET */
# if defined(ERIS_LIBDWARF_LIBDWARF_H) && ERIS_LIBDWARF_LIBDWARF_H
#  include <libdwarf/libdwarf.h>
#  include <libdwarf/dwarf.h>
# else
#  include <libdwarf.h>
#  include <dwarf.h>
# endif /* ERIS_LIBDWARF_LIBDWARF_H */
#endif /* ERIS_LIBDWARF_BUNDLED */

#ifdef ERIS_LUA_BUNDLED
# include "lua.h"
# include "lauxlib.h"
#else
# include <lua.h>
# include <lauxlib.h>
#endif /* ERIS_LUA_BUNDLED */

#include "eris-typing.h"
#include "eris-trace.h"
#include "eris-util.h"

#include <libelf.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>


#ifndef ERIS_LIB_SUFFIX
#define ERIS_LIB_SUFFIX ".so"
#endif /* !ERIS_LIB_SUFFIX */


typedef struct _ErisLibrary  ErisLibrary;



/*
 * Data needed for each library loaded by "eris.load()".
 */
struct _ErisLibrary {
    REF_COUNTER;

    int           fd;
    void         *dl;

    Dwarf_Debug   d_debug;
    Dwarf_Global *d_globals;
    Dwarf_Signed  d_num_globals;
    Dwarf_Type   *d_types;
    Dwarf_Signed  d_num_types;
};


static const char ERIS_LIBRARY[]  = "org.perezdecastro.eris.Library";

static void eris_library_free (ErisLibrary*);
REF_COUNTER_FUNCTIONS (ErisLibrary, eris_library, static inline)


static inline void
eris_library_push_userdata (lua_State *L, ErisLibrary *el)
{
    CHECK_NOT_NULL (el);

    ErisLibrary **elp = lua_newuserdata (L, sizeof (ErisLibrary*));
    *elp = eris_library_ref (el);
    luaL_setmetatable (L, ERIS_LIBRARY);
}

static inline ErisLibrary*
to_eris_library (lua_State *L, int index)
{
    return *((ErisLibrary**) luaL_checkudata (L, index, ERIS_LIBRARY));
}


static const char ERIS_TYPEINFO[] = "org.perezdecastro.eris.TypeInfo";

static inline void
eris_typeinfo_push_userdata (lua_State *L, const ErisTypeInfo *ti)
{
    CHECK_NOT_NULL (ti);

    const ErisTypeInfo **tip = lua_newuserdata (L, sizeof (const ErisTypeInfo*));
    *tip = ti;
    luaL_setmetatable (L, ERIS_TYPEINFO);
}

static inline const ErisTypeInfo*
to_eris_typeinfo (lua_State *L, int index)
{
    return *((const ErisTypeInfo**) luaL_checkudata (L, index, ERIS_TYPEINFO));
}


static int
l_eris_typeinfo_tostring (lua_State *L)
{
    const ErisTypeInfo *typeinfo = to_eris_typeinfo (L, 1);
    lua_pushfstring (L,
                     "eris.type (%p:%s)",
                     typeinfo,
                     eris_typeinfo_name (typeinfo));
    return 1;
}

static int
l_eris_typeinfo_index (lua_State *L)
{
    const ErisTypeInfo *typeinfo = to_eris_typeinfo (L, 1);
    lua_settop (L, 2);
    if (lua_isstring (L, 2)) {
        const char *field = lua_tostring (L, 2);
        if (!strcmp ("name", field)) {
            lua_pushstring (L, eris_typeinfo_name (typeinfo));
        } else if (!strcmp ("sizeof", field)) {
            lua_pushinteger (L, eris_typeinfo_sizeof (typeinfo));
        } else if (!strcmp ("readonly", field)) {
            lua_pushboolean (L, eris_typeinfo_is_readonly (typeinfo));
        } else {
            return luaL_error (L, "invalid field '%s'", field);
        }
        return 1;
    } else {
        lua_Integer index = luaL_checkinteger (L, 2);
        if (index < 1)
            return luaL_error (L, "invalid index: %i", index);
        const ErisTypeInfoMember *member =
                eris_typeinfo_const_member (typeinfo, index);
        lua_pushstring (L, member->name);
        eris_typeinfo_push_userdata (L, member->typeinfo);
        lua_pushinteger (L, member->offset);
        return 3;
    }
}

static int
l_eris_typeinfo_len (lua_State *L)
{
    const ErisTypeInfo *typeinfo = to_eris_typeinfo (L, 1);
    lua_pushinteger (L, eris_typeinfo_n_members (typeinfo));
    return 1;
}

static int
l_eris_typeinfo_eq (lua_State *L)
{
    const ErisTypeInfo *typeinfo_a = to_eris_typeinfo (L, 1);
    const ErisTypeInfo *typeinfo_b = to_eris_typeinfo (L, 2);
    lua_pushboolean (L, eris_typeinfo_equal (typeinfo_a, typeinfo_b));
    return 1;
}

static int l_eris_typeinfo_call (lua_State *L);

static const luaL_Reg eris_typeinfo_methods[] = {
    { "__tostring", l_eris_typeinfo_tostring  },
    { "__index",    l_eris_typeinfo_index     },
    { "__len",      l_eris_typeinfo_len       },
    { "__eq",       l_eris_typeinfo_eq        },
    { "__call",     l_eris_typeinfo_call      },
    { NULL, NULL },
};


static Dwarf_Die lookup_die (ErisLibrary *library,
                             const char  *name,
                             Dwarf_Error *d_error);
static Dwarf_Die lookup_tue (ErisLibrary *library,
                             const char  *name,
                             Dwarf_Error *d_error);


/*
 * FIXME: This makes ErisVariable/ErisFunction keep a reference to their
 *        corresponding ErisLibrary, which itself might be GCd while there
 *        are still live references to it!
 */
#define ERIS_COMMON_FIELDS \
    ErisLibrary *library;  \
    void        *address;  \
    char        *name


/*
 * Any structure that uses ERIS_COMMON_FIELDS at its start can be casted to
 * this struct type.
 */
typedef struct {
    ERIS_COMMON_FIELDS;
} ErisSymbol;


typedef struct {
    ERIS_COMMON_FIELDS;
    Dwarf_Die d_die;
} ErisFunction;

typedef struct {
    ERIS_COMMON_FIELDS;
    const ErisTypeInfo  *typeinfo;
    size_t               n_items;
} ErisVariable;


static const char ERIS_FUNCTION[] = "org.perezdecastro.eris.Function";
static const char ERIS_VARIABLE[] = "org.perezdecastro.eris.Variable";


static inline ErisFunction*
to_eris_function (lua_State *L)
{
    return (ErisFunction*) luaL_checkudata (L, 1, ERIS_FUNCTION);
}

static inline ErisVariable*
to_eris_variable (lua_State *L)
{
    return (ErisVariable*) luaL_checkudata (L, 1, ERIS_VARIABLE);
}


static bool
find_library (const char *name, char path[PATH_MAX])
{
    /* TODO: Handle multilib systems (lib32 vs. lib64) */
    static const char *search_paths[] = {
        "", /* For relative and absolute paths. */
        "/lib/",
        "/usr/lib/",
        "/usr/local/lib/",
    };

    char try_path[PATH_MAX];
    for (size_t i = 0; i < LENGTH_OF (search_paths); i++) {
        if (snprintf (try_path, PATH_MAX, "%s%s" ERIS_LIB_SUFFIX,
                      search_paths[i], name) > PATH_MAX) {
            return false;
        }

        struct stat sb;
        if (realpath (try_path, path) &&
            stat (path, &sb) == 0 && S_ISREG (sb.st_mode) &&
            ((sb.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) != 0)) {
            return true;
        }
    }

    return false;
}


static
void eris_library_free (ErisLibrary *el)
{
    TRACE ("%p\n", el);

    if (el->d_globals)
        dwarf_globals_dealloc (el->d_debug, el->d_globals, el->d_num_globals);
    if (el->d_types)
        dwarf_pubtypes_dealloc (el->d_debug, el->d_types, el->d_num_types);

    Dwarf_Error d_error;
    dwarf_finish (el->d_debug, &d_error);

    close (el->fd);
    dlclose (el->dl);
    free (el);
}


static int
eris_library_gc (lua_State *L)
{
    ErisLibrary *el = to_eris_library (L, 1);
    TRACE ("%p\n", el);
    eris_library_unref (el);
    return 0;
}


static int
eris_library_tostring (lua_State *L)
{
    ErisLibrary *el = to_eris_library (L, 1);
    if (el->d_debug) {
        lua_pushfstring (L, "eris.library (%p)", el->d_debug);
    } else {
        lua_pushliteral (L, "eris.library (closed)");
    }
    return 1;
}


static inline void
eris_symbol_init (ErisSymbol  *symbol,
                  ErisLibrary *library,
                  void        *address,
                  const char  *name)
{
    CHECK_NOT_NULL (address);
    memset (symbol, 0x00, sizeof (ErisSymbol));
    if (library)
        symbol->library = eris_library_ref (library);
    if (name)
        symbol->name = strdup (name);
    symbol->address = address;
}


static inline void
eris_symbol_free (ErisSymbol *symbol)
{
    if (symbol->library)
        eris_library_unref (symbol->library);
    free (symbol->name);
    memset (symbol, 0xCA, sizeof (ErisSymbol));
}


static int
l_eris_typeinfo_call (lua_State *L)
{
    const ErisTypeInfo *typeinfo = to_eris_typeinfo (L, 1);
    lua_Integer n_items = luaL_optinteger (L, 2, 1);

    if (n_items < 1)
        return luaL_error (L, "argument #2 must be > 0");

    size_t payload = eris_typeinfo_sizeof (typeinfo) * n_items;
    ErisVariable *ev = lua_newuserdata (L, sizeof (ErisVariable) + payload);
    eris_symbol_init ((ErisSymbol*) ev, NULL, &ev[1], NULL);
    ev->typeinfo = typeinfo;
    ev->n_items = n_items;
    memset (ev->address, 0x00, payload);
    luaL_setmetatable (L, ERIS_VARIABLE);
    TRACE ("new ErisVariable* at %p (<Lua>)\n", ev);
    return 1;
}


static int
make_function_wrapper (lua_State   *L,
                       ErisLibrary *library,
                       void        *address,
                       const char  *name,
                       Dwarf_Die    d_die,
                       Dwarf_Half   d_tag)
{
    ErisFunction *ef = lua_newuserdata (L, sizeof (ErisFunction));
    eris_symbol_init ((ErisSymbol*) ef, library, address, name);
    ef->d_die = d_die;
    luaL_setmetatable (L, ERIS_FUNCTION);
    TRACE ("new ErisFunction* at %p (%p:%s)\n", ef, library, name);
    return 1;
}


static Dwarf_Die
die_get_die_reference_attribute (ErisLibrary *library,
                                 Dwarf_Die    d_die,
                                 Dwarf_Half   d_attr_tag,
                                 Dwarf_Error *d_error)
{
    Dwarf_Attribute d_attr = NULL;
    if (dwarf_attr (d_die, d_attr_tag, &d_attr, d_error) != DW_DLV_OK)
        return NULL;

    Dwarf_Die d_result_die = NULL;
    Dwarf_Die d_attr_die;
    Dwarf_Off d_offset;
    if (dwarf_global_formref (d_attr,
                              &d_offset,
                              d_error) == DW_DLV_OK &&
        dwarf_offdie (library->d_debug,
                      d_offset,
                      &d_attr_die,
                      d_error) == DW_DLV_OK) {
        d_result_die = d_attr_die;
    }

    dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);
    return d_result_die;
}


static char*
die_get_string_attribute (ErisLibrary *library,
                          Dwarf_Die    d_die,
                          Dwarf_Half   d_attr_tag,
                          Dwarf_Error *d_error)
{
    Dwarf_Attribute d_attr;
    if (dwarf_attr (d_die, d_attr_tag, &d_attr, d_error) != DW_DLV_OK)
        return NULL;

    char *d_result;
    if (dwarf_formstring (d_attr, &d_result, d_error) != DW_DLV_OK)
        d_result = NULL;

    dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);
    return d_result;
}


static bool
die_get_uint_attribute (ErisLibrary    *library,
                        Dwarf_Die       d_die,
                        Dwarf_Half      d_attr_tag,
                        Dwarf_Unsigned *d_result,
                        Dwarf_Error    *d_error)
{
    Dwarf_Attribute d_attr;
    if (dwarf_attr (d_die, d_attr_tag, &d_attr, d_error) != DW_DLV_OK)
        return false;

    bool success = dwarf_formudata (d_attr, d_result, d_error) == DW_DLV_OK;
    dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);
    return success;
}



static ErisTypeInfo*
die_base_type_get_typeinfo (ErisLibrary *library,
                            Dwarf_Die    d_type_die,
                            Dwarf_Error *d_error)
{
    Dwarf_Unsigned d_encoding, d_byte_size;

    if (!die_get_uint_attribute (library,
                                 d_type_die,
                                 DW_AT_encoding,
                                 &d_encoding,
                                 d_error) ||
        !die_get_uint_attribute (library,
                                 d_type_die,
                                 DW_AT_byte_size,
                                 &d_byte_size,
                                 d_error))
            return NULL;

#define TYPEINFO_ITEM(suffix, ctype)                            \
        case sizeof (ctype):                                    \
            return eris_typeinfo_new (ERIS_TYPE_ ## suffix, 0); \

    switch (d_encoding) {
        case DW_ATE_float:
            switch (d_byte_size) { FLOAT_TYPES (TYPEINFO_ITEM) }
            break;
        case DW_ATE_signed:
        case DW_ATE_signed_char:
            switch (d_byte_size) { INTEGER_S_TYPES (TYPEINFO_ITEM) }
            break;
        case DW_ATE_unsigned:
        case DW_ATE_unsigned_char:
            switch (d_byte_size) { INTEGER_U_TYPES (TYPEINFO_ITEM) }
            break;
    }
#undef TYPEINFO_ITEM

    return NULL;
}


static ErisTypeInfo*
die_to_typeinfo (ErisLibrary  *library,
                 Dwarf_Die     d_type_die,
                 Dwarf_Error  *d_error)
{
    Dwarf_Half d_tag;
    if (dwarf_tag (d_type_die,
                   &d_tag,
                   d_error) != DW_DLV_OK)
        return NULL;

    switch (d_tag) {
        case DW_TAG_base_type: {
            ErisTypeInfo *typeinfo = die_base_type_get_typeinfo (library,
                                                                 d_type_die,
                                                                 d_error);
            if (eris_typeinfo_is_valid (typeinfo)) {
                Dwarf_Error d_name_error;
                const char* name = die_get_string_attribute (library,
                                                             d_type_die,
                                                             DW_AT_name,
                                                             &d_name_error);
                if (name) eris_typeinfo_set_name (typeinfo, name);
            }
            return typeinfo;
        }

        case DW_TAG_const_type: {
            Dwarf_Die d_base_type_die =
                die_get_die_reference_attribute (library,
                                                 d_type_die,
                                                 DW_AT_type,
                                                 d_error);
            if (!d_base_type_die)
                return NULL;

            ErisTypeInfo *typeinfo = die_to_typeinfo (library,
                                                      d_base_type_die,
                                                      d_error);
            dwarf_dealloc (library->d_debug, d_base_type_die, DW_DLA_DIE);
            eris_typeinfo_set_is_readonly (typeinfo, true);
            return typeinfo;
        }

        case DW_TAG_typedef: {
            Dwarf_Die d_base_type_die =
                die_get_die_reference_attribute (library,
                                                 d_type_die,
                                                 DW_AT_type,
                                                 d_error);
            if (!d_base_type_die)
                return NULL;

            ErisTypeInfo *typeinfo = die_to_typeinfo (library,
                                                      d_base_type_die,
                                                      d_error);

            if (eris_typeinfo_is_valid (typeinfo)) {
                Dwarf_Error d_name_error;
                const char *name = die_get_string_attribute (library,
                                                             d_type_die,
                                                             DW_AT_name,
                                                             &d_name_error);
                if (name) eris_typeinfo_set_name (typeinfo, name);
            }
            dwarf_dealloc (library->d_debug, d_base_type_die, DW_DLA_DIE);
            return typeinfo;
        }
        default:
            return NULL;
    }
}


static inline const char*
dw_errmsg(Dwarf_Error d_error)
{
    return d_error ? dwarf_errmsg (d_error) : "no libdwarf error";
}


static int
make_variable_wrapper (lua_State   *L,
                       ErisLibrary *library,
                       void        *address,
                       const char  *name,
                       Dwarf_Die    d_die,
                       Dwarf_Half   d_tag)
{
    Dwarf_Error d_error = 0;
    Dwarf_Die d_type_die = die_get_die_reference_attribute (library,
                                                            d_die,
                                                            DW_AT_type,
                                                            &d_error);
    if (!d_type_die) {
        return luaL_error (L, "%s: Could not obtain DW_AT_type DIE (%s)",
                           name, dw_errmsg (d_error));
    }

    ErisTypeInfo *typeinfo = die_to_typeinfo (library,
                                              d_type_die,
                                              &d_error);
    dwarf_dealloc (library->d_debug, d_type_die, DW_DLA_DIE);
    dwarf_dealloc (library->d_debug, d_die, DW_DLA_DIE);

    if (!typeinfo || !eris_typeinfo_is_valid (typeinfo)) {
        return luaL_error (L, "%s: Could not convert DIE to ErisTypeInfo (%s)",
                           name, dw_errmsg (d_error));
    }

    ErisVariable *ev = lua_newuserdata (L, sizeof (ErisVariable));
    eris_symbol_init ((ErisSymbol*) ev, library, address, name);
    ev->typeinfo = typeinfo;
    ev->n_items  = 1; /* TODO: Set for arrays. */
    luaL_setmetatable (L, ERIS_VARIABLE);
    TRACE ("new ErisVariable* at %p (%p:%s)\n", ev, library, name);
    return 1;
}


static int
eris_library_index (lua_State *L)
{
    ErisLibrary *e = to_eris_library (L, 1);
    const char *name = luaL_checkstring (L, 2);
    const char *error = "unknown error";

    /* Find the entry point of the function. */
    void *address = dlsym (e->dl, name);
    if (!address) {
        error = dlerror ();
        goto return_error;
    }

    Dwarf_Error d_error = 0;
    Dwarf_Die d_die = lookup_die (e, name, &d_error);
    if (!d_die) {
        return luaL_error (L, "could not look up DWARF debug information "
                           "for symbol '%s' (library %p; %s)",
                           name, e, dw_errmsg (d_error));
    }

#if ERIS_RUNTIME_CHECKS
    /*
     * Check that the variable/function is an exported global, i.e. it has
     * the DW_AT_external attribute. If the attribute is missing, assume
     * that we can proceed.
     *
     * Note that lookup_die() uses the list of DWARF globals, all DIEs must
     * be always exported globals. Also, dlsym() will not resolve private
     * symbols. So, this is built as an optional a sanity check.
     */
    bool symbol_is_private = true;
    Dwarf_Attribute d_attr = 0;
    if (dwarf_attr (d_die,
                    DW_AT_external,
                    &d_attr,
                    &d_error) == DW_DLV_OK) {
        Dwarf_Bool d_flag_external;
        if (dwarf_formflag (d_attr,
                            &d_flag_external,
                            &d_error) == DW_DLV_OK) {
            symbol_is_private = !d_flag_external;
        }
        dwarf_dealloc (e->d_debug, d_attr, DW_DLA_ATTR);
    }
    CHECK (!symbol_is_private);
#endif /* ERIS_RUNTIME_CHECKS */

    if (symbol_is_private) {
        error = "symbol is private";
        goto dealloc_die_and_return_error;
    }

    /* Obtain the DIE type tag. */
    Dwarf_Half d_tag;
    if (dwarf_tag (d_die,
                   &d_tag,
                   &d_error) != DW_DLV_OK) {
        dwarf_dealloc (e->d_debug, d_die, DW_DLA_DIE);
        return luaL_error (L, "could not obtain DWARF debug information tag "
                           "for symbol '%s' (library %p)", name, e);
    }

    switch (d_tag) {
        case DW_TAG_reference_type:
            /* TODO: Implement dereferencing of references. */
            return luaL_error (L, "DW_TAG_reference_type: unimplemented");

        case DW_TAG_inlined_subroutine: /* TODO: Check whether inlines work. */
        case DW_TAG_entry_point:
        case DW_TAG_subprogram:
            return make_function_wrapper (L, e, address, name, d_die, d_tag);

        case DW_TAG_variable:
            return make_variable_wrapper (L, e, address, name, d_die, d_tag);

        default:
            error = "unsupported debug info kind (not function or data)";
            /* fall-through */
    }

dealloc_die_and_return_error:
    dwarf_dealloc (e->d_debug, d_die, DW_DLA_DIE);
return_error:
    lua_pushnil (L);
    lua_pushstring (L, error);
    return 2;
}

static int
eris_library_eq (lua_State *L)
{
    ErisLibrary *el_self  = to_eris_library (L, 1);
    ErisLibrary *el_other = to_eris_library (L, 2);
    lua_pushboolean (L, el_self == el_other);
    return 1;
}


/* Methods for ErisLibrary userdatas. */
static const luaL_Reg eris_library_methods[] = {
    { "__gc",       eris_library_gc       },
    { "__tostring", eris_library_tostring },
    { "__index",    eris_library_index    },
    { "__eq",       eris_library_eq       },
    { NULL, NULL }
};


static int
eris_function_call (lua_State *L)
{
    ErisFunction *ef = to_eris_function (L);
    TRACE ("%p (%p:%s)\n", ef, ef->library, ef->name);
    return 0;
}

static int
eris_function_gc (lua_State *L)
{
    ErisFunction *ef = to_eris_function (L);
    TRACE ("%p (%p:%s)\n", ef, ef->library, ef->name);

    dwarf_dealloc (ef->library->d_debug, ef->d_die, DW_DLA_DIE);
    ef->d_die = 0;

    eris_symbol_free ((ErisSymbol*) ef);
    return 0;
}

static int
eris_function_tostring (lua_State *L)
{
    ErisFunction *ef = to_eris_function (L);
    lua_pushfstring (L, "eris.function (%p:%s)", ef->library, ef->name);
    return 1;
}

static int
eris_function_name (lua_State *L)
{
    ErisFunction *ef = to_eris_function (L);
    lua_pushstring (L, ef->name);
    return 1;
}

static int
eris_function_library (lua_State *L)
{
    ErisFunction *ef = to_eris_function (L);
    eris_library_push_userdata (L, ef->library);
    return 1;
}

/* Methods for ErisFunction userdatas. */
static const luaL_Reg eris_function_methods[] = {
    { "__call",     eris_function_call     },
    { "__gc",       eris_function_gc       },
    { "__tostring", eris_function_tostring },
    { "name",       eris_function_name     },
    { "library",    eris_function_library  },
    { NULL, NULL }
};


/* Methods for ErisVariable userdatas. */
static int
eris_variable_gc (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);

#if ERIS_TRACE
    if (ev->library && ev->name) {
        TRACE ("eris.variable<%s>(%p:%s)\n",
               eris_typeinfo_name (ev->typeinfo),
               ev->library,
               ev->name);
    } else {
        TRACE ("eris.variable<%s>(%p)\n",
               eris_typeinfo_name (ev->typeinfo),
               ev->address);
    }
#endif /* ERIS_TRACE */

    eris_symbol_free ((ErisSymbol*) ev);
    return 0;
}

static int
eris_variable_tostring (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    if (ev->library && ev->name) {
        lua_pushfstring (L,
                         "eris.variable<%s>(%p:%s)",
                         eris_typeinfo_name (ev->typeinfo),
                         ev->library, ev->name);
    } else {
        lua_pushfstring (L,
                         "eris.variable<%s>(%p)",
                         eris_typeinfo_name (ev->typeinfo),
                         ev->address);
    }
    return 1;
}

static int
eris_variable_len (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    lua_pushinteger (L, ev->n_items);
    return 1;
}

static int
eris_variable_set (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    if (eris_typeinfo_is_readonly (ev->typeinfo)) {
        return luaL_error (L, "read-only variable (%p:%s)",
                           ev->library, ev->name);
    }

    lua_Integer index = 1;
    if (lua_gettop (L) == 3) {
        index = luaL_checkinteger (L, 2);
        if (index == 0) {
            return luaL_error (L, "0 is not a valid index");
        }
        /* Adjust index, do bounds checking. */
        if (index < 0) index += ev->n_items;
        if (index <= 0 || index > ev->n_items) {
            return luaL_error (L, "index %i out of bounds (effective=%i, length=%i)",
                               luaL_checkinteger (L, 2), index, ev->n_items);
        }
    } else if (lua_gettop (L) != 2) {
        return luaL_error (L, "wrong number of function arguments");
    }

    /* Convert from 1-based to 0-based indexing. */
    index--;

#define SET_INTEGER(suffix, ctype) \
        case ERIS_TYPE_ ## suffix: \
            ((ctype*) ev->address)[index] = (ctype) luaL_checkinteger (L, -1); \
            break;
#define SET_FLOAT(suffix, ctype) \
        case ERIS_TYPE_ ## suffix: \
            ((ctype*) ev->address)[index] = (ctype) luaL_checknumber (L, -1); \
            break;

    switch (eris_typeinfo_type (ev->typeinfo)) {
        INTEGER_TYPES (SET_INTEGER)
        FLOAT_TYPES (SET_FLOAT)
        default:
            return luaL_error (L, "no setter for variable (%p:%s)",
                               ev->library, ev->name);
    }

#undef SET_INTEGER
#undef SET_FLOAT

    lua_settop (L, 1);
    return 1;
}

static int
eris_variable_get (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    lua_Integer index = luaL_optinteger (L, 2, 1);

    /* Adjust index, do bounds checking. */
    if (index < 0) index += ev->n_items;
    if (index <= 0 || index > ev->n_items) {
        return luaL_error (L, "index %i out of bounds (effective=%i, length=%i)",
                           luaL_checkinteger (L, 2), index, ev->n_items);
    }
    /* Convert from 1-based to 0-based indexing. */
    index--;

#define GET_INTEGER(suffix, ctype) \
        case ERIS_TYPE_ ## suffix: \
            lua_pushinteger (L, ((ctype*) ev->address)[index]); \
            break;
#define GET_FLOAT(suffix, ctype) \
        case ERIS_TYPE_ ## suffix: \
            lua_pushnumber (L, ((ctype*) ev->address)[index]); \
            break;

    switch (eris_typeinfo_type (ev->typeinfo)) {
        INTEGER_TYPES (GET_INTEGER)
        FLOAT_TYPES (GET_FLOAT)
        default:
            return luaL_error (L, "no getter for variable (%p:%s)",
                               ev->library, ev->name);
    }

#undef GET_INTEGER
#undef GET_FLOAT
    return 1;
}

static int
eris_variable_name (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    lua_pushstring (L, ev->name);
    return 1;
}

static int
eris_variable_type (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    eris_typeinfo_push_userdata (L, ev->typeinfo);
    return 1;
}

static int
eris_variable_library (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    eris_library_push_userdata (L, ev->library);
    return 1;
}

static const luaL_Reg eris_variable_methods[] = {
    { "__gc",       eris_variable_gc        },
    { "__tostring", eris_variable_tostring  },
    { "__len",      eris_variable_len       },
    { "get",        eris_variable_get       },
    { "set",        eris_variable_set       },
    { "name",       eris_variable_name      },
    { "type",       eris_variable_type      },
    { "library",    eris_variable_library   },
    { NULL, NULL },
};


static void
create_meta (lua_State *L)
{
    /* ErisLibrary */
    luaL_newmetatable (L, ERIS_LIBRARY);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, eris_library_methods, 0);
    lua_pop (L, 1);

    /* ErisFunction */
    luaL_newmetatable (L, ERIS_FUNCTION);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, eris_function_methods, 0);
    lua_pop (L, 1);

    /* ErisVariable */
    luaL_newmetatable (L, ERIS_VARIABLE);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, eris_variable_methods, 0);
    lua_pop (L, 1);

    /* ErisTypeInfo */
    luaL_newmetatable (L, ERIS_TYPEINFO);
    luaL_setfuncs (L, eris_typeinfo_methods, 0);
    lua_pop (L, 1);
}


static int
eris_load (lua_State *L)
{
    size_t name_length;
    const char *name = luaL_checklstring (L, 1, &name_length);
    char path[PATH_MAX];

    errno = 0;
    if (!find_library (name, path)) {
        return luaL_error (L, "could not find library '%s' (%s)", name,
                           (errno == 0) ? "path too long" : strerror (errno));
    }
    TRACE ("found %s -> %s\n", name, path);

    void *dl = dlopen (path, RTLD_NOW | RTLD_GLOBAL);
    if (!dl) {
        return luaL_error (L, "could not link library '%s' (%s)",
                           path, dlerror ());
    }

    int fd = open (path, O_RDONLY, 0);
    if (fd < 0) {
        dlclose (dl);
        return luaL_error (L, "could not open '%s' for reading (%s)",
                           path, strerror (errno));
    }

    Dwarf_Handler d_error_handler = 0;
    Dwarf_Ptr d_error_argument = 0;
    Dwarf_Error d_error;
    Dwarf_Debug d_debug;

    if (dwarf_init (fd,
                    DW_DLC_READ,
                    d_error_handler,
                    d_error_argument,
                    &d_debug,
                    &d_error) != DW_DLV_OK) {
        close (fd);
        dlclose (dl);
        return luaL_error (L, "error reading debug information from '%s' (%s)",
                           path, dwarf_errmsg (d_error));
    }

    Dwarf_Signed d_num_globals = 0;
    Dwarf_Global *d_globals = NULL;
    if (dwarf_get_globals (d_debug,
                           &d_globals,
                           &d_num_globals,
                           &d_error) != DW_DLV_OK) {
        Dwarf_Error d_finish_error = 0;
        dwarf_finish (d_debug, &d_finish_error);
        close (fd);
        dlclose (dl);
        return luaL_error (L, "cannot read globals (%s)", dw_errmsg (d_error));
    }
    TRACE ("found %ld globals\n", (long) d_num_globals);

#if ERIS_TRACE
    for (Dwarf_Signed i = 0; i < d_num_globals; i++) {
        char *name = NULL;
        Dwarf_Error d_name_error = 0;
        if (dwarf_globname (d_globals[i], &name, &d_name_error) == DW_DLV_OK) {
            TRACE ("-- [%li] %s\n", (long) i, name);
            dwarf_dealloc (d_debug, name, DW_DLA_STRING);
        } else {
            TRACE ("-- [%li] ERROR: %s\n", (long) i, dw_errmsg (d_name_error));
        }
    }
#endif /* ERIS_TRACE */

    Dwarf_Signed d_num_types = 0;
    Dwarf_Type *d_types = NULL;
    if (dwarf_get_pubtypes (d_debug,
                            &d_types,
                            &d_num_types,
                            &d_error) != DW_DLV_OK) {
        Dwarf_Error d_finish_error = 0;
        dwarf_globals_dealloc (d_debug, d_globals, d_num_globals);
        dwarf_finish (d_debug, &d_finish_error);
        close (fd);
        dlclose (dl);
        return luaL_error (L, "cannot read types (%s)", dw_errmsg (d_error));
    }
    TRACE ("found %ld types\n", (long) d_num_types);

#if ERIS_TRACE
    for (Dwarf_Signed i = 0; i < d_num_types; i++) {
        char *name = NULL;
        Dwarf_Error d_name_error = 0;
        if (dwarf_pubtypename (d_types[i], &name, &d_name_error) == DW_DLV_OK) {
            TRACE ("-- [%li] %s\n", (long) i, name);
            dwarf_dealloc (d_debug, name, DW_DLA_STRING);
        } else {
            TRACE ("-- [%li] ERROR: %s\n", (long) i, dw_errmsg (d_name_error));
        }
    }
#endif /* ERIS_TRACE */

    ErisLibrary *el = calloc (1, sizeof (ErisLibrary));
    el->fd = fd;
    el->dl = dl;
    el->d_debug = d_debug;
    el->d_globals = d_globals;
    el->d_num_globals = d_num_globals;
    el->d_types = d_types;
    el->d_num_types = d_num_types;
    eris_library_push_userdata (L, el);
    TRACE ("new ErisLibrary* at %p\n", el);
    return 1;
}


static int
eris_type (lua_State *L)
{
    ErisLibrary *el = to_eris_library (L, 1);
    const char *name = luaL_checkstring (L, 2);

    Dwarf_Error d_error = 0;
    Dwarf_Die d_type_die = lookup_tue (el, name, &d_error);
    if (!d_type_die) {
        return luaL_error (L, "could not look up DWARF debug information "
                           "for type '%s' (library: %p; %s)",
                           name, el, dw_errmsg (d_error));
    }

    ErisTypeInfo *typeinfo = die_to_typeinfo (el,
                                              d_type_die,
                                              &d_error);
    dwarf_dealloc (el->d_debug, d_type_die, DW_DLA_DIE);

    if (!typeinfo || !eris_typeinfo_is_valid (typeinfo)) {
        return luaL_error (L, "%s: Could not convert TUE to ErisTypeInfo (%s)",
                           name, dw_errmsg (d_error));
    }

    eris_typeinfo_push_userdata (L, typeinfo);
    return 1;
}


static const luaL_Reg erislib[] = {
    { "load", eris_load },
    { "type", eris_type },
    { NULL, NULL },
};


LUAMOD_API int
luaopen_eris (lua_State *L)
{
    eris_trace_setup ();

    (void) elf_version (EV_NONE);
    if (elf_version (EV_CURRENT) == EV_NONE)
        return luaL_error (L, "outdated libelf version");

    luaL_newlib (L, erislib);
    create_meta (L);
    return 1;
}


static Dwarf_Die
lookup_die (ErisLibrary *el,
            const char  *name,
            Dwarf_Error *d_error)
{
    /*
     * TODO: This performs a linear search. Try to find an alternative way,
     * e.g. using the (optional) DWARF information that correlates entry point
     * addresses with their corresponding DIEs.
     */
    for (Dwarf_Signed i = 0; i < el->d_num_globals; i++) {
        char *global_name;
        Dwarf_Error d_globname_error = 0;
        if (dwarf_globname (el->d_globals[i],
                            &global_name,
                            &d_globname_error) != DW_DLV_OK) {
            TRACE ("skipped malformed global at index %i (%s)\n",
                   (int) i, dw_errmsg (d_globname_error));
            continue;
        }

        const bool found = (strcmp (global_name, name) == 0);
        dwarf_dealloc (el->d_debug, global_name, DW_DLA_STRING);
        global_name = NULL;

        if (found) {
            Dwarf_Off d_offset;
            if (dwarf_global_die_offset (el->d_globals[i],
                                         &d_offset,
                                         d_error) != DW_DLV_OK) {
                TRACE ("could not obtain DIE offset (%s)\n",
                       dw_errmsg (*d_error));
                return NULL;
            }

            Dwarf_Die d_die;
            if (dwarf_offdie (el->d_debug,
                              d_offset,
                              &d_die,
                              d_error) != DW_DLV_OK) {
                TRACE ("could not obtain DIE (%s)\n", dw_errmsg (*d_error));
                return NULL;
            }
            return d_die;
        }
    }

    return NULL;
}


static Dwarf_Die
lookup_tue (ErisLibrary *library,
            const char  *name,
            Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (name);
    CHECK_NOT_NULL (d_error);

    /*
     * TODO: This performs a linear search. Consider using a global type names
     *       cache -- likely, a hash table.
     */
    for (Dwarf_Signed i = 0; i < library->d_num_types; i++) {
        char *type_name;
        Dwarf_Error d_typename_error = 0;
        if (dwarf_pubtypename (library->d_types[i],
                               &type_name,
                               &d_typename_error) != DW_DLV_OK) {
            TRACE ("skipped malformed type at index %li (%s)\n",
                   (long) i, dw_errmsg (d_typename_error));
            continue;
        }

        const bool found = (strcmp (type_name, name) == 0);
        dwarf_dealloc (library->d_debug, type_name, DW_DLA_STRING);
        type_name = NULL;

        if (found) {
            Dwarf_Off d_offset;
            if (dwarf_pubtype_die_offset (library->d_types[i],
                                          &d_offset,
                                          d_error) != DW_DLV_OK) {
                TRACE ("could not obtain TUE offset (%s)\n",
                       dw_errmsg (*d_error));
                return NULL;
            }

            Dwarf_Die d_die;
            if (dwarf_offdie (library->d_debug,
                              d_offset,
                              &d_die,
                              d_error) != DW_DLV_OK) {
                TRACE ("could not obtain TUE (%s)\n", dw_errmsg (*d_error));
                return NULL;
            }
            return d_die;
        }
    }

    return NULL;
}
