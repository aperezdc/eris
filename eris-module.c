/*
 * eris-module.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"

#include "util.h"

#include <libdwarf/libdwarf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#ifndef ERIS_LIB_SUFFIX
#define ERIS_LIB_SUFFIX ".so"
#endif /* !ERIS_LIB_SUFFIX */


static bool
find_library (const char *name, char path[PATH_MAX])
{
    /* TODO: Handle multilib systems (lib32 vs. lib64) */
    static const char *search_paths[] = {
        "/lib",
        "/usr/lib",
        "/usr/local/lib",
    };

    for (size_t i = 0; i < LENGTH_OF (search_paths); i++) {
        if (snprintf (path, PATH_MAX, "%s/%s" ERIS_LIB_SUFFIX,
                      search_paths[i], name) > PATH_MAX) {
            return false;
        }

        struct stat sb;
        if (stat (path, &sb) == 0 && S_ISREG (sb.st_mode) &&
            ((sb.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) != 0)) {
            return true;
        }
    }

    return false;
}


static int
eris_load (lua_State *L)
{
    size_t name_length;
    const char *name = luaL_checklstring (L, 1, &name_length);
    char path[PATH_MAX];

    if (name_length > 1 && (name[0] == '/' || name[0] == '.')) {
        /* Absolute or relative path: resolve using realpath(). */
        if (!realpath (name, path)) {
            return luaL_error (L, "could not find real path of '%s' (%s)",
                               name, strerror (errno));
        }
    } else {
        /* Some name: find library in system directories. */
        if (!find_library (name, path)) {
            return luaL_error (L, "could not find library '%s'", name);
        }
    }

    lua_pushstring (L, path);
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
    return 1;
}
