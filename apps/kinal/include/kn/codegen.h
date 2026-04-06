#pragma once
#include "kn/ast.h"
#include "kn/profile.h"

typedef enum
{
    KN_CODEGEN_OBJECT = 0,
    KN_CODEGEN_ASSEMBLY = 1,
    KN_CODEGEN_LLVM_IR = 2,
    KN_CODEGEN_LLVM_BC = 3
} KnCodegenEmit;

typedef struct
{
    const char *target_triple;
    int emit_kind;      // KnCodegenEmit
    int emit_entry;     // 1: emit executable entry wrapper, 0: library/object mode
    int emit_pic;       // 1: emit position-independent code for shared libraries
    int env_kind;       // KnEnvKind
    int runtime_mode;   // KnRuntimeMode
    int panic_mode;     // KnPanicMode
    const char *entry_name;
} KnCodegenOptions;

int kn_codegen(const char *out_path, MetaList *metas, FuncList *funcs, ImportList *imports, ClassList *classes,
               InterfaceList *interfaces, StructList *structs, EnumList *enums, StmtList *globals,
               const KnCodegenOptions *opts);



