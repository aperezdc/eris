/*
 * libtest.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */


int intvar = 42;
int *intptrvar = &intvar;


static int
private_add (int a, int b)
{
    return a + b;
}


int
add (int a, int b)
{
    return private_add (a, b);
}
