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
    char               *name;
    uint32_t            size;
};
struct TI_pointer {
    const ErisTypeInfo *typeinfo;
};
struct TI_typedef {
    char               *name;
    const ErisTypeInfo *typeinfo;
};
struct TI_const {
    const ErisTypeInfo *typeinfo;
};
struct TI_array {
    const ErisTypeInfo *typeinfo;
    uint64_t            n_items;
};
struct TI_compound {
    char               *name;
    uint32_t            size;
    uint32_t            n_members;
    ErisTypeInfoMember  members[];
};


struct _ErisTypeInfo {
    ErisType type;
    union {
        struct TI_base     ti_base;
        struct TI_pointer  ti_pointer;
        struct TI_typedef  ti_typedef;
        struct TI_const    ti_const;
        struct TI_array    ti_array;
        struct TI_compound ti_compound;
    };
};


#define DEF_CONST_TYPEINFO_ITEM(suffix, tname, ctype) \
    const ErisTypeInfo *eris_typeinfo_ ## tname =     \
        &((ErisTypeInfo) {                            \
          .type = ERIS_TYPE_ ## suffix,               \
          .ti_base.name = #ctype,                     \
          .ti_base.size = sizeof (ctype)              \
        });

CONST_TYPES (DEF_CONST_TYPEINFO_ITEM)

#undef DEF_CONST_TYPEINFO_ITEM

const ErisTypeInfo *eris_typeinfo_pointer =
    &((ErisTypeInfo) {
        .type = ERIS_TYPE_POINTER,
        .ti_pointer.typeinfo =
            &((ErisTypeInfo) {
                .type = ERIS_TYPE_VOID,
                .ti_base.name = "void",
                .ti_base.size = 0,
            }),
    });


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
        case ERIS_TYPE_UNION:
        case ERIS_TYPE_STRUCT:
            TRACE (">%s %s",
                   (typeinfo->type == ERIS_TYPE_STRUCT) ? "struct" : "union",
                   (typeinfo->ti_compound.name
                        ? typeinfo->ti_compound.name
                        : "{}"));
            break;
        default:
            TRACE (">%s", typeinfo->ti_base.name
                            ? typeinfo->ti_base.name
                            : eris_type_name (typeinfo->type));
    }
}
# define TNAME(t) \
    do { TRACE (">" WHITE); trace_tname(t); TRACE (">" NORMAL); } while (0)
# define TTRACE(hint, t) \
    do { TRACE (">" #hint CYAN " ErisTypeInfo" GREEN " %p" NORMAL " (", (t)); \
         TNAME (t); TRACE (">" NORMAL ")\n"); } while (0)

#else
# define TNAME(t)        ((void) 0)
# define TTRACE(hint, t) ((void) 0)
#endif


const char*
eris_type_name (ErisType type)
{
#define TYPE_NAME_ITEM(suffix, name, ctype) \
        case ERIS_TYPE_ ## suffix: return #ctype;

    switch (type) { ALL_TYPES (TYPE_NAME_ITEM) }

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
    if (true
#define CHECK_IS_CONST_TYPEINFO(_, tname, __) \
            || (typeinfo == eris_typeinfo_ ## tname)
        CONST_TYPES (CHECK_IS_CONST_TYPEINFO)
#undef CHECK_IS_CONST_TYPEINFO
       ) {
        TTRACE (!, typeinfo);
        TRACE ("Attempted to free constant typeinfo\n");
        return;
    }

    TTRACE (<, typeinfo);
    switch (typeinfo->type) {
        case ERIS_TYPE_TYPEDEF:
            free (typeinfo->ti_typedef.name);
            break;

        case ERIS_TYPE_UNION:
        case ERIS_TYPE_STRUCT:
            free (typeinfo->ti_compound.name);
            break;

        default:
            break;
    }
    free (typeinfo);
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
    typeinfo->ti_typedef.name     = strdup (name);
    typeinfo->ti_typedef.typeinfo = base;

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
    typeinfo->ti_compound.name      = name ? strdup (name) : NULL;
    typeinfo->ti_compound.size      = size;
    typeinfo->ti_compound.n_members = n_members;

    TTRACE (>, typeinfo);
    return typeinfo;
}


ErisTypeInfo*
eris_typeinfo_new_union (const char *name,
                         uint32_t    size,
                         uint32_t    n_members)
{
    ErisTypeInfo *typeinfo = eris_typeinfo_new (ERIS_TYPE_UNION, n_members);
    typeinfo->ti_compound.name      = name ? strdup (name) : NULL;
    typeinfo->ti_compound.size      = size;
    typeinfo->ti_compound.n_members = n_members;

    TTRACE (>, typeinfo);
    return typeinfo;
}

const char*
eris_typeinfo_name (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case ERIS_TYPE_UNION:
        case ERIS_TYPE_STRUCT:
            return typeinfo->ti_compound.name;

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
eris_typeinfo_compound_n_members (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK (typeinfo->type == ERIS_TYPE_STRUCT ||
           typeinfo->type == ERIS_TYPE_UNION);

    return typeinfo->ti_compound.n_members;
}


bool
eris_typeinfo_struct_is_opaque (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_STRUCT, typeinfo->type);

    return typeinfo->ti_compound.n_members == 0
        && typeinfo->ti_compound.size == 0;
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
        case ERIS_TYPE_UNION:
        case ERIS_TYPE_STRUCT:
            return typeinfo->ti_compound.size;
        default:
            return typeinfo->ti_base.size;
    }
}


const ErisTypeInfo*
eris_typeinfo_get_compound (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case ERIS_TYPE_UNION:
        case ERIS_TYPE_STRUCT:
            return typeinfo;
        default:
            typeinfo = eris_typeinfo_base (typeinfo);
            return typeinfo ? eris_typeinfo_get_compound (typeinfo) : NULL;
    }
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

        case ERIS_TYPE_UNION:
        case ERIS_TYPE_STRUCT:
            if (a->ti_compound.size != b->ti_compound.size ||
                a->ti_compound.n_members != b->ti_compound.n_members ||
                !string_equal (a->ti_compound.name, b->ti_compound.name)) {
                    return false;
            }
            /* Check struct members. */
            for (uint32_t i = 0; i < a->ti_compound.n_members; i++) {
                if (!string_equal (a->ti_compound.members[i].name,
                                   b->ti_compound.members[i].name) ||
                    !eris_typeinfo_equal (a->ti_compound.members[i].typeinfo,
                                          b->ti_compound.members[i].typeinfo))
                        return false;
            }
            return true;

        default:
            return a->ti_base.size == b->ti_base.size
                && string_equal (a->ti_base.name, b->ti_base.name);
    }
}


const ErisTypeInfoMember*
eris_typeinfo_compound_const_named_member (const ErisTypeInfo *typeinfo,
                                           const char         *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);
    CHECK (typeinfo->type == ERIS_TYPE_STRUCT ||
           typeinfo->type == ERIS_TYPE_UNION);

    for (uint32_t i = 0; i < typeinfo->ti_compound.n_members; i++)
        if (string_equal (name, typeinfo->ti_compound.members[i].name))
            return &typeinfo->ti_compound.members[i];
    return NULL;
}


ErisTypeInfoMember*
eris_typeinfo_compound_named_member (ErisTypeInfo *typeinfo,
                                     const char   *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);
    CHECK (typeinfo->type == ERIS_TYPE_STRUCT ||
           typeinfo->type == ERIS_TYPE_UNION);

    return (ErisTypeInfoMember*)
        eris_typeinfo_compound_const_named_member (typeinfo, name);
}


const ErisTypeInfoMember*
eris_typeinfo_compound_const_member (const ErisTypeInfo *typeinfo,
                                     uint32_t            index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK (typeinfo->type == ERIS_TYPE_STRUCT ||
           typeinfo->type == ERIS_TYPE_UNION);
    CHECK_U32_LT (typeinfo->ti_compound.n_members, index);

    return &typeinfo->ti_compound.members[index];
}


ErisTypeInfoMember*
eris_typeinfo_compound_member (ErisTypeInfo *typeinfo,
                               uint32_t      index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK (typeinfo->type == ERIS_TYPE_STRUCT ||
           typeinfo->type == ERIS_TYPE_UNION);
    CHECK_U32_LT (typeinfo->ti_compound.n_members, index);

    return (ErisTypeInfoMember*)
        eris_typeinfo_compound_const_member (typeinfo, index);
}
