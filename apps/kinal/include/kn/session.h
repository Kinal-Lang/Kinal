#pragma once

#include "kn/diag.h"

typedef struct KnCompileSession KnCompileSession;

typedef struct
{
    const char *target_triple;
    int target_pointer_bits;
    int environment_kind;
    int runtime_mode;
} KnCompileSessionConfig;

KnCompileSession *kn_compile_session_create(const KnCompileSessionConfig *config);
void kn_compile_session_destroy(KnCompileSession *session);

// Entering a session makes its diagnostic context active on the current
// thread. Calls may be nested, but enter/leave must remain balanced.
void kn_compile_session_enter(KnCompileSession *session);
void kn_compile_session_leave(KnCompileSession *session);

KnDiagContext *kn_compile_session_diagnostics(KnCompileSession *session);
const char *kn_compile_session_target_triple(const KnCompileSession *session);
int kn_compile_session_target_pointer_bits(const KnCompileSession *session);
int kn_compile_session_environment_kind(const KnCompileSession *session);
int kn_compile_session_runtime_mode(const KnCompileSession *session);
