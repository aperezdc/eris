/*
 * eol-typing.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef EOL_TYPING_H
#define EOL_TYPING_H

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

#define BASE_TYPES(F)    \
    INTEGER_TYPES (F)    \
    FLOAT_TYPES (F)      \
    F (BOOL, bool, bool) \
    F (POINTER, pointer, void*)

#define SYNTHETIC_TYPES(F)        \
    F (TYPEDEF, typedef, typedef) \
    F (CONST,   const,   const)

#define COMPOUND_TYPES(F)      \
    F (STRUCT, struct, struct) \
    F (UNION, union, union)    \
    F (ARRAY,  array,  array)

#define CONST_TYPES(F)   \
    INTEGER_TYPES (F)    \
    FLOAT_TYPES (F)      \
    F (BOOL, bool, bool) \
    F (VOID, void, void)

#define ALL_TYPES(F)     \
    BASE_TYPES (F)       \
    SYNTHETIC_TYPES (F)  \
    COMPOUND_TYPES (F)   \
    F (ENUM, enum, enum) \
    F (VOID, void, void)


typedef enum {
#define TYPE_ENUM_ENTRY(suffix, name, ctype) EOL_TYPE_ ## suffix,
    ALL_TYPES (TYPE_ENUM_ENTRY)
#undef TYPE_ENUM_ENTRY
} EolType;


extern const char* eol_type_name (EolType type);

#define TYPE_CASE_TRUE(suffix, name, ctype) \
    case EOL_TYPE_ ## suffix : return true;

#define DECLARE_EOL_TYPE_IS(suffix, check)                    \
    static inline bool eol_type_is_ ## check (EolType type) { \
        switch (type) {                                       \
            SYNTHETIC_TYPES (TYPE_CASE_TRUE)                  \
            default: return false;                            \
        }                                                     \
    }

DECLARE_EOL_TYPE_IS (BASE,      base)
DECLARE_EOL_TYPE_IS (SYNTHETIC, synthetic)
DECLARE_EOL_TYPE_IS (COMPOUND,  compound)

#undef DECLARE_EOL_TYPE_IS
#undef TYPE_CASE_TRUE


typedef struct _EolTypeInfo EolTypeInfo;

#define DECL_CONST_TYPEINFO_ITEM(suffix, name, ctype) \
    extern const EolTypeInfo *eol_typeinfo_ ## name;

CONST_TYPES (DECL_CONST_TYPEINFO_ITEM)
extern const EolTypeInfo *eol_typeinfo_pointer;

#undef DECL_CONST_TYPEINFO_ITEM


typedef struct {
    const char                *name;
    union {
        int64_t                value;    /* EOL_TYPE_ENUM           */
        struct {
            uint32_t           offset;   /* EOL_TYPE_{UNION,STRUCT} */
            const EolTypeInfo *typeinfo; /* ditto.                  */
        };
    };
} EolTypeInfoMember;


extern EolTypeInfo* eol_typeinfo_new_const   (const EolTypeInfo *base);
extern EolTypeInfo* eol_typeinfo_new_pointer (const EolTypeInfo *base);
extern EolTypeInfo* eol_typeinfo_new_typedef (const EolTypeInfo *base,
                                              const char        *name);
extern EolTypeInfo* eol_typeinfo_new_array   (const EolTypeInfo *base,
                                              uint64_t           n_items);
extern EolTypeInfo* eol_typeinfo_new_struct  (const char *name,
                                              uint32_t    size,
                                              uint32_t    n_members);
extern EolTypeInfo* eol_typeinfo_new_enum    (const char *name,
                                              uint32_t    size,
                                              uint32_t    n_members);
extern EolTypeInfo* eol_typeinfo_new_union   (const char *name,
                                              uint32_t    size,
                                              uint32_t    n_members);

extern void eol_typeinfo_free (EolTypeInfo *typeinfo);

extern const EolTypeInfo* eol_typeinfo_base (const EolTypeInfo *typeinfo);

extern const char* eol_typeinfo_name   (const EolTypeInfo *typeinfo);
extern EolType     eol_typeinfo_type   (const EolTypeInfo *typeinfo);
extern bool        eol_typeinfo_equal  (const EolTypeInfo *a,
                                        const EolTypeInfo *b);
extern uint32_t    eol_typeinfo_sizeof (const EolTypeInfo *typeinfo);
extern uint64_t    eol_typeinfo_array_n_items (const EolTypeInfo* typeinfo);
extern bool        eol_typeinfo_struct_is_opaque (const EolTypeInfo *typeinfo);
extern uint32_t    eol_typeinfo_compound_n_members (const EolTypeInfo *typeinfo);
extern bool        eol_typeinfo_is_cstring (const EolTypeInfo *typeinfo);

extern bool  eol_typeinfo_is_readonly (const EolTypeInfo *typeinfo);
extern const EolTypeInfo* eol_typeinfo_get_compound (const EolTypeInfo *typeinfo);
extern const EolTypeInfo* eol_typeinfo_get_non_synthetic (const EolTypeInfo *typeinfo);


extern EolTypeInfoMember*
eol_typeinfo_compound_named_member (EolTypeInfo *typeinfo,
                                    const char  *name);
extern EolTypeInfoMember*
eol_typeinfo_compound_member (EolTypeInfo *typeinfo,
                              uint32_t     index);

extern const EolTypeInfoMember*
eol_typeinfo_compound_const_named_member (const EolTypeInfo *typeinfo,
                                          const char        *name);
extern const EolTypeInfoMember*
eol_typeinfo_compound_const_member (const EolTypeInfo *typeinfo,
                                    uint32_t           index);


#define EOL_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER(itername, typeinfo) \
    itername = eol_typeinfo_compound_const_member (typeinfo, 0);       \
    for (uint32_t itername  ## _i = 0, itername ## _n_members =        \
            eol_typeinfo_compound_n_members (typeinfo);                \
         itername ## _i < itername ## _n_members;                      \
         itername = eol_typeinfo_compound_const_member (typeinfo, itername ## _i++))

#define EOL_TYPEINFO_COMPOUND_FOREACH_MEMBER(itername, typeinfo) \
    itername = eol_typeinfo_compound_const_member (typeinfo, 0); \
    for (uint32_t itername  ## _i = 0, itername ## _n_members =  \
            eol_typeinfo_compound_n_members (typeinfo);          \
         itername ## _i < itername ## _n_members;                \
         itername = eol_typeinfo_compound_const_member (typeinfo, itername ## _i++))


#define DECLARE_TYPEINFO_IS_TYPE(suffix, name, ctype)                           \
    static inline bool eol_typeinfo_is_ ## name (const EolTypeInfo *typeinfo) { \
        typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);                   \
        return eol_typeinfo_type (typeinfo) == EOL_TYPE_ ## suffix; }

ALL_TYPES (DECLARE_TYPEINFO_IS_TYPE)

#undef DECLARE_TYPEINFO_IS_TYPE


#define DECLARE_EOL_TYPEINFO_IS(check)                                           \
    static inline bool eol_typeinfo_is_ ## check (const EolTypeInfo *typeinfo) { \
        typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);                    \
        return eol_type_is_ ## check (eol_typeinfo_type (typeinfo)); }

DECLARE_EOL_TYPEINFO_IS (base)
DECLARE_EOL_TYPEINFO_IS (synthetic)
DECLARE_EOL_TYPEINFO_IS (compound)

#undef DECLARE_EOL_TYPEINFO_IS

#endif /* !EOL_TYPING_H */
