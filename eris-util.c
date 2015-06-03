/*
 * eris-util.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-util.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(__GLIBC__) && __GLIBC__ >= 2
# ifndef ERIS_CHECK_BACKTRACE
# define ERIS_CHECK_BACKTRACE 5
# endif /* !ERIS_CHECK_BACKTRACE */
# include <execinfo.h>
#else
# undef ERIS_CHECK_BACKTRACE
#endif /* __GLIBC__ */


void
eris_runtime_check_failed (const char *file,
                           unsigned    line,
                           const char *func,
                           const char *fmt,
                           ...)
{
#ifdef ERIS_CHECK_BACKTRACE
    void *addresses[ERIS_CHECK_BACKTRACE + 1];
    size_t size = backtrace (addresses, LENGTH_OF (addresses));
    char **names = backtrace_symbols (addresses, size);

    fprintf (stderr, "\nSTACK:[37m\n");
    for (size_t i = size; i > 1; i--)
        fprintf (stderr, "%2u | %s\n", (unsigned) (i - 2), names[i-1]);
    free (names);
#endif /* ERIS_CHECK_BACKTRACE */

    fprintf (stderr,
             "\n[1;31m=== CHECK FAILED ===[0;0m at "
             "[1;1m%s[0;0m(), [36m%s[0m:%u\n",
             func, file, line);

    va_list args;
    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);
    fputc ('\n', stderr);
    fflush (stderr);
    abort ();
}

