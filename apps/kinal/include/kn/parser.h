#pragma once
#include "kn/ast.h"
#include "kn/lexer.h"
#include "kn/source.h"

void parse_program(const KnSource *src, MetaList *metas, FuncList *funcs, ImportList *imports, ClassList *classes,
                   InterfaceList *interfaces, StructList *structs, EnumList *enums, StmtList *globals);

