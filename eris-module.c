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
