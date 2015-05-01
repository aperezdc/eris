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
#include <unistd.h>
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


static const luaL_Reg testutillib[] = {
    { "isatty",  testutil_isatty  },
    { "fork",    testutil_fork    },
    { "waitpid", testutil_waitpid },
    { NULL, NULL }
};


LUAMOD_API int
luaopen_testutil (lua_State *L)
{
    luaL_newlib (L, testutillib);
    return 1;
}
