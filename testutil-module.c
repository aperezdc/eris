/*
 * testutil-module.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


static int
testutil_isatty (lua_State *L)
{
    luaL_Stream *f = (luaL_Stream*) luaL_checkudata (L, 1, LUA_FILEHANDLE);
    lua_pushboolean (L, isatty (fileno (f->f)));
    return 1;
}


static int
testutil_fork (lua_State *L)
{
    lua_pushinteger (L, (lua_Integer) fork ());
    return 1;
}


static int
testutil_waitpid (lua_State *L)
{
    int status;
    pid_t pid = waitpid ((pid_t) luaL_checkinteger (L, 1), &status, 0);

    if (WIFEXITED (status)) {
        lua_pushstring (L, "exit");
        lua_pushinteger (L, WEXITSTATUS (status));
    } else if (WIFSIGNALED (status)) {
        lua_pushstring (L, "signal");
        lua_pushinteger (L, WTERMSIG (status));
    } else {
        lua_pushnil (L);
        lua_pushnil (L);
    }

    lua_pushinteger (L, (lua_Integer) pid);
    return 3;
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


static const luaL_Reg testutillib[] = {
    { "isatty",  testutil_isatty  },
    { "fork",    testutil_fork    },
    { "waitpid", testutil_waitpid },
    { "listdir", testutil_listdir },
    { NULL, NULL }
};


LUAMOD_API int
luaopen_testutil (lua_State *L)
{
    luaL_newlib (L, testutillib);
    return 1;
}
