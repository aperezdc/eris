/*
 * eris-typing.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-typing.h"
#include "eris-util.h"

#include <stdlib.h>
#include <string.h>


struct _ErisTypeInfo {
    char              *name;          /* Optional, may be NULL.  */
    ErisType           type;          /* ERIS_TYPE_XX type code. */
    uint16_t           n_members;     /* Number of member items. */
    unsigned           size     : 31; /* sizeof (type).          */
    unsigned           readonly :  1; /* const?                  */
    ErisTypeInfoMember members[];     /* struct members.         */
};


const char*
eris_type_name (ErisType type)
{
#define TYPE_NAME_ITEM(suffix, ctype) \
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
#define TYPE_SIZE_ITEM(suffix, ctype) \
        case ERIS_TYPE_ ## suffix: return sizeof (ctype);

    switch (type) {
        BASE_TYPES (TYPE_SIZE_ITEM)
        default: return 0;
    }

#undef TYPE_SIZE_ITEM
}


ErisTypeInfo*
eris_typeinfo_new (ErisType type, uint16_t n_members)
{
    if (n_members > 0) CHECK (type == ERIS_TYPE_STRUCT);

    ErisTypeInfo *typeinfo = calloc (1,
                                     sizeof (ErisTypeInfo) +
                                     sizeof (ErisTypeInfoMember) * n_members);
    typeinfo->n_members = n_members;
    typeinfo->type = type;
    typeinfo->size = eris_type_size (type);
    return typeinfo;
}


bool
eris_typeinfo_is_valid (const ErisTypeInfo *typeinfo)
{
    return typeinfo && typeinfo->type != ERIS_TYPE_NONE;
}


bool
eris_typeinfo_is_readonly (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    return typeinfo->readonly;
}


void
eris_typeinfo_set_is_readonly (ErisTypeInfo *typeinfo,
                               bool          readonly)
{
    CHECK_NOT_NULL (typeinfo);
    typeinfo->readonly = readonly;
}


const char*
eris_typeinfo_name (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    return typeinfo->name;
}


void
eris_typeinfo_set_name (ErisTypeInfo *typeinfo,
                        const char   *name)
{
    CHECK_NOT_NULL (typeinfo);
    if (typeinfo->name != name) {
        free (typeinfo->name);
        typeinfo->name = strdup (name);
    }
}


ErisType
eris_typeinfo_type (const ErisTypeInfo *typeinfo)
{
    CHECK (typeinfo);
    return typeinfo->type;
}


uint16_t
eris_typeinfo_n_members (const ErisTypeInfo *typeinfo)
{
    CHECK (typeinfo);
    return typeinfo->n_members;
}


uint32_t
eris_typeinfo_sizeof (const ErisTypeInfo *typeinfo)
{
    CHECK (typeinfo);
    return typeinfo->size;
}


bool
eris_typeinfo_equal (const ErisTypeInfo *a,
                     const ErisTypeInfo *b)
{
    CHECK_NOT_NULL (a);
    CHECK_NOT_NULL (b);

    if (a == b)
        return true;

    if (a->type != b->type || a->size != b->size || a->n_members != b->n_members)
        return false;

    /* Check members. */
    for (uint16_t i = 0; i < a->n_members; i++) {
        const ErisTypeInfoMember *b_member =
                eris_typeinfo_const_named_member (b, a->members[i].name);
        if (!b_member || !eris_typeinfo_equal (a->members[i].typeinfo,
                                               b_member->typeinfo))
            return false;
    }
    return true;
}


const ErisTypeInfoMember*
eris_typeinfo_const_named_member (const ErisTypeInfo *typeinfo,
                                  const char         *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);

    if (typeinfo->type == ERIS_TYPE_STRUCT) {
        for (uint16_t i = 0; i < typeinfo->n_members; i++)
            if (!strcmp (name, typeinfo->members[i].name))
                return &typeinfo->members[i];
    }
    return NULL;
}


ErisTypeInfoMember*
eris_typeinfo_named_member (ErisTypeInfo *typeinfo,
                            const char   *name)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_NOT_NULL (name);

    if (typeinfo->type == ERIS_TYPE_STRUCT) {
        for (uint16_t i = 0; i < typeinfo->n_members; i++)
            if (!strcasecmp (name, typeinfo->members[i].name))
                return &typeinfo->members[i];
    }
    return NULL;
}


const ErisTypeInfoMember*
eris_typeinfo_const_member (const ErisTypeInfo *typeinfo,
                            uint16_t            index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_U16_LT (typeinfo->n_members, index);
    return &typeinfo->members[index];
}


ErisTypeInfoMember*
eris_typeinfo_member (ErisTypeInfo *typeinfo,
                      uint16_t      index)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_U16_LT (typeinfo->n_members, index);
    return &typeinfo->members[index];
}


#if 0
/*
 * TODO: Depending on the size of lua_Integer and lua_Number, it may not be
 *       possible to always represent 64-bit numbers. In that case, the 64-bit
 *       types should be left out at compile-time, or wrapped into userdata.
 */

#define MAKE_INTEGER_GETTER_AND_SETTER(suffix, ctype)        \
    static int eris_variable_get__ ## ctype (lua_State *L) { \
        ErisVariable *ev = to_eris_variable (L);             \
        lua_pushinteger (L, *((ctype *) ev->address));       \
        return 1;                                            \
    }                                                        \
    static int eris_variable_set__ ## ctype (lua_State *L) { \
        ErisVariable *ev = to_eris_variable (L);             \
        *((ctype *) ev->address) =                           \
            (ctype) luaL_checkinteger (L, 2);                \
        return 0;                                            \
    }

#define MAKE_FLOAT_GETTER_AND_SETTER(suffix, ctype)          \
    static int eris_variable_get__ ## ctype (lua_State *L) { \
        ErisVariable *ev = to_eris_variable (L);             \
        lua_pushnumber (L, *((ctype *) ev->address));        \
        return 1;                                            \
    }                                                        \
    static int eris_variable_set__ ## ctype (lua_State *L) { \
        ErisVariable *ev = to_eris_variable (L);             \
        *((ctype *) ev->address) =                           \
            (ctype) luaL_checknumber (L, 2);                 \
        return 0;                                            \
    }

INTEGER_TYPES (MAKE_INTEGER_GETTER_AND_SETTER)
FLOAT_TYPES (MAKE_FLOAT_GETTER_AND_SETTER)

#undef MAKE_INTEGER_GETTER_AND_SETTER
#undef MAKE_FLOAT_GETTER_AND_SETTER


static const ErisConverter builtin_converters[] = {
#define INTEGER_GETTER_SETTER_ITEM(suffix, ctype) { \
    .getter         = eris_variable_get__ ## ctype, \
    .setter         = eris_variable_set__ ## ctype, \
    .typeinfo.name  = #ctype,                       \
    .typeinfo.type  = ERIS_TYPE_ ## suffix,         \
    .typeinfo.size  = sizeof (ctype) },

#define FLOAT_GETTER_SETTER_ITEM(suffix, ctype) {   \
    .getter         = eris_variable_get__ ## ctype, \
    .setter         = eris_variable_set__ ## ctype, \
    .typeinfo.name  = #ctype,                       \
    .typeinfo.type  = ERIS_TYPE_ ## suffix,         \
    .typeinfo.size  = sizeof (ctype) },

    INTEGER_TYPES (INTEGER_GETTER_SETTER_ITEM)
    FLOAT_TYPES (FLOAT_GETTER_SETTER_ITEM)

#undef INTEGER_GETTER_SETTER_ITEM
#undef FLOAT_GETTER_SETTER_ITEM
    { 0, }
};


const ErisConverter*
eris_converter_for_typeinfo (const ErisTypeInfo *typeinfo)
{
    /*
     * XXX: Using LENGTH_OF() does not work because of the variable
     *      length array member of ErisTypeInfo.
     */
    for (size_t i = 0; builtin_converters[i].getter; i++) {
        if (eris_typeinfo_equal (typeinfo, &builtin_converters[i].typeinfo))
            return &builtin_converters[i];
    }
    return NULL;
}
#endif
