/*
 * eris-module.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"

#include "eris-util.h"

#include <libdwarf/libdwarf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>


#ifndef ERIS_LIB_SUFFIX
#define ERIS_LIB_SUFFIX ".so"
#endif /* !ERIS_LIB_SUFFIX */


static const char ERIS_LIBRARY[] = "org.perezdecastro.eris.Library";


typedef struct {
    int         fd;
    Dwarf_Debug dd;
} ErisLibrary;


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


#define to_eris_handle(L) \
    ((ErisLibrary*) luaL_checkudata (L, 1, ERIS_LIBRARY))


static int
eris_handle_close (lua_State *L)
{
    return 0;
}


static int
eris_handle_gc (lua_State *L)
{
    return 0;
}


static int
eris_handle_tostring (lua_State *L)
{
    ErisLibrary *e = to_eris_handle (L);
    if (e->dd) {
        lua_pushfstring (L, "library (%p)", e->dd);
    } else {
        lua_pushliteral (L, "library (closed)");
    }
    return 1;
}


/* Methods for Erisuserdatas. */
static const luaL_Reg eris_handle_methods[] = {
    { "close",      eris_handle_close    },
    { "__gc",       eris_handle_gc       },
    { "__tostring", eris_handle_tostring },
    { NULL, NULL }
};


static void
create_meta (lua_State *L)
{
    luaL_newmetatable (L, ERIS_LIBRARY);
    lua_pushvalue (L, -1);           /* Push metatable */
    lua_setfield (L, -2, "__index"); /* metatable.__index == metatable */
    luaL_setfuncs (L, eris_handle_methods, 0);
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

    int fd = open (path, O_RDONLY, 0);
    if (fd < 0) {
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
        return luaL_error (L, "error reading debug information from '%s' (%s)",
                           path, dwarf_errmsg (dd_error));
    }

    ErisLibrary *handle = lua_newuserdata (L, sizeof (ErisLibrary));
    handle->fd = fd;
    handle->dd = dd;
    luaL_setmetatable (L, ERIS_LIBRARY);
    return 1;
}


static const luaL_Reg erislib[] = {
    { "load", eris_load },
    { NULL, NULL },
};


LUAMOD_API int
luaopen_eris (lua_State *L)
{
    luaL_newlib (L, erislib);
    create_meta (L);
    return 1;
}
