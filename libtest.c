/*
 * libtest.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdint.h>

/* Simple integer variable, and a pointer to it. */
int intvar = 42;
int *intptrvar = &intvar;

/* Standard C99 integral typedefs. */
int8_t   var_i8  =  -8;
uint8_t  var_u8  =   8;
int16_t  var_i16 = -16;
uint16_t var_u16 =  16;
int32_t  var_i32 = -32;
uint32_t var_u32 =  32;
int64_t  var_i64 = -64;
uint64_t var_u64 =  64;

/* Floating point numbers. */
float    var_flt = 1.0;
double   var_dbl = 1.0;

/* Constant declaration. */
const int const_int = 42;


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
