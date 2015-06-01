/*
 * eris-typing.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_TYPING_H
#define ERIS_TYPING_H

#include <inttypes.h>
#include <stdbool.h>


#define INTEGER_S_TYPES(F) \
    F (S8,  s8,  int8_t  ) \
    F (S16, s16, int16_t ) \
    F (S32, s32, int32_t ) \
    F (S64, s64, int64_t )

#define INTEGER_U_TYPES(F) \
    F (U8,  u8,  uint8_t ) \
    F (U16, u16, uint16_t) \
    F (U32, u32, uint32_t) \
    F (U64, u64, uint64_t)

#define INTEGER_TYPES(F) \
    INTEGER_S_TYPES (F)  \
    INTEGER_U_TYPES (F)

#define FLOAT_TYPES(F)         \
    F (DOUBLE, double, double) \
    F (FLOAT,  float,  float)

#define BASE_TYPES(F) \
    INTEGER_TYPES (F) \
    FLOAT_TYPES (F)   \
    F (POINTER, pointer, uintptr_t)

#define SYNTHETIC_TYPES(F)     \
    F (TYPEDEF, typedef, void) \
    F (CONST,   const,   void)

#define COMPOUND_TYPES(F)    \
    F (STRUCT, struct, void) \
    F (ARRAY,  array,  void)

#define ALL_TYPES(F)    \
    BASE_TYPES (F)      \
    SYNTHETIC_TYPES (F) \
    COMPOUND_TYPES (F)  \
    F (VOID, void, void)


typedef enum {
#define TYPE_ENUM_ENTRY(suffix, name, ctype) ERIS_TYPE_ ## suffix,
    ALL_TYPES (TYPE_ENUM_ENTRY)
#undef TYPE_ENUM_ENTRY
} ErisType;


extern const char* eris_type_name (ErisType type);

#define TYPE_CASE_TRUE(suffix, name, ctype) \
    case ERIS_TYPE_ ## suffix : return true;

#define DECLARE_ERIS_TYPE_IS(suffix, check)                     \
    static inline bool eris_type_is_ ## check (ErisType type) { \
        switch (type) {                                         \
            SYNTHETIC_TYPES (TYPE_CASE_TRUE)                    \
            default: return false;                              \
        }                                                       \
    }

DECLARE_ERIS_TYPE_IS (BASE,      base)
DECLARE_ERIS_TYPE_IS (SYNTHETIC, synthetic)
DECLARE_ERIS_TYPE_IS (COMPOUND,  compound)

#undef DECLARE_ERIS_TYPE_IS
#undef TYPE_CASE_TRUE


typedef struct _ErisTypeInfo ErisTypeInfo;

typedef struct {
    const char         *name;
    uint32_t            offset;
    const ErisTypeInfo *typeinfo;
} ErisTypeInfoMember;


extern ErisTypeInfo* eris_typeinfo_new_const   (const ErisTypeInfo *base);
extern ErisTypeInfo* eris_typeinfo_new_typedef (const ErisTypeInfo *base,
                                                const char         *name);
extern ErisTypeInfo* eris_typeinfo_new_base    (ErisType    type,
                                                const char *name);
extern ErisTypeInfo* eris_typeinfo_new_array   (const ErisTypeInfo *base,
                                                uint64_t            n_items);
extern ErisTypeInfo* eris_typeinfo_new_struct  (const char *name,
                                                uint32_t    size,
                                                uint32_t    n_members);

extern void eris_typeinfo_free (ErisTypeInfo *typeinfo);

extern const ErisTypeInfo* eris_typeinfo_base (const ErisTypeInfo *typeinfo);

extern const char* eris_typeinfo_name (const ErisTypeInfo *typeinfo);
extern ErisType    eris_typeinfo_type (const ErisTypeInfo *typeinfo);
extern bool        eris_typeinfo_equal (const ErisTypeInfo *a,
                                        const ErisTypeInfo *b);
extern uint32_t    eris_typeinfo_sizeof (const ErisTypeInfo *typeinfo);
extern uint64_t    eris_typeinfo_array_n_items (const ErisTypeInfo* typeinfo);
extern uint32_t    eris_typeinfo_struct_n_members (const ErisTypeInfo *typeinfo);

extern bool eris_typeinfo_get_const (const ErisTypeInfo *typeinfo);
extern const ErisTypeInfo* eris_typeinfo_get_struct (const ErisTypeInfo *typeinfo);
extern const ErisTypeInfo* eris_typeinfo_get_non_synthetic (const ErisTypeInfo *typeinfo);


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

#define DECLARE_TYPEINFO_IS_TYPE(suffix, name, ctype) \
    static inline bool eris_typeinfo_is_ ## name (const ErisTypeInfo *typeinfo) { \
        return eris_typeinfo_type (typeinfo) == ERIS_TYPE_ ## suffix; }

ALL_TYPES (DECLARE_TYPEINFO_IS_TYPE)

#undef DECLARE_TYPEINFO_IS_TYPE


#define DECLARE_ERIS_TYPEINFO_IS(check) \
    static inline bool eris_typeinfo_is_ ## check (const ErisTypeInfo *typeinfo) { \
        return eris_type_is_ ## check (eris_typeinfo_type (typeinfo)); }

DECLARE_ERIS_TYPEINFO_IS (base)
DECLARE_ERIS_TYPEINFO_IS (synthetic)
DECLARE_ERIS_TYPEINFO_IS (compound)

#undef DECLARE_ERIS_TYPEINFO_IS

#endif /* !ERIS_TYPING_H */
