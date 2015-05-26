/*
 * eris-trace.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_TRACE_H
#define ERIS_TRACE_H

#include <stdbool.h>

#if defined(ERIS_TRACE) && ERIS_TRACE > 0
# undef ERIS_TRACE
# define ERIS_TRACE 1

# define TRACE(...)                                                 \
    do {                                                            \
        if (eris_trace_enabled)                                     \
            eris_trace (__FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

extern void eris_trace (const char* file,
                        unsigned    line,
                        const char* func,
                        const char* fmt,
                        ...);

/*
 * Reads the ERIS_TRACE environment variable and, if present, configures the
 * tracing mechanism according to its value. The value of the variable must
 * be a sequence of characters, which enable printing of different items
 * along with each message passed to the TRACE() macro:
 *
 *   'S' - source file name.
 *   'L' - line in source file.
 *   'F' - function name.
 *   'A' - all of the above.
 *
 * The lowercase counterparts disable printing of the corresponding item.
 * Characters other than the above (either upper- or lowercase) are ignored.
 *
 * Also, if the '>' or ':' characters are found, the rest of the value of the
 * ERIS_TRACE environment variable is taken as the name of a file to open for
 * writing (when using ':'), or appending messages (when using '>').
 */
extern void eris_trace_setup (void);

extern bool eris_trace_enabled;

#else
# undef ERIS_TRACE
# define ERIS_TRACE 0
# define eris_trace_setup( )  ((void)0)
# define TRACE(...)           ((void)0)
#endif /* ERIS_TRACE */

#endif /* !ERIS_TRACE_H */
