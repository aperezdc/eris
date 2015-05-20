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
