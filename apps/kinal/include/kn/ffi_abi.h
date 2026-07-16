#pragma once

#include "kn/ast.h"

typedef enum
{
    KN_FFI_ABI_INVALID = 0,
    KN_FFI_ABI_VOID,
    KN_FFI_ABI_INTEGER,
    KN_FFI_ABI_FLOAT,
    KN_FFI_ABI_POINTER
} KnFfiAbiKind;

typedef struct
{
    KnFfiAbiKind kind;
    int bits;
    int is_signed;
    int is_bool;
} KnFfiAbiType;

/*
 * Classify a Kinal source type at a C ABI boundary.  This description is
 * intentionally independent of LLVM so the same contract can be consumed by
 * validation, future HIR lowering, and a self-hosted backend bridge.
 */
KnFfiAbiType kn_ffi_abi_type(Type type, int target_pointer_bits);
bool kn_ffi_abi_type_supported(Type type, int target_pointer_bits);
