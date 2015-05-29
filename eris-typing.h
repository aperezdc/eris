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

    /* Base types. */
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

    /* Synthetic types. */
    ERIS_TYPE_POINTER,
    ERIS_TYPE_TYPEDEF,
    ERIS_TYPE_CONST,
    ERIS_TYPE_ARRAY,

    /* Compound types. */
    ERIS_TYPE_STRUCT,
} ErisType;


extern const char* eris_type_name (ErisType type);


typedef struct _ErisTypeInfo ErisTypeInfo;

typedef struct {
    const char   *name;
    uint32_t      offset;
    ErisTypeInfo *typeinfo;
} ErisTypeInfoMember;


extern const ErisTypeInfo* eris_typeinfo_new_const (const ErisTypeInfo *base);
extern const ErisTypeInfo* eris_typeinfo_new_typedef (const ErisTypeInfo *base,
                                                      const char         *name);
extern const ErisTypeInfo* eris_typeinfo_new_base_type (ErisType    type,
                                                        const char *name);
extern const ErisTypeInfo* eris_typeinfo_new_array_type (const ErisTypeInfo *base,
                                                         uint64_t            n_items);
extern const ErisTypeInfo* eris_typeinfo_base (const ErisTypeInfo *typeinfo);

extern const char* eris_typeinfo_name (const ErisTypeInfo *typeinfo);
extern ErisType    eris_typeinfo_type (const ErisTypeInfo *typeinfo);
extern bool        eris_typeinfo_equal (const ErisTypeInfo *a,
                                        const ErisTypeInfo *b);
extern uint32_t    eris_typeinfo_sizeof (const ErisTypeInfo *typeinfo);
extern bool        eris_typeinfo_is_valid (const ErisTypeInfo *typeinfo);
extern bool        eris_typeinfo_is_const (const ErisTypeInfo *typeinfo);
extern bool        eris_typeinfo_is_array (const ErisTypeInfo *typeinfo,
                                           uint64_t           *n_items);
extern bool        eris_typeinfo_is_struct (const ErisTypeInfo *typeinfo,
                                            uint32_t           *n_members);
extern uint64_t    eris_typeinfo_array_n_items (const ErisTypeInfo* typeinfo);
extern uint32_t    eris_typeinfo_struct_n_members (const ErisTypeInfo *typeinfo);

extern ErisTypeInfoMember*
eris_typeinfo_struct_named_member (ErisTypeInfo *typeinfo,
                                   const char   *name);
extern ErisTypeInfoMember*
eris_typeinfo_struct_member (ErisTypeInfo *typeinfo,
                             uint32_t      index);

extern const ErisTypeInfoMember*
eris_typeinfo_struct_const_named_member (const ErisTypeInfo *typeinfo,
                                         const char         *name);
extern const ErisTypeInfoMember*
eris_typeinfo_struct_const_member (const ErisTypeInfo *typeinfo,
                                   uint32_t            index);

#endif /* !ERIS_TYPING_H */
