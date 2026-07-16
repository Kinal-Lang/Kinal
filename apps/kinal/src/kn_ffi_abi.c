#include "kn/ffi_abi.h"

static KnFfiAbiType abi_type(KnFfiAbiKind kind, int bits, int is_signed, int is_bool)
{
    KnFfiAbiType out;
    out.kind = kind;
    out.bits = bits;
    out.is_signed = is_signed;
    out.is_bool = is_bool;
    return out;
}

KnFfiAbiType kn_ffi_abi_type(Type type, int target_pointer_bits)
{
    int pointer_bits = target_pointer_bits == 32 ? 32 : 64;
    switch (type.kind)
    {
    case TY_VOID:
        return abi_type(KN_FFI_ABI_VOID, 0, 0, 0);
    case TY_BOOL:
        /* Kinal bool interoperates with the project's C int/Win32 BOOL contract. */
        return abi_type(KN_FFI_ABI_INTEGER, 32, 0, 1);
    case TY_BYTE:
    case TY_CHAR:
    case TY_U8:
        return abi_type(KN_FFI_ABI_INTEGER, 8, 0, 0);
    case TY_I8:
        return abi_type(KN_FFI_ABI_INTEGER, 8, 1, 0);
    case TY_U16:
        return abi_type(KN_FFI_ABI_INTEGER, 16, 0, 0);
    case TY_I16:
        return abi_type(KN_FFI_ABI_INTEGER, 16, 1, 0);
    case TY_U32:
        return abi_type(KN_FFI_ABI_INTEGER, 32, 0, 0);
    case TY_INT:
    case TY_I32:
        return abi_type(KN_FFI_ABI_INTEGER, 32, 1, 0);
    case TY_U64:
        return abi_type(KN_FFI_ABI_INTEGER, 64, 0, 0);
    case TY_I64:
        return abi_type(KN_FFI_ABI_INTEGER, 64, 1, 0);
    case TY_ISIZE:
        return abi_type(KN_FFI_ABI_INTEGER, pointer_bits, 1, 0);
    case TY_USIZE:
        return abi_type(KN_FFI_ABI_INTEGER, pointer_bits, 0, 0);
    case TY_F32:
        return abi_type(KN_FFI_ABI_FLOAT, 32, 1, 0);
    case TY_FLOAT:
    case TY_F64:
        return abi_type(KN_FFI_ABI_FLOAT, 64, 1, 0);
    case TY_STRING:
    case TY_PTR:
        return abi_type(KN_FFI_ABI_POINTER, pointer_bits, 0, 0);
    default:
        return abi_type(KN_FFI_ABI_INVALID, 0, 0, 0);
    }
}

bool kn_ffi_abi_type_supported(Type type, int target_pointer_bits)
{
    return kn_ffi_abi_type(type, target_pointer_bits).kind != KN_FFI_ABI_INVALID;
}
