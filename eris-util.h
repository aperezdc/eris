/*
 * eris-util.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_UTIL_H
#define ERIS_UTIL_H

#define LENGTH_OF(array) \
    (sizeof (array) / sizeof (array[0]))

/*
 * Debug checks.
 */
#if defined(ERIS_RUNTIME_CHECKS) && ERIS_RUNTIME_CHECKS > 0

# define CHECK_FAILED(...) \
    eris_runtime_check_failed (__FILE__, __LINE__, __func__, __VA_ARGS__)

# define CHECK(expression)                                  \
    do {                                                    \
        if (!(expression))                                  \
            CHECK_FAILED ("expression: %s\n", #expression); \
    } while (0)

# define CHECK_NUMERIC_EQ(expect, expr, type, va_type, fmt) \
    do {                                                    \
        type eval_expect = (expect);                        \
        type eval_expr = (expr);                            \
        if (eval_expect != eval_expr)                       \
            CHECK_FAILED ("expression: %s\n"                \
                          "expected: " fmt "\n"             \
                          "value: " fmt "\n",               \
                          #expr,                            \
                          (va_type) eval_expect,            \
                          (va_type) eval_expr);             \
    } while (0)

#define CHECK_STR_EQ(expected, expression)                  \
    do {                                                    \
        const char *eval_expected = (expected);             \
        const char *eval_expression = (expression);         \
        if (strcmp (eval_expected, eval_expression) != 0)   \
            CHECK_FAILED ("expression: %s\n"                \
                          "expected: '%s'\n"                \
                          "value: '%s'\n",                  \
                          #expression,                      \
                          eval_expected,                    \
                          eval_expression);                 \
    } while (0);

extern void eris_runtime_check_failed (const char *file,
                                       unsigned    line,
                                       const char *func,
                                       const char *fmt,
                                       ...);
#else /* !ERIS_DEBUG_CHECKS */
# undef ERIS_RUNTIME_CHECKS
# define ERIS_RUNTIME_CHECKS 0
# define CHECK(e)                         ((void) 0)
# define CHECK_NUMERIC_EQ(e, x, t, vt, f) ((void) 0)
# define CHECK_STR_EQ(e, x)               ((void) 0)
#endif /* ERIS_DEBUG_CHECKS */

#define CHECK_I8_EQ(e, x)    CHECK_NUMERIC_EQ (e, x, int8_t, int, "%i")
#define CHECK_U8_EQ(e, x)    CHECK_NUMERIC_EQ (e, x, uint8_t, unsigned, "%u")
#define CHECK_I16_EQ(e, x)   CHECK_NUMERIC_EQ (e, x, int16_t, int, "%i")
#define CHECK_U16_EQ(e, x)   CHECK_NUMERIC_EQ (e, x, uint16_t, unsigned, "%u")
#define CHECK_I32_EQ(e, x)   CHECK_NUMERIC_EQ (e, x, int32_t, long, "%li")
#define CHECK_U32_EQ(e, x)   CHECK_NUMERIC_EQ (e, x, uint32_t, unsigned long, "%lu")
#define CHECK_INT_EQ(e, x)   CHECK_NUMERIC_EQ (e, x, int, int, "%i")
#define CHECK_UINT_EQ(e, x)  CHECK_NUMERIC_EQ (e, x, unsigned, unsigned, "%u")
#define CHECK_SIZE_EQ(e, x)  CHECK_NUMERIC_EQ (e, x, size_t, unsigned long long, "%llu")
#define CHECK_SSIZE_EQ(e, x) CHECK_NUMERIC_EQ (e, x, ssize_t, long long, "%lli")


/*
 * Helper macros to create reference-counted structure types. First, the
 * REF_COUNTER macro adds a member in a struct for the reference counter:
 *
 *    struct BigData {
 *          REF_COUNTER;
 *          // Other members.
 *    }
 *
 * Then, functions to manage the reference counter can be created using
 * REF_COUNTER_FUNCTIONS, which receives as parameters the name of the
 * struct type and a prefix for the created functions:
 *
 *   void big_data_free (struct BigData *data);
 *   REF_COUNTER_FUNCTIONS (struct BigData, big_data)
 *
 * The functions declared by the above macro expansion will have the
 * following signatures:
 *
 *   struct BigData* big_data_ref (struct BigData*);
 *   void big_data_unref (struct BigData*);
 *
 * Optionally, the REF_COUNTER_FUNCTIONS accepts an additional parameter
 * which can be used to specify the storage class of the functions, so:
 *
 *   REF_COUNTER_FUNCTIONS(struct BigData, big_data, static inline)
 *
 * will create the functions as:
 *
 *   static inline struct BigData* big_data_ref (struct BigData*);
 *   static inline void big_data_unref (struct BigData*);
 *
 * Note that the macro expects a big_data_free() function to be already
 * declared (or defined). That will be the function which gets called
 * when the reference counter hits 0 in order to free the struct BigData.
 *
 * Last, but not least, the REF_COUNTER_DECLARE_FUNCTIONS macro can be
 * used to only declare (but not define) the reference counting functions.
 * Like REF_COUNTER_FUNCTIONS, it accepts an optional parameter which can
 * be used to specify the storage class. This is typically used to declare
 * the functions in a header:
 *
 *   REF_COUNTER_DECLARE_FUNCTIONS (struct BigData, big_data, extern)
 */
#ifndef REF_COUNTER_TYPE
#define REF_COUNTER_TYPE unsigned int
#endif /* !REF_COUNTER_TYPE */

#ifndef REF_COUNTER_NAME
#define REF_COUNTER_NAME ref_counter
#endif /* !REF_COUNTER_NAME */

#define REF_COUNTER \
    REF_COUNTER_TYPE REF_COUNTER_NAME

#define REF_COUNTER_FUNCTIONS(type, prefix, ...)    \
    __VA_ARGS__ type* prefix ## _ref (type* obj) {  \
        obj->REF_COUNTER_NAME++; return obj;        \
    }                                               \
    __VA_ARGS__ bool prefix ## _unref (type* obj) { \
        if (--obj->REF_COUNTER_NAME == 0) {         \
            prefix ## _free (obj);                  \
            return true;                            \
        }                                           \
        return false;                               \
    }

#define REF_COUNTER_DECLARE_FUNCTIONS(type, prefix, ...) \
    __VA_ARGS__ bool  prefix ## _unref (type*);          \
    __VA_ARGS__ type* prefix ## _ref (type*)

#endif /* !ERIS_UTIL_H */
