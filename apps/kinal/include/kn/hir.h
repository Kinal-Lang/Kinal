#pragma once

#include "kn/ast.h"

typedef struct
{
    unsigned expressions;
    unsigned typed_expressions;
    unsigned calls;
    unsigned unresolved_calls;
    unsigned builtin_calls;
    unsigned function_calls;
    unsigned delegate_calls;
    unsigned static_method_calls;
    unsigned direct_method_calls;
    unsigned virtual_method_calls;
    unsigned interface_method_calls;
    unsigned binaries;
    unsigned unresolved_binaries;
    unsigned binary_numeric_arithmetic;
    unsigned binary_string_concat;
    unsigned binary_pointer_arithmetic;
    unsigned binary_bitwise;
    unsigned binary_string_equality;
    unsigned binary_reference_equality;
    unsigned binary_scalar_equality;
    unsigned binary_numeric_comparison;
    unsigned binary_logical_short_circuit;
} KnHirStats;

// Normalize sema's resolved AST annotations into backend-independent call
// targets. Must run after semantic analysis and before any backend.
void kn_hir_lower_calls(FuncList *funcs, ClassList *classes, InterfaceList *interfaces,
                        StmtList *globals);

// Collect deterministic structural statistics from the lowered typed tree.
void kn_hir_collect_stats(const FuncList *funcs, const ClassList *classes,
                          const InterfaceList *interfaces, const StmtList *globals,
                          KnHirStats *out);

const char *kn_hir_call_target_name(KnCallTargetKind kind);
const char *kn_hir_binary_lowering_name(KnBinaryLoweringKind kind);
