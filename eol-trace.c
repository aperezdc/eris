/*
 * eol-trace.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eol-trace.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef eol_trace_setup


enum {
    TRACE_NONE = 0,
    TRACE_FILE = (1 << 1),
    TRACE_LINE = (1 << 2),
    TRACE_FUNC = (1 << 3),
    TRACE_ALL  = (TRACE_FILE | TRACE_LINE | TRACE_FUNC)
};


/*
 * These values are configured by eol_trace_setup().
 */
bool eol_trace_enabled      = false;
static FILE* trace_output   = NULL;
static unsigned trace_items = TRACE_NONE;


void
eol_trace (const char *file,
           unsigned    line,
           const char* func,
           const char* fmt,
           ...)
{
    if (*fmt == '>') {
        fmt++;
    } else {
        bool insert_space = false;
        if (trace_items & TRACE_FILE) {
            fprintf (trace_output, "%s:", file);
            insert_space = true;
        }
        if (trace_items & TRACE_FUNC) {
            fprintf (trace_output, "%s:", func);
            insert_space = true;
        }
        if (trace_items & TRACE_LINE) {
            fprintf (trace_output, "%u:", line);
            insert_space = true;
        }
        if (insert_space) {
            fputc (' ', trace_output);
        }
    }

    va_list args;
    va_start (args, fmt);
    vfprintf (trace_output, fmt, args);
    va_end (args);

    fflush (trace_output);
}


void
eol_trace_setup (void)
{
    /* Tracing was already configured. */
    if (trace_output != NULL)
        return;

    trace_output = stderr;
    const char *env_value = getenv ("EOL_TRACE");

    /* Check for an empty environment variable. */
    if (!env_value || !*env_value)
        return;

    bool file_append = false;

    /* Check flags. */
    for (; *env_value; env_value++) {
        switch (*env_value) {
            case 'L':
                trace_items |= TRACE_LINE;
                break;
            case 'S':
                trace_items |= TRACE_FILE;
                break;
            case 'F':
                trace_items |= TRACE_FUNC;
                break;
            case 'A':
                trace_items |= TRACE_ALL;
                break;

            case 'l':
                trace_items &= ~TRACE_LINE;
                break;
            case 's':
                trace_items &= ~TRACE_FILE;
                break;
            case 'f':
                trace_items &= ~TRACE_FUNC;
                break;

            case '>':
                file_append = true;
                /* fall-through */
            case ':':
                env_value++;
                trace_output = fopen (env_value,
                                      file_append ? "ab" : "wb");
                if (!trace_output) {
                    fprintf (stderr,
                             "Could not open '%s' for %s (%s), using stderr\n",
                             env_value, file_append ? "appending" : "writing",
                             strerror (errno));
                    fflush (stderr);
                    trace_output = stderr;
                }
                goto setup_done;
                break;
        }
    }

setup_done:
    eol_trace_enabled = true;
}
