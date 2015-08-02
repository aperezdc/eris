/*
 * eol-typing.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eol-trace.h"
#include "eol-typing.h"
#include "eol-util.h"

#include <stdlib.h>
#include <string.h>

struct TI_base {
    char              *name;
    uint32_t           size;
};
struct TI_pointer {
    const EolTypeInfo *typeinfo;
};
struct TI_typedef {
    char              *name;
    const EolTypeInfo *typeinfo;
};
struct TI_const {
    const EolTypeInfo *typeinfo;
};
struct TI_array {
    const EolTypeInfo *typeinfo;
    uint64_t           n_items;
};
struct TI_compound {
    char              *name;
    uint32_t           size;
    uint32_t           n_members;
    EolTypeInfoMember  members[];
};


struct _EolTypeInfo {
    EolType type;
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
    const EolTypeInfo *eol_typeinfo_ ## tname =       \
        &((EolTypeInfo) {                             \
          .type = EOL_TYPE_ ## suffix,                \
          .ti_base.name = #ctype,                     \
          .ti_base.size = sizeof (ctype)              \
        });

CONST_TYPES (DEF_CONST_TYPEINFO_ITEM)

#undef DEF_CONST_TYPEINFO_ITEM

const EolTypeInfo *eol_typeinfo_pointer =
    &((EolTypeInfo) {
        .type = EOL_TYPE_POINTER,
        .ti_pointer.typeinfo =
            &((EolTypeInfo) {
                .type = EOL_TYPE_VOID,
                .ti_base.name = "void",
                .ti_base.size = 0,
            }),
    });


#if EOL_TRACE
static void
trace_tname (const EolTypeInfo *typeinfo)
{
    switch (typeinfo->type) {
        case EOL_TYPE_POINTER:
            trace_tname (typeinfo->ti_pointer.typeinfo);
            TRACE (">*");
            break;
        case EOL_TYPE_TYPEDEF:
            TRACE (">typedef ");
            trace_tname (typeinfo->ti_typedef.typeinfo);
            TRACE ("> %s", typeinfo->ti_typedef.name);
            break;
        case EOL_TYPE_CONST:
            TRACE (">const ");
            trace_tname (typeinfo->ti_const.typeinfo);
            break;
        case EOL_TYPE_ARRAY:
            trace_tname (typeinfo->ti_array.typeinfo);
            TRACE (">[%lu]", (long unsigned) typeinfo->ti_array.n_items);
            break;
        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT:
            TRACE (">%s %s",
                   (typeinfo->type == EOL_TYPE_STRUCT) ? "struct" : "union",
                   (typeinfo->ti_compound.name
                        ? typeinfo->ti_compound.name
                        : "{}"));
            break;
        default:
            TRACE (">%s", typeinfo->ti_base.name
                            ? typeinfo->ti_base.name
                            : eol_type_name (typeinfo->type));
    }
}
# define TNAME(t) \
    do { TRACE (">" WHITE); trace_tname(t); TRACE (">" NORMAL); } while (0)
# define TTRACE(hint, t) \
    do { TRACE (">" #hint CYAN " EolTypeInfo" GREEN " %p" NORMAL " (", (t)); \
         TNAME (t); TRACE (">" NORMAL ")\n"); } while (0)

#else
# define TNAME(t)        ((void) 0)
# define TTRACE(hint, t) ((void) 0)
#endif


const char*
eol_type_name (EolType type)
{
#define TYPE_NAME_ITEM(suffix, name, ctype) \
        case EOL_TYPE_ ## suffix: return #ctype;

    switch (type) { ALL_TYPES (TYPE_NAME_ITEM) }

#undef TYPE_NAME_ITEM
}


uint32_t
eol_type_size (EolType type)
{
#define TYPE_SIZE_ITEM(suffix, name, ctype) \
        case EOL_TYPE_ ## suffix: return sizeof (ctype);

    switch (type) {
        BASE_TYPES (TYPE_SIZE_ITEM)
        default: return 0;
    }

#undef TYPE_SIZE_ITEM
}


static inline EolTypeInfo*
eol_typeinfo_new (EolType type, uint32_t n_members)
{
    EolTypeInfo* typeinfo = calloc (1,
                                    sizeof (EolTypeInfo) +
                                    sizeof (EolTypeInfoMember) * n_members);
    typeinfo->type = type;
    return typeinfo;
}


void
eol_typeinfo_free (EolTypeInfo *typeinfo)
{
    if (true
#define CHECK_IS_CONST_TYPEINFO(_, tname, __) \
            || (typeinfo == eol_typeinfo_ ## tname)
        CONST_TYPES (CHECK_IS_CONST_TYPEINFO)
#undef CHECK_IS_CONST_TYPEINFO
       ) {
        TTRACE (!, typeinfo);
        TRACE (RED "Attempted to free constant typeinfo\n" NORMAL);
        return;
    }

    TTRACE (<, typeinfo);
    switch (typeinfo->type) {
        case EOL_TYPE_TYPEDEF:
            free (typeinfo->ti_typedef.name);
            break;

        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT:
            free (typeinfo->ti_compound.name);
            break;

        default:
            break;
    }
    free (typeinfo);
}


EolTypeInfo*
eol_typeinfo_new_const (const EolTypeInfo *base)
{
    CHECK_NOT_NULL (base);

    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_CONST, 0);
    typeinfo->ti_const.typeinfo = base;

    TTRACE (>, typeinfo);
    return typeinfo;
}


EolTypeInfo*
eol_typeinfo_new_pointer (const EolTypeInfo *base)
{
    CHECK_NOT_NULL (base);

    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_POINTER, 0);
    typeinfo->ti_pointer.typeinfo = base;

    TTRACE (>, typeinfo);
    return typeinfo;
}


EolTypeInfo*
eol_typeinfo_new_typedef (const EolTypeInfo *base,
                          const char        *name)
{
    CHECK_NOT_NULL (base);
    CHECK_NOT_NULL (name);

    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_TYPEDEF, 0);
    typeinfo->ti_typedef.name     = strdup (name);
    typeinfo->ti_typedef.typeinfo = base;

    TTRACE (>, typeinfo);
    return typeinfo;
}


EolTypeInfo*
eol_typeinfo_new_array (const EolTypeInfo *base,
                        uint64_t           n_items)
{
    CHECK_NOT_NULL (base);

    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_ARRAY, 0);
    typeinfo->ti_array.typeinfo = base;
    typeinfo->ti_array.n_items  = n_items;

    TTRACE (>, typeinfo);
    return typeinfo;
}


EolTypeInfo*
eol_typeinfo_new_struct (const char *name,
                         uint32_t    size,
                         uint32_t    n_members)
{
    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_STRUCT, n_members);
    typeinfo->ti_compound.name      = name ? strdup (name) : NULL;
    typeinfo->ti_compound.size      = size;
    typeinfo->ti_compound.n_members = n_members;

    TTRACE (>, typeinfo);
    return typeinfo;
}


EolTypeInfo*
eol_typeinfo_new_enum (const char *name,
                       uint32_t    size,
                       uint32_t    n_members)
{
    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_ENUM, n_members);
    typeinfo->ti_compound.name      = name ? strdup (name) : NULL;
    typeinfo->ti_compound.size      = size;
    typeinfo->ti_compound.n_members = n_members;

    TTRACE (>, typeinfo);
    return typeinfo;
}


EolTypeInfo*
eol_typeinfo_new_union (const char *name,
                        uint32_t    size,
                        uint32_t    n_members)
{
    EolTypeInfo *typeinfo = eol_typeinfo_new (EOL_TYPE_UNION, n_members);
    typeinfo->ti_compound.name      = name ? strdup (name) : NULL;
    typeinfo->ti_compound.size      = size;
    typeinfo->ti_compound.n_members = n_members;

    TTRACE (>, typeinfo);
    return typeinfo;
}

const char*
eol_typeinfo_name (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT:
            return typeinfo->ti_compound.name;

        case EOL_TYPE_POINTER:
            return eol_typeinfo_name (typeinfo->ti_pointer.typeinfo);

        case EOL_TYPE_TYPEDEF:
            return typeinfo->ti_typedef.name
                 ? typeinfo->ti_typedef.name
                 : eol_typeinfo_name (typeinfo->ti_typedef.typeinfo);

        case EOL_TYPE_CONST:
            return eol_typeinfo_name (typeinfo->ti_const.typeinfo);

        case EOL_TYPE_ARRAY:
            return eol_typeinfo_name (typeinfo->ti_array.typeinfo);

        default:
            return typeinfo->ti_base.name;
    }
}


EolType
eol_typeinfo_type (const EolTypeInfo *typeinfo)
{
    CHECK (typeinfo);
    return typeinfo->type;
}


uint32_t
eol_typeinfo_compound_n_members (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK (typeinfo->type == EOL_TYPE_STRUCT ||
           typeinfo->type == EOL_TYPE_UNION ||
           typeinfo->type == EOL_TYPE_ENUM);

    return typeinfo->ti_compound.n_members;
}


bool
eol_typeinfo_struct_is_opaque (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (EOL_TYPE_STRUCT, typeinfo->type);

    return typeinfo->ti_compound.n_members == 0
        && typeinfo->ti_compound.size == 0;
}


bool
eol_typeinfo_is_cstring (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);
    const EolTypeInfo *base =
            eol_typeinfo_get_non_synthetic (typeinfo->ti_pointer.typeinfo);
    return typeinfo->type == EOL_TYPE_POINTER &&
        (base->type == EOL_TYPE_S8 || base->type == EOL_TYPE_U8);
}


uint64_t
eol_typeinfo_array_n_items (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (EOL_TYPE_ARRAY, typeinfo->type);

    return typeinfo->ti_array.n_items;
}


uint32_t
eol_typeinfo_sizeof (const EolTypeInfo *typeinfo)
{
    CHECK (typeinfo);

    switch (typeinfo->type) {
        case EOL_TYPE_VOID:
            return 0;

        case EOL_TYPE_POINTER:
            return sizeof (void*);

        case EOL_TYPE_CONST:
        case EOL_TYPE_TYPEDEF:
            return eol_typeinfo_sizeof (eol_typeinfo_base (typeinfo));

        case EOL_TYPE_ARRAY:
            return eol_typeinfo_sizeof (eol_typeinfo_base (typeinfo)) *
                   typeinfo->ti_array.n_items;

        case EOL_TYPE_ENUM:
        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT:
            return typeinfo->ti_compound.size;

        default:
            return typeinfo->ti_base.size;
    }
}


const EolTypeInfo*
eol_typeinfo_get_compound (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case EOL_TYPE_ENUM:
        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT:
            return typeinfo;
        default:
            typeinfo = eol_typeinfo_base (typeinfo);
            return typeinfo ? eol_typeinfo_get_compound (typeinfo) : NULL;
    }
}


bool
eol_typeinfo_is_readonly (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    if (typeinfo->type == EOL_TYPE_CONST)
        return true;

    typeinfo = eol_typeinfo_base (typeinfo);
    return typeinfo ? eol_typeinfo_is_readonly (typeinfo) : false;
}


const EolTypeInfo*
eol_typeinfo_get_non_synthetic (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    while (typeinfo && eol_type_is_synthetic (typeinfo->type))
        typeinfo = eol_typeinfo_base (typeinfo);
    return typeinfo;
}


const EolTypeInfo*
eol_typeinfo_base (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    switch (typeinfo->type) {
        case EOL_TYPE_POINTER:
            return typeinfo->ti_pointer.typeinfo;
        case EOL_TYPE_TYPEDEF:
            return typeinfo->ti_typedef.typeinfo;
        case EOL_TYPE_CONST:
            return typeinfo->ti_const.typeinfo;
        case EOL_TYPE_ARRAY:
            return typeinfo->ti_array.typeinfo;
        default:
            return NULL;
    }
}


bool
eol_typeinfo_equal (const EolTypeInfo *a,
                    const EolTypeInfo *b)
{
    CHECK_NOT_NULL (a);
    CHECK_NOT_NULL (b);

    if (a == b)
        return true;

    a = eol_typeinfo_get_non_synthetic (a);
    b = eol_typeinfo_get_non_synthetic (b);

    if (a->type != b->type)
        return false;

    switch (a->type) {
        case EOL_TYPE_POINTER:
            return eol_typeinfo_equal (a->ti_pointer.typeinfo,
                                       b->ti_pointer.typeinfo);

        case EOL_TYPE_ARRAY:
            return a->ti_array.n_items == b->ti_array.n_items
                && eol_typeinfo_equal (a->ti_array.typeinfo,
                                       b->ti_array.typeinfo);

        case EOL_TYPE_ENUM:
        case EOL_TYPE_UNION:
        case EOL_TYPE_STRUCT:
            return a->ti_compound.name
                && b->ti_compound.name
                && string_equal (a->ti_compound.name, b->ti_compound.name);

        default:
            return true;
    }
}


const EolTypeInfoMember*
eol_typeinfo_compound_const_named_member (const EolTypeInfo *typeinfo,
                                          const char        *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);
    CHECK (typeinfo->type == EOL_TYPE_STRUCT ||
           typeinfo->type == EOL_TYPE_UNION);

    for (uint32_t i = 0; i < typeinfo->ti_compound.n_members; i++)
        if (string_equal (name, typeinfo->ti_compound.members[i].name))
            return &typeinfo->ti_compound.members[i];
    return NULL;
}


EolTypeInfoMember*
eol_typeinfo_compound_named_member (EolTypeInfo *typeinfo,
                                    const char  *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);
    CHECK (typeinfo->type == EOL_TYPE_STRUCT ||
           typeinfo->type == EOL_TYPE_UNION ||
           typeinfo->type == EOL_TYPE_ENUM);

    return (EolTypeInfoMember*)
        eol_typeinfo_compound_const_named_member (typeinfo, name);
}


const EolTypeInfoMember*
eol_typeinfo_compound_const_member (const EolTypeInfo *typeinfo,
                                    uint32_t           index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK (typeinfo->type == EOL_TYPE_STRUCT ||
           typeinfo->type == EOL_TYPE_UNION ||
           typeinfo->type == EOL_TYPE_ENUM);
    CHECK_U32_LT (typeinfo->ti_compound.n_members, index);

    return &typeinfo->ti_compound.members[index];
}


EolTypeInfoMember*
eol_typeinfo_compound_member (EolTypeInfo *typeinfo,
                              uint32_t     index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK (typeinfo->type == EOL_TYPE_STRUCT ||
           typeinfo->type == EOL_TYPE_UNION ||
           typeinfo->type == EOL_TYPE_ENUM);
    CHECK_U32_LT (typeinfo->ti_compound.n_members, index);

    return (EolTypeInfoMember*)
        eol_typeinfo_compound_const_member (typeinfo, index);
}
