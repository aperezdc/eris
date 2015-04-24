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
#include <libelf.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


#ifndef ERIS_LIB_SUFFIX
#define ERIS_LIB_SUFFIX ".so"
#endif /* !ERIS_LIB_SUFFIX */


/*
 * Data needed for each library loaded by "eris.load()".
 */
typedef struct {
    int         fd;
    Dwarf_Debug dd;
    void       *dl;
    intptr_t    dl_diff;
} ErisLibrary;


static const char ERIS_LIBRARY[]  = "org.perezdecastro.eris.Library";


static inline ErisLibrary*
to_eris_library (lua_State *L)
{
    return (ErisLibrary*) luaL_checkudata (L, 1, ERIS_LIBRARY);
}


static bool find_library_base_address (ErisLibrary *e);


/*
 * Represents a callable function from a library, as returned by
 * "lib:lookup()".
 */
typedef struct {
    ErisLibrary *library; /* Link back to the library. */
    void        *address;
    char        *name;
} ErisFunction;


static const char ERIS_FUNCTION[] = "org.perezdecastro.eris.Function";


static inline ErisFunction*
to_eris_function (lua_State *L)
{
    return (ErisFunction*) luaL_checkudata (L, 1, ERIS_FUNCTION);
}


static bool lookup_function_info (ErisFunction *f);


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
    ErisLibrary *e = to_eris_library (L);
    TRACE ("%p\n", e);

    Dwarf_Error dd_error;
    dwarf_finish (e->dd, &dd_error);
    close (e->fd);
    dlclose (e->dl);

    return 0;
}


static int
eris_library_tostring (lua_State *L)
{
    ErisLibrary *e = to_eris_library (L);
    if (e->dd) {
        lua_pushfstring (L, "eris.library (%p)", e->dd);
    } else {
        lua_pushliteral (L, "eris.library (closed)");
    }
    return 1;
}


static int
eris_library_lookup (lua_State *L)
{
    ErisLibrary *e = to_eris_library (L);
    const char *name = luaL_checkstring (L, 2);

    /* Find the entry point of the function. */
    void *address = dlsym (e->dl, name);
    if (!address) {
        lua_pushnil (L);
        lua_pushstring (L, dlerror ());
        return 2;
    }

    /* Find the DIEs needed to generate a wrapper for the symbol. */
    ErisFunction *f = lua_newuserdata (L, sizeof (ErisFunction));
    f->library = e;
    f->name = strdup (name);
    f->address = address;
    if (!lookup_function_info (f)) {
        return luaL_error (L, "could not look up DWARF information for "
                           "symbol '%s' (library %p)", name, e);
    }
    luaL_setmetatable (L, ERIS_FUNCTION);
    TRACE ("new ErisFunction* at %p (%p:%s)\n",
           f, f->library, f->name);
    return 1;
}


static int
eris_function_call (lua_State *L)
{
    ErisFunction *f = to_eris_function (L);
    TRACE ("%p:%p (%s)\n", f->library, f, f->name);
    return 0;
}


static int
eris_function_gc (lua_State *L)
{
    ErisFunction *f = to_eris_function (L);
    TRACE ("%p (%p:%s)\n", f, f->library, f->name);
    free (f->name);
    return 0;
}


static int
eris_function_tostring (lua_State *L)
{
    ErisFunction *f = to_eris_function (L);
    lua_pushfstring (L, "eris.function (%p:%s)", f->library, f->name);
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


static void
create_meta (lua_State *L)
{
    /* ErisLibrary */
    luaL_newmetatable (L, ERIS_LIBRARY);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, eris_library_methods, 0);
    lua_pop (L, 1);

    /* ErisFunctionInfo */
    luaL_newmetatable (L, ERIS_FUNCTION);
    luaL_setfuncs (L, eris_function_methods, 0);
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

    Dwarf_Handler dd_error_handler = 0;
    Dwarf_Ptr dd_error_argument = 0;
    Dwarf_Error dd_error;
    Dwarf_Debug dd;

    if (dwarf_init (fd, DW_DLC_READ,
                    dd_error_handler, dd_error_argument,
                    &dd, &dd_error) != DW_DLV_OK) {
        close (fd);
        dlclose (dl);
        return luaL_error (L, "error reading debug information from '%s' (%s)",
                           path, dwarf_errmsg (dd_error));
    }

    ErisLibrary *e = lua_newuserdata (L, sizeof (ErisLibrary));
    e->fd = fd;
    e->dd = dd;
    e->dl = dl;

    if (!find_library_base_address (e)) {
        dwarf_finish (dd, &dd_error);
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
    TRACE ("new ErisLibrary* at %p\n", e);
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
find_library_base_address (ErisLibrary *e)
{
    struct link_map *map = NULL;
    if (dlinfo (e->dl, RTLD_DI_LINKMAP, &map) != 0) {
        return false;
    }
    e->dl_diff = map->l_addr;
    TRACE ("diff = %#llx (link_map)\n", (long long) e->dl_diff);
    return true;
}

#else
# error No method chosen to determe the load address of libraries
#endif /* ERIS_USE_LINK_H */


static bool
lookup_function_info (ErisFunction *f)
{
    /* TODO: Implement. */
    return true;
}
