/*
 * eris-fcall.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

/*
 * Select the appropriate implementation of eris_function_call()
 * depending on the target architecture and operating system.
 */
#if defined(ERIS_FCALL_FFI) && ERIS_FCALL_FFI > 0
# ifdef ERIS_FCALL_IMPLEMENT
#  include "eris-fcall-ffi.c"
# else
#  include "eris-fcall-ffi.h"
# endif
#elif defined(__x86_64__) || defined(__x86_64) || \
    defined(__amd64__)  || defined(__amd64)
# ifdef ERIS_FCALL_IMPLEMENT
#  include "eris-fcall-x64.h"
# endif
#else
# error No eris_fcall implementation chosen, you may want to configure with --enable-ffi
#endif
