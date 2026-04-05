#pragma once

#include "kn/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// Injects compiler/runtime-provided builtin classes into the given class list.
// This mirrors the injection done by the CLI driver, and is also used by tools
// like the LSP server to keep analysis consistent.
void kn_inject_builtins(ClassList *classes);

#ifdef __cplusplus
}
#endif

