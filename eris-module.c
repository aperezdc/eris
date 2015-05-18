/*
 * eris-module.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"

#include "eris-trace.h"
#include "eris-util.h"

#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
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


typedef enum {
    ERIS_TYPE_INTEGRAL,
} ErisTypeKind;


typedef enum {
    ERIS_TYPE_SIGNED  = 1 << 1,
    ERIS_TYPE_CONST   = 1 << 2,
} ErisTypeFlags;


typedef struct {
    ErisTypeKind  kind;
    ErisTypeFlags flags;
    uint16_t      size; /* sizeof (type) */
} ErisTypeInfo;


/*
 * Data needed for each library loaded by "eris.load()".
 */
typedef struct {
    int           fd;
    void         *dl;
    intptr_t      dl_diff;

    Dwarf_Debug   d_debug;
    Dwarf_Global *d_globals;
    Dwarf_Signed  d_num_globals;
} ErisLibrary;


static const char ERIS_LIBRARY[]  = "org.perezdecastro.eris.Library";


static inline ErisLibrary*
to_eris_library (lua_State *L)
{
    return (ErisLibrary*) luaL_checkudata (L, 1, ERIS_LIBRARY);
}


static bool find_library_base_address (ErisLibrary *el);
static Dwarf_Die lookup_die (ErisLibrary *el, const void *address, const char *name);


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
    lua_CFunction getter;
    lua_CFunction setter;
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



static int
eris_library_gc (lua_State *L)
{
    ErisLibrary *el = to_eris_library (L);
    TRACE ("%p\n", el);

    if (el->d_globals) {
        dwarf_globals_dealloc (el->d_debug, el->d_globals, el->d_num_globals);
    }

    Dwarf_Error d_error;
    dwarf_finish (el->d_debug, &d_error);

    close (el->fd);
    dlclose (el->dl);

    return 0;
}


static int
eris_library_tostring (lua_State *L)
{
    ErisLibrary *el = to_eris_library (L);
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
    symbol->library = library;
    symbol->name = strdup (name);
    symbol->address = address;
}


static inline void
eris_symbol_free (ErisSymbol *symbol)
{
    free (symbol->name);
    memset (symbol, 0x00, sizeof (ErisSymbol));
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


static bool
find_variable_callbacks (const ErisTypeInfo *typeinfo,
                         lua_CFunction      *getter,
                         lua_CFunction      *setter);


static bool
base_type_to_typeinfo (ErisLibrary  *library,
                       Dwarf_Die     d_type_die,
                       ErisTypeInfo *typeinfo)
{
    bool success = false;

    memset (typeinfo, 0x00, sizeof (ErisTypeInfo));
    typeinfo->kind = ERIS_TYPE_INTEGRAL;

    Dwarf_Error d_error = 0;
    Dwarf_Attribute d_attr = 0;
    if (dwarf_attr (d_type_die,
                    DW_AT_encoding,
                    &d_attr,
                    &d_error) != DW_DLV_OK)
        goto return_from_function;

    Dwarf_Unsigned d_uval = 0;
    if (dwarf_formudata (d_attr, &d_uval, &d_error) != DW_DLV_OK)
        goto dealloc_attribute;

    switch (d_uval) {
        case DW_ATE_signed:
            typeinfo->flags |= ERIS_TYPE_SIGNED;
            break;
        case DW_ATE_unsigned:
            typeinfo->flags &= ~ERIS_TYPE_SIGNED;
            break;
        default:
            goto dealloc_attribute;
    }

    dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);
    if (dwarf_attr (d_type_die,
                    DW_AT_byte_size,
                    &d_attr,
                    &d_error) != DW_DLV_OK)
        goto return_from_function;

    if (dwarf_formudata (d_attr, &d_uval, &d_error) != DW_DLV_OK)
        goto dealloc_attribute;

    typeinfo->size = d_uval;
    success = true;

dealloc_attribute:
    dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);
return_from_function:
    return success;
}


static bool
die_to_typeinfo (ErisLibrary  *library,
                 Dwarf_Die     d_type_die,
                 ErisTypeInfo *typeinfo)
{
    Dwarf_Half d_tag;
    Dwarf_Error d_error;
    if (dwarf_tag (d_type_die,
                   &d_tag,
                   &d_error) != DW_DLV_OK)
        return false;

    switch (d_tag) {
        case DW_TAG_base_type:
            return base_type_to_typeinfo (library, d_type_die, typeinfo);

        default:
            return false;
    }
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
    Dwarf_Attribute d_attr = 0;
    if (dwarf_attr (d_die,
                    DW_AT_type,
                    &d_attr,
                    &d_error) != DW_DLV_OK) {
        dwarf_dealloc (library->d_debug, d_die, DW_DLA_DIE);
        return luaL_error (L, "could not obtain DW_AT_type attribute");
    }

    Dwarf_Off d_offset = 0;
    if (dwarf_global_formref (d_attr,
                              &d_offset,
                              &d_error) != DW_DLV_OK) {
        dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);
        dwarf_dealloc (library->d_debug, d_die, DW_DLA_DIE);
        return luaL_error (L, "could not obtain DW_AT_type DIE offset");
    }
    dwarf_dealloc (library->d_debug, d_attr, DW_DLA_ATTR);

    Dwarf_Die d_type_die = 0;
    if (dwarf_offdie (library->d_debug,
                      d_offset,
                      &d_type_die,
                      &d_error) != DW_DLV_OK) {
        dwarf_dealloc (library->d_debug, d_die, DW_DLA_DIE);
        return luaL_error (L, "could not obtain DW_AT_type DIE");
    }

    ErisTypeInfo typeinfo;
    bool success = die_to_typeinfo (library, d_type_die, &typeinfo);
    dwarf_dealloc (library->d_debug, d_type_die, DW_DLA_DIE);
    dwarf_dealloc (library->d_debug, d_die, DW_DLA_DIE);

    if (!success) {
        return luaL_error (L, "Could not convert DIE to ErisTypeInfo");
    }

    lua_CFunction getter, setter;
    if (!find_variable_callbacks (&typeinfo, &getter, &setter)) {
        return luaL_error (L, "Unsupported variable type");
    }

    /*
     * TODO: Check whether the DIE signals the variable as a constant,
     *       and make the setter NULL to avoid writes.
     */

    ErisVariable *ev = lua_newuserdata(L, sizeof (ErisVariable));
    eris_symbol_init ((ErisSymbol*) ev, library, address, name);
    ev->getter = getter;
    ev->setter = setter;
    luaL_setmetatable (L, ERIS_VARIABLE);
    TRACE ("new ErisVariable* at %p (%p:%s)\n", ev, library, name);
    return 1;
}


static int
eris_library_lookup (lua_State *L)
{
    ErisLibrary *e = to_eris_library (L);
    const char *name = luaL_checkstring (L, 2);
    const char *error = "unknown error";

    /* Find the entry point of the function. */
    void *address = dlsym (e->dl, name);
    if (!address) {
        error = dlerror ();
        goto return_error;
    }

    Dwarf_Die d_die = lookup_die (e, address, name);
    if (!d_die) {
        return luaL_error (L, "could not look up DWARF debug information "
                           "for symbol '%s' (library %p)", name, e);
    }

    /*
     * Check that the variable/function is an exported global, i.e. it has
     * the DW_AT_external attribute. If the attribute is missing, assume
     * that we can proceed.
     *
     * TODO: Provided that lookup_die() uses the list of DWARF globals, all
     *       DIEs must be always exported globals. Maybe this can be turned
     *       into a debug-build-only check. Plus: dlsym() won't resolve it.
     */
    bool symbol_is_private = true;
    Dwarf_Error d_error = 0;
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


/* Methods for ErisLibrary userdatas. */
static const luaL_Reg eris_library_methods[] = {
    { "__gc",       eris_library_gc       },
    { "__tostring", eris_library_tostring },
    { "lookup",     eris_library_lookup   },
    { NULL, NULL }
};


/* Methods for ErisFunction userdatas. */
static const luaL_Reg eris_function_methods[] = {
    { "__call",     eris_function_call     },
    { "__gc",       eris_function_gc       },
    { "__tostring", eris_function_tostring },
    { NULL, NULL }
};


/* Methods for ErisVariable userdatas. */
static int
eris_variable_gc (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    TRACE ("%p (%p:%s)\n", ev, ev->library, ev->name);
    eris_symbol_free ((ErisSymbol*) ev);
    return 0;
}

static int
eris_variable_tostring (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    lua_pushfstring (L, "eris.variable (%p:%s)", ev->library, ev->name);
    return 1;
}

static int
eris_variable_set (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    return (*ev->setter) (L);
}

static int
eris_variable_get (lua_State *L)
{
    ErisVariable *ev = to_eris_variable (L);
    return (*ev->getter) (L);
}


static const luaL_Reg eris_variable_methods[] = {
    { "__gc",       eris_variable_gc       },
    { "__tostring", eris_variable_tostring },
    { "get",        eris_variable_get      },
    { "set",        eris_variable_set      },
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
    luaL_setfuncs (L, eris_function_methods, 0);
    lua_pop (L, 1);

    /* ErisVariable */
    luaL_newmetatable (L, ERIS_VARIABLE);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, eris_variable_methods, 0);
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
        dwarf_finish (d_debug, &d_error);
        close (fd);
        dlclose (dl);
        /* TODO: Provide a better error message. */
        return luaL_error (L, "cannot read globals");
    }
    TRACE ("found %ld globals\n", (long) d_num_globals);
#if defined(ERIS_TRACE) && ERIS_TRACE > 0
    for (Dwarf_Signed i = 0; i < d_num_globals; i++) {
        char *name = NULL;
        if (dwarf_globname (d_globals[i], &name, &d_error) == DW_DLV_OK) {
            TRACE ("-- %s\n", name);
            dwarf_dealloc (d_debug, name, DW_DLA_STRING);
            name = NULL;
        }
    }
#endif /* ERIS_TRACE > 0 */

    ErisLibrary *el = lua_newuserdata (L, sizeof (ErisLibrary));
    el->fd = fd;
    el->dl = dl;
    el->d_debug = d_debug;
    el->d_globals = d_globals;
    el->d_num_globals = d_num_globals;

    if (!find_library_base_address (el)) {
        dwarf_finish (d_debug, &d_error);
        close (fd);
        dlclose (dl);
#ifdef ERIS_USE_LINK_H
        return luaL_error (L, "cannot determine library load address (%s)",
                           dlerror ());
#else
        return luaL_error (L, "cannot determine library load address");
#endif /* ERIS_USE_LINK_H */
    }

    luaL_setmetatable (L, ERIS_LIBRARY);
    TRACE ("new ErisLibrary* at %p\n", el);
    return 1;
}


static const luaL_Reg erislib[] = {
    { "load", eris_load },
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


#ifdef ERIS_USE_LINK_H
# include <link.h>

static bool
find_library_base_address (ErisLibrary *el)
{
    struct link_map *map = NULL;
    if (dlinfo (el->dl, RTLD_DI_LINKMAP, &map) != 0) {
        return false;
    }
    el->dl_diff = map->l_addr;
    TRACE ("diff = %#llx (link_map)\n", (long long) el->dl_diff);
    return true;
}

#else
# error No method chosen to determe the load address of libraries
#endif /* ERIS_USE_LINK_H */


static Dwarf_Die
lookup_die (ErisLibrary *el,
            const void  *address,
            const char  *name)
{
    /*
     * TODO: This performs a linear search. Try to find an alternative way,
     * e.g. using the (optional) DWARF information that correlates entry point
     * addresses with their corresponding DIEs.
     */
    for (Dwarf_Signed i = 0; i < el->d_num_globals; i++) {
        char *global_name;
        Dwarf_Error d_error;
        if (dwarf_globname (el->d_globals[i],
                            &global_name,
                            &d_error) != DW_DLV_OK) {
            TRACE ("skipped malformed global at index %i\n", (int) i);
            continue;
        }

        const bool found = (strcmp (global_name, name) == 0);
        dwarf_dealloc (el->d_debug, global_name, DW_DLA_STRING);
        global_name = NULL;

        if (found) {
            Dwarf_Error d_error;
            Dwarf_Off d_offset;

            if (dwarf_global_die_offset (el->d_globals[i],
                                         &d_offset,
                                         &d_error) != DW_DLV_OK) {
                /* TODO: Print Dwarf_Error to trace log. */
                TRACE ("could not obtain DIE offset\n");
                return NULL;
            }

            Dwarf_Die d_die;
            if (dwarf_offdie (el->d_debug,
                              d_offset,
                              &d_die,
                              &d_error) != DW_DLV_OK) {
                /* TODO: Print Dwarf_Error to trace log. */
                TRACE ("could not obtain DIE\n");
                return NULL;
            }
            return d_die;
        }
    }

    return NULL;
}


#define INTEGER_TYPES(F) \
    F (int8_t,   true)   \
    F (uint8_t,  false)  \
    F (int16_t,  true)   \
    F (uint16_t, false)  \
    F (int32_t,  true)   \
    F (uint32_t, false)


#define MAKE_INTEGER_GETTER_AND_SETTER(ctype, is_signed)     \
    static int eris_variable_get__ ## ctype (lua_State *L) { \
        ErisVariable *ev = to_eris_variable (L);             \
        lua_pushinteger (L, *((ctype *) ev->address));       \
        return 1;                                            \
    }                                                        \
    static int eris_variable_set__ ## ctype (lua_State *L) { \
        ErisVariable *ev = to_eris_variable (L);             \
        *((ctype *) ev->address) =                           \
            (ctype) luaL_checkinteger (L, 2);                \
        return 0;                                            \
    }

INTEGER_TYPES (MAKE_INTEGER_GETTER_AND_SETTER)

#undef MAKE_INTEGER_GETTER_AND_SETTER


static const struct {
    const char   *typename;
    lua_CFunction getter;
    lua_CFunction setter;
    ErisTypeInfo  typeinfo;
} builtin_type_callbacks[] = {
#define INTEGER_GETTER_ITEM(ctype, is_signed) {         \
    .typename       = #ctype,                           \
    .getter         = eris_variable_get__ ## ctype,     \
    .setter         = eris_variable_set__ ## ctype,     \
    .typeinfo.kind  = ERIS_TYPE_INTEGRAL,               \
    .typeinfo.flags = is_signed ? ERIS_TYPE_SIGNED : 0, \
    .typeinfo.size  = sizeof (ctype) },

    INTEGER_TYPES (INTEGER_GETTER_ITEM)

#undef INTEGER_GETTER_ITEM
};


static bool
typeinfo_equal (const ErisTypeInfo *a,
                const ErisTypeInfo *b)
{
    if (a->kind != b->kind || a->size != b->size)
        return false;

    /* Ignore the ERIS_TYPE_CONST flag. */
    const ErisTypeFlags a_flags = a->flags & ~ERIS_TYPE_CONST;
    const ErisTypeFlags b_flags = b->flags & ~ERIS_TYPE_CONST;

    return a_flags == b_flags;
}


static bool
find_variable_callbacks (const ErisTypeInfo *typeinfo,
                         lua_CFunction      *getter,
                         lua_CFunction      *setter)
{
    for (size_t i = 0; i < LENGTH_OF (builtin_type_callbacks); i++) {
        if (typeinfo_equal (typeinfo, &builtin_type_callbacks[i].typeinfo)) {
            if (getter)
                *getter = builtin_type_callbacks[i].getter;
            if (setter)
                *setter = builtin_type_callbacks[i].setter;
            return true;
        }
    }
    return false;
}
