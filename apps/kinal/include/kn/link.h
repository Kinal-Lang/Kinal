#pragma once
#include "kn/profile.h"

typedef enum
{
    KN_LINKER_LLD = 0,
    KN_LINKER_ZIG = 1,
    KN_LINKER_MSVC = 2
} KnLinkerKind;

typedef enum
{
    KN_ARTIFACT_EXE = 0,
    KN_ARTIFACT_STATIC_LIB = 1,
    KN_ARTIFACT_SHARED_LIB = 2
} KnArtifactKind;

typedef enum
{
    KN_LTO_OFF = 0,
    KN_LTO_FULL = 1,
    KN_LTO_THIN = 2
} KnLtoMode;

typedef enum
{
    KN_CRT_AUTO = 0,
    KN_CRT_DYNAMIC = 1,
    KN_CRT_STATIC = 2
} KnCrtMode;

typedef enum
{
    KN_LINK_ITEM_FILE = 0,
    KN_LINK_ITEM_LIB_DIR = 1,
    KN_LINK_ITEM_LIB_NAME = 2,
    KN_LINK_ITEM_ARG = 3
} KnLinkItemKind;

typedef struct
{
    int kind; // KnLinkItemKind
    const char *value;
} KnLinkItem;

typedef struct
{
    const char *obj_path;
    const char *out_path;
    const char *linker_path; // explicit linker executable path
    const KnLinkItem *items;
    int item_count;
    const char *target_triple;
    int linker_kind;     // KnLinkerKind
    int artifact_kind;   // KnArtifactKind
    int include_runtime; // 1: include kn_runtime support
    int lto_mode;        // KnLtoMode
    const char **exports;
    int export_count;
    int env_kind;        // KnEnvKind
    int runtime_mode;    // KnRuntimeMode
    int panic_mode;      // KnPanicMode
    int crt_mode;        // KnCrtMode
    int no_crt;
    int no_default_libs;
    const char *entry_symbol;
    const char *link_script;
    int trace;
    int show_link;
    int show_link_search;
} KnLinkOptions;

int kn_link(const KnLinkOptions *opts);


