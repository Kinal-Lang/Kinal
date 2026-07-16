#include "kn/session.h"
#include "kn/profile.h"
#include "kn/std.h"
#include "kn/util.h"

struct KnCompileSession
{
    KnDiagContext *diagnostics;
    KnDiagContext *previous_diagnostics;
    KnTempArena *temp_arena;
    KnTempArena *previous_temp_arena;
    char *target_triple;
    int target_pointer_bits;
    int environment_kind;
    int runtime_mode;
    int std_profile;
    int previous_std_profile;
    int enter_depth;
};

static char *session_copy_string(const char *value)
{
    if (!value || !value[0])
        return 0;
    size_t length = kn_strlen(value);
    char *copy = (char *)kn_malloc(length + 1);
    if (!copy)
        return 0;
    kn_memcpy(copy, value, length + 1);
    return copy;
}

KnCompileSession *kn_compile_session_create(const KnCompileSessionConfig *config)
{
    KnCompileSession *session = (KnCompileSession *)kn_malloc(sizeof(KnCompileSession));
    if (!session)
        return 0;
    kn_memset(session, 0, sizeof(*session));
    session->environment_kind = -1;
    session->runtime_mode = -1;
    session->std_profile = kn_std_get_profile();

    session->diagnostics = kn_diag_context_create();
    if (!session->diagnostics)
    {
        kn_free(session);
        return 0;
    }
    session->temp_arena = kn_temp_arena_create();
    if (!session->temp_arena)
    {
        kn_diag_context_destroy(session->diagnostics);
        kn_free(session);
        return 0;
    }

    if (config)
    {
        session->target_pointer_bits = config->target_pointer_bits;
        session->environment_kind = config->environment_kind;
        session->runtime_mode = config->runtime_mode;
        if (config->environment_kind == KN_ENV_HOSTED)
            session->std_profile = KN_STD_PROFILE_HOSTED;
        else if (config->runtime_mode == KN_RUNTIME_NONE)
            session->std_profile = KN_STD_PROFILE_FREESTANDING_CORE;
        else if (config->runtime_mode == KN_RUNTIME_ALLOC)
            session->std_profile = KN_STD_PROFILE_FREESTANDING_ALLOC;
        else if (config->runtime_mode == KN_RUNTIME_GC)
            session->std_profile = KN_STD_PROFILE_FREESTANDING_GC;
        session->target_triple = session_copy_string(config->target_triple);
        if (config->target_triple && config->target_triple[0] && !session->target_triple)
        {
            kn_diag_context_destroy(session->diagnostics);
            kn_temp_arena_destroy(session->temp_arena);
            kn_free(session);
            return 0;
        }
    }
    return session;
}

void kn_compile_session_destroy(KnCompileSession *session)
{
    if (!session)
        return;
    while (session->enter_depth > 0)
        kn_compile_session_leave(session);
    kn_diag_context_destroy(session->diagnostics);
    kn_temp_arena_destroy(session->temp_arena);
    kn_free(session->target_triple);
    kn_free(session);
}

void kn_compile_session_enter(KnCompileSession *session)
{
    if (!session)
        return;
    if (session->enter_depth == 0)
    {
        session->previous_diagnostics = kn_diag_context_set_current(session->diagnostics);
        session->previous_temp_arena = kn_temp_arena_set_current(session->temp_arena);
        kn_temp_arena_begin();
        session->previous_std_profile = kn_std_get_profile();
        kn_std_set_profile(session->std_profile);
    }
    session->enter_depth++;
}

void kn_compile_session_leave(KnCompileSession *session)
{
    if (!session || session->enter_depth <= 0)
        return;
    session->enter_depth--;
    if (session->enter_depth == 0)
    {
        session->std_profile = kn_std_get_profile();
        kn_std_set_profile(session->previous_std_profile);
        kn_temp_arena_end();
        kn_temp_arena_set_current(session->previous_temp_arena);
        kn_diag_context_set_current(session->previous_diagnostics);
        session->previous_temp_arena = 0;
        session->previous_diagnostics = 0;
    }
}

KnDiagContext *kn_compile_session_diagnostics(KnCompileSession *session)
{
    return session ? session->diagnostics : 0;
}

const char *kn_compile_session_target_triple(const KnCompileSession *session)
{
    return (session && session->target_triple) ? session->target_triple : "";
}

int kn_compile_session_target_pointer_bits(const KnCompileSession *session)
{
    return session ? session->target_pointer_bits : 0;
}

int kn_compile_session_environment_kind(const KnCompileSession *session)
{
    return session ? session->environment_kind : -1;
}

int kn_compile_session_runtime_mode(const KnCompileSession *session)
{
    return session ? session->runtime_mode : -1;
}
