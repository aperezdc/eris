/*
 * eol-util.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eol-util.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(__GLIBC__) && __GLIBC__ >= 2
# ifndef EOL_CHECK_BACKTRACE
# define EOL_CHECK_BACKTRACE 5
# endif /* !EOL_CHECK_BACKTRACE */
# include <execinfo.h>
#else
# undef EOL_CHECK_BACKTRACE
#endif /* __GLIBC__ */


void
eol_runtime_check_failed (const char *file,
                          unsigned    line,
                          const char *func,
                          const char *fmt,
                          ...)
{
#ifdef EOL_CHECK_BACKTRACE
    void *addresses[EOL_CHECK_BACKTRACE + 1];
    size_t size = backtrace (addresses, LENGTH_OF (addresses));
    char **names = backtrace_symbols (addresses, size);

    fprintf (stderr, "\nSTACK:[37m\n");
    for (size_t i = size; i > 1; i--)
        fprintf (stderr, "%2u | %s\n", (unsigned) (i - 2), names[i-1]);
    free (names);
#endif /* EOL_CHECK_BACKTRACE */

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


void
lauto_free (void *ptr)
{
    void **location = ptr;
    if (*location) {
        free (*location);
        *location = NULL;
    }
}
