/*
 * harness-testutil.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "../eol-lua.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


static int
testutil_isatty (lua_State *L)
{
    luaL_Stream *f = (luaL_Stream*) luaL_checkudata (L, 1, LUA_FILEHANDLE);
    lua_pushboolean (L, isatty (fileno (f->f)));
    return 1;
}


static int
testutil_listdir (lua_State *L)
{
    const char *path = luaL_checkstring (L, 1);
    DIR *d = opendir (path);
    if (d == NULL) {
        return luaL_error (L, "%s: %s", path, strerror (errno));
    }

    int index = 0;
    struct dirent *de = NULL;

    lua_newtable (L); /* Stack: Table */
    while ((de = readdir (d)) != NULL) {
        /* Skip "." and ".." entries. */
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' ||
             (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
            continue;
        }
        lua_pushstring (L, de->d_name); /* Stack: Table Filename */
        lua_rawseti (L, -2, ++index);   /* Stack: Table */
    }
    closedir (d);

    return 1;
}


static int
testutil_realpath (lua_State *L)
{
    const char *path = luaL_checkstring (L, 1);
    char buffer[PATH_MAX];
    if (realpath (path, buffer) == NULL) {
        return luaL_error (L, "%s: %s", path, strerror (errno));
    }
    lua_pushstring (L, buffer);
    return 1;
}


static int
testutil_isfile (lua_State *L)
{
    struct stat sb;
    const char *path = luaL_checkstring (L, 1);
    if (stat (path, &sb) == 0) {
        lua_pushboolean (L, S_ISREG (sb.st_mode));
    } else if (errno == ENOENT || errno == ENOTDIR) {
        lua_pushboolean (L, false);
    } else {
        return luaL_error (L, "%s: %s", path, strerror (errno));
    }
    return 1;
}


static int
testutil_isdir (lua_State *L)
{
    struct stat sb;
    const char *path = luaL_checkstring (L, 1);
    if (stat (path, &sb) == 0) {
        lua_pushboolean (L, S_ISDIR (sb.st_mode));
    } else if (errno == ENOENT || errno == ENOTDIR) {
        lua_pushboolean (L, false);
    } else {
        return luaL_error (L, "%s: %s", path, strerror (errno));
    }
    return 1;
}


static int
testutil_getcwd (lua_State *L)
{
    char path[PATH_MAX];
    if (getcwd (path, PATH_MAX) == NULL)
        return luaL_error (L, "getcwd(): %s", strerror (errno));
    lua_pushstring (L, path);
    return 1;
}


static const luaL_Reg testutillib[] = {
    { "isatty",   testutil_isatty   },
    { "listdir",  testutil_listdir  },
    { "realpath", testutil_realpath },
    { "isfile",   testutil_isfile   },
    { "isdir",    testutil_isdir    },
    { "getcwd",   testutil_getcwd   },
    { NULL, NULL }
};


LUAMOD_API int
luaopen_testutil (lua_State *L)
{
    luaL_newlib (L, testutillib);
    return 1;
}
