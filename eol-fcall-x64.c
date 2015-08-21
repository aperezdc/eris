/*
** This file has been pre-processed with DynASM.
** http://luajit.org/dynasm.html
** DynASM version 1.3.0, DynASM x64 version 1.3.0
** DO NOT EDIT! The original file is in "eol-fcall-x86.dasc".
*/

#line 1 "eol-fcall-x86.dasc"
/*
 * eol-fcall-x86.dasc
 * Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "dynasm/dasm_proto.h"
#include "dynasm/dasm_x86.h"
#include "eol-fcall-x64.h"
#include "eol-typing.h"
#include "eol-util.h"
#include "eol-lua.h"
#include <stdbool.h>


//|.if X64
//|.arch x64
#if DASM_VERSION != 10300
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 19 "eol-fcall-x86.dasc"
#define DASM_X64 1
//|.else
//|.arch x86
#line 23 "eol-fcall-x86.dasc"
//|.endif

//|.if X64
//|.define REG_A, rax
//|.else
//|.define REG_A, eax
//|.define RET_H, edx  // Used for returning int64_t values
//|.define RET_L, eax
//|.endif


//| // These macros allow calling into C functions, and are used to generate
//| // calls from the assembler to the Lua C API.
//|.macro call_rrp, func, arg1, arg2, arg3
//| mov64 rdx, arg3
//| mov   rsi, arg2
//| mov   rdi, arg1
//| call  func
//|.endmacro
//|
//|.macro call_rr, func, arg1, arg2
//| mov   rsi, arg2
//| mov   rdi, arg1
//| call  func
//|.endmacro


enum {
#if DASM_X64
    FJ_REGS_INT = 6, /* rdi, rsi, rdx, rcx, r8, r9 */
    FJ_REGS_FLT = 8, /* xmm0-xmm7 */
#else
    FJ_REGS_INT = 0,
    FJ_REGS_FLT = 0,
#endif
};

typedef struct {
    uint16_t ints;
    uint16_t floats;
    uint32_t stack_offset;
} FjAllocation;


static inline void
fj_allocation_add_param (Dst_DECL, FjAllocation *alloc)
{
    if (alloc->ints < FJ_REGS_INT) {
        switch (alloc->ints) {
            case 0:
                //| mov rdi, REG_A
                dasm_put(Dst, 0);
#line 74 "eol-fcall-x86.dasc"
                break;
            case 1:
                //| mov rsi, REG_A
                dasm_put(Dst, 4);
#line 77 "eol-fcall-x86.dasc"
                break;
            case 2:
                //| mov rdx, REG_A
                dasm_put(Dst, 8);
#line 80 "eol-fcall-x86.dasc"
                break;
            case 3:
                //| mov rcx, REG_A
                dasm_put(Dst, 12);
#line 83 "eol-fcall-x86.dasc"
                break;
            case 4:
                //| mov r8, REG_A
                dasm_put(Dst, 16);
#line 86 "eol-fcall-x86.dasc"
                break;
            case 5:
                //| mov r0, REG_A
                dasm_put(Dst, 20);
#line 89 "eol-fcall-x86.dasc"
                break;
            default:
                CHECK_UNREACHABLE ();
        }
    } else {
        //|.if X64
        //| mov [rsp + alloc->stack_offset], rax
        //|.else
        //| mov [rsp + alloc->stack_offset], eax
        //|.endif
        dasm_put(Dst, 24, alloc->stack_offset);
#line 99 "eol-fcall-x86.dasc"
#if DASM_X64
        alloc->stack_offset += 8;
#else
        alloc->stack_offset += 4;
#endif
    }
    alloc->ints++;
}

//|.actionlist fj_function_trampoline
static const unsigned char fj_function_trampoline[137] = {
  72.0,137.0,199.0,255,72.0,137.0,198.0,255,72.0,137.0,194.0,255,72.0,137.0,
  193.0,255,73.0,137.0,192.0,255,72.0,137.0,192.0,255,72.0,137.0,132.0,253,
  36.0,233,255,72.0,199.0,198.0,237,72.0,199.0,199.0,237,232,251,1,0,72.0,131.0,
  252,248.0,0,15.0,149.0,208.0,72.0,15.0,182.0,192.0,255,72.0,199.0,198.0,237,
  72.0,199.0,199.0,237,232,251,1,1,72.0,15.0,182.0,192.0,255,72.0,199.0,198.0,
  237,72.0,199.0,199.0,237,232,251,1,1,72.0,15.0,190.0,192.0,255,72.0,199.0,
  198.0,237,72.0,199.0,199.0,237,232,251,1,1,72.0,15.0,183.0,192.0,255,72.0,
  199.0,198.0,237,72.0,199.0,199.0,237,232,251,1,1,72.0,15.0,191.0,192.0,255,
  72.0,199.0,198.0,237,72.0,199.0,199.0,237,232,251,1,1,255
};

#line 109 "eol-fcall-x86.dasc"


static void
fcall_jit_compile (lua_State *L,
                   const EolFunction *ef)
{
    /*
     * Builds a lua_CFunction trampoline which calls the
     * given EolFunction.
     */
    dasm_State *dasm;
    dasm_init (&dasm, 0);
    dasm_setup (&dasm, fj_function_trampoline);
    FjAllocation alloc = { 0, };
    dasm_State **Dst = &dasm;

    for (uint32_t i = 0; i < ef->n_param; i++) {
        const EolTypeInfo *typeinfo = ef->param_types[i];
        switch (eol_typeinfo_type (typeinfo)) {
            case EOL_TYPE_BOOL:
                //| call_rr extern lua_toboolean, L, i + 2
                //| cmp REG_A, 0
                //| setne al
                //| movzx REG_A, al
                dasm_put(Dst, 31, i + 2, L);
#line 133 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;

            case EOL_TYPE_U8:
                //| call_rr extern luaL_checkinteger, L, i + 2
                //| movzx REG_A, al
                dasm_put(Dst, 56, i + 2, L);
#line 139 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;

            case EOL_TYPE_S8:
                //| call_rr extern luaL_checkinteger, L, i + 2
                //| movsx REG_A, al
                dasm_put(Dst, 73, i + 2, L);
#line 145 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;

            case EOL_TYPE_U16:
                //| call_rr extern luaL_checkinteger, L, i + 2
                //| movzx REG_A, ax
                dasm_put(Dst, 90, i + 2, L);
#line 151 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;

            case EOL_TYPE_S16:
                //| call_rr extern luaL_checkinteger, L, i + 2
                //| movsx REG_A, ax
                dasm_put(Dst, 107, i + 2, L);
#line 157 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;

            case EOL_TYPE_U32:
                //| call_rr extern luaL_checkinteger, L, i + 2
                //|.if not X64
                //| movzx REG_A, eax
                //|.endif
                dasm_put(Dst, 124, i + 2, L);
#line 165 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;

            case EOL_TYPE_S32:
                //| call_rr extern luaL_checkinteger, L, i + 2
                //|.if not X64
                //| movsx REG_A, eax
                //|.endif
                dasm_put(Dst, 124, i + 2, L);
#line 173 "eol-fcall-x86.dasc"
                fj_allocation_add_param (Dst, &alloc);
                break;
        }
    }
}

/* vim: set ft=c: */
