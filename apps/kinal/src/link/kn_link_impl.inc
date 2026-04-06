static void kn_append_windows_implib_arg(char *cmd, size_t cap, size_t *off, const char *out_path)
{
    char implib[1024];
    if (!kn_path_replace_ext(implib, sizeof(implib), out_path, ".lib"))
        return;
    kn_append(cmd, cap, off, " /implib:");
    kn_append_quoted(cmd, cap, off, implib);
}

static void kn_append_windows_toolchain_libpaths(char *cmd, size_t cap, size_t *off, const KnWindowsToolchainDirs *dirs)
{
    if (!dirs) return;
    if (dirs->has_bundle_lib_dir)
    {
        kn_append(cmd, cap, off, " /libpath:");
        kn_append_quoted(cmd, cap, off, dirs->bundle_lib_dir);
    }
    if (dirs->has_sdk_um_lib)
    {
        kn_append(cmd, cap, off, " /libpath:");
        kn_append_quoted(cmd, cap, off, dirs->sdk_um_lib);
    }
    if (dirs->has_sdk_ucrt_lib)
    {
        kn_append(cmd, cap, off, " /libpath:");
        kn_append_quoted(cmd, cap, off, dirs->sdk_ucrt_lib);
    }
    if (dirs->has_msvc_lib)
    {
        kn_append(cmd, cap, off, " /libpath:");
        kn_append_quoted(cmd, cap, off, dirs->msvc_lib);
    }
}

static void kn_append_windows_toolchain_libpaths_gnu(char *cmd, size_t cap, size_t *off, const KnWindowsToolchainDirs *dirs)
{
    if (!dirs) return;
    if (dirs->has_bundle_lib_dir)
    {
        kn_append(cmd, cap, off, " -L");
        kn_append_quoted(cmd, cap, off, dirs->bundle_lib_dir);
    }
    if (dirs->has_sdk_um_lib)
    {
        kn_append(cmd, cap, off, " -L");
        kn_append_quoted(cmd, cap, off, dirs->sdk_um_lib);
    }
    if (dirs->has_sdk_ucrt_lib)
    {
        kn_append(cmd, cap, off, " -L");
        kn_append_quoted(cmd, cap, off, dirs->sdk_ucrt_lib);
    }
    if (dirs->has_msvc_lib)
    {
        kn_append(cmd, cap, off, " -L");
        kn_append_quoted(cmd, cap, off, dirs->msvc_lib);
    }
}

static void kn_append_windows_crt_libs(char *cmd, size_t cap, size_t *off, int crt_mode)
{
    static const char *dynamic_libs[] = { "oldnames", "msvcrt", "vcruntime", "ucrt" };
    static const char *static_libs[] = { "oldnames", "libcmt", "libvcruntime", "libucrt" };
    const char **libs = crt_mode == KN_CRT_STATIC ? static_libs : dynamic_libs;
    int count = crt_mode == KN_CRT_STATIC
        ? (int)(sizeof(static_libs) / sizeof(static_libs[0]))
        : (int)(sizeof(dynamic_libs) / sizeof(dynamic_libs[0]));
    for (int i = 0; i < count; i++)
    {
        kn_append(cmd, cap, off, " ");
        kn_append_windows_lib_name(cmd, cap, off, libs[i]);
    }
}

static void kn_append_windows_crt_msvc_side(char *cmd, size_t cap, size_t *off, int crt_mode)
{
    static const char *dynamic_libs[] = { "oldnames", "msvcrt", "vcruntime" };
    static const char *static_libs[] = { "oldnames", "libcmt", "libvcruntime" };
    const char **libs = crt_mode == KN_CRT_STATIC ? static_libs : dynamic_libs;
    int count = crt_mode == KN_CRT_STATIC
        ? (int)(sizeof(static_libs) / sizeof(static_libs[0]))
        : (int)(sizeof(dynamic_libs) / sizeof(dynamic_libs[0]));
    for (int i = 0; i < count; i++)
    {
        kn_append(cmd, cap, off, " ");
        kn_append_windows_lib_name(cmd, cap, off, libs[i]);
    }
}

static void kn_append_windows_crt_ucrt_side(char *cmd, size_t cap, size_t *off, int crt_mode)
{
    kn_append(cmd, cap, off, " ");
    kn_append_windows_lib_name(cmd, cap, off, crt_mode == KN_CRT_STATIC ? "libucrt" : "ucrt");
}

static void kn_append_windows_crt_libs_gnu(char *cmd, size_t cap, size_t *off, int crt_mode)
{
    if (crt_mode == KN_CRT_STATIC)
        kn_append(cmd, cap, off, " -loldnames -llibcmt -llibvcruntime -llibucrt");
    else
        kn_append(cmd, cap, off, " -loldnames -lmsvcrt -lvcruntime -lucrt");
}

static void kn_append_windows_crt_msvc_side_gnu(char *cmd, size_t cap, size_t *off, int crt_mode)
{
    if (crt_mode == KN_CRT_STATIC)
        kn_append(cmd, cap, off, " -loldnames -llibcmt -llibvcruntime");
    else
        kn_append(cmd, cap, off, " -loldnames -lmsvcrt -lvcruntime");
}

static void kn_append_windows_crt_ucrt_side_gnu(char *cmd, size_t cap, size_t *off, int crt_mode)
{
    if (crt_mode == KN_CRT_STATIC)
        kn_append(cmd, cap, off, " -llibucrt");
    else
        kn_append(cmd, cap, off, " -lucrt");
}

static void kn_append_windows_hosted_defaults_msvc(const KnLinkOptions *opts,
                                                   char *cmd, size_t cap, size_t *off,
                                                   const KnWindowsToolchainDirs *dirs,
                                                   const char *kernel32_lib,
                                                   const char *user32_lib,
                                                   const char *gdi32_lib,
                                                   int has_ucrt_fallback,
                                                   const char *ucrtbase_lib)
{
    if (!opts || opts->no_default_libs)
        return;

    kn_append_windows_toolchain_libpaths(cmd, cap, off, dirs);

    if (dirs && (dirs->has_bundle_system_libs || dirs->has_sdk_um_lib))
    {
        kn_append(cmd, cap, off, " ");
        kn_append_windows_lib_name(cmd, cap, off, "kernel32");
        kn_append(cmd, cap, off, " ");
        kn_append_windows_lib_name(cmd, cap, off, "user32");
        kn_append(cmd, cap, off, " ");
        kn_append_windows_lib_name(cmd, cap, off, "gdi32");
    }
    else
    {
        kn_append(cmd, cap, off, " ");
        kn_append_quoted(cmd, cap, off, kernel32_lib);
        kn_append(cmd, cap, off, " ");
        kn_append_quoted(cmd, cap, off, user32_lib);
        kn_append(cmd, cap, off, " ");
        kn_append_quoted(cmd, cap, off, gdi32_lib);
    }

    if (dirs &&
        ((kn_windows_crt_mode(opts) == KN_CRT_STATIC && dirs->has_bundle_static_crt) ||
         (kn_windows_crt_mode(opts) != KN_CRT_STATIC && dirs->has_bundle_dynamic_crt)))
        kn_append_windows_crt_libs(cmd, cap, off, kn_windows_crt_mode(opts));
    else if (dirs && dirs->has_msvc_lib && dirs->has_sdk_ucrt_lib)
        kn_append_windows_crt_libs(cmd, cap, off, kn_windows_crt_mode(opts));
    else
    {
        if (dirs && dirs->has_msvc_lib)
            kn_append_windows_crt_msvc_side(cmd, cap, off, kn_windows_crt_mode(opts));
        if (dirs && dirs->has_sdk_ucrt_lib)
            kn_append_windows_crt_ucrt_side(cmd, cap, off, kn_windows_crt_mode(opts));
        else if (has_ucrt_fallback)
        {
            kn_append(cmd, cap, off, " ");
            kn_append_quoted(cmd, cap, off, ucrtbase_lib);
        }
    }
}

static void kn_append_windows_hosted_defaults_gnu(const KnLinkOptions *opts,
                                                  char *cmd, size_t cap, size_t *off,
                                                  const KnWindowsToolchainDirs *dirs,
                                                  const char *kernel32_lib,
                                                  const char *user32_lib,
                                                  const char *gdi32_lib,
                                                  int has_ucrt_fallback,
                                                  const char *ucrtbase_lib)
{
    if (!opts || opts->no_default_libs)
        return;

    kn_append_windows_toolchain_libpaths_gnu(cmd, cap, off, dirs);

    if (dirs && (dirs->has_bundle_system_libs || dirs->has_sdk_um_lib))
        kn_append(cmd, cap, off, " -lkernel32 -luser32 -lgdi32");
    else
    {
        kn_append(cmd, cap, off, " ");
        kn_append_quoted(cmd, cap, off, kernel32_lib);
        kn_append(cmd, cap, off, " ");
        kn_append_quoted(cmd, cap, off, user32_lib);
        kn_append(cmd, cap, off, " ");
        kn_append_quoted(cmd, cap, off, gdi32_lib);
    }

    if (dirs &&
        ((kn_windows_crt_mode(opts) == KN_CRT_STATIC && dirs->has_bundle_static_crt) ||
         (kn_windows_crt_mode(opts) != KN_CRT_STATIC && dirs->has_bundle_dynamic_crt)))
        kn_append_windows_crt_libs_gnu(cmd, cap, off, kn_windows_crt_mode(opts));
    else if (dirs && dirs->has_msvc_lib && dirs->has_sdk_ucrt_lib)
        kn_append_windows_crt_libs_gnu(cmd, cap, off, kn_windows_crt_mode(opts));
    else
    {
        if (dirs && dirs->has_msvc_lib)
            kn_append_windows_crt_msvc_side_gnu(cmd, cap, off, kn_windows_crt_mode(opts));
        if (dirs && dirs->has_sdk_ucrt_lib)
            kn_append_windows_crt_ucrt_side_gnu(cmd, cap, off, kn_windows_crt_mode(opts));
        else if (has_ucrt_fallback)
        {
            kn_append(cmd, cap, off, " ");
            kn_append_quoted(cmd, cap, off, ucrtbase_lib);
        }
    }
}

static int kn_link_windows_lld(const KnLinkOptions *opts)
{
    char lld[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(lld, sizeof(lld), opts->linker_path);
        if (!kn_file_exists(lld))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_tool(lld, sizeof(lld), "lld-link.exe");
    }

    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));
    char ucrtbase_lib[1024];
    int has_ucrt = has_math ? kn_resolve_ucrtbase_lib(opts->target_triple, ucrtbase_lib, sizeof(ucrtbase_lib)) : 0;

    char kernel32_lib[1024];
    char user32_lib[1024];
    char gdi32_lib[1024];
    kn_resolve_windows_system_libs(opts,
                                   kernel32_lib, sizeof(kernel32_lib),
                                   user32_lib, sizeof(user32_lib),
                                   gdi32_lib, sizeof(gdi32_lib));
    KnWindowsToolchainDirs dirs;
    kn_detect_windows_toolchain_dirs(opts, &dirs);
    if (opts->no_default_libs)
        kn_memset(&dirs, 0, sizeof(dirs));
    kn_dump_windows_toolchain_search_dirs(opts, &dirs);

    const char *triple = opts->target_triple ? opts->target_triple : "";
    const char *machine = kn_triple_is_x86(triple) ? "/machine:x86" : "/machine:x64";

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, lld);
    kn_append(cmd, sizeof(cmd), &off, " /nologo ");
    kn_append(cmd, sizeof(cmd), &off, machine);
    kn_append(cmd, sizeof(cmd), &off, " ");
    if (opts->lto_mode != KN_LTO_OFF)
        kn_append(cmd, sizeof(cmd), &off, "/LTCG ");

    if (opts->artifact_kind == KN_ARTIFACT_EXE)
        kn_append(cmd, sizeof(cmd), &off, "/subsystem:console /entry:mainCRTStartup ");
    else
    {
        kn_append(cmd, sizeof(cmd), &off, "/dll /noentry ");
        kn_append_windows_implib_arg(cmd, sizeof(cmd), &off, opts->out_path);
        kn_append(cmd, sizeof(cmd), &off, " ");
    }

    kn_append(cmd, sizeof(cmd), &off, "/out:");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_windows(cmd, sizeof(cmd), &off, opts->items, opts->item_count);
    kn_append_windows_hosted_defaults_msvc(opts, cmd, sizeof(cmd), &off, &dirs,
                                           kernel32_lib, user32_lib, gdi32_lib,
                                           has_ucrt, ucrtbase_lib);

    if (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB)
    {
        for (int i = 0; i < opts->export_count; i++)
        {
            const char *sym = opts->exports ? opts->exports[i] : 0;
            if (!sym || !sym[0]) continue;
            kn_append(cmd, sizeof(cmd), &off, " /export:");
            kn_append(cmd, sizeof(cmd), &off, sym);
        }
    }

    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(lld, cmd);
}

static int kn_link_windows_msvc(const KnLinkOptions *opts)
{
    char link_exe[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(link_exe, sizeof(link_exe), opts->linker_path);
        if (!kn_file_exists(link_exe))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_tool(link_exe, sizeof(link_exe), "link.exe");
        if (!kn_file_exists(link_exe))
            kn_detect_msvc_link_exe(opts, link_exe, sizeof(link_exe));
    }

    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));
    char ucrtbase_lib[1024];
    int has_ucrt_msvc = has_math ? kn_resolve_ucrtbase_lib(opts->target_triple, ucrtbase_lib, sizeof(ucrtbase_lib)) : 0;

    char kernel32_lib[1024];
    char user32_lib[1024];
    char gdi32_lib[1024];
    kn_resolve_windows_system_libs(opts,
                                   kernel32_lib, sizeof(kernel32_lib),
                                   user32_lib, sizeof(user32_lib),
                                   gdi32_lib, sizeof(gdi32_lib));
    KnWindowsToolchainDirs dirs;
    kn_detect_windows_toolchain_dirs(opts, &dirs);
    if (opts->no_default_libs)
        kn_memset(&dirs, 0, sizeof(dirs));
    kn_dump_windows_toolchain_search_dirs(opts, &dirs);

    const char *triple = opts->target_triple ? opts->target_triple : "";
    const char *machine = kn_triple_is_x86(triple) ? "/machine:x86" : "/machine:x64";

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, link_exe);
    kn_append(cmd, sizeof(cmd), &off, " /nologo ");
    kn_append(cmd, sizeof(cmd), &off, machine);
    kn_append(cmd, sizeof(cmd), &off, " ");
    if (opts->lto_mode != KN_LTO_OFF)
        kn_append(cmd, sizeof(cmd), &off, "/LTCG ");

    if (opts->artifact_kind == KN_ARTIFACT_EXE)
        kn_append(cmd, sizeof(cmd), &off, "/subsystem:console /entry:mainCRTStartup ");
    else
    {
        kn_append(cmd, sizeof(cmd), &off, "/dll /noentry ");
        kn_append_windows_implib_arg(cmd, sizeof(cmd), &off, opts->out_path);
        kn_append(cmd, sizeof(cmd), &off, " ");
    }

    kn_append(cmd, sizeof(cmd), &off, "/out:");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_windows(cmd, sizeof(cmd), &off, opts->items, opts->item_count);
    kn_append_windows_hosted_defaults_msvc(opts, cmd, sizeof(cmd), &off, &dirs,
                                           kernel32_lib, user32_lib, gdi32_lib,
                                           has_ucrt_msvc, ucrtbase_lib);

    if (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB)
    {
        for (int i = 0; i < opts->export_count; i++)
        {
            const char *sym = opts->exports ? opts->exports[i] : 0;
            if (!sym || !sym[0]) continue;
            kn_append(cmd, sizeof(cmd), &off, " /export:");
            kn_append(cmd, sizeof(cmd), &off, sym);
        }
    }

    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(link_exe, cmd);
}

static int kn_link_windows_with_zig(const KnLinkOptions *opts)
{
    char zig[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(zig, sizeof(zig), opts->linker_path);
        if (!kn_file_exists(zig))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_zig(zig, sizeof(zig), 1);
    }
    if (!kn_check_tool_version(zig))
    {
        kn_write_str("link failed: zig toolchain is not available\n");
        return -1;
    }

    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));
    char ucrtbase_lib[1024];
    int has_ucrt = has_math ? kn_resolve_ucrtbase_lib(opts->target_triple, ucrtbase_lib, sizeof(ucrtbase_lib)) : 0;

    char kernel32_lib[1024];
    char user32_lib[1024];
    char gdi32_lib[1024];
    kn_resolve_windows_system_libs(opts,
                                   kernel32_lib, sizeof(kernel32_lib),
                                   user32_lib, sizeof(user32_lib),
                                   gdi32_lib, sizeof(gdi32_lib));
    KnWindowsToolchainDirs dirs;
    kn_detect_windows_toolchain_dirs(opts, &dirs);
    if (opts->no_default_libs)
        kn_memset(&dirs, 0, sizeof(dirs));
    kn_dump_windows_toolchain_search_dirs(opts, &dirs);

    char zt[256];
    kn_to_zig_target(opts->target_triple, zt, sizeof(zt));

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, zig);
    kn_append(cmd, sizeof(cmd), &off, " cc -target ");
    kn_append(cmd, sizeof(cmd), &off, zt);
    if (opts->lto_mode == KN_LTO_FULL)
        kn_append(cmd, sizeof(cmd), &off, " -flto ");
    else if (opts->lto_mode == KN_LTO_THIN)
        kn_append(cmd, sizeof(cmd), &off, " -flto=thin ");
    kn_append(cmd, sizeof(cmd), &off, " -nostdlib ");
    if (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB)
    {
        kn_append(cmd, sizeof(cmd), &off, " -shared ");
        {
            char implib[1024];
            if (kn_path_replace_ext(implib, sizeof(implib), opts->out_path, ".lib"))
            {
                kn_append(cmd, sizeof(cmd), &off, " -Xlinker --out-implib -Xlinker ");
                kn_append_quoted(cmd, sizeof(cmd), &off, implib);
            }
        }
    }
    else
        kn_append(cmd, sizeof(cmd), &off, " -Wl,-e,mainCRTStartup -Wl,--subsystem,console ");
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_gnu(cmd, sizeof(cmd), &off, opts->items, opts->item_count);
    kn_append_windows_hosted_defaults_gnu(opts, cmd, sizeof(cmd), &off, &dirs,
                                          kernel32_lib, user32_lib, gdi32_lib,
                                          has_ucrt, ucrtbase_lib);
    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(zig, cmd);
}

static int kn_link_linux_with_zig(const KnLinkOptions *opts)
{
    kn_dump_link_search_dirs(opts);
    char zig[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(zig, sizeof(zig), opts->linker_path);
        if (!kn_file_exists(zig))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_zig(zig, sizeof(zig), 1);
    }
    if (!kn_check_tool_version(zig))
    {
        kn_write_str("link failed: zig toolchain is not available\n");
        return -1;
    }

    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));

    char zt[256];
    kn_to_zig_target(opts->target_triple, zt, sizeof(zt));

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, zig);
    kn_append(cmd, sizeof(cmd), &off, " cc -target ");
    kn_append(cmd, sizeof(cmd), &off, zt);
    if (opts->lto_mode == KN_LTO_FULL)
        kn_append(cmd, sizeof(cmd), &off, " -flto ");
    else if (opts->lto_mode == KN_LTO_THIN)
        kn_append(cmd, sizeof(cmd), &off, " -flto=thin ");
    if (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB)
        kn_append(cmd, sizeof(cmd), &off, " -shared -fPIC");
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_gnu(cmd, sizeof(cmd), &off, opts->items, opts->item_count);

    if (!opts->no_default_libs)
        kn_append(cmd, sizeof(cmd), &off, " -ldl");
    if (has_math && !opts->no_default_libs)
        kn_append(cmd, sizeof(cmd), &off, " -lm");
    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(zig, cmd);
}

static int kn_link_macos_with_zig(const KnLinkOptions *opts)
{
    kn_dump_link_search_dirs(opts);
    char zig[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(zig, sizeof(zig), opts->linker_path);
        if (!kn_file_exists(zig))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_zig(zig, sizeof(zig), 1);
    }
    if (!kn_check_tool_version(zig))
    {
        kn_write_str("link failed: zig toolchain is not available\n");
        return -1;
    }

    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));

    char zt[256];
    kn_to_zig_target(opts->target_triple, zt, sizeof(zt));

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, zig);
    kn_append(cmd, sizeof(cmd), &off, " cc -target ");
    kn_append(cmd, sizeof(cmd), &off, zt);
    if (opts->lto_mode == KN_LTO_FULL)
        kn_append(cmd, sizeof(cmd), &off, " -flto ");
    else if (opts->lto_mode == KN_LTO_THIN)
        kn_append(cmd, sizeof(cmd), &off, " -flto=thin ");
    if (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB)
        kn_append(cmd, sizeof(cmd), &off, " -dynamiclib");
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_gnu(cmd, sizeof(cmd), &off, opts->items, opts->item_count);

    if (has_math && !opts->no_default_libs)
        kn_append(cmd, sizeof(cmd), &off, " -lm");
    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(zig, cmd);
}

static int kn_link_static_windows(const KnLinkOptions *opts)
{
    kn_dump_link_search_dirs(opts);
    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));

    char llvm_lib_tool[1024];
    kn_resolve_tool(llvm_lib_tool, sizeof(llvm_lib_tool), "llvm-lib.exe");

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, llvm_lib_tool);
    kn_append(cmd, sizeof(cmd), &off, " /nologo /out:");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_windows(cmd, sizeof(cmd), &off, opts->items, opts->item_count);
    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(llvm_lib_tool, cmd);
}

static int kn_link_static_with_zig(const KnLinkOptions *opts)
{
    kn_dump_link_search_dirs(opts);
    char zig[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(zig, sizeof(zig), opts->linker_path);
        if (!kn_file_exists(zig))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_zig(zig, sizeof(zig), 1);
    }
    if (!kn_check_tool_version(zig))
    {
        kn_write_str("link failed: zig toolchain is not available\n");
        return -1;
    }

    char runtime_obj[1024];
    int has_runtime = kn_prepare_runtime_obj(opts, runtime_obj, sizeof(runtime_obj));
    if (opts->include_runtime && !has_runtime)
    {
        kn_write_str("link failed: cannot prepare runtime object\n");
        return -1;
    }
    char math_obj[1024];
    int has_math = kn_prepare_math_obj(opts, math_obj, sizeof(math_obj));

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, zig);
    kn_append(cmd, sizeof(cmd), &off, " ar rcs ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    if (has_runtime)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_obj);
    }
    if (has_math)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, math_obj);
    }

    kn_append_link_items_gnu(cmd, sizeof(cmd), &off, opts->items, opts->item_count);
    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(zig, cmd);
}

static int kn_link_freestanding_lld(const KnLinkOptions *opts)
{
    kn_dump_link_search_dirs(opts);
    char lld[1024];
    if (opts->linker_path && opts->linker_path[0])
    {
        kn_copy_str(lld, sizeof(lld), opts->linker_path);
        if (!kn_file_exists(lld))
        {
            kn_write_str("link failed: configured linker not found\n");
            return -1;
        }
    }
    else
    {
        kn_resolve_tool(lld, sizeof(lld), "ld.lld.exe");
    }

    const char *triple = opts->target_triple ? opts->target_triple : "";
    const char *machine = 0;
    if (kn_triple_is_x64(triple))
        machine = "elf_x86_64";
    else if (kn_triple_is_arm64(triple))
        machine = "aarch64elf";
    else
    {
        kn_write_str("link failed: unsupported freestanding target triple\n");
        return -1;
    }

    const char *entry = (opts->entry_symbol && opts->entry_symbol[0]) ? opts->entry_symbol : "__kn_entry";

    char cmd[16384];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, lld);
    kn_append(cmd, sizeof(cmd), &off, " -m ");
    kn_append(cmd, sizeof(cmd), &off, machine);
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->out_path);
    kn_append(cmd, sizeof(cmd), &off, " -e ");
    kn_append(cmd, sizeof(cmd), &off, entry);
    if (opts->link_script && opts->link_script[0])
    {
        kn_append(cmd, sizeof(cmd), &off, " -T ");
        kn_append_quoted(cmd, sizeof(cmd), &off, opts->link_script);
    }
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, opts->obj_path);

    for (int i = 0; i < opts->item_count; i++)
    {
        const KnLinkItem *item = &opts->items[i];
        const char *value = item->value;
        if (!value || !value[0]) continue;
        if (item->kind == KN_LINK_ITEM_ARG)
        {
            kn_append(cmd, sizeof(cmd), &off, " ");
            kn_append(cmd, sizeof(cmd), &off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_DIR)
        {
            kn_append(cmd, sizeof(cmd), &off, " -L ");
            kn_append_quoted(cmd, sizeof(cmd), &off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_NAME)
        {
            kn_append(cmd, sizeof(cmd), &off, " -l");
            kn_append(cmd, sizeof(cmd), &off, value);
            continue;
        }
        kn_append(cmd, sizeof(cmd), &off, " ");
        kn_append_quoted(cmd, sizeof(cmd), &off, value);
    }

    if (opts->trace || opts->show_link)
    {
        kn_write_str("[Trace] link cmd: ");
        kn_write_str(cmd);
        kn_write_str("\n");
    }
    return kn_run_process(lld, cmd);
}

int kn_link(const KnLinkOptions *opts)
{
    if (!opts || !opts->obj_path || !opts->out_path)
        return -1;

    const char *triple = (opts->target_triple && opts->target_triple[0]) ? opts->target_triple : "x86_64-pc-windows-msvc";
    int is_windows = kn_triple_is_windows(triple);
    int is_linux = kn_triple_is_linux(triple);
    int is_macos = kn_triple_is_macos(triple);
    int is_freestanding = (opts->env_kind == KN_ENV_FREESTANDING) || kn_str_contains_ci(triple, "none-elf");

    if (opts->artifact_kind == KN_ARTIFACT_STATIC_LIB)
    {
        if (is_windows)
            return kn_link_static_windows(opts);
        return kn_link_static_with_zig(opts);
    }

    if (is_freestanding)
    {
        if (opts->linker_kind == KN_LINKER_MSVC)
        {
            kn_write_str("link failed: msvc linker is not available for freestanding targets\n");
            return -1;
        }
        return kn_link_freestanding_lld(opts);
    }

    if (is_windows)
    {
        if (opts->linker_kind == KN_LINKER_MSVC)
            return kn_link_windows_msvc(opts);
        if (opts->linker_kind == KN_LINKER_ZIG)
            return kn_link_windows_with_zig(opts);
        return kn_link_windows_lld(opts);
    }

    if (is_linux)
    {
        if (opts->linker_kind == KN_LINKER_MSVC)
        {
            kn_write_str("link failed: msvc linker is only available for Windows targets\n");
            return -1;
        }
        if (opts->linker_kind == KN_LINKER_LLD)
        {
            kn_write_str("link failed: lld backend for Linux target is not available in this build\n");
            return -1;
        }
        return kn_link_linux_with_zig(opts);
    }

    if (is_macos)
    {
        if (opts->linker_kind == KN_LINKER_MSVC)
        {
            kn_write_str("link failed: msvc linker is only available for Windows targets\n");
            return -1;
        }
        if (opts->linker_kind == KN_LINKER_LLD)
        {
            kn_write_str("link failed: lld backend for macOS target is not available in this build\n");
            return -1;
        }
        return kn_link_macos_with_zig(opts);
    }

    kn_write_str("link failed: unsupported target triple\n");
    return -1;
}
