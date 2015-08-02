/*
 * eol-lua.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef EOL_LUA_H
#define EOL_LUA_H

#if defined(EOL_LUA_BUNDLED) && EOL_LUA_BUNDLED
# include "lua.h"
# include "lauxlib.h"
#else
# include <lua.h>
# include <lauxlib.h>
#endif

#endif /* !EOL_LUA_H */
