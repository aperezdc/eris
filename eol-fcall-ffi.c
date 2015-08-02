/*
 * eol-fcall-ffi.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eol-fcall-ffi.h"


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
eol_ffi_struct_type_count_items (const EolTypeInfo *typeinfo)
{
    typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);
    uint32_t result = 0;

    const EolTypeInfoMember *member;
    EOL_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER (member, typeinfo) {
        const EolTypeInfo *T =
                eol_typeinfo_get_non_synthetic (member->typeinfo);

        if (eol_typeinfo_is_array (T)) {
            result += eol_typeinfo_array_n_items (T);
        } else {
            result++;
        }
    }
    return result;
}


static ffi_type* eol_ffi_get_type (const EolTypeInfo*);


static inline ffi_type*
eol_ffi_get_struct_type (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (EOL_TYPE_STRUCT, eol_typeinfo_type (typeinfo));

    uint32_t n_items = eol_ffi_struct_type_count_items (typeinfo);
    ffi_type *type   = calloc (1, sizeof (ffi_type) +
                                  sizeof (ffi_type*) * (n_items + 1));
    type->elements   = (void*) &type[1];
    type->type       = FFI_TYPE_STRUCT;

    const EolTypeInfoMember *member;
    uint32_t element_index = 0;

    EOL_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER (member, typeinfo) {
        const EolTypeInfo *member_typeinfo =
                eol_typeinfo_get_non_synthetic (member->typeinfo);

        if (eol_typeinfo_is_array (member_typeinfo)) {
            ffi_type *item_type =
                    eol_ffi_get_type (eol_typeinfo_base (member_typeinfo));
            uint32_t n = eol_typeinfo_array_n_items (member_typeinfo);

            CHECK_UINT_LT (n_items, element_index + n);
            while (n--) {
                type->elements[element_index++] = item_type;
                CHECK_UINT_LT (n_items, element_index);
            }
        } else {
            type->elements[element_index++] =
                    eol_ffi_get_type (member_typeinfo);
        }
    }
    CHECK_UINT_EQ (n_items, element_index);
    CHECK (type->elements[n_items] == NULL);

    return type;
}


static inline ffi_type*
eol_ffi_get_array_type (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (EOL_TYPE_ARRAY, eol_typeinfo_type (typeinfo));

    ffi_type *base   = eol_ffi_get_type (eol_typeinfo_base (typeinfo));
    uint32_t n_items = eol_typeinfo_array_n_items (typeinfo);
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
eol_ffi_get_union_type (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);
    CHECK_UINT_EQ (EOL_TYPE_UNION, eol_typeinfo_type (typeinfo));

    const EolTypeInfo *biggest_typeinfo = NULL;
    const EolTypeInfoMember *member;
    uint32_t biggest_size = 0;

    EOL_TYPEINFO_COMPOUND_FOREACH_CONST_MEMBER (member, typeinfo) {
        const uint32_t member_size = eol_typeinfo_sizeof (member->typeinfo);
        if (member_size > biggest_size) {
            biggest_typeinfo = member->typeinfo;
            biggest_size     = member_size;
        }
    }

    return eol_ffi_get_type (biggest_typeinfo);
}


static ffi_type*
eol_ffi_get_type (const EolTypeInfo *typeinfo)
{
    CHECK_NOT_NULL (typeinfo);

    typeinfo = eol_typeinfo_get_non_synthetic (typeinfo);

    switch (eol_typeinfo_type (typeinfo)) {
        case EOL_TYPE_VOID:    return &ffi_type_void;
        case EOL_TYPE_BOOL:    return &ffi_type_uint8;
        case EOL_TYPE_S8:      return &ffi_type_sint8;
        case EOL_TYPE_U8:      return &ffi_type_uint8;
        case EOL_TYPE_S16:     return &ffi_type_sint16;
        case EOL_TYPE_U16:     return &ffi_type_uint16;
        case EOL_TYPE_S32:     return &ffi_type_sint32;
        case EOL_TYPE_U32:     return &ffi_type_uint32;
        case EOL_TYPE_S64:     return &ffi_type_sint64;
        case EOL_TYPE_U64:     return &ffi_type_uint64;
        case EOL_TYPE_FLOAT:   return &ffi_type_float;
        case EOL_TYPE_DOUBLE:  return &ffi_type_double;
        case EOL_TYPE_POINTER: return &ffi_type_pointer;
        case EOL_TYPE_STRUCT:  return eol_ffi_get_struct_type (typeinfo);
        case EOL_TYPE_ARRAY:   return eol_ffi_get_array_type (typeinfo);
        case EOL_TYPE_UNION:   return eol_ffi_get_union_type (typeinfo);

        case EOL_TYPE_ENUM:
            /* Enums are passed as integers of the corresponding width. */
            switch (eol_typeinfo_sizeof (typeinfo)) {
                case 1: return &ffi_type_sint8;
                case 2: return &ffi_type_sint16;
                case 4: return &ffi_type_sint32;
                case 8: return &ffi_type_sint64;
            }
            /* fall-through */

        case EOL_TYPE_TYPEDEF:
        case EOL_TYPE_CONST:
            CHECK_UNREACHABLE ();

        default:
            TRACE (RED "Unsupported type: " NORMAL "%s\n",
                   eol_typeinfo_name (typeinfo));
            abort (); /* TODO: Handle a bit more gracefully. */
    }
}


static void
eol_fcall_ffi_map_types (EolFunction *ef)
{
    CHECK_NOT_NULL (ef);

    if (ef->return_typeinfo) {
        ef->fcall_ffi_scratch_size = eol_typeinfo_sizeof (ef->return_typeinfo);
        ef->fcall_ffi_return_type  = eol_ffi_get_type (ef->return_typeinfo);
    } else {
        ef->fcall_ffi_scratch_size = 0;
        ef->fcall_ffi_return_type  = &ffi_type_void;
    }

    if (ef->n_param > 0) {
        ef->fcall_ffi_param_types = calloc (ef->n_param, sizeof (ffi_type*));
        for (uint32_t i = 0; i < ef->n_param; i++) {
            ef->fcall_ffi_scratch_size +=
                    eol_typeinfo_sizeof (ef->param_types[i]);
            ef->fcall_ffi_param_types[i] =
                    eol_ffi_get_type (ef->param_types[i]);
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
function_call (lua_State *L)
{
    EolFunction *ef = to_eol_function (L);

    if (lua_gettop (L) - 1 != ef->n_param) {
        return luaL_error (L, "wrong number of parameters"
                           " (given=%d, expected=%d)",
                           lua_gettop (L) - 1,
                           ef->n_param);
    }
    TRACE (BLUE "%s()" NORMAL ": FFI call address=%p\n", ef->name, ef->address);

    if (!ef->fcall_ffi_return_type)
        eol_fcall_ffi_map_types (ef);

    uintptr_t scratch[ef->fcall_ffi_scratch_size / sizeof (uintptr_t) + 1];
    void *params[ef->n_param];

    TRACE (FBLUE "%s()" NORMAL ": FFI scratch buffer size=%lu (requested=%lu)\n",
           ef->name, sizeof (scratch), ef->fcall_ffi_scratch_size);

    uintptr_t addr = (uintptr_t) scratch;
    if (ef->return_typeinfo) {
        addr += eol_typeinfo_sizeof (ef->return_typeinfo);
    }

    /*
     * Convert function arguments in C types.
     */
    for (uint32_t i = 0; i < ef->n_param; i++) {
        params[i] = (void*) addr;
        TRACE (FBLUE "%s()" NORMAL ": Parameter %" PRIu32 ", type %s\n",
               ef->name, i, eol_typeinfo_name (ef->param_types[i]));
        cvalue_get (L, i + 2, ef->param_types[i], (void*) addr);
        addr += eol_typeinfo_sizeof (ef->param_types[i]);
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
eol_fcall_ffi_init (EolFunction *ef)
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
eol_ffi_free_elements (ffi_type *type)
{
    CHECK_NOT_NULL (type);

    if (type->type != FFI_TYPE_STRUCT)
        return;
    for (size_t i = 0; type->elements[i]; i++)
        eol_ffi_free_elements (type->elements[i]);
    free (type);
}


static inline void
eol_fcall_ffi_free (EolFunction *ef)
{
    CHECK_NOT_NULL (ef);

    if (!ef->fcall_ffi_return_type)
        return;

    eol_ffi_free_elements (ef->fcall_ffi_return_type);
    for (uint32_t i = 0; i < ef->n_param; i++)
        eol_ffi_free_elements (ef->fcall_ffi_param_types[i]);
    free (ef->fcall_ffi_param_types);
}
