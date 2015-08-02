/*
 * eol-fcall-ffi.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef EOL_FCALL_FFI_H
#define EOL_FCALL_FFI_H

#include <ffi.h>

#define EOL_FUNCTION_FCALL_FIELDS     \
    ffi_cif    fcall_ffi_cif;         \
    ffi_type  *fcall_ffi_return_type; \
    ffi_type **fcall_ffi_param_types; \
    size_t     fcall_ffi_scratch_size

#define EOL_FUNCTION_FCALL_INIT \
    eol_fcall_ffi_init

#define EOL_FUNCTION_FCALL_FREE \
    eol_fcall_ffi_free

static inline void eol_fcall_ffi_init (EolFunction *ef);
static inline void eol_fcall_ffi_free (EolFunction *ef);

#endif /* !EOL_FCALL_FFI_H */
