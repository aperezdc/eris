/*
 * eol-fcall-x64.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef EOL_FCALL_X64_H
#define EOL_FCALL_X64_H

#define EOL_FUNCTION_FCALL_FIELDS lua_CFunction fcall_jit_func
#define EOL_FUNCTION_FCALL_INIT(ef) fcall_jit_compile (L, ef)
#define EOL_FUNCTION_FCALL_FREE     fcall_jit_free

static void fcall_jit_compile (lua_State*, const EolFunction*);
static void fcall_jit_free (EolFunction*);

#endif /* !EOL_FCALL_X64_H */
