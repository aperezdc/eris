/*
 * eris-fcall-ffi.c
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "eris-fcall-ffi.h"


static ffi_type*
eris_fcall_ffi_map_type (const ErisTypeInfo *typeinfo,
                         size_t             *accumulator)
{
    *accumulator += eris_typeinfo_sizeof (typeinfo);

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

        case ERIS_TYPE_CONST:
        case ERIS_TYPE_TYPEDEF:
            CHECK_UNREACHABLE ();
            return NULL;
    }
}


static void
eris_fcall_ffi_map_types (ErisFunction *ef)
{
    CHECK_NOT_NULL (ef);

    size_t scratch = 0;
    ef->fcall_ffi_return_type = ef->return_typeinfo
            ? eris_fcall_ffi_map_type (ef->return_typeinfo, &scratch)
            : &ffi_type_void;

    if (ef->n_param > 0) {
        ef->fcall_ffi_param_types = calloc (ef->n_param, sizeof (ffi_type*));
        for (uint32_t i = 0; i < ef->n_param; i++) {
            ef->fcall_ffi_param_types[i] =
                    eris_fcall_ffi_map_type (ef->param_types[i], &scratch);
        }
    }

    CHECK_NOT_NULL (ef->fcall_ffi_return_type);

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

    ef->fcall_ffi_scratch_size = scratch;
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
        cvalue_get (L, i + 2, ef->param_types[i], (void*) addr);
        addr += eris_typeinfo_sizeof (ef->param_types[i]);
    }

    TRACE (FBLUE "%s()" NORMAL ": Invoking ... ", ef->name);
    ffi_call (&ef->fcall_ffi_cif, ef->address, scratch, params);
    TRACE (">" BLUE "done\n" NORMAL);

    if (ef->return_typeinfo) {
        return cvalue_push (L, ef->return_typeinfo, scratch);
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


static inline void
eris_fcall_ffi_free (ErisFunction *ef)
{
    CHECK_NOT_NULL (ef);
    free (ef->fcall_ffi_param_types);
}
