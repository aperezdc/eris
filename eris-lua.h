/*
 * eris-lua.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_LUA_H
#define ERIS_LUA_H

#if defined(ERIS_LUA_BUNDLED) && ERIS_LUA_BUNDLED
# include "lua.h"
# include "lauxlib.h"
#else
# include <lua.h>
# include <lauxlib.h>
#endif

#endif /* !ERIS_LUA_H */
