/*
 * eol-fcall-dasm.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

static int
function_call (lua_State *L)
{
    EolFunction *ef = to_eol_function (L);
    CHECK_NOT_NULL (ef->fcall_jit_func);
    return (*ef->fcall_jit_func) (L);
}


static void
fcall_jit_free (EolFunction *ef)
{
}
