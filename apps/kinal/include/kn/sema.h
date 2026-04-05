#pragma once
#include "kn/ast.h"
#include "kn/profile.h"
#include "kn/source.h"

typedef struct
{
    int require_entry;
    int env_kind;
    const char *entry_name;
    int target_os;
    int target_arch;
    int target_env;
    int target_pointer_bits;
    const char *target_path_separator;
    const char *target_new_line;
    const char *target_exe_suffix;
    const char *target_object_suffix;
    const char *target_dynamic_library_suffix;
    const char *target_static_library_suffix;
    int host_os;
    int host_arch;
    int host_env;
    int host_pointer_bits;
    const char *host_path_separator;
    const char *host_new_line;
    const char *host_exe_suffix;
    const char *host_object_suffix;
    const char *host_dynamic_library_suffix;
    const char *host_static_library_suffix;
    int runtime_backend;
} KnSemaOptions;

enum
{
    KN_INFO_OS_UNKNOWN = 0,
    KN_INFO_OS_WINDOWS = 1,
    KN_INFO_OS_LINUX = 2,
    KN_INFO_OS_MACOS = 3
};

enum
{
    KN_INFO_ARCH_UNKNOWN = 0,
    KN_INFO_ARCH_X86 = 1,
    KN_INFO_ARCH_X64 = 2,
    KN_INFO_ARCH_ARM64 = 3
};

enum
{
    KN_INFO_ENV_UNKNOWN = 0,
    KN_INFO_ENV_HOSTED = 1,
    KN_INFO_ENV_FREESTANDING = 2
};

enum
{
    KN_INFO_RUNTIME_UNKNOWN = 0,
    KN_INFO_RUNTIME_NATIVE  = 1,
    KN_INFO_RUNTIME_VM      = 2
};

int kn_sema_check(const KnSource *src, MetaList *metas, FuncList *funcs, ImportList *imports, ClassList *classes,
                  InterfaceList *interfaces, StructList *structs, EnumList *enums, StmtList *globals,
                  const KnSemaOptions *opts);


