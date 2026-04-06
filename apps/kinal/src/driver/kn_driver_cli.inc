#ifndef KN_HOSTLIB
#include <kn/version.h>

static void print_version(int verbose)
{
    kn_write_str("Kinal ");
    kn_write_str(KN_VERSION_STRING);
    kn_write_str("\n");
    if (verbose)
    {
        kn_write_str("kinalvm ");
        kn_write_str(KN_VM_VERSION_STRING);
        kn_write_str("\n");
    }
}

static void print_help(void)
{
    const char *fallback =
        "Kinal Compiler\n"
        "Usage:\n"
        "  kinal <subcommand> [options] [args...]\n"
        "  kinal build [options] <input file> [file2 ...]\n"
        "  kinal run [options] <file.kn> [-- program-args...]\n"
        "\n"
        "Subcommands:\n"
        "  build       Compile source files to native executables, objects, assembly, or IR\n"
        "  run         Compile a source file to a temporary native executable and run it\n"
        "  fmt         Format .kn source files\n"
        "  vm          Bytecode operations (build, run, pack)\n"
        "  pkg         Package management (build, info, unpack)\n"
        "\n"
        "Build quick reference:\n"
        "Output:\n"
        "  -o, --output <file>  Output file path\n"
        "  --emit <bin|obj|asm|ir>  Output kind (default: bin)\n"
        "  --kind <exe|static|shared>  Output artifact kind (link mode only)\n"
        "\n"
        "Target:\n"
        "  --target <triple|alias>  Target triple (default: host)\n"
        "                     aliases: win86, win64, linux64, linux-arm64, bare64, bare-arm64\n"
        "  --freestanding    Shortcut for freestanding core mode on the host architecture\n"
        "  --env <hosted|freestanding>  Select execution environment\n"
        "  --runtime <none|alloc|gc>  Select freestanding runtime profile\n"
        "  --panic <trap|loop>  Freestanding panic behavior\n"
        "  --entry <symbol>   Override executable entry symbol (Main/KMain by default)\n"
        "\n"
        "Project:\n"
        "  --project <file|dir>  Build from a project manifest (JSON)\n"
        "  --pkg-root <dir>  Add a local package root\n"
        "  --stdpkg-root <dir>  Add an official stdpkg root override\n"
        "\n"
        "Link:\n"
        "  --crt <auto|dynamic|static>  Hosted CRT selection (default: auto)\n"
        "  -L, --lib-dir <dir>  Add a library search directory\n"
        "  -l, --lib <name>    Link a library by name\n"
        "  --link-file <file>  Add an exact link input (.obj/.lib/.a)\n"
        "  --link-root <dir>   Expand a dependency root into target-specific search dirs\n"
        "  --link-arg <arg>    Pass one raw argument to the linker backend\n"
        "  --link-script <file>  Pass linker script to the freestanding linker\n"
        "  --no-crt          Do not link CRT startup objects\n"
        "  --no-default-libs Do not inject default platform libraries\n"
        "  --linker <lld|zig|msvc>  Select linker backend\n"
        "  --linker-path <file>  Explicit linker executable path (requires --linker)\n"
        "  --show-link      Print the final linker command\n"
        "  --show-link-search  Print expanded library search directories\n"
        "\n"
        "Frontend:\n"
        "  --no-module-discovery  Disable source/module auto-discovery\n"
        "  --keep-temps     Keep intermediate outputs generated for linking\n"
        "  --lto[=off|full|thin]  Enable link-time optimization (default: off)\n"
        "\n"
        "Diagnostics:\n"
        "  -h, --help       Show this help\n"
        "  --color <auto|always|never>  Diagnostic color mode\n"
        "  --lang <en|zh>   Diagnostic language\n"
        "  --locale-file <file>  External locale JSON map\n"
        "  --Werror         Treat warnings as errors\n"
        "  --warn-level <n> Warning level (0 disables warnings)\n"
        "\n"
        "Developer:\n"
        "  --dump-locale-en <file>  Export English locale template JSON\n"
        "  --trace          Print compile phase trace\n"
        "  --time           Print compile phase timing (ms)\n"
        "\n"
        "Run quick reference:\n"
        "  kinal run [options] <file.kn> [-- program-args...]\n"
        "  --no-module-discovery  Disable source/module auto-discovery\n"
        "  --keep-temps     Keep the temporary executable after execution\n"
        "  --pkg-root <dir>  Add a local package root\n"
        "  --stdpkg-root <dir>  Add an official stdpkg root override\n"
        "  --color <auto|always|never>  Diagnostic color mode\n"
        "  --lang <en|zh>   Diagnostic language\n"
        "  --locale-file <file>  External locale JSON map\n"
        "  --Werror         Treat warnings as errors\n"
        "  --warn-level <n> Warning level (0 disables warnings)\n"
        "  .knc files: use \"kinal vm run\" instead\n"
        "\n"
        "VM / package quick reference:\n"
        "  kinal vm build <file.kn> -o <file.knc>\n"
        "  kinal vm run <file.kn|file.knc> [-- program-args...]\n"
        "  kinal vm pack <file.kn|file.knc> -o <file>\n"
        "  kinal pkg build --manifest <file|dir> -o <file.klib>\n"
        "  kinal pkg info <file.klib>\n"
        "  kinal pkg unpack <file.klib> -o <dir>\n"
        "\n"
        "Input types:\n"
        "  .kn/.kinal/.fx   Full frontend compile\n"
        "  .ll/.s/.asm      Continue from intermediate and compile forward\n"
        "  .o/.obj          Continue from object and link\n"
        "  .knc             Use \"kinal vm run\" or \"kinal vm pack\"\n"
        "\n"
        "Use \"kinal <subcommand> --help\" for detailed subcommand help.\n";
    kn_write_str(kn_diag_localize("ui.help.full", fallback));
}

static void print_build_help(void)
{
    const char *text =
        "Usage: kinal build [options] <file.kn> [file2.kn ...]\n"
        "\n"
        "Compile source files to native executables, objects, assembly, or LLVM IR.\n"
        "\n"
        "Output:\n"
        "  -o, --output <file>  Output file path\n"
        "  --emit <bin|obj|asm|ir>  Output kind (default: bin)\n"
        "  --kind <exe|static|shared>  Output artifact kind (link mode only)\n"
        "\n"
        "Target:\n"
        "  --target <triple|alias>  Target triple (default: host)\n"
        "                     aliases: win86, win64, linux64, linux-arm64, bare64, bare-arm64\n"
        "  --freestanding    Shortcut for freestanding core mode on the host architecture\n"
        "  --env <hosted|freestanding>  Select execution environment\n"
        "  --runtime <none|alloc|gc>  Select freestanding runtime profile\n"
        "  --panic <trap|loop>  Freestanding panic behavior\n"
        "  --entry <symbol>   Override executable entry symbol (Main/KMain by default)\n"
        "\n"
        "Project:\n"
        "  --project <file|dir>  Build from a project manifest (JSON)\n"
        "  --pkg-root <dir>  Add a local package root\n"
        "  --stdpkg-root <dir>  Add an official stdpkg root override\n"
        "\n"
        "Link:\n"
        "  --crt <auto|dynamic|static>  Hosted CRT selection (default: auto)\n"
        "  -L, --lib-dir <dir>  Add a library search directory\n"
        "  -l, --lib <name>    Link a library by name\n"
        "  --link-file <file>  Add an exact link input (.obj/.lib/.a)\n"
        "  --link-root <dir>   Expand a dependency root into target-specific search dirs\n"
        "  --link-arg <arg>    Pass one raw argument to the linker backend\n"
        "  --link-script <file>  Pass linker script to the freestanding linker\n"
        "  --no-crt          Do not link CRT startup objects\n"
        "  --no-default-libs Do not inject default platform libraries\n"
        "  --linker <lld|zig|msvc>  Select linker backend\n"
        "  --linker-path <file>  Explicit linker executable path (requires --linker)\n"
        "  --show-link      Print the final linker command\n"
        "  --show-link-search  Print expanded library search directories\n"
        "\n"
        "Frontend:\n"
        "  --no-module-discovery  Disable source/module auto-discovery\n"
        "  --keep-temps     Keep intermediate outputs generated for linking\n"
        "  --lto[=off|full|thin]  Enable link-time optimization (default: off)\n"
        "\n"
        "Diagnostics:\n"
        "  --color <auto|always|never>  Diagnostic color mode\n"
        "  --lang <en|zh>   Diagnostic language\n"
        "  --locale-file <file>  External locale JSON map\n"
        "  --Werror         Treat warnings as errors\n"
        "  --warn-level <n> Warning level (0 disables warnings)\n"
        "\n"
        "Developer:\n"
        "  --dump-locale-en <file>  Export English locale template JSON\n"
        "  --trace          Print compile phase trace\n"
        "  --time           Print compile phase timing (ms)\n";
    kn_write_str(text);
}

static void print_run_help(void)
{
    const char *text =
        "Usage: kinal run [options] <file.kn> [-- program-args...]\n"
        "\n"
        "Compile a source file to a native executable and run it immediately.\n"
        "The temporary executable is removed after the program exits.\n"
        "\n"
        "Options:\n"
        "  --no-module-discovery  Disable source/module auto-discovery\n"
        "  --pkg-root <dir>  Add a local package root\n"
        "  --stdpkg-root <dir>  Add an official stdpkg root override\n"
        "  --keep-temps     Keep the temporary executable after execution\n"
        "  --lang <en|zh>   Diagnostic language\n"
        "  --locale-file <file>  External locale JSON map\n"
        "  --color <auto|always|never>  Diagnostic color mode\n"
        "  --Werror         Treat warnings as errors\n"
        "  --warn-level <n> Warning level (0 disables warnings)\n"
        "\n"
        "Arguments after '--' are forwarded to the compiled program.\n"
        "\n"
        "To run bytecode via KinalVM instead, use 'kinal vm run'.\n";
    kn_write_str(text);
}

static void print_vm_help(void)
{
    const char *text =
        "Usage: kinal vm <subcommand> [options] [args...]\n"
        "\n"
        "Bytecode operations.\n"
        "\n"
        "Subcommands:\n"
        "  build   Compile source files to .knc bytecode\n"
        "  run     Execute .kn source or .knc bytecode via KinalVM\n"
        "  pack    Bundle source or .knc into a standalone vm-exe\n"
        "\n"
        "Use \"kinal vm <subcommand> --help\" for details.\n";
    kn_write_str(text);
}

static void print_pkg_help(void)
{
    const char *text =
        "Usage: kinal pkg <subcommand> [options] [args...]\n"
        "\n"
        "Package management.\n"
        "\n"
        "Subcommands:\n"
        "  build   Build a .klib from a package manifest\n"
        "  info    Show .klib metadata\n"
        "  unpack  Extract a .klib into a directory\n"
        "\n"
        "Use \"kinal pkg <subcommand> --help\" for details.\n";
    kn_write_str(text);
}

static int parse_linker_kind(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "lld")) { *out = KN_LINKER_LLD; return 1; }
    if (str_eq_ci(text, "zig")) { *out = KN_LINKER_ZIG; return 1; }
    if (str_eq_ci(text, "msvc")) { *out = KN_LINKER_MSVC; return 1; }
    return 0;
}

static int parse_artifact_kind(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "exe")) { *out = KN_ARTIFACT_EXE; return 1; }
    if (str_eq_ci(text, "static") || str_eq_ci(text, "staticlib") || str_eq_ci(text, "lib")) { *out = KN_ARTIFACT_STATIC_LIB; return 1; }
    if (str_eq_ci(text, "shared") || str_eq_ci(text, "dll") || str_eq_ci(text, "so")) { *out = KN_ARTIFACT_SHARED_LIB; return 1; }
    return 0;
}

static int parse_emit_mode(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "bin") || str_eq_ci(text, "link") || str_eq_ci(text, "exe"))
    {
        *out = KN_EMIT_LINK;
        return 1;
    }
    if (str_eq_ci(text, "obj") || str_eq_ci(text, "object"))
    {
        *out = KN_EMIT_OBJECT;
        return 1;
    }
    if (str_eq_ci(text, "asm") || str_eq_ci(text, "assembly") || str_eq_ci(text, "s"))
    {
        *out = KN_EMIT_ASSEMBLY;
        return 1;
    }
    if (str_eq_ci(text, "llvm-ir") || str_eq_ci(text, "llvm") || str_eq_ci(text, "ir"))
    {
        *out = KN_EMIT_LLVM_IR;
        return 1;
    }
    if (str_eq_ci(text, "knc") || str_eq_ci(text, "bytecode"))
    {
        *out = KN_EMIT_KNC;
        return 1;
    }
    if (str_eq_ci(text, "vm-exe") || str_eq_ci(text, "vm-bundle") || str_eq_ci(text, "standalone") ||
        str_eq_ci(text, "knc-exe") || str_eq_ci(text, "knc-bundle"))
    {
        *out = KN_EMIT_KNC_EXE;
        return 1;
    }
    return 0;
}

static int parse_lto_mode(const char *text, int *out)
{
    if (!out) return 0;
    if (!text || !text[0] || str_eq_ci(text, "full") || str_eq_ci(text, "on"))
    {
        *out = KN_LTO_FULL;
        return 1;
    }
    if (str_eq_ci(text, "off") || str_eq_ci(text, "none"))
    {
        *out = KN_LTO_OFF;
        return 1;
    }
    if (str_eq_ci(text, "thin"))
    {
        *out = KN_LTO_THIN;
        return 1;
    }
    return 0;
}

static int parse_color_mode(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "auto")) { *out = KN_COLOR_AUTO; return 1; }
    if (str_eq_ci(text, "always")) { *out = KN_COLOR_ALWAYS; return 1; }
    if (str_eq_ci(text, "never")) { *out = KN_COLOR_NEVER; return 1; }
    return 0;
}

static int parse_env_kind(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "hosted")) { *out = KN_ENV_HOSTED; return 1; }
    if (str_eq_ci(text, "freestanding") || str_eq_ci(text, "bare")) { *out = KN_ENV_FREESTANDING; return 1; }
    return 0;
}

static int parse_runtime_mode(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "none") || str_eq_ci(text, "core")) { *out = KN_RUNTIME_NONE; return 1; }
    if (str_eq_ci(text, "alloc")) { *out = KN_RUNTIME_ALLOC; return 1; }
    if (str_eq_ci(text, "gc")) { *out = KN_RUNTIME_GC; return 1; }
    return 0;
}

static int parse_panic_mode(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "trap") || str_eq_ci(text, "abort")) { *out = KN_PANIC_TRAP; return 1; }
    if (str_eq_ci(text, "loop") || str_eq_ci(text, "halt")) { *out = KN_PANIC_LOOP; return 1; }
    return 0;
}

static int parse_crt_mode(const char *text, int *out)
{
    if (!text || !out) return 0;
    if (str_eq_ci(text, "auto")) { *out = KN_CRT_AUTO; return 1; }
    if (str_eq_ci(text, "dynamic") || str_eq_ci(text, "shared")) { *out = KN_CRT_DYNAMIC; return 1; }
    if (str_eq_ci(text, "static")) { *out = KN_CRT_STATIC; return 1; }
    return 0;
}

static const char *resolve_default_locale_path(const char *lang)
{
    static char path[KN_MAX_PATH];
    path[0] = 0;

    if (!lang || (!str_eq_ci(lang, "zh") && !str_eq_ci(lang, "zh-CN")))
        return 0;

    char exe_dir[KN_MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (path_join(path, sizeof(path), exe_dir, "locales\\zh-CN.json") && file_exists(path))
            return path;
    }
    if (file_exists("locales\\zh-CN.json"))
    {
        copy_str(path, sizeof(path), "locales\\zh-CN.json");
        return path;
    }
    return 0;
}

static const char *default_freestanding_target(void)
{
#if defined(__aarch64__)
    return "aarch64-unknown-none-elf";
#else
    return "x86_64-unknown-none-elf";
#endif
}

static void make_default_output(char *out, size_t cap, int emit_mode, int artifact_kind, int env_kind, const char *target, const char *primary_input)
{
    char stem[KN_MAX_PATH];
    stem[0] = 0;
    if (primary_input && primary_input[0])
    {
        size_t len = kn_strlen(primary_input);
        size_t start = len;
        while (start > 0 && primary_input[start - 1] != '\\' && primary_input[start - 1] != '/')
            start--;
        const char *base = primary_input + start;
        size_t bn = kn_strlen(base);
        if (bn >= sizeof(stem))
            bn = sizeof(stem) - 1;
        kn_memcpy(stem, base, bn);
        stem[bn] = 0;
        for (size_t i = bn; i > 0; i--)
        {
            if (stem[i - 1] == '.')
            {
                stem[i - 1] = 0;
                break;
            }
        }
    }
    if (!stem[0])
        copy_str(stem, sizeof(stem), "main");

    if (!out || cap == 0) return;
    out[0] = 0;
    size_t off = 0;
    if (emit_mode == KN_EMIT_OBJECT)
    {
        kn_append(out, cap, &off, stem);
        kn_append(out, cap, &off, target_is_windows(target) ? ".obj" : ".o");
        return;
    }
    if (emit_mode == KN_EMIT_KNC)
    {
        kn_append(out, cap, &off, stem);
        kn_append(out, cap, &off, ".knc");
        return;
    }
    if (emit_mode == KN_EMIT_KNC_EXE)
    {
        kn_append(out, cap, &off, stem);
        if (target_is_windows(target))
            kn_append(out, cap, &off, ".exe");
        return;
    }
    if (emit_mode == KN_EMIT_ASSEMBLY)
    {
        kn_append(out, cap, &off, stem);
        kn_append(out, cap, &off, ".s");
        return;
    }
    if (emit_mode == KN_EMIT_LLVM_IR)
    {
        kn_append(out, cap, &off, stem);
        kn_append(out, cap, &off, ".ll");
        return;
    }

    if (artifact_kind == KN_ARTIFACT_STATIC_LIB)
    {
        if (target_is_windows(target))
        {
            kn_append(out, cap, &off, stem);
            kn_append(out, cap, &off, ".lib");
        }
        else
        {
            kn_append(out, cap, &off, "lib");
            kn_append(out, cap, &off, stem);
            kn_append(out, cap, &off, ".a");
        }
        return;
    }
    if (artifact_kind == KN_ARTIFACT_SHARED_LIB)
    {
        if (target_is_windows(target))
        {
            kn_append(out, cap, &off, stem);
            kn_append(out, cap, &off, ".dll");
        }
        else
        {
            kn_append(out, cap, &off, "lib");
            kn_append(out, cap, &off, stem);
            kn_append(out, cap, &off, ".so");
        }
        return;
    }
    kn_append(out, cap, &off, stem);
    if (env_kind == KN_ENV_FREESTANDING)
        kn_append(out, cap, &off, ".elf");
    else if (target_is_windows(target))
        kn_append(out, cap, &off, ".exe");
}

/* Forward declaration — the core subcommand entry points. */
static int kn_build_main(int argc, char **argv);
static int kn_run_main(int argc, char **argv);
static int kn_vm_main(int argc, char **argv);
static int kn_pkg_main(int argc, char **argv);

static int kn_main(int argc, char **argv)
{
    const char *lang = "en";
    const char *locale_file = 0;
    int i = 1;

    while (i < argc)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "--lang") == 0 && i + 1 < argc) { lang = argv[i + 1]; i += 2; continue; }
        if (kn_strcmp(arg, "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[i + 1]; i += 2; continue; }
        break;
    }

    kn_diag_reset();
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);

    if (argc < 2)
    {
        print_help();
        return 1;
    }
    if (i < argc && (kn_strcmp(argv[i], "-h") == 0 || kn_strcmp(argv[i], "--help") == 0))
    {
        print_help();
        return 0;
    }
    if (i < argc && (kn_strcmp(argv[i], "-V") == 0 || kn_strcmp(argv[i], "--version") == 0))
    {
        int verbose = (i + 1 < argc && kn_strcmp(argv[i + 1], "--verbose") == 0);
        print_version(verbose);
        return 0;
    }
    if (i < argc && argv[i][0] == '-')
    {
        kn_write_str("unknown top-level option: ");
        kn_write_str(argv[i]);
        kn_write_str("\n");
        print_help();
        return 2;
    }

    if (i >= argc)
    {
        print_help();
        return 1;
    }

    if (kn_strcmp(argv[i], "fmt") == 0)
        return kn_fmt_main(argc - i, argv + i);
    if (kn_strcmp(argv[i], "build") == 0)
        return kn_build_main(argc - i, argv + i);
    if (kn_strcmp(argv[i], "run") == 0)
        return kn_run_main(argc - i, argv + i);
    if (kn_strcmp(argv[i], "vm") == 0)
        return kn_vm_main(argc - i, argv + i);
    if (kn_strcmp(argv[i], "pkg") == 0)
        return kn_pkg_main(argc - i, argv + i);

    kn_write_str("unknown subcommand: ");
    kn_write_str(argv[i]);
    kn_write_str("\n");
    print_help();
    return 2;
}

/* ================================================================
 *  kinal build — the primary build subcommand.
 *  Delegates to the same kn_compile() but enforces subcommand-clean
 *  argument parsing (no --vm, no --emit knc/vm-exe, no pkg ops).
 * ================================================================ */
static int kn_build_main(int argc, char **argv)
{
    const char *lang = "en";
    const char *locale_file = 0;
    for (int i = 1; i < argc; i++)
    {
        if (kn_strcmp(argv[i], "--lang") == 0 && i + 1 < argc) { lang = argv[++i]; continue; }
        if (kn_strcmp(argv[i], "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[++i]; continue; }
    }
    kn_diag_reset();
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);

    if (argc < 2) { print_build_help(); return 1; }

    const char *out = 0;
    const char *inputs[64];
    KnLinkItem raw_link_items[256];
    int raw_link_item_count = 0;
    KnLinkItem link_items[512];
    char link_item_storage[512][KN_MAX_PATH];
    int link_item_storage_count = 0;
    int in_count = 0;
    int auto_link = 1;
    int trace = 0;
    int profile = 0;
    int emit_mode = KN_EMIT_LINK;
    int artifact_kind = KN_ARTIFACT_EXE;
    int linker_kind = KN_LINKER_LLD;
    const char *linker_path = 0;
    int linker_explicit = 0;
    int lto_mode = KN_LTO_OFF;
    int color_mode = KN_COLOR_AUTO;
    int warn_as_error = 0;
    int warn_level = 1;
    int keep_temps = 0;
    const char *dump_locale_en = 0;
    const char *target_raw = default_target_triple();
    int artifact_explicit = 0;
    int target_explicit = 0;
    int env_kind = KN_ENV_HOSTED;
    int runtime_mode = KN_RUNTIME_GC;
    int panic_mode = KN_PANIC_TRAP;
    int crt_mode = KN_CRT_AUTO;
    const char *entry_symbol = 0;
    const char *link_script = 0;
    int no_crt = 0;
    int no_default_libs = 0;
    int env_explicit = 0;
    int runtime_explicit = 0;
    int panic_explicit = 0;
    int crt_explicit = 0;
    int freestanding_shortcut = 0;
    int show_link = 0;
    int show_link_search = 0;
    const char *project_manifest = 0;
    StrList pkg_roots = {0};
    StrList stdpkg_roots = {0};

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) { print_build_help(); return 0; }
        if (kn_strcmp(arg, "--no-module-discovery") == 0) { auto_link = 0; continue; }
        if (kn_strcmp(arg, "--trace") == 0) { trace = 1; continue; }
        if (kn_strcmp(arg, "--time") == 0) { profile = 1; continue; }
        if (kn_strcmp(arg, "--show-link") == 0) { show_link = 1; continue; }
        if (kn_strcmp(arg, "--show-link-search") == 0) { show_link_search = 1; continue; }
        if (kn_strcmp(arg, "--keep-temps") == 0) { keep_temps = 1; continue; }
        if (kn_strcmp(arg, "--Werror") == 0) { warn_as_error = 1; continue; }
        if (kn_strcmp(arg, "--warn-level") == 0 && i + 1 < argc) {
            warn_level = 0; const char *w = argv[++i];
            while (*w >= '0' && *w <= '9') { warn_level = warn_level * 10 + (*w - '0'); w++; }
            continue;
        }
        if (kn_strcmp(arg, "--emit") == 0 && i + 1 < argc) {
            if (!parse_emit_mode(argv[i + 1], &emit_mode)) {
                kn_write_str("unknown emit mode: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1;
            }
            if (emit_mode == KN_EMIT_KNC || emit_mode == KN_EMIT_KNC_EXE) {
                kn_write_str("'kinal build' does not support --emit knc/vm-exe; use 'kinal vm build' or 'kinal vm pack'\n");
                return 2;
            }
            i++; continue;
        }
        if (kn_strcmp(arg, "--freestanding") == 0) {
            freestanding_shortcut = 1; env_kind = KN_ENV_FREESTANDING;
            runtime_mode = KN_RUNTIME_NONE; no_crt = 1; no_default_libs = 1;
            env_explicit = 1; runtime_explicit = 1; continue;
        }
        if (kn_strcmp(arg, "--color") == 0 && i + 1 < argc) {
            if (!parse_color_mode(argv[i + 1], &color_mode)) {
                kn_write_str("unknown color mode: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1;
            }
            i++; continue;
        }
        if (kn_strcmp(arg, "--lang") == 0 && i + 1 < argc) { lang = argv[++i]; continue; }
        if (kn_strcmp(arg, "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[++i]; continue; }
        if (kn_strcmp(arg, "--dump-locale-en") == 0 && i + 1 < argc) { dump_locale_en = argv[++i]; continue; }
        if (kn_strcmp(arg, "--project") == 0 && i + 1 < argc) { project_manifest = argv[++i]; continue; }
        if (kn_strcmp(arg, "--pkg-root") == 0 && i + 1 < argc) { append_root_list(&pkg_roots, argv[++i]); continue; }
        if (kn_strcmp(arg, "--stdpkg-root") == 0 && i + 1 < argc) { append_root_list(&stdpkg_roots, argv[++i]); continue; }
        if (kn_strcmp(arg, "--lto") == 0) { lto_mode = KN_LTO_FULL; continue; }
        if (kn_strncmp(arg, "--lto=", 6) == 0) {
            if (!parse_lto_mode(arg + 6, &lto_mode)) {
                kn_write_str("unknown lto mode: "); kn_write_str(arg + 6); kn_write_str("\n"); return 1;
            }
            continue;
        }
        if ((kn_strcmp(arg, "-o") == 0 || kn_strcmp(arg, "--output") == 0) && i + 1 < argc) { out = argv[++i]; continue; }
        if ((kn_strcmp(arg, "-L") == 0 || kn_strcmp(arg, "--lib-dir") == 0) && i + 1 < argc) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_LIB_DIR, argv[++i])) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if (kn_strncmp(arg, "-L", 2) == 0 && arg[2]) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_LIB_DIR, arg + 2)) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if ((kn_strcmp(arg, "-l") == 0 || kn_strcmp(arg, "--lib") == 0) && i + 1 < argc) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_LIB_NAME, argv[++i])) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if (kn_strncmp(arg, "-l", 2) == 0 && arg[2]) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_LIB_NAME, arg + 2)) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if (kn_strcmp(arg, "--link-root") == 0 && i + 1 < argc) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_ROOT, argv[++i])) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if (kn_strcmp(arg, "--link-arg") == 0 && i + 1 < argc) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_ARG, argv[++i])) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if (kn_strcmp(arg, "--env") == 0 && i + 1 < argc) {
            if (!parse_env_kind(argv[i + 1], &env_kind)) { kn_write_str("unknown environment: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1; }
            env_explicit = 1; i++; continue;
        }
        if (kn_strcmp(arg, "--runtime") == 0 && i + 1 < argc) {
            if (!parse_runtime_mode(argv[i + 1], &runtime_mode)) { kn_write_str("unknown runtime mode: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1; }
            runtime_explicit = 1; i++; continue;
        }
        if (kn_strcmp(arg, "--panic") == 0 && i + 1 < argc) {
            if (!parse_panic_mode(argv[i + 1], &panic_mode)) { kn_write_str("unknown panic mode: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1; }
            panic_explicit = 1; i++; continue;
        }
        if (kn_strcmp(arg, "--crt") == 0 && i + 1 < argc) {
            if (!parse_crt_mode(argv[i + 1], &crt_mode)) { kn_write_str("unknown crt mode: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1; }
            crt_explicit = 1; i++; continue;
        }
        if (kn_strcmp(arg, "--target") == 0 && i + 1 < argc) { target_raw = argv[++i]; target_explicit = 1; continue; }
        if (kn_strcmp(arg, "--entry") == 0 && i + 1 < argc) { entry_symbol = argv[++i]; continue; }
        if (kn_strcmp(arg, "--link-script") == 0 && i + 1 < argc) { link_script = argv[++i]; continue; }
        if (kn_strcmp(arg, "--linker") == 0 && i + 1 < argc) {
            if (!parse_linker_kind(argv[i + 1], &linker_kind)) { kn_write_str("unknown linker: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1; }
            linker_explicit = 1; i++; continue;
        }
        if (kn_strcmp(arg, "--linker-path") == 0 && i + 1 < argc) { linker_path = argv[++i]; continue; }
        if (kn_strcmp(arg, "--no-crt") == 0) { no_crt = 1; continue; }
        if (kn_strcmp(arg, "--no-default-libs") == 0) { no_default_libs = 1; continue; }
        if (kn_strcmp(arg, "--kind") == 0 && i + 1 < argc) {
            if (!parse_artifact_kind(argv[i + 1], &artifact_kind)) { kn_write_str("unknown kind: "); kn_write_str(argv[i + 1]); kn_write_str("\n"); return 1; }
            artifact_explicit = 1; i++; continue;
        }
        if (kn_strcmp(arg, "--link-file") == 0 && i + 1 < argc) {
            if (!link_item_add(raw_link_items, &raw_link_item_count, (int)(sizeof(raw_link_items)/sizeof(raw_link_items[0])), KN_CLI_LINK_FILE, argv[++i])) { kn_write_str("too many link items\n"); return 1; }
            continue;
        }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); print_build_help(); return 2; }
        if (in_count < (int)(sizeof(inputs)/sizeof(inputs[0]))) inputs[in_count++] = arg;
    }

    if (dump_locale_en && dump_locale_en[0] && in_count == 0) {
        kn_diag_reset();
        if (kn_diag_export_english_template(dump_locale_en) != 0) { kn_write_str("failed to export locale template\n"); return 1; }
        return 0;
    }

    if (project_manifest) {
        KnPkgManifest manifest;
        char manifest_path[KN_MAX_PATH], manifest_entry[KN_MAX_PATH], manifest_out[KN_MAX_PATH], local_pkg_root[KN_MAX_PATH];
        if (!resolve_manifest_input_path(project_manifest, 1, manifest_path, sizeof(manifest_path))) {
            kn_write_str("invalid manifest path: "); kn_write_str(project_manifest); kn_write_str("\n"); return 1;
        }
        if (!parse_pkg_manifest(manifest_path, &manifest)) {
            kn_write_str("invalid manifest: "); kn_write_str(manifest_path); kn_write_str("\n"); return 1;
        }
        if (!manifest.is_project) { kn_write_str("--project expects a project manifest\n"); return 1; }
        for (int pr = 0; pr < manifest.package_root_count; pr++) {
            char resolved_root[KN_MAX_PATH];
            if (manifest_resolve_path(&manifest, manifest.package_roots[pr], resolved_root, sizeof(resolved_root)))
                append_root_list(&pkg_roots, resolved_root);
        }
        if (path_join(local_pkg_root, sizeof(local_pkg_root), manifest.manifest_dir, "kpkg"))
            append_root_list(&pkg_roots, local_pkg_root);
        if (!out && manifest.out_path[0] && manifest_resolve_path(&manifest, manifest.out_path, manifest_out, sizeof(manifest_out)))
            out = dup_str(manifest_out);
        if (in_count == 0 && manifest.entry[0] && manifest_resolve_path(&manifest, manifest.entry, manifest_entry, sizeof(manifest_entry)))
            inputs[in_count++] = dup_str(manifest_entry);
    }
    collect_default_official_package_roots(&stdpkg_roots);

    if (in_count == 0) { kn_write_str("no input files\n"); return 1; }
    if (linker_path && linker_path[0] && !linker_explicit) { kn_write_str("--linker-path requires --linker <lld|zig|msvc>\n"); return 1; }
    if (emit_mode != KN_EMIT_LINK && (show_link || show_link_search)) { kn_write_str("--show-link requires link mode\n"); return 1; }
    if (freestanding_shortcut && !target_explicit) target_raw = default_freestanding_target();

    const char *target_triple = normalize_target_triple(target_raw);
    if (!target_triple || !target_triple[0]) target_triple = default_target_triple();
    if (!env_explicit && target_is_freestanding(target_triple)) env_kind = KN_ENV_FREESTANDING;
    if (emit_mode != KN_EMIT_LINK && artifact_explicit) { kn_write_str("--kind is only valid when linking\n"); return 1; }

    if (env_kind == KN_ENV_HOSTED) {
        runtime_mode = KN_RUNTIME_GC;
        if (runtime_explicit || panic_explicit || no_crt || link_script) { kn_write_str("hosted builds do not accept freestanding-only options\n"); return 1; }
    } else {
        if (crt_explicit) { kn_write_str("freestanding builds do not accept hosted CRT options\n"); return 1; }
        if (!runtime_explicit && !freestanding_shortcut) runtime_mode = KN_RUNTIME_NONE;
        if (!entry_symbol || !entry_symbol[0]) entry_symbol = "KMain";
        if (!linker_explicit) linker_kind = KN_LINKER_LLD;
    }
    if (env_kind == KN_ENV_HOSTED && !linker_explicit) linker_kind = default_hosted_linker_kind(target_triple);
    if (env_kind == KN_ENV_HOSTED && (!entry_symbol || !entry_symbol[0])) entry_symbol = "Main";
    if (env_kind == KN_ENV_FREESTANDING && artifact_kind == KN_ARTIFACT_SHARED_LIB) { kn_write_str("freestanding does not support shared libraries\n"); return 1; }

    int link_item_count = 0;
    for (int li = 0; li < raw_link_item_count; li++) {
        const KnLinkItem *raw = &raw_link_items[li];
        if (!raw->value || !raw->value[0]) continue;
        if (raw->kind == KN_CLI_LINK_ROOT) {
            if (!expand_link_root(link_items, &link_item_count, (int)(sizeof(link_items)/sizeof(link_items[0])),
                                  link_item_storage, &link_item_storage_count, (int)(sizeof(link_item_storage)/sizeof(link_item_storage[0])),
                                  raw->value, target_triple))
            { kn_write_str("failed to expand link root: "); kn_write_str(raw->value); kn_write_str("\n"); return 1; }
            continue;
        }
        if (!link_item_add(link_items, &link_item_count, (int)(sizeof(link_items)/sizeof(link_items[0])), raw->kind, raw->value))
        { kn_write_str("too many link items\n"); return 1; }
    }

    char out_default[KN_MAX_PATH];
    if (!out) {
        make_default_output(out_default, sizeof(out_default), emit_mode, artifact_kind, env_kind, target_triple, (in_count > 0) ? inputs[0] : 0);
        out = out_default;
    }

    kn_diag_reset();
    kn_diag_set_color_mode(color_mode);
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);
    kn_diag_set_warn_as_error(warn_as_error);
    kn_diag_set_warning_level(warn_level);
    if (env_kind == KN_ENV_HOSTED) kn_std_set_profile(KN_STD_PROFILE_HOSTED);
    else if (runtime_mode == KN_RUNTIME_NONE) kn_std_set_profile(KN_STD_PROFILE_FREESTANDING_CORE);
    else if (runtime_mode == KN_RUNTIME_ALLOC) kn_std_set_profile(KN_STD_PROFILE_FREESTANDING_ALLOC);
    else kn_std_set_profile(KN_STD_PROFILE_FREESTANDING_GC);

    KnCompileConfig cfg;
    kn_memset(&cfg, 0, sizeof(cfg));
    cfg.out_path = out;
    cfg.auto_link = auto_link;
    cfg.trace = trace;
    cfg.profile = profile;
    cfg.package_roots = pkg_roots.items;
    cfg.package_root_count = pkg_roots.count;
    cfg.official_package_roots = stdpkg_roots.items;
    cfg.official_package_root_count = stdpkg_roots.count;
    cfg.link_items = link_items;
    cfg.link_item_count = link_item_count;
    cfg.target_triple = target_triple;
    cfg.linker_kind = linker_kind;
    cfg.linker_path = linker_path;
    cfg.artifact_kind = artifact_kind;
    cfg.emit_mode = emit_mode;
    cfg.lto_mode = lto_mode;
    cfg.keep_temps = keep_temps;
    cfg.env_kind = env_kind;
    cfg.runtime_mode = runtime_mode;
    cfg.panic_mode = panic_mode;
    cfg.crt_mode = crt_mode;
    cfg.no_crt = no_crt;
    cfg.no_default_libs = no_default_libs;
    cfg.entry_symbol = entry_symbol;
    cfg.link_script = link_script;
    cfg.show_link = show_link;
    cfg.show_link_search = show_link_search;
    cfg.knc_superloop = 1;

    int c_rc = kn_compile(in_count, inputs, &cfg);
    kn_diag_print_summary();
    if (dump_locale_en && dump_locale_en[0]) {
        if (kn_diag_export_english_template(dump_locale_en) != 0) { kn_write_str("failed to export locale template\n"); return 1; }
    }
    return c_rc == 0 ? 0 : 1;
}

/* ================================================================
 *  kinal run — compile to native exe and execute immediately.
 * ================================================================ */
static int kn_run_main(int argc, char **argv)
{
    const char *lang = "en";
    const char *locale_file = 0;
    int auto_link = 1;
    int keep_temps = 0;
    int color_mode = KN_COLOR_AUTO;
    int warn_as_error = 0;
    int warn_level = 1;
    const char *input = 0;
    const char *prog_args[256];
    int prog_argc = 0;
    int dashdash = 0;
    StrList pkg_roots = {0};
    StrList stdpkg_roots = {0};

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (dashdash)
        {
            /* Everything after -- is forwarded to the compiled program */
            if (prog_argc < (int)(sizeof(prog_args)/sizeof(prog_args[0])))
                prog_args[prog_argc++] = arg;
            continue;
        }
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) { print_run_help(); return 0; }
        if (kn_strcmp(arg, "--no-module-discovery") == 0) { auto_link = 0; continue; }
        if (kn_strcmp(arg, "--keep-temps") == 0) { keep_temps = 1; continue; }
        if (kn_strcmp(arg, "--lang") == 0 && i + 1 < argc) { lang = argv[++i]; continue; }
        if (kn_strcmp(arg, "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[++i]; continue; }
        if (kn_strcmp(arg, "--color") == 0 && i + 1 < argc) {
            if (!parse_color_mode(argv[i + 1], &color_mode)) { kn_write_str("unknown color mode\n"); return 1; }
            i++; continue;
        }
        if (kn_strcmp(arg, "--Werror") == 0) { warn_as_error = 1; continue; }
        if (kn_strcmp(arg, "--warn-level") == 0 && i + 1 < argc) {
            warn_level = 0; const char *w = argv[++i];
            while (*w >= '0' && *w <= '9') { warn_level = warn_level * 10 + (*w - '0'); w++; }
            continue;
        }
        if (kn_strcmp(arg, "--pkg-root") == 0 && i + 1 < argc) { append_root_list(&pkg_roots, argv[++i]); continue; }
        if (kn_strcmp(arg, "--stdpkg-root") == 0 && i + 1 < argc) { append_root_list(&stdpkg_roots, argv[++i]); continue; }
        if (kn_strcmp(arg, "--") == 0) { dashdash = 1; continue; }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); print_run_help(); return 2; }
        if (!input) input = arg;
        else { kn_write_str("'kinal run' accepts exactly one input file\n"); return 2; }
    }

    if (!input) { kn_write_str("no input file\n"); print_run_help(); return 1; }

    kn_diag_reset();
    kn_diag_set_color_mode(color_mode);
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);
    kn_diag_set_warn_as_error(warn_as_error);
    kn_diag_set_warning_level(warn_level);
    kn_std_set_profile(KN_STD_PROFILE_HOSTED);

    KnInputKind kind = classify_input(input);
    if (kind == KN_INPUT_KNC) {
        kn_write_str("'kinal run' no longer runs .knc bytecode; use 'kinal vm run' instead\n");
        return 2;
    }
    if (kind != KN_INPUT_FX) {
        kn_write_str("'kinal run' requires a .kn/.kinal source file\n");
        return 1;
    }

    /* Compile to a temp native executable */
    char temp_exe[KN_MAX_PATH];
    if (!derive_temp_run_exe_path(input, temp_exe, sizeof(temp_exe))) {
        kn_write_str("failed to create temporary output path\n"); return 1;
    }

    collect_default_official_package_roots(&stdpkg_roots);

    KnCompileConfig cfg;
    kn_memset(&cfg, 0, sizeof(cfg));
    cfg.out_path = temp_exe;
    cfg.auto_link = auto_link;
    cfg.emit_mode = KN_EMIT_LINK;
    cfg.artifact_kind = KN_ARTIFACT_EXE;
    cfg.target_triple = default_target_triple();
    cfg.linker_kind = default_hosted_linker_kind(cfg.target_triple);
    cfg.env_kind = KN_ENV_HOSTED;
    cfg.runtime_mode = KN_RUNTIME_GC;
    cfg.panic_mode = KN_PANIC_TRAP;
    cfg.crt_mode = KN_CRT_AUTO;
    cfg.entry_symbol = "Main";
    cfg.package_roots = pkg_roots.items;
    cfg.package_root_count = pkg_roots.count;
    cfg.official_package_roots = stdpkg_roots.items;
    cfg.official_package_root_count = stdpkg_roots.count;

    const char *inputs[1];
    inputs[0] = input;
    int c_rc = kn_compile(1, inputs, &cfg);
    kn_diag_print_summary();
    if (c_rc != 0) {
        if (!keep_temps) try_delete_file(temp_exe);
        return 1;
    }

    int prog_rc = run_native_program(temp_exe, prog_argc, prog_args);
    if (!keep_temps) try_delete_file(temp_exe);
    if (prog_rc < 0) { kn_write_str("failed to launch compiled program\n"); return 1; }
    return prog_rc;
}

/* ================================================================
 *  kinal vm — bytecode subcommand group.
 * ================================================================ */
static int kn_vm_build_main(int argc, char **argv);
static int kn_vm_run_main(int argc, char **argv);
static int kn_vm_pack_main(int argc, char **argv);

static int kn_vm_main(int argc, char **argv)
{
    if (argc < 2) { print_vm_help(); return 1; }
    if (kn_strcmp(argv[1], "-h") == 0 || kn_strcmp(argv[1], "--help") == 0) { print_vm_help(); return 0; }
    if (kn_strcmp(argv[1], "build") == 0) return kn_vm_build_main(argc - 1, argv + 1);
    if (kn_strcmp(argv[1], "run") == 0) return kn_vm_run_main(argc - 1, argv + 1);
    if (kn_strcmp(argv[1], "pack") == 0) return kn_vm_pack_main(argc - 1, argv + 1);
    kn_write_str("unknown vm subcommand: "); kn_write_str(argv[1]); kn_write_str("\n");
    print_vm_help();
    return 2;
}

static int kn_vm_build_main(int argc, char **argv)
{
    const char *out = 0;
    const char *input = 0;
    int auto_link = 1;
    int knc_superloop = 1;
    int color_mode = KN_COLOR_AUTO;
    const char *lang = "en";
    const char *locale_file = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) {
            kn_write_str("Usage: kinal vm build [options] <file.kn>\n\n"
                "  -o, --output <file>  Output .knc path\n"
                "  --no-superloop       Disable superloop fusion\n"
                "  --no-module-discovery  Disable auto-discovery\n");
            return 0;
        }
        if ((kn_strcmp(arg, "-o") == 0 || kn_strcmp(arg, "--output") == 0) && i + 1 < argc) { out = argv[++i]; continue; }
        if (kn_strcmp(arg, "--no-superloop") == 0) { knc_superloop = 0; continue; }
        if (kn_strcmp(arg, "--superloop") == 0) { knc_superloop = 1; continue; }
        if (kn_strcmp(arg, "--no-module-discovery") == 0) { auto_link = 0; continue; }
        if (kn_strcmp(arg, "--color") == 0 && i + 1 < argc) { parse_color_mode(argv[i + 1], &color_mode); i++; continue; }
        if (kn_strcmp(arg, "--lang") == 0 && i + 1 < argc) { lang = argv[++i]; continue; }
        if (kn_strcmp(arg, "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[++i]; continue; }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); return 2; }
        if (!input) input = arg;
        else { kn_write_str("'kinal vm build' accepts exactly one input file\n"); return 2; }
    }

    if (!input) { kn_write_str("no input file\n"); return 1; }

    kn_diag_reset();
    kn_diag_set_color_mode(color_mode);
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);
    kn_std_set_profile(KN_STD_PROFILE_HOSTED);

    char out_default[KN_MAX_PATH];
    if (!out) {
        make_default_output(out_default, sizeof(out_default), KN_EMIT_KNC, KN_ARTIFACT_EXE, KN_ENV_HOSTED, default_target_triple(), input);
        out = out_default;
    }

    KnCompileConfig cfg;
    kn_memset(&cfg, 0, sizeof(cfg));
    cfg.out_path = out;
    cfg.auto_link = auto_link;
    cfg.emit_mode = KN_EMIT_KNC;
    cfg.artifact_kind = KN_ARTIFACT_EXE;
    cfg.target_triple = default_target_triple();
    cfg.linker_kind = default_hosted_linker_kind(cfg.target_triple);
    cfg.env_kind = KN_ENV_HOSTED;
    cfg.runtime_mode = KN_RUNTIME_GC;
    cfg.entry_symbol = "Main";
    cfg.knc_superloop = knc_superloop;

    const char *inputs[1];
    inputs[0] = input;
    int rc = kn_compile(1, inputs, &cfg);
    kn_diag_print_summary();
    return rc == 0 ? 0 : 1;
}

static int kn_vm_run_main(int argc, char **argv)
{
    const char *input = 0;
    const char *vm_path = 0;
    int knc_superloop = 1;
    int auto_link = 1;
    int keep_temps = 0;
    int color_mode = KN_COLOR_AUTO;
    const char *lang = "en";
    const char *locale_file = 0;
    int warn_as_error = 0;
    int warn_level = 1;
    const char *prog_args[256];
    int prog_argc = 0;
    int dashdash = 0;
    StrList pkg_roots = {0};
    StrList stdpkg_roots = {0};

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (dashdash)
        {
            if (prog_argc < (int)(sizeof(prog_args) / sizeof(prog_args[0])))
                prog_args[prog_argc++] = arg;
            continue;
        }
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) {
            kn_write_str("Usage: kinal vm run [options] <file.kn|file.knc> [-- program-args...]\n\n"
                "  --vm-path <file>  Explicit kinalvm path\n"
                "  --no-superloop    Disable superloop when compiling source to .knc\n"
                "  --no-module-discovery  Disable auto-discovery when compiling source\n"
                "  --keep-temps      Keep the temporary .knc file\n"
                "  --pkg-root <dir>  Add a local package root\n"
                "  --stdpkg-root <dir>  Add an official stdpkg root override\n");
            return 0;
        }
        if (kn_strcmp(arg, "--vm-path") == 0 && i + 1 < argc) { vm_path = argv[++i]; continue; }
        if (kn_strcmp(arg, "--no-superloop") == 0) { knc_superloop = 0; continue; }
        if (kn_strcmp(arg, "--superloop") == 0) { knc_superloop = 1; continue; }
        if (kn_strcmp(arg, "--no-module-discovery") == 0) { auto_link = 0; continue; }
        if (kn_strcmp(arg, "--keep-temps") == 0) { keep_temps = 1; continue; }
        if (kn_strcmp(arg, "--pkg-root") == 0 && i + 1 < argc) { append_root_list(&pkg_roots, argv[++i]); continue; }
        if (kn_strcmp(arg, "--stdpkg-root") == 0 && i + 1 < argc) { append_root_list(&stdpkg_roots, argv[++i]); continue; }
        if (kn_strcmp(arg, "--lang") == 0 && i + 1 < argc) { lang = argv[++i]; continue; }
        if (kn_strcmp(arg, "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[++i]; continue; }
        if (kn_strcmp(arg, "--color") == 0 && i + 1 < argc) { if (!parse_color_mode(argv[i + 1], &color_mode)) { kn_write_str("unknown color mode\n"); return 1; } i++; continue; }
        if (kn_strcmp(arg, "--Werror") == 0) { warn_as_error = 1; continue; }
        if (kn_strcmp(arg, "--warn-level") == 0 && i + 1 < argc) { warn_level = 0; const char *w = argv[++i]; while (*w >= '0' && *w <= '9') { warn_level = warn_level * 10 + (*w - '0'); w++; } continue; }
        if (kn_strcmp(arg, "--") == 0) { dashdash = 1; continue; }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); return 2; }
        if (!input) input = arg;
        else { kn_write_str("'kinal vm run' accepts exactly one input file\n"); return 2; }
    }

    if (!input) { kn_write_str("no input file\n"); return 1; }

    kn_diag_reset();
    kn_diag_set_color_mode(color_mode);
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);
    kn_diag_set_warn_as_error(warn_as_error);
    kn_diag_set_warning_level(warn_level);
    kn_std_set_profile(KN_STD_PROFILE_HOSTED);

    KnInputKind kind = classify_input(input);

    if (kind == KN_INPUT_FX)
    {
        char temp_knc[KN_MAX_PATH];
        if (!derive_temp_knc_path(input, temp_knc, sizeof(temp_knc))) {
            kn_write_str("failed to create temporary .knc path\n"); return 1;
        }

        collect_default_official_package_roots(&stdpkg_roots);

        KnCompileConfig cfg;
        kn_memset(&cfg, 0, sizeof(cfg));
        cfg.out_path = temp_knc;
        cfg.auto_link = auto_link;
        cfg.emit_mode = KN_EMIT_KNC;
        cfg.artifact_kind = KN_ARTIFACT_EXE;
        cfg.target_triple = default_target_triple();
        cfg.linker_kind = default_hosted_linker_kind(cfg.target_triple);
        cfg.env_kind = KN_ENV_HOSTED;
        cfg.runtime_mode = KN_RUNTIME_GC;
        cfg.panic_mode = KN_PANIC_TRAP;
        cfg.crt_mode = KN_CRT_AUTO;
        cfg.entry_symbol = "Main";
        cfg.knc_superloop = knc_superloop;
        cfg.package_roots = pkg_roots.items;
        cfg.package_root_count = pkg_roots.count;
        cfg.official_package_roots = stdpkg_roots.items;
        cfg.official_package_root_count = stdpkg_roots.count;

        const char *inputs[1];
        inputs[0] = input;
        int c_rc = kn_compile(1, inputs, &cfg);
        kn_diag_print_summary();
        if (c_rc != 0) {
            if (!keep_temps) try_delete_file(temp_knc);
            return 1;
        }

        char vm_exe[KN_MAX_PATH];
        if (!resolve_vm_executable(vm_exe, sizeof(vm_exe), vm_path)) {
            if (!keep_temps) try_delete_file(temp_knc);
            kn_write_str("failed to locate kinalvm; pass --vm-path <file>\n"); return 1;
        }
        int rc = run_knc_vm_ex(vm_exe, temp_knc, prog_argc, prog_args);
        if (!keep_temps) try_delete_file(temp_knc);
        if (rc < 0) { kn_write_str("failed to launch kinalvm\n"); return 1; }
        return rc;
    }
    if (kind != KN_INPUT_KNC) {
        kn_write_str("'kinal vm run' requires a .kn/.kinal source file or a .knc file\n"); return 1;
    }

    char vm_exe[KN_MAX_PATH];
    if (!resolve_vm_executable(vm_exe, sizeof(vm_exe), vm_path)) {
        kn_write_str("failed to locate kinalvm; pass --vm-path <file>\n"); return 1;
    }
    int rc = run_knc_vm_ex(vm_exe, input, prog_argc, prog_args);
    if (rc < 0) { kn_write_str("failed to launch kinalvm\n"); return 1; }
    return rc;
}

static int kn_vm_pack_main(int argc, char **argv)
{
    const char *out = 0;
    const char *input = 0;
    const char *vm_path = 0;
    const char *target_raw = default_target_triple();
    int color_mode = KN_COLOR_AUTO;
    const char *lang = "en";
    const char *locale_file = 0;
    int auto_link = 1;
    int knc_superloop = 1;
    int keep_temps = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) {
            kn_write_str("Usage: kinal vm pack [options] <file.kn|file.knc>\n\n"
                "  -o, --output <file>  Output executable path\n"
                "  --vm-path <file>     KinalVM executable path\n"
                "  --target <triple>    Target platform\n");
            return 0;
        }
        if ((kn_strcmp(arg, "-o") == 0 || kn_strcmp(arg, "--output") == 0) && i + 1 < argc) { out = argv[++i]; continue; }
        if (kn_strcmp(arg, "--vm-path") == 0 && i + 1 < argc) { vm_path = argv[++i]; continue; }
        if (kn_strcmp(arg, "--target") == 0 && i + 1 < argc) { target_raw = argv[++i]; continue; }
        if (kn_strcmp(arg, "--no-module-discovery") == 0) { auto_link = 0; continue; }
        if (kn_strcmp(arg, "--no-superloop") == 0) { knc_superloop = 0; continue; }
        if (kn_strcmp(arg, "--superloop") == 0) { knc_superloop = 1; continue; }
        if (kn_strcmp(arg, "--keep-temps") == 0) { keep_temps = 1; continue; }
        if (kn_strcmp(arg, "--color") == 0 && i + 1 < argc) { parse_color_mode(argv[i + 1], &color_mode); i++; continue; }
        if (kn_strcmp(arg, "--lang") == 0 && i + 1 < argc) { lang = argv[++i]; continue; }
        if (kn_strcmp(arg, "--locale-file") == 0 && i + 1 < argc) { locale_file = argv[++i]; continue; }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); return 2; }
        if (!input) input = arg;
        else { kn_write_str("'kinal vm pack' accepts exactly one input\n"); return 2; }
    }

    if (!input) { kn_write_str("no input file\n"); return 1; }

    kn_diag_reset();
    kn_diag_set_color_mode(color_mode);
    kn_diag_set_language(lang);
    if (!locale_file || !locale_file[0]) locale_file = resolve_default_locale_path(lang);
    kn_diag_set_locale_file(locale_file);
    kn_std_set_profile(KN_STD_PROFILE_HOSTED);

    const char *target_triple = normalize_target_triple(target_raw);
    if (!target_triple || !target_triple[0]) target_triple = default_target_triple();

    char out_default[KN_MAX_PATH];
    if (!out) {
        make_default_output(out_default, sizeof(out_default), KN_EMIT_KNC_EXE, KN_ARTIFACT_EXE, KN_ENV_HOSTED, target_triple, input);
        out = out_default;
    }

    KnInputKind kind = classify_input(input);
    const char *bundle_knc_input = 0;
    char bundle_knc_path[KN_MAX_PATH];
    int cleanup_bundle_knc = 0;
    bundle_knc_path[0] = 0;

    if (kind == KN_INPUT_FX) {
        if (!derive_temp_knc_path(input, bundle_knc_path, sizeof(bundle_knc_path))) {
            kn_write_str("failed to create temporary .knc path\n"); return 1;
        }
        KnCompileConfig knc_cfg;
        kn_memset(&knc_cfg, 0, sizeof(knc_cfg));
        knc_cfg.out_path = bundle_knc_path;
        knc_cfg.auto_link = auto_link;
        knc_cfg.emit_mode = KN_EMIT_KNC;
        knc_cfg.artifact_kind = KN_ARTIFACT_EXE;
        knc_cfg.target_triple = target_triple;
        knc_cfg.linker_kind = default_hosted_linker_kind(knc_cfg.target_triple);
        knc_cfg.env_kind = KN_ENV_HOSTED;
        knc_cfg.runtime_mode = KN_RUNTIME_GC;
        knc_cfg.entry_symbol = "Main";
        knc_cfg.knc_superloop = knc_superloop;
        const char *inputs[1]; inputs[0] = input;
        int rc = kn_compile(1, inputs, &knc_cfg);
        kn_diag_print_summary();
        if (rc != 0) { try_delete_file(bundle_knc_path); return 1; }
        bundle_knc_input = bundle_knc_path;
        cleanup_bundle_knc = keep_temps ? 0 : 1;
    } else if (kind == KN_INPUT_KNC) {
        bundle_knc_input = input;
    } else {
        kn_write_str("'kinal vm pack' requires .kn/.kinal/.fx or .knc input\n"); return 1;
    }

    /* Resolve VM executable for embedding */
    char vm_exe[KN_MAX_PATH];
    if (resolve_vm_executable(vm_exe, sizeof(vm_exe), vm_path)) {
        int rc = embed_knc_into_exe(vm_exe, bundle_knc_input, out);
        if (cleanup_bundle_knc) try_delete_file(bundle_knc_input);
        return rc;
    }

    /* Fallback: build VM from source */
    char vm_src[KN_MAX_PATH];
    if (!resolve_vm_main_source(vm_src, sizeof(vm_src))) {
        if (cleanup_bundle_knc) try_delete_file(bundle_knc_input);
        kn_write_str("failed to locate KinalVM runtime sources\n"); return 1;
    }

    char temp_exe[KN_MAX_PATH];
    {
        size_t te_off = 0; temp_exe[0] = 0;
        kn_append(temp_exe, sizeof(temp_exe), &te_off, bundle_knc_input);
        for (size_t i = te_off; i > 0; i--) { if (temp_exe[i - 1] == '.') { temp_exe[i - 1] = 0; te_off = i - 1; break; } }
        kn_append(temp_exe, sizeof(temp_exe), &te_off, ".vm");
        if (target_is_windows(target_triple)) kn_append(temp_exe, sizeof(temp_exe), &te_off, ".exe");
    }

    const char *vm_inputs[1]; vm_inputs[0] = vm_src;
    KnCompileConfig exe_cfg;
    kn_memset(&exe_cfg, 0, sizeof(exe_cfg));
    exe_cfg.out_path = temp_exe;
    exe_cfg.auto_link = 1;
    exe_cfg.emit_mode = KN_EMIT_LINK;
    exe_cfg.artifact_kind = KN_ARTIFACT_EXE;
    exe_cfg.target_triple = target_triple;
    exe_cfg.linker_kind = default_hosted_linker_kind(exe_cfg.target_triple);
    exe_cfg.env_kind = KN_ENV_HOSTED;
    exe_cfg.runtime_mode = KN_RUNTIME_GC;
    exe_cfg.entry_symbol = "Main";
    exe_cfg.knc_superloop = knc_superloop;
    kn_diag_reset();
    kn_diag_set_color_mode(color_mode);
    kn_diag_set_language(lang);
    kn_diag_set_locale_file(locale_file);
    int rc = kn_compile(1, vm_inputs, &exe_cfg);
    kn_diag_print_summary();
    if (rc != 0) { if (cleanup_bundle_knc) try_delete_file(bundle_knc_input); try_delete_file(temp_exe); return 1; }

    rc = embed_knc_into_exe(temp_exe, bundle_knc_input, out);
    if (cleanup_bundle_knc) try_delete_file(bundle_knc_input);
    try_delete_file(temp_exe);
    return rc;
}

/* ================================================================
 *  kinal pkg — package management subcommand group.
 * ================================================================ */
static int kn_pkg_build_main(int argc, char **argv);
static int kn_pkg_info_main(int argc, char **argv);
static int kn_pkg_unpack_main(int argc, char **argv);

static int kn_pkg_main(int argc, char **argv)
{
    if (argc < 2) { print_pkg_help(); return 1; }
    if (kn_strcmp(argv[1], "-h") == 0 || kn_strcmp(argv[1], "--help") == 0) { print_pkg_help(); return 0; }
    if (kn_strcmp(argv[1], "build") == 0) return kn_pkg_build_main(argc - 1, argv + 1);
    if (kn_strcmp(argv[1], "info") == 0) return kn_pkg_info_main(argc - 1, argv + 1);
    if (kn_strcmp(argv[1], "unpack") == 0) return kn_pkg_unpack_main(argc - 1, argv + 1);
    kn_write_str("unknown pkg subcommand: "); kn_write_str(argv[1]); kn_write_str("\n");
    print_pkg_help();
    return 2;
}

static int kn_pkg_build_main(int argc, char **argv)
{
    const char *manifest_arg = 0;
    const char *out = 0;
    const char *layout = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) {
            kn_write_str("Usage: kinal pkg build --manifest <file|dir> [-o <output.klib>]\n       kinal pkg build --manifest <file|dir> --layout <dir>\n");
            return 0;
        }
        if (kn_strcmp(arg, "--manifest") == 0 && i + 1 < argc) { manifest_arg = argv[++i]; continue; }
        if ((kn_strcmp(arg, "-o") == 0 || kn_strcmp(arg, "--output") == 0) && i + 1 < argc) { out = argv[++i]; continue; }
        if (kn_strcmp(arg, "--layout") == 0 && i + 1 < argc) { layout = argv[++i]; continue; }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); return 2; }
        kn_write_str("unexpected argument: "); kn_write_str(arg); kn_write_str("\n"); return 2;
    }

    if (!manifest_arg) { kn_write_str("--manifest is required\n"); return 1; }
    if (out && layout) { kn_write_str("-o and --layout cannot be used together\n"); return 1; }

    char manifest_path[KN_MAX_PATH];
    if (!resolve_manifest_input_path(manifest_arg, 0, manifest_path, sizeof(manifest_path))) {
        kn_write_str("invalid manifest path: "); kn_write_str(manifest_arg); kn_write_str("\n"); return 1;
    }

    if (layout && layout[0]) {
        if (emit_local_package_from_manifest(manifest_path, layout) != 0) return 1;
        return 0;
    }

    /* Default: build klib */
    if (!out || !out[0]) {
        KnPkgManifest manifest;
        if (parse_pkg_manifest(manifest_path, &manifest)) {
            static char klib_out[KN_MAX_PATH];
            size_t off = 0; klib_out[0] = 0;
            kn_append(klib_out, sizeof(klib_out), &off, manifest.name[0] ? manifest.name : "package");
            if (manifest.version[0]) { kn_append(klib_out, sizeof(klib_out), &off, "-"); kn_append(klib_out, sizeof(klib_out), &off, manifest.version); }
            kn_append(klib_out, sizeof(klib_out), &off, ".klib");
            out = klib_out;
        } else {
            out = "output.klib";
        }
    }
    if (klib_pack_from_manifest(manifest_path, out) != 0) return 1;
    return 0;
}

static int kn_pkg_info_main(int argc, char **argv)
{
    const char *input = 0;
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) {
            kn_write_str("Usage: kinal pkg info <file.klib>\n"); return 0;
        }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); return 2; }
        if (!input) input = arg;
        else { kn_write_str("expected exactly one .klib file\n"); return 2; }
    }
    if (!input) { kn_write_str("no input file\n"); return 1; }
    print_klib_info(input);
    return 0;
}

static int kn_pkg_unpack_main(int argc, char **argv)
{
    const char *input = 0;
    const char *out = 0;
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (kn_strcmp(arg, "-h") == 0 || kn_strcmp(arg, "--help") == 0) {
            kn_write_str("Usage: kinal pkg unpack <file.klib> [-o <dir>]\n"); return 0;
        }
        if ((kn_strcmp(arg, "-o") == 0 || kn_strcmp(arg, "--output") == 0) && i + 1 < argc) { out = argv[++i]; continue; }
        if (arg[0] == '-' && arg[1]) { kn_write_str("unknown option: "); kn_write_str(arg); kn_write_str("\n"); return 2; }
        if (!input) input = arg;
        else { kn_write_str("expected exactly one .klib file\n"); return 2; }
    }
    if (!input) { kn_write_str("no input file\n"); return 1; }

    char extract_out[KN_MAX_PATH];
    if (!out || !out[0]) {
        const char *base = path_basename_local(input);
        char stem[KN_MAX_PATH]; size_t off = 0; stem[0] = 0;
        for (size_t si = 0; base[si] && base[si] != '.' && off + 1 < sizeof(stem); si++) stem[off++] = base[si];
        stem[off] = 0;
        copy_str(extract_out, sizeof(extract_out), stem[0] ? stem : "klib-extract");
        out = extract_out;
    }
    if (klib_extract_to_dir(input, out) != 0) { kn_write_str("failed to extract klib\n"); return 1; }
    return 0;
}
// This is required when statically linking LLVM (C++), so we use `main` instead
// of a freestanding `mainCRTStartup` entry point.
#if defined(_WIN32)
int main(void)
{
    KnArgs args = kn_parse_cmdline();
    return kn_main(args.argc, args.argv);
}
#else
int main(int argc, char **argv)
{
    return kn_main(argc, argv);
}
#endif
#endif
