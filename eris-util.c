/*
 * eris-util.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-util.h"
#include <stdlib.h>
#include <stdio.h>


void
eris_runtime_check_failed (const char *file,
                           unsigned    line,
                           const char *func,
                           const char *fmt,
                           ...)
{
    fprintf (stderr,
             "=== CHECK FAILED at %s(), %s:%u ===\n",
             func, file, line);

    va_list args;
    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);
    fputc ('\n', stderr);
    fflush (stderr);
    abort ();
}

