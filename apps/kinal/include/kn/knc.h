#pragma once
#include "kn/ast.h"

typedef struct
{
    const char *entry_name;
    int enable_superloop;
} KnKncOptions;

int kn_emit_knc(const char *out_path,
                MetaList *metas,
                FuncList *funcs,
                ImportList *imports,
                ClassList *classes,
                InterfaceList *interfaces,
                StructList *structs,
                EnumList *enums,
                StmtList *globals,
                const KnKncOptions *opts);
