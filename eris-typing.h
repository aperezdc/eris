/*
 * eris-typing.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_TYPING_H
#define ERIS_TYPING_H

#include <stdint.h>
#include <stdbool.h>


#define INTEGER_S_TYPES(F) \
    F (S8,  int8_t  )      \
    F (S16, int16_t )      \
    F (S32, int32_t )      \
    F (S64, int64_t )

#define INTEGER_U_TYPES(F) \
    F (U8,  uint8_t )      \
    F (U16, uint16_t)      \
    F (U32, uint32_t)      \
    F (U64, uint64_t)

#define INTEGER_TYPES(F) \
    INTEGER_S_TYPES (F)  \
    INTEGER_U_TYPES (F)

#define FLOAT_TYPES(F) \
    F (DOUBLE, double) \
    F (FLOAT,  float)

#define BASE_TYPES(F) \
    INTEGER_TYPES (F) \
    FLOAT_TYPES (F) \
    F (POINTER, uintptr_t)


typedef enum {
    ERIS_TYPE_NONE = 0,

    ERIS_TYPE_VOID,

    ERIS_TYPE_S8,
    ERIS_TYPE_U8,
    ERIS_TYPE_S16,
    ERIS_TYPE_U16,
    ERIS_TYPE_S32,
    ERIS_TYPE_U32,
    ERIS_TYPE_S64,
    ERIS_TYPE_U64,

    ERIS_TYPE_FLOAT,
    ERIS_TYPE_DOUBLE,

    ERIS_TYPE_POINTER,
    ERIS_TYPE_TYPEDEF,

    ERIS_TYPE_STRUCT,
} ErisType;


extern const char* eris_type_name (ErisType type);


typedef struct _ErisTypeInfo ErisTypeInfo;

typedef struct {
    const char   *name;
    uint32_t      offset;
    ErisTypeInfo *typeinfo;
} ErisTypeInfoMember;


extern ErisTypeInfo* eris_typeinfo_new (ErisType type, uint16_t n_members);
extern const char*   eris_typeinfo_name (const ErisTypeInfo *typeinfo);
extern ErisType      eris_typeinfo_type (const ErisTypeInfo *typeinfo);
extern bool          eris_typeinfo_equal (const ErisTypeInfo *a,
                                          const ErisTypeInfo *b);
extern uint32_t      eris_typeinfo_sizeof (const ErisTypeInfo *typeinfo);
extern bool          eris_typeinfo_is_valid (const ErisTypeInfo *typeinfo);
extern uint16_t      eris_typeinfo_n_members (const ErisTypeInfo *typeinfo);
extern bool          eris_typeinfo_is_readonly (const ErisTypeInfo *typeinfo);
extern void          eris_typeinfo_set_name (ErisTypeInfo *typeinfo,
                                             const char   *name);
extern void          eris_typeinfo_set_is_readonly (ErisTypeInfo *typeinfo,
                                                    bool          readonly);

extern ErisTypeInfoMember*
eris_typeinfo_named_member (ErisTypeInfo *typeinfo,
                            const char   *name);
extern ErisTypeInfoMember*
eris_typeinfo_member (ErisTypeInfo *typeinfo,
                      uint16_t      index);

extern const ErisTypeInfoMember*
eris_typeinfo_const_named_member (const ErisTypeInfo *typeinfo,
                                  const char         *name);
extern const ErisTypeInfoMember*
eris_typeinfo_const_member (const ErisTypeInfo *typeinfo,
                            uint16_t            index);

#endif /* !ERIS_TYPING_H */
