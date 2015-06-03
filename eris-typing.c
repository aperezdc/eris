/*
 * eris-typing.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-trace.h"
#include "eris-typing.h"
#include "eris-util.h"

#include <stdlib.h>
#include <string.h>

struct TI_base {
    const char         *name;
    uint32_t            size;
};
struct TI_pointer {
    const ErisTypeInfo *typeinfo;
};
struct TI_typedef {
    const char         *name;
    const ErisTypeInfo *typeinfo;
};
struct TI_const {
    const ErisTypeInfo *typeinfo;
};
struct TI_array {
    const ErisTypeInfo *typeinfo;
    uint64_t            n_items;
};
struct TI_struct {
    const char         *name;
    uint32_t            size;
    uint32_t            n_members;
    ErisTypeInfoMember  members[];
};


struct _ErisTypeInfo {
    ErisType type;
    union {
        struct TI_base    ti_base;
        struct TI_pointer ti_pointer;
        struct TI_typedef ti_typedef;
        struct TI_const   ti_const;
        struct TI_array   ti_array;
        struct TI_struct  ti_struct;
    };
};


#if ERIS_TRACE
static void
trace_tname (const ErisTypeInfo *typeinfo)
{
    switch (typeinfo->type) {
        case ERIS_TYPE_POINTER:
            trace_tname (typeinfo->ti_pointer.typeinfo);
            TRACE (">*");
            break;
        case ERIS_TYPE_TYPEDEF:
            TRACE (">typedef ");
            trace_tname (typeinfo->ti_typedef.typeinfo);
            TRACE ("> %s", typeinfo->ti_typedef.name);
            break;
        case ERIS_TYPE_CONST:
            TRACE (">const ");
            trace_tname (typeinfo->ti_const.typeinfo);
            break;
        case ERIS_TYPE_ARRAY:
            trace_tname (typeinfo->ti_array.typeinfo);
            TRACE (">[%lu]", (long unsigned) typeinfo->ti_array.n_items);
            break;
        case ERIS_TYPE_STRUCT:
            TRACE (">struct %s", typeinfo->ti_struct.name ? : "{}");
            break;
        default:
            TRACE (">%s", typeinfo->ti_base.name ? : eris_type_name (typeinfo->type));
    }
}
# define TNAME(t) \
    do { TRACE (">" WHITE); trace_tname(t); TRACE (">" NORMAL); } while (0)
# define TTRACE(hint, t) \
    do { TRACE (">" #hint CYAN " ErisTypeInfo" GREEN " %p" NORMAL " (", (t)); \
         TNAME (t); TRACE (">" NORMAL ")\n"); } while (0)

#else
# define TNAME(t) ((void) 0)
#endif


const char*
eris_type_name (ErisType type)
{
#define TYPE_NAME_ITEM(suffix, name, ctype) \
        case ERIS_TYPE_ ## suffix: return #ctype;

    switch (type) {
        INTEGER_TYPES (TYPE_NAME_ITEM)
        FLOAT_TYPES (TYPE_NAME_ITEM)
        default: return NULL;
    }

#undef TYPE_NAME_ITEM
}


uint32_t
eris_type_size (ErisType type)
{
#define TYPE_SIZE_ITEM(suffix, name, ctype) \
        case ERIS_TYPE_ ## suffix: return sizeof (ctype);

    switch (type) {
        BASE_TYPES (TYPE_SIZE_ITEM)
        default: return 0;
    }

#undef TYPE_SIZE_ITEM
}


static inline ErisTypeInfo*
eris_typeinfo_new (ErisType type, uint32_t n_members)
{
    ErisTypeInfo* typeinfo = calloc (1,
                                     sizeof (ErisTypeInfo) +
                                     sizeof (ErisTypeInfoMember) * n_members);
    typeinfo->type = type;
    return typeinfo;
}


void
eris_typeinfo_free (ErisTypeInfo *typeinfo)
{
    TTRACE (<, typeinfo);
    free (typeinfo);
}


ErisTypeInfo*
eris_typeinfo_new_base (ErisType    type,
                        const char *name)
{
    CHECK_UINT_NE (ERIS_TYPE_VOID,    type);
    CHECK_UINT_NE (ERIS_TYPE_POINTER, type);
    CHECK_UINT_NE (ERIS_TYPE_TYPEDEF, type);
    CHECK_UINT_NE (ERIS_TYPE_CONST,   type);
    CHECK_UINT_NE (ERIS_TYPE_ARRAY,   type);
    CHECK_UINT_NE (ERIS_TYPE_STRUCT,  type);

    ErisTypeInfo *typeinfo = eris_typeinfo_new (type, 0);
    typeinfo->ti_base.name = name ? name : eris_type_name (type);
    typeinfo->ti_base.size = eris_type_size (type);

    TTRACE (>, typeinfo);
    return typeinfo;
}


ErisTypeInfo*
eris_typeinfo_new_const (const ErisTypeInfo *base)
{
    CHECK_NOT_NULL (base);

    ErisTypeInfo *typeinfo = eris_typeinfo_new (ERIS_TYPE_CONST, 0);
    typeinfo->ti_const.typeinfo = base;

    TTRACE (>, typeinfo);
    return typeinfo;
}


ErisTypeInfo*
eris_typeinfo_new_pointer (const ErisTypeInfo *base)
{
    CHECK_NOT_NULL (base);

    ErisTypeInfo *typeinfo = eris_typeinfo_new (ERIS_TYPE_POINTER, 0);
    typeinfo->ti_pointer.typeinfo = base;

    TTRACE (>, typeinfo);
    return typeinfo;
}


ErisTypeInfo*
eris_typeinfo_new_typedef (const ErisTypeInfo *base,
                           const char         *name)
{
    CHECK_NOT_NULL (base);
    CHECK_NOT_NULL (name);

    ErisTypeInfo *typeinfo = eris_typeinfo_new (ERIS_TYPE_TYPEDEF, 0);
    typeinfo->ti_typedef.typeinfo = base,
    typeinfo->ti_typedef.name     = name;

    TTRACE (>, typeinfo);
    return typeinfo;
}


ErisTypeInfo*
eris_typeinfo_new_array (const ErisTypeInfo *base,
                         uint64_t            n_items)
{
    CHECK_NOT_NULL (base);

    ErisTypeInfo *typeinfo = eris_typeinfo_new (ERIS_TYPE_ARRAY, 0);
    typeinfo->ti_array.typeinfo = base;
    typeinfo->ti_array.n_items  = n_items;

    TTRACE (>, typeinfo);
    return typeinfo;
}


ErisTypeInfo*
eris_typeinfo_new_struct (const char *name,
                          uint32_t    size,
                          uint32_t    n_members)
{
    ErisTypeInfo *typeinfo = eris_typeinfo_new (ERIS_TYPE_STRUCT, n_members);
    typeinfo->ti_struct.name      = name ? name : "<struct>";
    typeinfo->ti_struct.size      = size;
    typeinfo->ti_struct.n_members = n_members;

    TTRACE (>, typeinfo);
    return typeinfo;
}


const char*
eris_typeinfo_name (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case ERIS_TYPE_VOID:
            return "void";
        case ERIS_TYPE_STRUCT:
            return typeinfo->ti_struct.name;
        case ERIS_TYPE_POINTER:
            return eris_typeinfo_name (typeinfo->ti_pointer.typeinfo);
        case ERIS_TYPE_TYPEDEF:
            return typeinfo->ti_typedef.name
                 ? typeinfo->ti_typedef.name
                 : eris_typeinfo_name (typeinfo->ti_typedef.typeinfo);
        case ERIS_TYPE_CONST:
            return eris_typeinfo_name (typeinfo->ti_const.typeinfo);
        case ERIS_TYPE_ARRAY:
            return eris_typeinfo_name (typeinfo->ti_array.typeinfo);
        default:
            return typeinfo->ti_base.name;
    }
}


ErisType
eris_typeinfo_type (const ErisTypeInfo *typeinfo)
{
    CHECK (typeinfo);
    return typeinfo->type;
}


uint32_t
eris_typeinfo_struct_n_members (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_STRUCT, typeinfo->type);

    return typeinfo->ti_struct.n_members;
}


uint64_t
eris_typeinfo_array_n_items (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_ARRAY, typeinfo->type);

    return typeinfo->ti_array.n_items;
}


uint32_t
eris_typeinfo_sizeof (const ErisTypeInfo *typeinfo)
{
    CHECK (typeinfo);

    switch (typeinfo->type) {
        case ERIS_TYPE_VOID:
            return 0;
        case ERIS_TYPE_POINTER:
            return sizeof (void*);
        case ERIS_TYPE_TYPEDEF:
            return eris_typeinfo_sizeof (typeinfo->ti_typedef.typeinfo);
        case ERIS_TYPE_CONST:
            return eris_typeinfo_sizeof (typeinfo->ti_const.typeinfo);
        case ERIS_TYPE_ARRAY:
            return eris_typeinfo_sizeof (typeinfo->ti_array.typeinfo) *
                   typeinfo->ti_array.n_items;
        case ERIS_TYPE_STRUCT:
            return typeinfo->ti_struct.size;
        default:
            return typeinfo->ti_base.size;
    }
}


const ErisTypeInfo*
eris_typeinfo_get_struct (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    if (typeinfo->type == ERIS_TYPE_STRUCT)
        return typeinfo;

    typeinfo = eris_typeinfo_base (typeinfo);
    return typeinfo ? eris_typeinfo_get_struct (typeinfo) : NULL;
}


bool
eris_typeinfo_get_const (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    if (typeinfo->type == ERIS_TYPE_CONST)
        return true;

    typeinfo = eris_typeinfo_base (typeinfo);
    return typeinfo ? eris_typeinfo_get_const (typeinfo) : false;
}


const ErisTypeInfo*
eris_typeinfo_get_non_synthetic (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    while (typeinfo && eris_type_is_synthetic (typeinfo->type))
        typeinfo = eris_typeinfo_base (typeinfo);
    return typeinfo;
}


const ErisTypeInfo*
eris_typeinfo_base (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case ERIS_TYPE_POINTER:
            return typeinfo->ti_pointer.typeinfo;
        case ERIS_TYPE_TYPEDEF:
            return typeinfo->ti_typedef.typeinfo;
        case ERIS_TYPE_CONST:
            return typeinfo->ti_const.typeinfo;
        case ERIS_TYPE_ARRAY:
            return typeinfo->ti_array.typeinfo;
        default:
            return NULL;
    }
}


bool
eris_typeinfo_equal (const ErisTypeInfo *a,
                     const ErisTypeInfo *b)
{
    CHECK_NOT_NULL (a);
    CHECK_NOT_NULL (b);

    if (a == b)
        return true;

    if (a->type != b->type)
        return false;

    switch (a->type) {
        case ERIS_TYPE_POINTER:
            return eris_typeinfo_equal (a->ti_pointer.typeinfo,
                                        b->ti_pointer.typeinfo);

        case ERIS_TYPE_TYPEDEF:
            return string_equal (a->ti_typedef.name, b->ti_typedef.name)
                && eris_typeinfo_equal (a->ti_typedef.typeinfo,
                                        b->ti_typedef.typeinfo);

        case ERIS_TYPE_CONST:
            return eris_typeinfo_equal (a->ti_typedef.typeinfo,
                                        b->ti_typedef.typeinfo);

        case ERIS_TYPE_ARRAY:
            return a->ti_array.n_items == b->ti_array.n_items
                && eris_typeinfo_equal (a->ti_array.typeinfo,
                                        b->ti_array.typeinfo);

        case ERIS_TYPE_STRUCT:
            if (a->ti_struct.size != b->ti_struct.size ||
                a->ti_struct.n_members != b->ti_struct.n_members ||
                !string_equal (a->ti_struct.name, b->ti_struct.name)) {
                    return false;
            }
            /* Check struct members. */
            for (uint32_t i = 0; i < a->ti_struct.n_members; i++) {
                if (!string_equal (a->ti_struct.members[i].name,
                                   b->ti_struct.members[i].name) ||
                    !eris_typeinfo_equal (a->ti_struct.members[i].typeinfo,
                                          b->ti_struct.members[i].typeinfo))
                        return false;
            }
            return true;

        default:
            return a->ti_base.size == b->ti_base.size
                && string_equal (a->ti_base.name, b->ti_base.name);
    }
}


const ErisTypeInfoMember*
eris_typeinfo_struct_const_named_member (const ErisTypeInfo *typeinfo,
                                         const char         *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_STRUCT, typeinfo->type);
    CHECK_NOT_NULL (name);

    for (uint32_t i = 0; i < typeinfo->ti_struct.n_members; i++)
        if (string_equal (name, typeinfo->ti_struct.members[i].name))
            return &typeinfo->ti_struct.members[i];
    return NULL;
}


ErisTypeInfoMember*
eris_typeinfo_struct_named_member (ErisTypeInfo *typeinfo,
                                   const char   *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);

    return (ErisTypeInfoMember*)
        eris_typeinfo_struct_const_named_member (typeinfo, name);
}


const ErisTypeInfoMember*
eris_typeinfo_struct_const_member (const ErisTypeInfo *typeinfo,
                                   uint32_t            index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_STRUCT, typeinfo->type);
    CHECK_U32_LT (typeinfo->ti_struct.n_members, index);

    return &typeinfo->ti_struct.members[index];
}


ErisTypeInfoMember*
eris_typeinfo_struct_member (ErisTypeInfo *typeinfo,
                             uint32_t      index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_STRUCT, typeinfo->type);
    CHECK_U32_LT (typeinfo->ti_struct.n_members, index);

    return (ErisTypeInfoMember*)
        eris_typeinfo_struct_const_member (typeinfo, index);
}
