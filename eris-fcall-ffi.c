/*
 * eris-fcall-ffi.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-fcall-ffi.h"


/*
 * Counts the number of ffi_type items needed to represent a particular
 * struct. Note that this is particularly tricky because libffi has no
 * awareness of array types, so embedded, fixed-size arrays like the
 * following:
 *
 *   struct Foo { void* userdata;, int values[2] };
 *
 * has to be repsented like it was:
 *
 *   struct Foo {
 *     void* userdata;  // elements[0] = &ffi_type_pointer
 *     int values_0;    // elements[1] = &ffi_type_sint
 *     int values_1;    // elements[2] = &ffi_type_sint
 *   };
 *
 * Note that while "struct Foo" has 2 members, we have to tell libffi
 * that it does have 3 elements. Otherwise libffi will not calculate the
 * correct size of the type.
 */
static inline uint32_t
eris_ffi_struct_type_count_items (const ErisTypeInfo *typeinfo)
{
    typeinfo = eris_typeinfo_get_non_synthetic (typeinfo);
    uint32_t result = 0;

    const ErisTypeInfoMember *member;
    ERIS_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER (member, typeinfo) {
        const ErisTypeInfo *T =
                eris_typeinfo_get_non_synthetic (member->typeinfo);

        if (eris_typeinfo_is_array (T)) {
            result += eris_typeinfo_array_n_items (T);
        } else {
            result++;
        }
    }
    return result;
}


static ffi_type* eris_ffi_get_type (const ErisTypeInfo*);


static inline ffi_type*
eris_ffi_get_struct_type (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_STRUCT, eris_typeinfo_type (typeinfo));

    uint32_t n_items = eris_ffi_struct_type_count_items (typeinfo);
    ffi_type *type   = calloc (1, sizeof (ffi_type) +
                                  sizeof (ffi_type*) * (n_items + 1));
    type->elements   = (void*) &type[1];
    type->type       = FFI_TYPE_STRUCT;

    const ErisTypeInfoMember *member;
    uint32_t element_index = 0;

    ERIS_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER (member, typeinfo) {
        const ErisTypeInfo *member_typeinfo =
                eris_typeinfo_get_non_synthetic (member->typeinfo);

        if (eris_typeinfo_is_array (member_typeinfo)) {
            ffi_type *item_type =
                    eris_ffi_get_type (eris_typeinfo_base (member_typeinfo));
            uint32_t n = eris_typeinfo_array_n_items (member_typeinfo);

            CHECK_UINT_LT (n_items, element_index + n);
            while (n--) {
                type->elements[element_index++] = item_type;
                CHECK_UINT_LT (n_items, element_index);
            }
        } else {
            type->elements[element_index++] =
                    eris_ffi_get_type (member_typeinfo);
        }
    }
    CHECK_UINT_EQ (n_items, element_index);
    CHECK (type->elements[n_items] == NULL);

    return type;
}


static inline ffi_type*
eris_ffi_get_array_type (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_ARRAY, eris_typeinfo_type (typeinfo));

    ffi_type *base   = eris_ffi_get_type (eris_typeinfo_base (typeinfo));
    uint32_t n_items = eris_typeinfo_array_n_items (typeinfo);
    ffi_type* type   = calloc (1, sizeof (ffi_type) +
                                  sizeof (ffi_type*) * (n_items + 1));
    type->elements   = (void*) &type[1];
    type->type       = FFI_TYPE_STRUCT;

    for (uint32_t i = 0; i < n_items; i++) {
        type->elements[i] = base;
    }
    CHECK (type->elements[n_items] == NULL);

    return type;
}


/*
 * As libffi does not know about unions, choose the biggest of the union
 * members to make sure it passes around a value big enough to hold any
 * of the possible values the union can hold.
 */
static inline ffi_type*
eris_ffi_get_union_type (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (ERIS_TYPE_UNION, eris_typeinfo_type (typeinfo));

    const ErisTypeInfo *biggest_typeinfo = NULL;
    const ErisTypeInfoMember *member;
    uint32_t biggest_size = 0;

    ERIS_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER (member, typeinfo) {
        const uint32_t member_size = eris_typeinfo_sizeof (member->typeinfo);
        if (member_size > biggest_size) {
            biggest_typeinfo = member->typeinfo;
            biggest_size     = member_size;
        }
    }

    return eris_ffi_get_type (biggest_typeinfo);
}


static ffi_type*
eris_ffi_get_type (const ErisTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    typeinfo = eris_typeinfo_get_non_synthetic (typeinfo);

    switch (eris_typeinfo_type (typeinfo)) {
        case ERIS_TYPE_VOID:    return &ffi_type_void;
        case ERIS_TYPE_BOOL:    return &ffi_type_uint8;
        case ERIS_TYPE_S8:      return &ffi_type_sint8;
        case ERIS_TYPE_U8:      return &ffi_type_uint8;
        case ERIS_TYPE_S16:     return &ffi_type_sint16;
        case ERIS_TYPE_U16:     return &ffi_type_uint16;
        case ERIS_TYPE_S32:     return &ffi_type_sint32;
        case ERIS_TYPE_U32:     return &ffi_type_uint32;
        case ERIS_TYPE_S64:     return &ffi_type_sint64;
        case ERIS_TYPE_U64:     return &ffi_type_uint64;
        case ERIS_TYPE_FLOAT:   return &ffi_type_float;
        case ERIS_TYPE_DOUBLE:  return &ffi_type_double;
        case ERIS_TYPE_POINTER: return &ffi_type_pointer;
        case ERIS_TYPE_STRUCT:  return eris_ffi_get_struct_type (typeinfo);
        case ERIS_TYPE_ARRAY:   return eris_ffi_get_array_type (typeinfo);
        case ERIS_TYPE_UNION:   return eris_ffi_get_union_type (typeinfo);

        case ERIS_TYPE_ENUM:
            /* Enums are passed as integers of the corresponding width. */
            switch (eris_typeinfo_sizeof (typeinfo)) {
                case 1: return &ffi_type_sint8;
                case 2: return &ffi_type_sint16;
                case 4: return &ffi_type_sint32;
                case 8: return &ffi_type_sint64;
            }
            /* fall-through */

        case ERIS_TYPE_TYPEDEF:
        case ERIS_TYPE_CONST:
            CHECK_UNREACHABLE ();

        default:
            TRACE (RED "Unsupported type: " NORMAL "%s\n",
                   eris_typeinfo_name (typeinfo));
            abort (); /* TODO: Handle a bit more gracefully. */
    }
}


static void
eris_fcall_ffi_map_types (ErisFunction *ef)
{
    CHECK_NOT_NULL (ef);

    if (ef->return_typeinfo) {
        ef->fcall_ffi_scratch_size = eris_typeinfo_sizeof (ef->return_typeinfo);
        ef->fcall_ffi_return_type  = eris_ffi_get_type (ef->return_typeinfo);
    } else {
        ef->fcall_ffi_scratch_size = 0;
        ef->fcall_ffi_return_type  = &ffi_type_void;
    }

    if (ef->n_param > 0) {
        ef->fcall_ffi_param_types = calloc (ef->n_param, sizeof (ffi_type*));
        for (uint32_t i = 0; i < ef->n_param; i++) {
            ef->fcall_ffi_scratch_size +=
                    eris_typeinfo_sizeof (ef->param_types[i]);
            ef->fcall_ffi_param_types[i] =
                    eris_ffi_get_type (ef->param_types[i]);
        }
    }

    ffi_status status = ffi_prep_cif (&ef->fcall_ffi_cif,
                                      FFI_DEFAULT_ABI,
                                      ef->n_param,
                                      ef->fcall_ffi_return_type,
                                      ef->fcall_ffi_param_types);
    if (status != FFI_OK) {
        TRACE ("%s(): cannot map typeinfos to FFI types\n",
               ef->name ? ef->name : "?");
        /* TODO: Report instead of aborting. */
        abort ();
    }
}


static int
eris_function_call (lua_State *L)
{
    ErisFunction *ef = to_eris_function (L);

    if (lua_gettop (L) - 1 != ef->n_param) {
        return luaL_error (L, "wrong number of parameters"
                           " (given=%d, expected=%d)",
                           lua_gettop (L) - 1,
                           ef->n_param);
    }
    TRACE (BLUE "%s()" NORMAL ": FFI call address=%p\n", ef->name, ef->address);

    if (!ef->fcall_ffi_return_type)
        eris_fcall_ffi_map_types (ef);

    uintptr_t scratch[ef->fcall_ffi_scratch_size / sizeof (uintptr_t) + 1];
    void *params[ef->n_param];

    TRACE (FBLUE "%s()" NORMAL ": FFI scratch buffer size=%lu (requested=%lu)\n",
           ef->name, sizeof (scratch), ef->fcall_ffi_scratch_size);

    uintptr_t addr = (uintptr_t) scratch;
    if (ef->return_typeinfo) {
        addr += eris_typeinfo_sizeof (ef->return_typeinfo);
    }

    /*
     * Convert function arguments in C types.
     */
    for (uint32_t i = 0; i < ef->n_param; i++) {
        params[i] = (void*) addr;
        TRACE (FBLUE "%s()" NORMAL ": Parameter %" PRIu32 ", type %s\n",
               ef->name, i, eris_typeinfo_name (ef->param_types[i]));
        cvalue_get (L, i + 2, ef->param_types[i], (void*) addr);
        addr += eris_typeinfo_sizeof (ef->param_types[i]);
    }

    TRACE (FBLUE "%s()" NORMAL ": Invoking ... ", ef->name);
    ffi_call (&ef->fcall_ffi_cif, ef->address, scratch, params);
    TRACE (">" BLUE "done\n" NORMAL);

    if (ef->return_typeinfo) {
        return cvalue_push (L, ef->return_typeinfo, scratch, true);
    } else {
        return 0;
    }
}


static inline void
eris_fcall_ffi_init (ErisFunction *ef)
{
    /*
     * Zero ffi_type pointers, to mark FFI types to be filled-in
     * later on lazily, on the first call to the function.
     */
    CHECK_NOT_NULL (ef);
    ef->fcall_ffi_return_type = NULL;
    ef->fcall_ffi_param_types = NULL;
}


static void
eris_ffi_free_elements (ffi_type *type)
{
    CHECK_NOT_NULL (type);

    if (type->type != FFI_TYPE_STRUCT)
        return;
    for (size_t i = 0; type->elements[i]; i++)
        eris_ffi_free_elements (type->elements[i]);
    free (type);
}


static inline void
eris_fcall_ffi_free (ErisFunction *ef)
{
    CHECK_NOT_NULL (ef);

    if (!ef->fcall_ffi_return_type)
        return;

    eris_ffi_free_elements (ef->fcall_ffi_return_type);
    for (uint32_t i = 0; i < ef->n_param; i++)
        eris_ffi_free_elements (ef->fcall_ffi_param_types[i]);
    free (ef->fcall_ffi_param_types);
}
