/*
 * libtest.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

extern int intvar;

int
add_intvar (int a)
{
    return a + intvar;
}
