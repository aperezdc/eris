/*
 * eol-fcall.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

/*
 * Select the appropriate implementation of eol_function_call()
 * depending on the target architecture and operating system.
 */
#if defined(EOL_FCALL_FFI) && EOL_FCALL_FFI > 0
# ifdef EOL_FCALL_IMPLEMENT
#  include "eol-fcall-ffi.c"
# else
#  include "eol-fcall-ffi.h"
# endif
#elif defined(__x86_64__) || defined(__x86_64) || \
    defined(__amd64__)  || defined(__amd64)
# ifdef EOL_FCALL_IMPLEMENT
#  include "eol-fcall-x64.h"
# endif
#else
# error No eol_fcall implementation chosen, you may want to configure with --enable-ffi
#endif
