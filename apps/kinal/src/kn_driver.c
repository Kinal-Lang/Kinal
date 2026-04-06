#include "kn/util.h"
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <stdlib.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include "kn/parser.h"
#include "kn/sema.h"
#include "kn/codegen.h"
#include "kn/link.h"
#include "kn/diag.h"
#include "kn/format.h"
#include "kn/source.h"
#include "kn/lexer.h"
#include "kn/std.h"
#include "kn/knc.h"
#include "kn/profile.h"
#include "kn/builtins.h"

#ifndef KN_LLVM_BIN
#define KN_LLVM_BIN ""
#endif

typedef struct
{
    const char **items;
    int count;
    int cap;
} StrList;

typedef struct
{
    const char *module;
    StrList paths;
} ModulePath;

typedef struct
{
    ModulePath *items;
    int count;
    int cap;
} ModuleMap;

typedef struct
{
    KnSource **items;
    int count;
    int cap;
} SourceList;

typedef enum
{
    KN_EMIT_LINK = 0,
    KN_EMIT_OBJECT = 1,
    KN_EMIT_ASSEMBLY = 2,
    KN_EMIT_LLVM_IR = 3,
    KN_EMIT_KNC = 4,
    KN_EMIT_KNC_EXE = 5
} KnEmitMode;

typedef struct
{
    const char *out_path;
    int auto_link;
    int trace;
    int profile;
    const char **package_roots;
    int package_root_count;
    const char **official_package_roots;
    int official_package_root_count;
    const KnLinkItem *link_items;
    int link_item_count;
    const char *target_triple;
    int linker_kind;   // KnLinkerKind
    const char *linker_path; // explicit linker executable path
    int artifact_kind; // KnArtifactKind
    int emit_mode;     // KnEmitMode
    int lto_mode;      // KnLtoMode
    int keep_temps;    // keep intermediate outputs (e.g. .obj) in link mode
    int env_kind;      // KnEnvKind
    int runtime_mode;  // KnRuntimeMode
    int panic_mode;    // KnPanicMode
    int crt_mode;      // KnCrtMode
    int no_crt;
    int no_default_libs;
    const char *entry_symbol;
    const char *link_script;
    int show_link;
    int show_link_search;
    int knc_superloop;
} KnCompileConfig;

typedef enum
{
    KN_INPUT_UNKNOWN = 0,
    KN_INPUT_FX = 1,
    KN_INPUT_OBJ = 2,
    KN_INPUT_ASM = 3,
    KN_INPUT_LL = 4,
    KN_INPUT_KNC = 5
} KnInputKind;

typedef enum
{
    KN_CLI_LINK_FILE = KN_LINK_ITEM_FILE,
    KN_CLI_LINK_LIB_DIR = KN_LINK_ITEM_LIB_DIR,
    KN_CLI_LINK_LIB_NAME = KN_LINK_ITEM_LIB_NAME,
    KN_CLI_LINK_ARG = KN_LINK_ITEM_ARG,
    KN_CLI_LINK_ROOT = 100
} KnCliLinkItemKind;

#include "driver/kn_driver_support.inc"
#include "driver/kn_driver_pkg.inc"
#include "driver/kn_driver_compile.inc"
#include "driver/kn_driver_cli.inc"
