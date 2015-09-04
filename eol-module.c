/*
 * eol-module.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eol-libdwarf.h"
#include "eol-lua.h"
#include "eol-typing.h"
#include "eol-typecache.h"
#include "eol-trace.h"
#include "eol-util.h"

#include <libelf.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

typedef struct _EolFunction EolFunction;
#include "eol-fcall.h"

#ifndef EOL_LIB_SUFFIX
#define EOL_LIB_SUFFIX ".so"
#endif /* !EOL_LIB_SUFFIX */


typedef struct EolSpecial EolSpecial;
typedef enum {
    EOL_SPECIAL_NAME,
    EOL_SPECIAL_TYPE,
    EOL_SPECIAL_VALUE,
    EOL_SPECIAL_LIBRARY,
} EolSpecialCode;

#include "specials.inc"


typedef struct _EolLibrary  EolLibrary;

/*
 * Data needed for each library loaded by "eol.load()".
 */
struct _EolLibrary {
    REF_COUNTER;

    char         *path;

    int           fd;
    void         *dl;

    Dwarf_Debug   d_debug;
    Dwarf_Global *d_globals;
    Dwarf_Signed  d_num_globals;
    Dwarf_Type   *d_types;
    Dwarf_Signed  d_num_types;

    EolTypeCache  type_cache;
    EolLibrary   *next;
};


static EolLibrary *library_list = NULL;


static const char EOL_LIBRARY[]  = "org.perezdecastro.eol.Library";

static void library_free (EolLibrary*);
REF_COUNTER_FUNCTIONS (EolLibrary, library, static inline)


static inline void
library_push_userdata (lua_State *L, EolLibrary *el)
{
    CHECK_NOT_NULL (el);

    EolLibrary **elp = lua_newuserdata (L, sizeof (EolLibrary*));
    *elp = library_ref (el);
    luaL_setmetatable (L, EOL_LIBRARY);
}

static inline EolLibrary*
to_eol_library (lua_State *L, int index)
{
    return *((EolLibrary**) luaL_checkudata (L, index, EOL_LIBRARY));
}


static const char EOL_TYPEINFO[] = "org.perezdecastro.eol.TypeInfo";

static inline void
typeinfo_push_userdata (lua_State *L, const EolTypeInfo *ti)
{
    CHECK_NOT_NULL (ti);

    const EolTypeInfo **tip = lua_newuserdata (L, sizeof (const EolTypeInfo*));
    *tip = ti;
    luaL_setmetatable (L, EOL_TYPEINFO);
}

static inline const EolTypeInfo*
to_eol_typeinfo (lua_State *L, int index)
{
    return *((const EolTypeInfo**) luaL_checkudata (L, index, EOL_TYPEINFO));
}


static void
typeinfo_tobuffer (luaL_Buffer       *b,
                   const EolTypeInfo *typeinfo,
                   bool               verbose)
{
    CHECK_NOT_NULL (b);
    CHECK_NOT_NULL (typeinfo);

    bool has_name = eol_typeinfo_name (typeinfo) != NULL;

    switch (eol_typeinfo_type (typeinfo)) {
        case EOL_TYPE_CONST:
            luaL_addstring (b, "const ");
            typeinfo_tobuffer (b, eol_typeinfo_base (typeinfo), false);
            break;

        case EOL_TYPE_TYPEDEF:
            if (verbose) {
                luaL_addstring (b, "typedef ");
                typeinfo_tobuffer (b, eol_typeinfo_base (typeinfo), false);
                luaL_addchar (b, ' ');
            }
            luaL_addstring (b, eol_typeinfo_name (typeinfo));
            break;

        case EOL_TYPE_POINTER:
            typeinfo_tobuffer (b, eol_typeinfo_base (typeinfo), false);
            luaL_addchar (b, '*');
            break;

        case EOL_TYPE_ARRAY:
            typeinfo_tobuffer (b, eol_typeinfo_base (typeinfo), false);
            luaL_addchar (b, '[');
            lua_pushinteger (b->L, eol_typeinfo_array_n_items (typeinfo));
            luaL_addvalue (b);
            luaL_addchar (b, ']');
            break;

        case EOL_TYPE_ENUM:
            luaL_addstring (b, "enum");
            if (has_name) {
                luaL_addchar (b, ' ');
                luaL_addstring (b, eol_typeinfo_name (typeinfo));
            }
            if (verbose || !has_name) {
                luaL_addstring (b, " {");
                const uint32_t n = eol_typeinfo_compound_n_members (typeinfo);
                for (uint32_t i = 0; i < n; i++) {
                    if (i > 0) luaL_addstring (b, ",");
                    const EolTypeInfoMember *member =
                            eol_typeinfo_compound_const_member (typeinfo, i);
                    luaL_addchar (b, ' ');
                    luaL_addstring (b, member->name);
                }
                luaL_addstring (b, " }");
            }
            break;

        case EOL_TYPE_STRUCT:
            luaL_addstring (b, "struct");
            if (has_name) {
                luaL_addchar (b, ' ');
                luaL_addstring (b, eol_typeinfo_name (typeinfo));
            }
            if (verbose || !has_name) {
                luaL_addstring (b, " {");
                const uint32_t n = eol_typeinfo_compound_n_members (typeinfo);
                for (uint32_t i = 0; i < n; i++) {
                    if (i > 0) luaL_addstring (b, ";");
                    const EolTypeInfoMember *member =
                            eol_typeinfo_compound_const_member (typeinfo, i);
                    luaL_addchar (b, ' ');
                    typeinfo_tobuffer (b, member->typeinfo, false);
                    if (member->name) {
                        luaL_addchar (b, ' ');
                        luaL_addstring (b, member->name);
                    }
                }
                luaL_addstring (b, " }");
            }
            break;

        default:
            if (has_name) {
                luaL_addstring (b, eol_typeinfo_name (typeinfo));
            } else {
                luaL_addstring (b, "(unnamed)");
            }
    }
}


static int
typeinfo_push_stringrep (lua_State         *L,
                         const EolTypeInfo *typeinfo,
                         bool               verbose)
{
    CHECK_NOT_NULL (typeinfo);
    luaL_Buffer b;
    luaL_buffinit (L, &b);
    luaL_addstring (&b, "eol.type (");
    typeinfo_tobuffer (&b, typeinfo, verbose);
    luaL_addchar (&b, ')');
    luaL_pushresult (&b);
    return 1;
}


static int
typeinfo_tostring (lua_State *L)
{
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    return typeinfo_push_stringrep (L, typeinfo, false);
}


static inline const char*
typeinfo_type_string (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

#define RETURN_TYPE_STRING_ITEM(suffix, name, _) \
        case EOL_TYPE_ ## suffix: return #name;

    switch (eol_typeinfo_type (typeinfo)) {
        ALL_TYPES (RETURN_TYPE_STRING_ITEM)
    }

#undef RETURN_TYPE_STRING_ITEM

    CHECK_UNREACHABLE ();
    return NULL;
}


static int
typeinfo_pointerto (lua_State *L)
{
    /*
     * FIXME: This leaks the newly created EolTypeInfo. It should be added to
     *        the type cache, or (even better) look up an existing item from
     *        it before adding it.
     */
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    typeinfo_push_userdata (L, eol_typeinfo_new_pointer (typeinfo));
    return 1;
}


static int
typeinfo_arrayof (lua_State *L)
{
    /*
     * FIXME: This leaks the newly created EolTypeInfo. It should be added to
     *        the type cache, or (even better) look up an existing item from
     *        it before adding it.
     */
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    lua_Integer n_items = luaL_checkinteger (L, 2);
    if (n_items <= 0) {
        return luaL_error (L, "parameter #2 must be a positive integer");
    }
    typeinfo_push_userdata (L, eol_typeinfo_new_array (typeinfo, n_items));
    return 1;
}


static int
typeinfo_index (lua_State *L)
{
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    lua_settop (L, 2);
    if (lua_isinteger (L, 2)) {
        if (!(typeinfo = eol_typeinfo_get_compound (typeinfo))) {
            return luaL_error (L, "type is not a struct or union");
        }

        uint32_t n_members = eol_typeinfo_compound_n_members (typeinfo);
        lua_Integer index = luaL_checkinteger (L, 2);
        if (index < 1 || index > n_members) {
            return luaL_error (L, "index %d out of bounds (length=%d)",
                               index, (lua_Integer) n_members);
        }

        const EolTypeInfoMember *member =
                eol_typeinfo_compound_const_member (typeinfo, index - 1);
        lua_createtable (L, 0, 3);

        lua_pushstring (L, member->name);
        lua_setfield (L, -2, "name");

        if (eol_typeinfo_is_enum (typeinfo)) {
            lua_pushinteger (L, member->value);
            lua_setfield (L, -2, "value");
        } else {
            typeinfo_push_userdata (L, member->typeinfo);
            lua_setfield (L, -2, "type");
            lua_pushinteger (L, member->offset);
            lua_setfield (L, -2, "offset");
        }
    } else {
        const char *field = luaL_checkstring (L, 2);
        if (!strcmp ("name", field)) {
            lua_pushstring (L, eol_typeinfo_name (typeinfo));
        } else if (!strcmp ("sizeof", field)) {
            lua_pushinteger (L, eol_typeinfo_sizeof (typeinfo));
        } else if (!strcmp ("readonly", field)) {
            lua_pushboolean (L, eol_typeinfo_is_readonly (typeinfo));
        } else if (string_equal ("kind", field)) {
            lua_pushstring (L, typeinfo_type_string (typeinfo));
        } else if (string_equal ("type", field)) {
            const EolTypeInfo *base = eol_typeinfo_base (typeinfo);
            if (base) typeinfo_push_userdata (L, base);
        } else if (string_equal ("pointerto", field)) {
            lua_pushcfunction (L, typeinfo_pointerto);
        } else if (string_equal ("arrayof", field)) {
            lua_pushcfunction (L, typeinfo_arrayof);
        } else {
            return luaL_error (L, "invalid field '%s'", field);
        }
    }
    return 1;
}

static int
typeinfo_len (lua_State *L)
{
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);

    if (eol_typeinfo_is_array (typeinfo)) {
        lua_pushinteger (L, eol_typeinfo_array_n_items (typeinfo));
    } else if ((typeinfo = eol_typeinfo_get_compound (typeinfo))) {
        lua_pushinteger (L, eol_typeinfo_compound_n_members (typeinfo));
    } else {
        return luaL_error (L, "type is not a struct or union");
    }
    return 1;
}

static int
typeinfo_eq (lua_State *L)
{
    const EolTypeInfo *typeinfo_a = to_eol_typeinfo (L, 1);
    const EolTypeInfo *typeinfo_b = to_eol_typeinfo (L, 2);
    lua_pushboolean (L, eol_typeinfo_equal (typeinfo_a, typeinfo_b));
    return 1;
}

static int typeinfo_call (lua_State *L);

static const luaL_Reg typeinfo_methods[] = {
    { "__tostring", typeinfo_tostring },
    { "__index",    typeinfo_index    },
    { "__len",      typeinfo_len      },
    { "__eq",       typeinfo_eq       },
    { "__call",     typeinfo_call     },
    { NULL, NULL },
};



static Dwarf_Off
library_get_tue_offset (EolLibrary *library,
                        const char  *name,
                        Dwarf_Error *d_error);
static Dwarf_Die
library_fetch_die (EolLibrary  *library,
                   Dwarf_Off    d_offset,
                   Dwarf_Error *d_error);

static inline const EolTypeInfo*
library_fetch_die_type_ref_cached (EolLibrary  *library,
                                   Dwarf_Die    d_die,
                                   Dwarf_Half   d_tag,
                                   Dwarf_Error *d_error);

static const EolTypeInfo*
library_lookup_type (EolLibrary  *library,
                     Dwarf_Off    d_offset,
                     Dwarf_Error *d_error);

static const EolTypeInfo*
library_build_typeinfo (EolLibrary  *library,
                        Dwarf_Off    d_offset,
                        Dwarf_Error *d_error);

static Dwarf_Die lookup_die (EolLibrary  *library,
                             const char  *name,
                             Dwarf_Error *d_error);


/*
 * FIXME: This makes EolVariable/EolFunction keep a reference to their
 *        corresponding EolLibrary, which itself might be GCd while there
 *        are still live references to it!
 */
#define EOL_COMMON_FIELDS \
    EolLibrary *library;  \
    void       *address;  \
    char       *name


/*
 * Any structure that uses EOL_COMMON_FIELDS at its start can be casted to
 * this struct type.
 */
typedef struct {
    EOL_COMMON_FIELDS;
} EolSymbol;


struct _EolFunction {
    EOL_COMMON_FIELDS;
    EOL_FUNCTION_FCALL_FIELDS;
    const EolTypeInfo *return_typeinfo;
    uint32_t           n_param;
    const EolTypeInfo *param_types[];
};

typedef struct {
    EOL_COMMON_FIELDS;
    union {
        EolTypeInfo       *typeinfo;
        const EolTypeInfo *typeinfo_const;
    };
    bool                    typeinfo_owned;
} EolVariable;


static const char EOL_FUNCTION[] = "org.perezdecastro.eol.Function";
static const char EOL_VARIABLE[] = "org.perezdecastro.eol.Variable";


static inline EolFunction*
to_eol_function (lua_State *L)
{
    return (EolFunction*) luaL_checkudata (L, 1, EOL_FUNCTION);
}

static inline EolVariable*
to_eol_variable (lua_State *L, int index)
{
    return (EolVariable*) luaL_checkudata (L, index, EOL_VARIABLE);
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
        if (snprintf (try_path, PATH_MAX, "%s%s" EOL_LIB_SUFFIX,
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
void library_free (EolLibrary *el)
{
    TRACE_PTR (<, EolLibrary, el, "\n");

    eol_type_cache_free (&el->type_cache);

    if (el->d_globals)
        dwarf_globals_dealloc (el->d_debug, el->d_globals, el->d_num_globals);
    if (el->d_types)
        dwarf_pubtypes_dealloc (el->d_debug, el->d_types, el->d_num_types);

    Dwarf_Error d_error = DW_DLE_NE;
    dwarf_finish (el->d_debug, &d_error);

    free (el->path);
    close (el->fd);
    dlclose (el->dl);

    if (library_list == el) {
        library_list = library_list->next;
    } else {
        EolLibrary *prev = library_list;
        while (prev->next && prev->next != el) prev = prev->next;
        prev->next = prev->next->next;
    }

    free (el);
}


static int
library_gc (lua_State *L)
{
    EolLibrary *el = to_eol_library (L, 1);
    library_unref (el);
    return 0;
}


static int
library_tostring (lua_State *L)
{
    EolLibrary *el = to_eol_library (L, 1);
    if (el->d_debug) {
        lua_pushfstring (L, "eol.library (%p)", el->d_debug);
    } else {
        lua_pushliteral (L, "eol.library (closed)");
    }
    return 1;
}


static inline void
symbol_init (EolSymbol  *symbol,
             EolLibrary *library,
             void        *address,
             const char  *name)
{
    CHECK_NOT_NULL (address);
    memset (symbol, 0x00, sizeof (EolSymbol));
    if (library)
        symbol->library = library_ref (library);
    if (name)
        symbol->name = strdup (name);
    symbol->address = address;
}


static inline void
symbol_free (EolSymbol *symbol)
{
    if (symbol->library)
        library_unref (symbol->library);
    free (symbol->name);
    memset (symbol, 0xCA, sizeof (EolSymbol));
}


static int
typeinfo_call (lua_State *L)
{
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    lua_Integer n_items = luaL_optinteger (L, 2, 1);

    if (n_items < 1)
        return luaL_error (L, "argument #2 must be > 0");

    bool typeinfo_owned = false;
    if (lua_gettop (L) > 1) {
        typeinfo = eol_typeinfo_new_array (typeinfo, n_items);
        typeinfo_owned = true;
    }

    size_t payload = eol_typeinfo_sizeof (typeinfo);
    EolVariable *ev = lua_newuserdata (L, sizeof (EolVariable) + payload);
    symbol_init ((EolSymbol*) ev, NULL, &ev[1], NULL);
    ev->typeinfo_owned = typeinfo_owned;
    ev->typeinfo_const = typeinfo;
    memset (ev->address, 0x00, payload);
    luaL_setmetatable (L, EOL_VARIABLE);
    TRACE_PTR (>, EolVariable, ev, " (<lua>)\n");
    return 1;
}


static inline Dwarf_Off
library_get_die_ref_attribute_offset (EolLibrary *library,
                                      Dwarf_Die    d_die,
                                      Dwarf_Half   d_attr_tag,
                                      Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_die);
    CHECK_NOT_NULL (d_error);

    dw_lattr_t value = { library->d_debug };
    if (dwarf_attr (d_die, d_attr_tag, &value.attr, d_error) != DW_DLV_OK)
        return DW_DLV_BADOFFSET;

    CHECK_NOT_NULL (value.attr);

    Dwarf_Off d_offset;
    bool success = dwarf_global_formref (value.attr,
                                         &d_offset,
                                         d_error) == DW_DLV_OK;
    return success ? d_offset : DW_DLV_BADOFFSET;
}


/*
 * A function like this:
 *
 *   int add(int a, int b);
 *
 * Becomes:
 *
 *   DW_TAG_subprogram
 *     DW_AT_type                 <die-ref-offset> (return type)
 *     DW_TAG_formal_parameter
 *       DW_AT_location           <opcodes>
 *       DW_AT_name               a
 *       DW_AT_type               <die-ref-offset>
 *     DW_TAG_formal_parameter
 *       DW_AT_location           <opcodes>
 *       DW_AT_name               b
 *       DW_AT_type               <die-ref-offset>
 */
static EolFunction*
function_parameters (lua_State         *L,
                     EolLibrary        *library,
                     Dwarf_Die          d_param_die,
                     Dwarf_Error       *d_error,
                     const EolTypeInfo *return_typeinfo,
                     void              *func_address,
                     const char        *func_name,
                     uint32_t           index)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_error);
    CHECK_NOT_NULL (return_typeinfo);

    if (!d_param_die) {
        /* No more entries. Create a EolFunction and fill-in the paramtype. */
        const size_t payload = sizeof (EolTypeInfo*) * index;
        EolFunction *ef = lua_newuserdata (L, sizeof (EolFunction) + payload);
        memset (ef, 0x00, sizeof (EolFunction) + payload);
        symbol_init ((EolSymbol*) ef, library, func_address, func_name);
        ef->return_typeinfo = return_typeinfo;
        ef->n_param         = index;
        luaL_setmetatable (L, EOL_FUNCTION);
        return ef;
    }

    Dwarf_Half d_tag;
    if (dwarf_tag (d_param_die, &d_tag, d_error) != DW_DLV_OK) {
        DW_TRACE_DIE_ERROR ("%s[%d]: cannot get tag\n",
                            library->d_debug, d_param_die, *d_error,
                            func_name, index);
        return NULL;
    }

    const EolTypeInfo *typeinfo = NULL;
    if (d_tag == DW_TAG_formal_parameter) {
        if (!(typeinfo = library_fetch_die_type_ref_cached (library,
                                                            d_param_die,
                                                            DW_AT_type,
                                                            d_error))) {
            DW_TRACE_DIE_ERROR ("%s[%d]: cannot get type information\n",
                                library->d_debug, d_param_die, *d_error,
                                func_name, index);
            return NULL;
        }
        DW_TRACE_DIE ("%s[%d]: type " GREEN "%p\n" NORMAL,
                      library->d_debug, d_param_die,
                      func_name, index, typeinfo);
    }

    /*
     * Advance to the next item.
     */
    Dwarf_Die d_next_param_die;
    int status = dwarf_siblingof (library->d_debug,
                                  d_param_die,
                                  &d_next_param_die,
                                  d_error);
    if (status == DW_DLV_ERROR) {
        DW_TRACE_DIE_ERROR ("%s[%d]: cannot get sibling\n",
                            library->d_debug, d_param_die, *d_error,
                            func_name, index);
        return NULL;
    }
    if (status == DW_DLV_NO_ENTRY) {
        d_next_param_die = NULL;
    }

    EolFunction *ef = function_parameters (L,
                                           library,
                                           d_next_param_die,
                                           d_error,
                                           return_typeinfo,
                                           func_address,
                                           func_name,
                                           typeinfo ? index + 1 : index);
    if (ef && typeinfo) ef->param_types[index] = typeinfo;
    dwarf_dealloc (library->d_debug, d_next_param_die, DW_DLA_DIE);
    return ef;
}


static int
make_function_wrapper (lua_State  *L,
                       EolLibrary *library,
                       void       *address,
                       const char *name,
                       Dwarf_Die   d_die,
                       Dwarf_Half  d_tag)
{
    Dwarf_Error d_error = DW_DLE_NE;

    TRACE (YELLOW "%s()" GREY " requested\n" NORMAL, name);

    Dwarf_Bool has_return;
    if (dwarf_hasattr (d_die, DW_AT_type, &has_return, &d_error) != DW_DLV_OK) {
        return luaL_error (L, "%s: %s", name, dw_errmsg (d_error));
    }

    const EolTypeInfo *return_typeinfo = has_return
            ? library_fetch_die_type_ref_cached (library,
                                                 d_die,
                                                 DW_AT_type,
                                                 &d_error)
            : eol_typeinfo_void;
    if (!return_typeinfo) {
        DW_TRACE_DIE_ERROR ("%s: cannot get return type\n",
                            library->d_debug, d_die, d_error, name);
        return luaL_error (L, "%s: cannot get return type information (%s)\n",
                           name, dw_errmsg (d_error));
    }
    TRACE ("%s[@]: return type " GREEN "%p\n" NORMAL, name, return_typeinfo);

    dw_ldie_t child = { library->d_debug };
    int status = dwarf_child (d_die, &child.die, &d_error);
    if (status == DW_DLV_ERROR) {
        DW_TRACE_DIE_ERROR ("%s: cannot get child\n",
                            library->d_debug, d_die, d_error, name);
        return luaL_error (L, "%s: cannot obtain child DIE (%s)\n",
                           name, dw_errmsg (d_error));
    }

    if (status == DW_DLV_NO_ENTRY) {
        CHECK (child.die == NULL);
    }

    EolFunction *ef = function_parameters (L,
                                           library,
                                           child.die,
                                           &d_error,
                                           return_typeinfo,
                                           address,
                                           name,
                                           0);
    if (!ef) {
        DW_TRACE_DIE_ERROR ("%s: cannot get parameter types\n",
                            library->d_debug, d_die, d_error, name);
        return luaL_error (L, "%s: cannot get parameter types (%s)",
                           name, dw_errmsg (d_error));
    }
    EOL_FUNCTION_FCALL_INIT (ef);

    TRACE (BGREEN "%s() " NORMAL, name);
    TRACE_PTR (->, EolFunction, ef, "\n");
    return 1;
}


static EolVariable*
variable_push_userdata (lua_State         *L,
                        EolLibrary        *library,
                        const EolTypeInfo *typeinfo,
                        void              *address,
                        const char        *name,
                        bool               copy)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (address);

    EolVariable *ev;
    if (copy) {
        const uint32_t size = eol_typeinfo_sizeof (typeinfo);
        ev = lua_newuserdata (L, sizeof (EolVariable) + size);
        symbol_init ((EolSymbol*) ev, library, &ev[1], name);
        memcpy (ev->address, address, size);
    } else {
        ev = lua_newuserdata (L, sizeof (EolVariable));
        symbol_init ((EolSymbol*) ev, library, address, name);
    }
    ev->typeinfo_const = typeinfo;
    ev->typeinfo_owned = false;
    luaL_setmetatable (L, EOL_VARIABLE);

    TRACE_PTR (+, EolVariable, ev, " type " GREEN "%p" NORMAL "(%s)\n",
               typeinfo, name ? name : "?");
    return ev;
}


static int
make_variable_wrapper (lua_State  *L,
                       EolLibrary *library,
                       void       *address,
                       const char *name,
                       Dwarf_Die   d_die,
                       Dwarf_Half  d_tag)
{
    Dwarf_Error d_error = DW_DLE_NE;
    const EolTypeInfo* typeinfo =
            library_fetch_die_type_ref_cached (library,
                                               d_die,
                                               DW_AT_type,
                                               &d_error);
    if (!typeinfo) {
        DW_TRACE_DIE_ERROR ("%s: cannot get type information\n",
                            library->d_debug, d_die, d_error, name);
        return luaL_error (L, "%s: could not obtain type information (%s)",
                           dw_errmsg (d_error));
    }
    variable_push_userdata (L, library, typeinfo, address, name, false);
    return 1;
}


static int
library_index (lua_State *L)
{
    EolLibrary *e = to_eol_library (L, 1);
    const char *name = luaL_checkstring (L, 2);
    const char *error = "unknown error";

    /* Find the entry point of the function. */
    void *address = dlsym (e->dl, name);
    if (!address) {
        error = dlerror ();
        goto return_error;
    }

    Dwarf_Error d_error = DW_DLE_NE;
    Dwarf_Die d_die = lookup_die (e, name, &d_error);
    if (!d_die) {
        return luaL_error (L, "could not look up DWARF debug information "
                           "for symbol '%s' (library %p; %s)",
                           name, e, dw_errmsg (d_error));
    }

#if EOL_RUNTIME_CHECKS
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
#endif /* EOL_RUNTIME_CHECKS */

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

    dwarf_dealloc (e->d_debug, d_die, DW_DLA_DIE);
return_error:
    lua_pushnil (L);
    lua_pushstring (L, error);
    return 2;
}

static int
library_eq (lua_State *L)
{
    EolLibrary *el_self  = to_eol_library (L, 1);
    EolLibrary *el_other = to_eol_library (L, 2);
    lua_pushboolean (L, el_self == el_other);
    return 1;
}


/* Methods for EolLibrary userdatas. */
static const luaL_Reg library_methods[] = {
    { "__gc",       library_gc       },
    { "__tostring", library_tostring },
    { "__index",    library_index    },
    { "__eq",       library_eq       },
    { NULL, NULL }
};


static int function_call (lua_State *L);


static int
function_gc (lua_State *L)
{
    EolFunction *ef = to_eol_function (L);

    TRACE_PTR (<, EolFunction, ef, " (%s)\n", ef->name ? ef->name : "?");

    EOL_FUNCTION_FCALL_FREE (ef);
    symbol_free ((EolSymbol*) ef);
    return 0;
}

static int
function_tostring (lua_State *L)
{
    EolFunction *ef = to_eol_function (L);
    lua_pushfstring (L, "eol.function (%p:%s)", ef->library, ef->name);
    return 1;
}

static int
function_index (lua_State *L)
{
    EolFunction *ef = to_eol_function (L);
    size_t length;
    const char *name = luaL_checklstring (L, 2, &length);
    const EolSpecial *s = (length > 2 && name[0] == '_' && name[1] == '_')
        ? eol_special_lookup (name + 2, length - 2)
        : NULL;

    if (!s) return luaL_error (L, "invalid field '%s'", name);

    switch (s->code) {
        case EOL_SPECIAL_NAME:
            lua_pushstring (L, ef->name);
            break;
        case EOL_SPECIAL_TYPE:
            typeinfo_push_userdata (L, ef->return_typeinfo);
            break;
        case EOL_SPECIAL_LIBRARY:
            library_push_userdata (L, ef->library);
            break;
        case EOL_SPECIAL_VALUE:
            return luaL_error (L, "invalid field '%s'", name);
    }
    return 1;
}

/* Methods for EolFunction userdatas. */
static const luaL_Reg function_methods[] = {
    { "__call",     function_call     },
    { "__gc",       function_gc       },
    { "__tostring", function_tostring },
    { "__index",    function_index    },
    { NULL, NULL }
};


/* Methods for EolVariable userdatas. */
static int
variable_gc (lua_State *L)
{
    EolVariable *ev = to_eol_variable (L, 1);

    TRACE_PTR (<, EolVariable, ev, " type " GREEN "%p" NORMAL " (%s)\n",
               ev->typeinfo, ev->name ? ev->name : "?");

    if (ev->typeinfo_owned) {
        eol_typeinfo_free (ev->typeinfo);
    }
    symbol_free ((EolSymbol*) ev);
    return 0;
}

static int
variable_tostring (lua_State *L)
{
    EolVariable *ev = to_eol_variable (L, 1);
    if (ev->library && ev->name) {
        lua_pushfstring (L,
                         "eol.variable<%s>(%p:%s)",
                         eol_typeinfo_name (ev->typeinfo),
                         ev->library, ev->name);
    } else {
        lua_pushfstring (L,
                         "eol.variable<%s>(%p)",
                         eol_typeinfo_name (ev->typeinfo),
                         ev->address);
    }
    return 1;
}

static int
variable_len (lua_State *L)
{
    EolVariable *ev = to_eol_variable (L, 1);
    const EolTypeInfo *typeinfo =
            eol_typeinfo_get_non_synthetic (ev->typeinfo);
    lua_pushinteger (L, eol_typeinfo_is_array (typeinfo)
                            ? eol_typeinfo_array_n_items (typeinfo) : 1);
    return 1;
}


#define L_BOUNDS_CHECK(vname, i, expr)                       \
    lua_Integer vname = luaL_checkinteger (L, (i));          \
    do {                                                     \
        size_t max = (expr);                                 \
        if (vname < 0) vname += max;                         \
        if (vname <= 0 || vname > max)                       \
            return luaL_error (L, "index %d out of bounds ", \
                               "(effective=%d, max=%d)",     \
                               luaL_checkinteger (L, (i)),   \
                               vname, max);                  \
        vname--;                                             \
    } while (0)

#define ADDR_OFF(ctype, base, offset) \
    ((ctype*) (((uintptr_t) base) + offset))

#define FLOAT_TO_LUA(suffix, name, ctype) \
        case EOL_TYPE_ ## suffix:         \
            lua_pushnumber (L, *ADDR_OFF (ctype, address, 0)); return 1;

#define INTEGER_TO_LUA(suffix, name, ctype) \
        case EOL_TYPE_ ## suffix:           \
            lua_pushinteger (L, *ADDR_OFF (ctype, address, 0)); return 1;

static inline int
cvalue_push (lua_State         *L,
             const EolTypeInfo *typeinfo,
             void              *address,
             bool               allocate)
{
    CHECK_NOT_ZERO (address);
    typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);
    switch (eol_typeinfo_type (typeinfo)) {
        INTEGER_TYPES (INTEGER_TO_LUA)
        FLOAT_TYPES (FLOAT_TO_LUA)

        case EOL_TYPE_ENUM:
            switch (eol_typeinfo_sizeof (typeinfo)) {
                case 1:
                    lua_pushinteger (L, *ADDR_OFF (int8_t, address, 0));
                    break;
                case 2:
                    lua_pushinteger (L, *ADDR_OFF (int16_t, address, 0));
                    break;
                case 4:
                    lua_pushinteger (L, *ADDR_OFF (int32_t, address, 0));
                    break;
                case 8:
                    lua_pushinteger (L, *ADDR_OFF (int64_t, address, 0));
                    break;
                default:
                    typeinfo_push_stringrep (L, typeinfo, false);
                    return luaL_error (L, "size %d for type '%s' unsupported",
                                       eol_typeinfo_sizeof (typeinfo),
                                       lua_tostring (L, -1));
            }
            return 1;

        case EOL_TYPE_BOOL:
            lua_pushboolean (L, *ADDR_OFF (bool, address, 0));
            return 1;

        case EOL_TYPE_POINTER:
            if (*ADDR_OFF (void*, address, 0)) {
                variable_push_userdata (L, NULL, typeinfo,
                                        *ADDR_OFF (void*, address, 0),
                                        NULL, false);
            } else {
                /* Map NULL pointers to "nil". */
                lua_pushnil (L);
            }
            return 1;

        case EOL_TYPE_UNION:
        case EOL_TYPE_ARRAY:
        case EOL_TYPE_STRUCT:
            variable_push_userdata (L, NULL, typeinfo,
                                    address, NULL, allocate);
            return 1;

        case EOL_TYPE_VOID:
            return 0; /* Nothing to push. */

        default:
            typeinfo_push_stringrep (L, typeinfo, true);
            return luaL_error (L, "unsupported type: %s", lua_tostring (L, -1));
    }
}

#undef INTEGER_TO_LUA
#undef FLOAT_TO_LUA


static inline int
variable_index_special (lua_State     *L,
                        EolVariable   *V,
                        EolSpecialCode code)
{
    CHECK_NOT_NULL (L);
    CHECK_NOT_NULL (V);

    switch (code) {
        case EOL_SPECIAL_NAME:
            lua_pushstring (L, V->name);
            break;
        case EOL_SPECIAL_TYPE:
            typeinfo_push_userdata (L, V->typeinfo);
            break;
        case EOL_SPECIAL_VALUE:
            return cvalue_push (L, V->typeinfo, V->address, false);
        case EOL_SPECIAL_LIBRARY:
            library_push_userdata (L, V->library);
            break;
    }
    return 1;
}


static int
variable_index (lua_State *L)
{
    EolVariable *V = to_eol_variable (L, 1);
    const EolTypeInfo *T = eol_typeinfo_get_non_synthetic (V->typeinfo);
    if (!T) return luaL_error (L, "cannot get actual type");

    const char *named_field = NULL;
    size_t named_field_length = 0;
    if (lua_type (L, 2) == LUA_TSTRING) {
        named_field = lua_tolstring (L, 2, &named_field_length);
        const EolSpecial *s = (named_field_length > 2 &&
                               named_field[0] == '_' &&
                               named_field[1] == '_')
            ? eol_special_lookup (named_field + 2, named_field_length - 2)
            : NULL;
        if (s) return variable_index_special (L, V, s->code);
    }

    switch (eol_typeinfo_type (T)) {
        case EOL_TYPE_ARRAY: {
            L_BOUNDS_CHECK (index, 2, eol_typeinfo_array_n_items (T));
            T = eol_typeinfo_get_non_synthetic (eol_typeinfo_base (T));
            return cvalue_push (L, T,
                                ADDR_OFF (void, V->address,
                                          index * eol_typeinfo_sizeof (T)),
                                false);
        }
        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT: {
            const EolTypeInfoMember *member = NULL;
            if (!named_field) {
                L_BOUNDS_CHECK (index, 2, eol_typeinfo_compound_n_members (T));
                member = eol_typeinfo_compound_const_member (T, index);
            } else if (!(member = eol_typeinfo_compound_const_named_member (T, named_field))) {
                typeinfo_push_stringrep (L, T, true);
                return luaL_error (L, "%s: no such member in type '%s'",
                                   named_field, lua_tostring (L, -1));
            }

            CHECK_NOT_NULL (member);
            return cvalue_push (L, member->typeinfo,
                                eol_typeinfo_is_struct (T)
                                    ? ADDR_OFF (void, V->address, member->offset)
                                    : V->address,
                                false);
        }
        default:
            return luaL_error (L, "not indexable");
    }
}


static void
l_typecheck (lua_State         *L,
             int                idx,
             const EolTypeInfo *dst,
             const EolTypeInfo *src)
{
    CHECK_NOT_NULL (dst);
    CHECK_NOT_NULL (src);

    if (!eol_typeinfo_equal (dst, src)) {
        typeinfo_push_stringrep (L, dst, false);
        typeinfo_push_stringrep (L, src, false);
        luaL_error (L, "#%d: expected value of type '%s', given '%s'",
                    (idx < 1) ? (lua_gettop (L) + idx) : idx,
                    lua_tostring (L, -2), lua_tostring (L, -1));
    }
}


#define FLOAT_FROM_LUA(suffix, name, ctype)           \
        case EOL_TYPE_ ## suffix:                     \
            *ADDR_OFF (ctype, address, 0) =           \
                (ctype) luaL_checknumber (L, lindex); \
            break;

#define INTEGER_FROM_LUA(suffix, name, ctype)          \
        case EOL_TYPE_ ## suffix:                      \
            *ADDR_OFF (ctype, address, 0) =            \
                (ctype) luaL_checkinteger (L, lindex); \
            break;

static inline int
cvalue_get (lua_State         *L,
            int                lindex,
            const EolTypeInfo* typeinfo,
           void               *address)
{
    CHECK_NOT_ZERO (address);
    typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);
    switch (eol_typeinfo_type (typeinfo)) {
        INTEGER_TYPES (INTEGER_FROM_LUA)
        FLOAT_TYPES (FLOAT_FROM_LUA)

        case EOL_TYPE_BOOL:
            *ADDR_OFF (bool, address, 0) = lua_toboolean (L, lindex);
            break;

        case EOL_TYPE_POINTER:
            if (eol_typeinfo_is_cstring (typeinfo) &&
                    lua_type (L, lindex) == LUA_TSTRING) {
                *ADDR_OFF (const char*, address, 0) = lua_tostring (L, lindex);
            } else {
                EolVariable *ev = to_eol_variable (L, lindex);
                l_typecheck (L, lindex - 1, typeinfo,
                             eol_typeinfo_get_non_synthetic (ev->typeinfo));
                *ADDR_OFF (void*, address, 0) = ev->address;
            }
            break;

        case EOL_TYPE_STRUCT: {
            EolVariable *ev = to_eol_variable (L, lindex);
            l_typecheck (L, lindex - 1, typeinfo,
                         eol_typeinfo_get_non_synthetic (ev->typeinfo));
            CHECK_SIZE_EQ (eol_typeinfo_sizeof (typeinfo),
                           eol_typeinfo_sizeof (ev->typeinfo));
            memcpy (ADDR_OFF (void, address, 0),
                    ADDR_OFF (void, ev->address, 0),
                    eol_typeinfo_sizeof (typeinfo));
            break;
        }
        default:
            typeinfo_push_stringrep (L, typeinfo, true);
            return luaL_error (L, "unsupported type: %s", lua_tostring (L, -1));
    }

    return 1;
}

#undef INTEGER_FROM_LUA
#undef FLOAT_FROM_LUA
#undef SLOT


static inline int
variable_newindex_special (lua_State     *L,
                           int            lindex,
                           EolVariable   *V,
                           EolSpecialCode code)
{
    CHECK_NOT_NULL (L);
    CHECK_NOT_NULL (V);

    switch (code) {
        case EOL_SPECIAL_VALUE:
            return cvalue_get (L, lindex, V->typeinfo, V->address);
        case EOL_SPECIAL_NAME:
            return luaL_error (L, "__name is read-only");
        case EOL_SPECIAL_TYPE:
            return luaL_error (L, "__type is read-only");
        case EOL_SPECIAL_LIBRARY:
            return luaL_error (L, "__library is read-only");
    }
}


static int
variable_newindex (lua_State *L)
{
    EolVariable *V = to_eol_variable (L, 1);

    if (eol_typeinfo_is_readonly (V->typeinfo)) {
        return luaL_error (L, "read-only variable (%p:%s)",
                           V->library, V->name);
    }

    if (lua_type (L, 2) == LUA_TSTRING) {
        size_t length;
        const char *name = lua_tolstring (L, 2, &length);
        const EolSpecial *s = (length > 2 && name[0] == '_' && name[1] == '_')
            ? eol_special_lookup (name + 2, length - 2)
            : NULL;
        if (s) return variable_newindex_special (L, 3, V, s->code);
    }

    const EolTypeInfo *T = eol_typeinfo_get_non_synthetic (V->typeinfo);
    if (!T) return luaL_error (L, "cannot get actual type");

    switch (eol_typeinfo_type (T)) {
        case EOL_TYPE_ARRAY: {
            L_BOUNDS_CHECK (index, 2, eol_typeinfo_array_n_items (T));
            T = eol_typeinfo_get_non_synthetic (eol_typeinfo_base (T));
            return cvalue_get (L, 3, T,
                               ADDR_OFF (void, V->address,
                                         index * eol_typeinfo_sizeof (T)));
        }
        case EOL_TYPE_STRUCT: {
            const EolTypeInfoMember *member;
            if (lua_isinteger (L, 2)) {
                L_BOUNDS_CHECK (index, 2, eol_typeinfo_compound_n_members (T));
                member = eol_typeinfo_compound_const_member (T, index);
            } else {
                const char *name = luaL_checkstring (L, 2);
                member = eol_typeinfo_compound_const_named_member (T, name);
                if (!member) {
                    return luaL_error (L, "%s: no such struct member", name);
                }
            }
            return cvalue_get (L, 3, member->typeinfo,
                               ADDR_OFF (void, V->address, member->offset));
        }
        default:
            return luaL_error (L, "not indexable");
    }

    return 0;
}


static const luaL_Reg variable_methods[] = {
    { "__gc",       variable_gc       },
    { "__tostring", variable_tostring },
    { "__len",      variable_len      },
    { "__index",    variable_index    },
    { "__newindex", variable_newindex },
    { NULL, NULL },
};


static void
create_meta (lua_State *L)
{
    /* EolLibrary */
    luaL_newmetatable (L, EOL_LIBRARY);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, library_methods, 0);
    lua_pop (L, 1);

    /* EolFunction */
    luaL_newmetatable (L, EOL_FUNCTION);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, function_methods, 0);
    lua_pop (L, 1);

    /* EolVariable */
    luaL_newmetatable (L, EOL_VARIABLE);
    luaL_setfuncs (L, variable_methods, 0);
    lua_pop (L, 1);

    /* EolypeInfo */
    luaL_newmetatable (L, EOL_TYPEINFO);
    luaL_setfuncs (L, typeinfo_methods, 0);
    lua_pop (L, 1);
}


static int
eol_load (lua_State *L)
{
    size_t name_length;
    const char *name = luaL_checklstring (L, 1, &name_length);

    unsigned dlopen_flags = RTLD_NOW;
    if (lua_gettop (L) == 2) {
        if (!lua_isboolean (L, 2)) {
            return luaL_error (L, "paramter #2 should be a boolean value");
        }
        if (lua_toboolean (L, 2)) {
            dlopen_flags |= RTLD_GLOBAL;
        }
    }

    char path[PATH_MAX];
    errno = 0;
    if (!find_library (name, path)) {
        return luaL_error (L, "could not find library '%s' (%s)", name,
                           (errno == 0) ? "path too long" : strerror (errno));
    }
    TRACE ("found %s -> %s\n", name, path);

    /*
     * If the library at the resolved path has been already loaded, return
     * a reference to the existing one instead of opening it multiple times.
     */
    EolLibrary *library = NULL;
    for (library = library_list; library; library = library->next) {
        if (string_equal (path, library->path)) {
            TRACE_PTR (+, EolLibrary, library, " [%s]\n", library->path);
            library_push_userdata (L, library);
            return 1;
        }
    }

    void *dl = dlopen (path, dlopen_flags);
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
    Dwarf_Error d_error = DW_DLE_NE;
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
        Dwarf_Error d_finish_error = DW_DLE_NE;
        dwarf_finish (d_debug, &d_finish_error);
        close (fd);
        dlclose (dl);
        return luaL_error (L, "cannot read globals (%s)", dw_errmsg (d_error));
    }
    TRACE ("found %ld globals\n", (long) d_num_globals);

#if EOL_TRACE > 1
    for (Dwarf_Signed i = 0; i < d_num_globals; i++) {
        char *name = NULL;
        Dwarf_Error d_name_error = DW_DLE_NE;
        if (dwarf_globname (d_globals[i], &name, &d_name_error) == DW_DLV_OK) {
            TRACE (">  [%li] %s\n", (long) i, name);
            dwarf_dealloc (d_debug, name, DW_DLA_STRING);
        } else {
            TRACE (">  [%li] ERROR: %s\n", (long) i, dw_errmsg (d_name_error));
        }
    }
#endif /* EOL_TRACE */

    Dwarf_Signed d_num_types = 0;
    Dwarf_Type *d_types = NULL;
    if (dwarf_get_pubtypes (d_debug,
                            &d_types,
                            &d_num_types,
                            &d_error) != DW_DLV_OK) {
        Dwarf_Error d_finish_error = DW_DLE_NE;
        dwarf_globals_dealloc (d_debug, d_globals, d_num_globals);
        dwarf_finish (d_debug, &d_finish_error);
        close (fd);
        dlclose (dl);
        return luaL_error (L, "cannot read types (%s)", dw_errmsg (d_error));
    }
    TRACE ("found %ld types\n", (long) d_num_types);

#if EOL_TRACE > 1
    for (Dwarf_Signed i = 0; i < d_num_types; i++) {
        char *name = NULL;
        Dwarf_Error d_name_error = DW_DLE_NE;
        if (dwarf_pubtypename (d_types[i], &name, &d_name_error) == DW_DLV_OK) {
            TRACE (">  [%li] %s\n", (long) i, name);
            dwarf_dealloc (d_debug, name, DW_DLA_STRING);
        } else {
            TRACE (">  [%li] ERROR: %s\n", (long) i, dw_errmsg (d_name_error));
        }
    }
#endif /* EOL_TRACE */

    EolLibrary *el = calloc (1, sizeof (EolLibrary));
    el->fd = fd;
    el->dl = dl;
    el->path = strdup (path);
    el->d_debug = d_debug;
    el->d_globals = d_globals;
    el->d_num_globals = d_num_globals;
    el->d_types = d_types;
    el->d_num_types = d_num_types;
    el->next = library_list;
    library_list = el;
    eol_type_cache_init (&el->type_cache);
    library_push_userdata (L, el);

    TRACE_PTR (>, EolLibrary, el, " [%s]\n", el->path);

    return 1;
}


static int
eol_type (lua_State *L)
{
    EolLibrary *el = to_eol_library (L, 1);
    const char *name = luaL_checkstring (L, 2);

    Dwarf_Error d_error = DW_DLE_NE;
    Dwarf_Off d_offset = library_get_tue_offset (el, name, &d_error);
    if (d_offset == DW_DLV_BADOFFSET) {
        return luaL_error (L, "%s: could not look up DWARF TUE offset "
                           "(library: %p; %s)", name, el, dw_errmsg (d_error));
    }

    const EolTypeInfo *typeinfo = library_lookup_type (el, d_offset, &d_error);
    if (!typeinfo) {
        return luaL_error (L, "%s: no type info (%s)",
                           name, dw_errmsg (d_error));
    }
    typeinfo_push_userdata (L, typeinfo);
    return 1;
}


/*
 * Usage: eol.sizeof(ct [, nelem])
 *
 * TODO: Handle second "nelem" parameter for VLAs.
 */
static int
eol_sizeof (lua_State *L)
{
    const EolTypeInfo *typeinfo = NULL;

    EolVariable *ev;
    if ((ev = luaL_testudata (L, 1, EOL_VARIABLE))) {
        typeinfo = ev->typeinfo;
    } else {
        typeinfo = to_eol_typeinfo (L, 1);
    }

    CHECK_NOT_NULL (typeinfo);
    lua_pushinteger (L, eol_typeinfo_sizeof (typeinfo));
    return 1;
}


static int
eol_cast (lua_State *L)
{
    const EolTypeInfo *typeinfo = to_eol_typeinfo (L, 1);
    EolVariable *ev = to_eol_variable (L, 2);

    variable_push_userdata (L,
                            ev->library,
                            typeinfo,
                            ev->address,
                            ev->name,
                            false);

    return 1;
}


/*
 * Usage: typeinfo = eol.typeof(ct)
 */
static int
eol_typeof (lua_State *L)
{
    if (luaL_testudata (L, 1, EOL_TYPEINFO)) {
        lua_settop (L, 1);
    } else {
        EolVariable *ev = luaL_testudata (L, 1, EOL_VARIABLE);
        if (ev) {
            typeinfo_push_userdata (L, ev->typeinfo);
        } else {
            const char *name = luaL_checkstring (L, 1);
            for (EolLibrary *el = library_list; el; el = el->next) {
                Dwarf_Error d_error = DW_DLE_NE;
                Dwarf_Off d_offset =
                        library_get_tue_offset (el, name, &d_error);
                if (d_offset == DW_DLV_BADOFFSET) {
                    if (d_error != DW_DLE_NE) {
                        return luaL_error (L, "%s: could not lookup DWARF TUE "
                                           "offset (library: %p; %s)",
                                           name, el, dw_errmsg (d_error));
                    }
                    continue;
                }

                const EolTypeInfo *typeinfo =
                        library_lookup_type (el, d_offset, &d_error);
                if (!typeinfo) {
                    return luaL_error (L, "%s: no type info (library: %p; %s)",
                                       name, el, dw_errmsg (d_error));
                }
                typeinfo_push_userdata (L, typeinfo);
                return 1;
            }
            lua_pushnil (L);
        }
    }
    return 1;
}


/*
 * Usage: offset [, bpos, bsize] = eol.offsetof(ct, field)
 *
 * TODO: Implement returning "bpos" and "bsize" for bit fields.
 */
static int
eol_offsetof (lua_State *L)
{
    const EolTypeInfo *typeinfo = NULL;

    EolVariable *ev;
    if ((ev = luaL_testudata (L, 1, EOL_VARIABLE))) {
        typeinfo = ev->typeinfo;
    } else {
        typeinfo = to_eol_typeinfo (L, 1);
    }

    CHECK_NOT_NULL (typeinfo);

    if (!(typeinfo = eol_typeinfo_get_compound (typeinfo))) {
        return luaL_error (L, "parameter #1 is not a struct or union");
    }

    const EolTypeInfoMember *member;
    if (lua_isinteger (L, 2)) {
        uint32_t n_members = eol_typeinfo_compound_n_members (typeinfo);
        lua_Integer index = luaL_checkinteger (L, 2);
        if (index < 0) index += n_members;
        if (index <= 0 || index > n_members) {
            return luaL_error (L, "index %d out of bounds "
                               "(effective=%d, length=%d)",
                               luaL_checkinteger (L, 2), index, n_members);
        }
        member = eol_typeinfo_compound_const_member (typeinfo, index - 1);
    } else {
        const char *name = luaL_checkstring (L, 2);
        member = eol_typeinfo_compound_const_named_member (typeinfo, name);
        if (!member) {
            const char *typename = eol_typeinfo_name (typeinfo);
            return luaL_error (L, "%s.%s: no such member field",
                               typename ? typename : "<struct>", name);
        }
    }

    CHECK_NOT_NULL (member);
    lua_pushinteger (L, member->offset);
    return 1;
}


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define EOL_LE 1
# define EOL_BE 0
#else
# define EOL_LE 0
# define EOL_BE 1
#endif

static int
eol_abi (lua_State *L)
{
    const char *param = luaL_checkstring (L, 1);

    if (string_equal (param, "le")) {
        lua_pushboolean (L, EOL_LE);
    } else if (string_equal (param, "be")) {
        lua_pushboolean (L, EOL_BE);
    } else if (string_equal (param, "32bit")) {
        lua_pushboolean (L, sizeof (ptrdiff_t) == 4);
    } else if (string_equal (param, "64bit")) {
        lua_pushboolean (L, sizeof (ptrdiff_t) == 8);

    // TODO: The following flags are unsupported.
#if 0
    } else if (string_equal (param, "fpu")) {
    } else if (string_equal (param, "softfpu")) {
    } else if (string_equal (param, "eabi")) {
    } else if (string_equal (param, "win")) {
#endif
    } else {
        return luaL_error (L, "invalid parameter '%s'", param);
    }
    return 1;
}


static const luaL_Reg eollib[] = {
    { "load",     eol_load     },
    { "type",     eol_type     },
    { "sizeof",   eol_sizeof   },
    { "typeof",   eol_typeof   },
    { "offsetof", eol_offsetof },
    { "cast",     eol_cast     },
    { "abi",      eol_abi      },
    { NULL, NULL },
};


LUAMOD_API int
luaopen_eol (lua_State *L)
{
    eol_trace_setup ();

    (void) elf_version (EV_NONE);
    if (elf_version (EV_CURRENT) == EV_NONE)
        return luaL_error (L, "outdated libelf version");

    luaL_newlib (L, eollib);
    create_meta (L);
    return 1;
}


static Dwarf_Die
lookup_die (EolLibrary  *el,
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
        Dwarf_Error d_globname_error = DW_DLE_NE;
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
library_fetch_die (EolLibrary *library,
                   Dwarf_Off    d_offset,
                   Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_SIZE_NE (DW_DLV_BADOFFSET, d_offset);
    CHECK_NOT_NULL (d_error);

    Dwarf_Die d_die = NULL;
    if (dwarf_offdie (library->d_debug,
                      d_offset,
                      &d_die,
                      d_error) != DW_DLV_OK) {
        TRACE ("could not fetch DIE %#lx (%s)\n",
               (long int) d_offset,
               dw_errmsg (*d_error));
        return NULL;
    }

    CHECK_NOT_NULL (d_die);
    return d_die;
}


static const EolTypeInfo*
library_lookup_type (EolLibrary  *library,
                     Dwarf_Off    d_offset,
                     Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_SIZE_NE (DW_DLV_BADOFFSET, d_offset);
    CHECK_NOT_NULL (d_error);

    const EolTypeInfo *typeinfo =
            eol_type_cache_lookup (&library->type_cache, d_offset);
    if (!typeinfo) {
        typeinfo = library_build_typeinfo (library, d_offset, d_error);
        CHECK_NOT_NULL (typeinfo);
        eol_type_cache_add (&library->type_cache, d_offset, typeinfo);
    }
    return typeinfo;
}


static const EolTypeInfo*
library_build_base_type_typeinfo (EolLibrary  *library,
                                  Dwarf_Die    d_type_die,
                                  Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);

    Dwarf_Unsigned d_encoding, d_byte_size;

    if (!dw_die_get_uint_attr (library->d_debug,
                               d_type_die,
                               DW_AT_encoding,
                               &d_encoding,
                               d_error) ||
        !dw_die_get_uint_attr (library->d_debug,
                               d_type_die,
                               DW_AT_byte_size,
                               &d_byte_size,
                               d_error))
            return NULL;

#define TYPEINFO_ITEM(_, name, ctype) \
        case sizeof (ctype):          \
            return eol_typeinfo_ ## name;

    switch (d_encoding) {
        case DW_ATE_boolean:
            return eol_typeinfo_bool;
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

    CHECK_UNREACHABLE ();
    return NULL;
}


static const EolTypeInfo*
library_build_typedef_typeinfo (EolLibrary  *library,
                                Dwarf_Die    d_type_die,
                                Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);

    const EolTypeInfo *base = library_fetch_die_type_ref_cached (library,
                                                                 d_type_die,
                                                                 DW_AT_type,
                                                                 d_error);
    if (!base) {
        DW_TRACE_DIE_ERROR ("cannot get type info\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    dw_lstring_t name = {
        library->d_debug,
        dw_die_name (d_type_die, d_error)
    };
    if (!name.string) {
        DW_TRACE_DIE_ERROR ("cannot get name\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    return eol_typeinfo_new_typedef (base, name.string);
}


static inline const EolTypeInfo*
library_fetch_die_type_ref_cached (EolLibrary  *library,
                                   Dwarf_Die    d_die,
                                   Dwarf_Half   d_tag,
                                   Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_die);
    CHECK_NOT_NULL (d_error);

    Dwarf_Off d_offset = library_get_die_ref_attribute_offset (library,
                                                               d_die,
                                                               d_tag,
                                                               d_error);
    if (d_offset == DW_DLV_BADOFFSET) {
        DW_TRACE_DIE_ERROR ("cannot get attribute offset\n",
                            library->d_debug, d_die, *d_error);
        return NULL;
    }

    return library_lookup_type (library, d_offset, d_error);
}



static const EolTypeInfo*
library_build_pointer_type_typeinfo (EolLibrary  *library,
                                     Dwarf_Die    d_type_die,
                                     Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);

    Dwarf_Bool has_type;
    if (dwarf_hasattr (d_type_die, DW_AT_type, &has_type, d_error) != DW_DLV_OK) {
        DW_TRACE_DIE_ERROR ("cannot check attribute presence\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    if (!has_type)
        return eol_typeinfo_pointer;

    const EolTypeInfo *base = library_fetch_die_type_ref_cached (library,
                                                                 d_type_die,
                                                                 DW_AT_type,
                                                                 d_error);
    if (!base) {
        DW_TRACE_DIE_ERROR ("cannot get typeinfo\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }
    return eol_typeinfo_new_pointer (base);
}


static const EolTypeInfo*
library_build_array_type_typeinfo (EolLibrary  *library,
                                   Dwarf_Die    d_type_die,
                                   Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);

    Dwarf_Unsigned n_items;
    if (!dw_tue_array_get_n_items (library->d_debug,
                                   d_type_die,
                                   &n_items,
                                   d_error)) {
        DW_TRACE_DIE_ERROR ("cannot get array items\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    const EolTypeInfo *base = library_fetch_die_type_ref_cached (library,
                                                                 d_type_die,
                                                                 DW_AT_type,
                                                                 d_error);
    if (!base) {
        DW_TRACE_DIE_ERROR ("cannot get TUE\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    return eol_typeinfo_new_array (base, (uint64_t) n_items);
}


static const EolTypeInfo*
library_build_const_type_typeinfo (EolLibrary  *library,
                                   Dwarf_Die    d_type_die,
                                   Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);

    const EolTypeInfo *base = library_fetch_die_type_ref_cached (library,
                                                                 d_type_die,
                                                                 DW_AT_type,
                                                                 d_error);
    if (!base) {
        DW_TRACE_DIE_ERROR ("cannot get base type\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    return eol_typeinfo_new_const (base);
}


/*
 * A structure like this:
 *
 *   struct Point {
 *     int x, y;
 *   }
 *
 * Becomes:
 *
 *   DW_TAG_structure_type
 *     DW_AT_name                      Point
 *     DW_AT_byte_size                 8
 *       DW_TAG_member
 *         DW_AT_name                  x
 *         DW_AT_type                  <die-ref-offset>
 *         DW_AT_data_member_location  <in-struct-offset>
 *       DW_TAG_member
 *         DW_AT_name                  y
 *         DW_AT_type                  <die-ref-offset>
 *         DW_AT_data_member_location  <in-struct-offset>
 */

typedef EolTypeInfo* (*NewCompoundCb) (const char* name,
                                       uint32_t    size,
                                       uint32_t    n_members);

static EolTypeInfo*
compound_type_members (EolLibrary   *library,
                       Dwarf_Die     d_member_die,
                       Dwarf_Error  *d_error,
                       NewCompoundCb compound_new,
                       const char   *compound_name,
                       uint32_t      compound_size,
                       uint32_t      index)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_error);
    CHECK_NOT_NULL (compound_new);

    if (!d_member_die) {
        /* No more entries: create type information. */
        return (*compound_new) (compound_name, compound_size, index);
    }

    DW_TRACE_DIE ("\n", library->d_debug, d_member_die);

    Dwarf_Half d_tag;
    if (dwarf_tag (d_member_die, &d_tag, d_error) != DW_DLV_OK) {
        DW_TRACE_DIE_ERROR ("cannot get tag\n",
                            library->d_debug, d_member_die, *d_error);
        return NULL;
    }

    dw_lstring_t member_name       = { library->d_debug };
    Dwarf_Unsigned d_member_offset = DW_DLV_BADOFFSET;
    const EolTypeInfo *typeinfo    = NULL;

    if (d_tag == DW_TAG_member) {
        if (!dw_die_get_uint_attr (library->d_debug,
                                   d_member_die,
                                   DW_AT_data_member_location,
                                   &d_member_offset,
                                   d_error)) {
            DW_TRACE_DIE_ERROR ("%s: cannot get DW_AT_data_member_location\n",
                                library->d_debug, d_member_die, *d_error,
                                compound_name ? compound_name : "@");
            return NULL;
        }

        if (!(typeinfo = library_fetch_die_type_ref_cached (library,
                                                            d_member_die,
                                                            DW_AT_type,
                                                            d_error))) {
            DW_TRACE_DIE_ERROR ("%s: cannot get type information\n",
                                library->d_debug, d_member_die, *d_error,
                                compound_name ? compound_name : "@");
            return NULL;
        }

        *d_error = DW_DLE_NE;
        member_name.string = dw_die_name (d_member_die, d_error);
        if (!member_name.string && *d_error != DW_DLE_NE) {
            DW_TRACE_DIE_ERROR ("%s: cannot get name\n",
                                library->d_debug, d_member_die, *d_error,
                                compound_name ? compound_name : "@");
            return NULL;
        }
    }

    /*
     * Advance to the next item.
     */
    dw_ldie_t next_member = { library->d_debug };
    int status = dwarf_siblingof (library->d_debug,
                                  d_member_die,
                                  &next_member.die,
                                  d_error);
    if (status == DW_DLV_ERROR) {
        DW_TRACE_DIE_ERROR ("%s: cannot get next sibling\n",
                            library->d_debug, d_member_die, *d_error,
                            compound_name ? compound_name : "@");
        return NULL;
    }
    if (status == DW_DLV_NO_ENTRY) {
        CHECK (next_member.die == NULL);
    }

    EolTypeInfo *result = compound_type_members (library,
                                                 next_member.die,
                                                 d_error,
                                                 compound_new,
                                                 compound_name,
                                                 compound_size,
                                                 typeinfo ? index + 1 : index);
    if (result && typeinfo) {
        EolTypeInfoMember *member = eol_typeinfo_compound_member (result, index);
        member->name     = member_name.string ? strdup (member_name.string) : NULL;
        member->offset   = (uint32_t) d_member_offset;
        member->typeinfo = typeinfo;
    }
    return result;
}


static const EolTypeInfo*
library_build_union_type_typeinfo (EolLibrary  *library,
                                   Dwarf_Die    d_type_die,
                                   Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);
    CHECK (*d_error == DW_DLE_NE);

    DW_TRACE_DIE ("\n", library->d_debug, d_type_die);

    dw_lstring_t name = {
        library->d_debug,
        dw_die_name (d_type_die, d_error)
    };

    if (*d_error != DW_DLE_NE) {
        DW_TRACE_DIE_ERROR ("cannot get name\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    Dwarf_Unsigned d_byte_size;
    if (!dw_die_get_uint_attr (library->d_debug,
                               d_type_die,
                               DW_AT_byte_size,
                               &d_byte_size,
                               d_error)) {
        DW_TRACE_DIE_ERROR ("cannot get DW_AT_byte_size\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    dw_ldie_t child = { library->d_debug };
    if (dwarf_child (d_type_die, &child.die, d_error) != DW_DLV_OK)
        return NULL;

    return compound_type_members (library,
                                  child.die,
                                  d_error,
                                  eol_typeinfo_new_union,
                                  name.string,
                                  d_byte_size,
                                  0);
}


/*
 * An enum like this:
 *
 *   enum Foo {
 *     FOO_FROB,
 *     FOO_BAR,
 *   };
 *
 * Becomes:
 *
 *   DW_TAG_enumeration_type
 *     DW_AT_name           Foo
 *     DW_AT_byte_size      4
 *     DW_TAG_enumerator
 *       DW_AT_name         FOO_FROB
 *       DW_AT_const_value  0
 *     DW_TAG_enumerator
 *       DW_AT_name         FOO_BAR
 *       DW_AT_const_value  1
 */
static EolTypeInfo*
enum_members (Dwarf_Debug  d_debug,
              Dwarf_Die    d_member_die,
              Dwarf_Error *d_error,
              const char  *enum_name,
              uint32_t     enum_size,
              uint32_t     index)
{
    CHECK_NOT_NULL (d_debug);
    CHECK_NOT_NULL (d_error);

    if (!d_member_die) {
        /* No more entries: create type information. */
        return eol_typeinfo_new_enum (enum_name, enum_size, index);
    }

    DW_TRACE_DIE ("\n", d_debug, d_member_die);

    Dwarf_Half d_tag;
    if (dwarf_tag (d_member_die, &d_tag, d_error) != DW_DLV_OK) {
        DW_TRACE_DIE_ERROR ("cannot get tag\n",
                            d_debug, d_member_die, *d_error);
        return NULL;
    }

    dw_lstring_t member_name = { d_debug };
    Dwarf_Signed d_value     = 0;

    if (d_tag == DW_TAG_enumerator) {
        if (!dw_die_get_sint_attr (d_debug,
                                   d_member_die,
                                   DW_AT_const_value,
                                   &d_value,
                                   d_error)) {
            DW_TRACE_DIE_ERROR ("%s: cannot get DW_AT_const_value\n",
                                d_debug, d_member_die, *d_error,
                                enum_name ? enum_name : "#");
            return NULL;
        }

        CHECK (*d_error == DW_DLE_NE);
        member_name.string = dw_die_name (d_member_die, d_error);
        if (!member_name.string) {
            DW_TRACE_DIE_ERROR ("%s: cannot get name\n",
                                d_debug, d_member_die, *d_error,
                                enum_name ? enum_name : "#");
            return NULL;
        }
    }

    /*
     * Advance to the next item.
     */
    dw_ldie_t next_member = { d_debug };
    switch (dwarf_siblingof (d_debug, d_member_die, &next_member.die, d_error)) {
        case DW_DLV_ERROR:
            DW_TRACE_DIE_ERROR ("%s: cannot get next sibling\n",
                                d_debug, d_member_die, *d_error,
                                enum_name ? enum_name : "#");
            return NULL;
        case DW_DLV_NO_ENTRY:
            CHECK (next_member.die == NULL);
        case DW_DLV_OK:
            break;
        default:
            CHECK_UNREACHABLE ();
    }

    EolTypeInfo *result =
            enum_members (d_debug,
                          next_member.die,
                          d_error,
                          enum_name,
                          enum_size,
                          member_name.string ? index + 1 : index);

    if (result && member_name.string) {
        EolTypeInfoMember *member = eol_typeinfo_compound_member (result,
                                                                  index);
        member->name  = strdup (member_name.string);
        member->value = (int64_t) d_value;
    }
    return result;
}


static const EolTypeInfo*
library_build_enumeration_type_typeinfo (EolLibrary  *library,
                                         Dwarf_Die    d_type_die,
                                         Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);
    CHECK (*d_error == DW_DLE_NE);

    Dwarf_Unsigned d_byte_size;
    if (!dw_die_get_uint_attr (library->d_debug,
                               d_type_die,
                               DW_AT_byte_size,
                               &d_byte_size,
                               d_error)) {
        DW_TRACE_DIE_ERROR ("cannot get DW_AT_byte_size\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    dw_lstring_t name = {
        library->d_debug,
        dw_die_name (d_type_die, d_error),
    };
    if (!name.string && *d_error != DW_DLE_NE) {
        DW_TRACE_DIE_ERROR ("cannot get name\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    dw_ldie_t child = { library->d_debug };
    switch (dwarf_child (d_type_die, &child.die, d_error)) {
        case DW_DLV_ERROR:
            DW_TRACE_DIE_ERROR ("cannot obtain child\n",
                                library->d_debug, d_type_die, *d_error);
            return NULL;
        case DW_DLV_NO_ENTRY:
            CHECK (child.die == NULL);
        case DW_DLV_OK:
            break;
        default:
            CHECK_UNREACHABLE ();
    }

    return enum_members (library->d_debug,
                         child.die,
                         d_error,
                         name.string,
                         d_byte_size,
                         0);
}


static const EolTypeInfo*
library_build_structure_type_typeinfo (EolLibrary  *library,
                                       Dwarf_Die    d_type_die,
                                       Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_NOT_NULL (d_type_die);
    CHECK_NOT_NULL (d_error);
    CHECK (*d_error == DW_DLE_NE);

    dw_lstring_t name = {
        library->d_debug,
        dw_die_name (d_type_die, d_error),
    };
    if (!name.string && *d_error != DW_DLE_NE) {
        TRACE ("cannot get TUE DW_AT_name (%s)\n", dw_errmsg (*d_error));
        return NULL;
    }

    Dwarf_Unsigned d_byte_size;
    if (!dw_die_get_uint_attr (library->d_debug,
                               d_type_die,
                               DW_AT_byte_size,
                               &d_byte_size,
                               d_error)) {
        return eol_typeinfo_new_struct (name.string, 0, 0);
    }

    dw_ldie_t child = { library->d_debug };
    int status = dwarf_child (d_type_die, &child.die, d_error);
    if (status == DW_DLV_ERROR) {
        DW_TRACE_DIE_ERROR ("cannot obtain child\n",
                            library->d_debug, d_type_die, *d_error);
        return NULL;
    }

    if (status == DW_DLV_NO_ENTRY) {
        CHECK (child.die == NULL);
    }

    return compound_type_members (library,
                                  child.die,
                                  d_error,
                                  eol_typeinfo_new_struct,
                                  name.string,
                                  d_byte_size,
                                  0);
}


static const EolTypeInfo*
library_build_typeinfo (EolLibrary  *library,
                        Dwarf_Off    d_offset,
                        Dwarf_Error *d_error)
{
    CHECK_NOT_NULL (library);
    CHECK_SIZE_NE (DW_DLV_BADOFFSET, d_offset);
    CHECK_NOT_NULL (d_error);

    Dwarf_Die d_type_die = library_fetch_die (library, d_offset, d_error);
    if (!d_type_die) return NULL;

    const EolTypeInfo *result = NULL;
    Dwarf_Half d_tag;
    if (dwarf_tag (d_type_die, &d_tag, d_error) == DW_DLV_OK) {
        switch (d_tag) {
#define BUILD_TYPEINFO(name)                                       \
        case DW_TAG_ ## name:                                      \
            result = library_build_ ## name ## _typeinfo (library, \
                                      d_type_die, d_error); break;

            DW_TYPE_TAG_NAMES (BUILD_TYPEINFO)

#undef BUILD_TYPEINFO

            case DW_TAG_subroutine_type:
                /*
                 * TODO: Support function types. For now just pass function
                 *       pointers around as opaque void* pointers.
                 */
                TRACE (TODO YELLOW "DW_TAG_subroutine_type" NORMAL "\n");
                result = eol_typeinfo_void;
                break;

            default:
                DW_TRACE_DIE ("unsupported tag %#x\n",
                              library->d_debug, d_type_die, (unsigned) d_tag);
                result = NULL;
        }
    }

    dwarf_dealloc (library->d_debug, d_type_die, DW_DLA_DIE);
    return result;
}


static Dwarf_Off
library_get_tue_offset (EolLibrary  *library,
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
        Dwarf_Error d_typename_error = DW_DLE_NE;
        if (dwarf_pubtypename (library->d_types[i],
                               &type_name,
                               &d_typename_error) != DW_DLV_OK) {
            TRACE ("skipped malformed type at index %li (%s)\n",
                   (long) i, dw_errmsg (d_typename_error));
            continue;
        }

        const bool found = (strcmp (type_name, name) == 0);
        dwarf_dealloc (library->d_debug, type_name, DW_DLA_STRING);

        if (found) {
            Dwarf_Off d_offset;
            if (dwarf_pubtype_die_offset (library->d_types[i],
                                          &d_offset,
                                          d_error) != DW_DLV_OK) {
                TRACE ("could not obtain TUE offset (%s)\n",
                       dw_errmsg (*d_error));
                d_offset = DW_DLV_BADOFFSET;
            }
            return d_offset;
        }
    }

    return DW_DLV_BADOFFSET;
}


#define EOL_FCALL_IMPLEMENT 1
#include "eol-fcall.h"
