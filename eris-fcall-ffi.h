/*
 * eris-fcall-ffi.h
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef ERIS_FCALL_FFI_H
#define ERIS_FCALL_FFI_H

#include <ffi.h>

#define ERIS_FUNCTION_FCALL_FIELDS    \
    ffi_cif    fcall_ffi_cif;         \
    ffi_type  *fcall_ffi_return_type; \
    ffi_type **fcall_ffi_param_types; \
    size_t     fcall_ffi_scratch_size

#define ERIS_FUNCTION_FCALL_INIT \
    eris_fcall_ffi_init

#define ERIS_FUNCTION_FCALL_FREE \
    eris_fcall_ffi_free

static inline void eris_fcall_ffi_init (ErisFunction *ef);
static inline void eris_fcall_ffi_free (ErisFunction *ef);

#endif /* !ERIS_FCALL_FFI_H */
