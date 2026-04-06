static void kn_to_zig_target(const char *triple, char *out, size_t cap)
{
    if (!triple || !triple[0])
    {
        kn_copy_str(out, cap, "x86_64-windows-msvc");
        return;
    }
    if (kn_triple_is_windows(triple))
    {
        if (kn_triple_is_x86(triple))
            kn_copy_str(out, cap, "i386-windows-msvc");
        else
            kn_copy_str(out, cap, "x86_64-windows-msvc");
        return;
    }
    if (kn_triple_is_linux(triple))
    {
        if (kn_triple_is_arm64(triple))
            kn_copy_str(out, cap, "aarch64-linux-gnu");
        else
            kn_copy_str(out, cap, "x86_64-linux-gnu");
        return;
    }
    if (kn_triple_is_macos(triple))
    {
        if (kn_triple_is_arm64(triple))
            kn_copy_str(out, cap, "aarch64-macos");
        else
            kn_copy_str(out, cap, "x86_64-macos");
        return;
    }
    kn_copy_str(out, cap, triple);
}

static int kn_compile_runtime_with_clang(const char *triple, const char *out_obj, int pic)
{
    char runtime_src[1024];
    if (!kn_resolve_runtime_src(runtime_src, sizeof(runtime_src)))
        return 0;
    char runtime_inc[1024];
    int has_inc = kn_resolve_runtime_include(runtime_inc, sizeof(runtime_inc));

    char clang[1024];
    kn_resolve_tool(clang, sizeof(clang), "clang.exe");

    char target_arg[512];
    size_t off = 0;
    target_arg[0] = 0;
    kn_append(target_arg, sizeof(target_arg), &off, "--target=");
    kn_append(target_arg, sizeof(target_arg), &off, triple);

    char cmd[8192];
    off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, clang);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append(cmd, sizeof(cmd), &off, target_arg);
    kn_append(cmd, sizeof(cmd), &off, " -c ");
    kn_append_quoted(cmd, sizeof(cmd), &off, runtime_src);
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, out_obj);
    kn_append(cmd, sizeof(cmd), &off, " -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector");
    kn_append(cmd, sizeof(cmd), &off, " -fno-exceptions -fno-asynchronous-unwind-tables -D_CRT_SECURE_NO_WARNINGS");
    if (pic)
        kn_append(cmd, sizeof(cmd), &off, " -fPIC");
    if (has_inc)
    {
        kn_append(cmd, sizeof(cmd), &off, " -I");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_inc);
    }
    if (kn_run_process(clang, cmd) != 0)
        return 0;
    return kn_file_exists(out_obj);
}

static int kn_compile_runtime_with_zig(const char *triple, const char *out_obj, int pic, const char *zig_path)
{
    char runtime_src[1024];
    if (!kn_resolve_runtime_src(runtime_src, sizeof(runtime_src)))
        return 0;
    char runtime_inc[1024];
    int has_inc = kn_resolve_runtime_include(runtime_inc, sizeof(runtime_inc));

    char zig[1024];
    if (zig_path && zig_path[0])
        kn_copy_str(zig, sizeof(zig), zig_path);
    else
        kn_resolve_zig(zig, sizeof(zig), 1);
    if (!kn_check_tool_version(zig))
    {
        kn_write_str("link failed: zig toolchain is not available\n");
        return 0;
    }

    char zt[256];
    kn_to_zig_target(triple, zt, sizeof(zt));

    char cmd[8192];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, zig);
    kn_append(cmd, sizeof(cmd), &off, " cc -target ");
    kn_append(cmd, sizeof(cmd), &off, zt);
    kn_append(cmd, sizeof(cmd), &off, " -c ");
    kn_append_quoted(cmd, sizeof(cmd), &off, runtime_src);
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, out_obj);
    kn_append(cmd, sizeof(cmd), &off, " -O2 -ffreestanding -fno-builtin -fno-stack-protector");
    kn_append(cmd, sizeof(cmd), &off, " -fno-exceptions -fno-asynchronous-unwind-tables");
    if (pic)
        kn_append(cmd, sizeof(cmd), &off, " -fPIC");
    if (kn_triple_is_windows(triple))
        kn_append(cmd, sizeof(cmd), &off, " -D_CRT_SECURE_NO_WARNINGS");
    if (has_inc)
    {
        kn_append(cmd, sizeof(cmd), &off, " -I");
        kn_append_quoted(cmd, sizeof(cmd), &off, runtime_inc);
    }
    if (kn_run_process(zig, cmd) != 0)
        return 0;
    return kn_file_exists(out_obj);
}

static int kn_resolve_math_src(char *out, size_t cap)
{
    char p[1024];
    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\src\\kn_math.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "..\\runtime\\src\\kn_math.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
    }
    if (kn_file_exists("runtime/src/kn_math.c"))
    {
        kn_copy_str(out, cap, "runtime/src/kn_math.c");
        return 1;
    }
    if (kn_file_exists("libs/runtime/src/kn_math.c"))
    {
        kn_copy_str(out, cap, "libs/runtime/src/kn_math.c");
        return 1;
    }
    return 0;
}

static int kn_compile_math_with_zig(const char *triple, const char *out_obj, int pic, const char *zig_path)
{
    char math_src[1024];
    if (!kn_resolve_math_src(math_src, sizeof(math_src)))
        return 0;

    char zig[1024];
    if (zig_path && zig_path[0])
        kn_copy_str(zig, sizeof(zig), zig_path);
    else
        kn_resolve_zig(zig, sizeof(zig), 1);
    if (!kn_check_tool_version(zig))
        return 0;

    char zt[256];
    kn_to_zig_target(triple, zt, sizeof(zt));

    char cmd[8192];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, zig);
    kn_append(cmd, sizeof(cmd), &off, " cc -target ");
    kn_append(cmd, sizeof(cmd), &off, zt);
    kn_append(cmd, sizeof(cmd), &off, " -c ");
    kn_append_quoted(cmd, sizeof(cmd), &off, math_src);
    kn_append(cmd, sizeof(cmd), &off, " -o ");
    kn_append_quoted(cmd, sizeof(cmd), &off, out_obj);
    kn_append(cmd, sizeof(cmd), &off, " -O2");
    if (pic)
        kn_append(cmd, sizeof(cmd), &off, " -fPIC");
    if (kn_run_process(zig, cmd) != 0)
        return 0;
    return kn_file_exists(out_obj);
}

static int kn_prepare_runtime_obj(const KnLinkOptions *opts, char *out, size_t cap)
{
    if (!opts || !out || cap == 0) return 0;
    out[0] = 0;
    if (!opts->include_runtime)
        return 0;

    const char *triple = opts->target_triple && opts->target_triple[0] ? opts->target_triple : "x86_64-pc-windows-msvc";

    const int need_pic = (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB &&
                          (kn_triple_is_linux(triple) || kn_triple_is_macos(triple))) ? 1 : 0;

    if (!need_pic && (kn_triple_is_windows(triple) || kn_triple_is_linux(triple)))
    {
        if (kn_resolve_runtime_obj(triple, out, cap))
            return 1;
    }

    const char *ext = kn_triple_is_windows(triple) ? ".kn_runtime.obj" : (need_pic ? ".kn_runtime.pic.o" : ".kn_runtime.o");
    if (!kn_path_with_suffix(out, cap, opts->out_path, ext))
        return 0;

    if (kn_triple_is_windows(triple))
    {
        if (kn_compile_runtime_with_clang(triple, out, 0))
            return 1;
        if (kn_compile_runtime_with_zig(triple, out, 0, opts->linker_path))
            return 1;
        return 0;
    }

    if (kn_compile_runtime_with_zig(triple, out, need_pic, opts->linker_path))
        return 1;
    return 0;
}

static int kn_prepare_math_obj(const KnLinkOptions *opts, char *out, size_t cap)
{
    if (!opts || !out || cap == 0) return 0;
    out[0] = 0;

    const char *triple = opts->target_triple && opts->target_triple[0] ? opts->target_triple : "x86_64-pc-windows-msvc";
    const int need_pic = (opts->artifact_kind == KN_ARTIFACT_SHARED_LIB &&
                          (kn_triple_is_linux(triple) || kn_triple_is_macos(triple))) ? 1 : 0;

    if (!need_pic && kn_resolve_math_obj(triple, out, cap))
        return 1;

    if (kn_triple_is_windows(triple))
        return 0;

    const char *ext = need_pic ? ".kn_math.pic.o" : ".kn_math.o";
    if (!kn_path_with_suffix(out, cap, opts->out_path, ext))
        return 0;
    return kn_compile_math_with_zig(triple, out, need_pic, opts->linker_path);
}

static int kn_ensure_import_lib(const char *base_out, const char *tag, const char *machine, const char *def_text,
                                char *out_lib, size_t cap)
{
    char def_path[1024];
    char lib_path[1024];

    def_path[0] = 0;
    lib_path[0] = 0;
    if (!kn_path_with_suffix(def_path, sizeof(def_path), base_out, tag))
        return 0;
    if (!kn_path_with_suffix(lib_path, sizeof(lib_path), base_out, tag))
        return 0;
    if (!kn_path_with_suffix(def_path, sizeof(def_path), def_path, ".def"))
        return 0;
    if (!kn_path_with_suffix(lib_path, sizeof(lib_path), lib_path, ".lib"))
        return 0;

    if (!kn_write_text_file(def_path, def_text))
        return 0;

    char llvm_lib_tool[1024];
    kn_resolve_tool(llvm_lib_tool, sizeof(llvm_lib_tool), "llvm-lib.exe");

    char cmd[4096];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, llvm_lib_tool);
    kn_append(cmd, sizeof(cmd), &off, " /nologo /def:");
    kn_append_quoted(cmd, sizeof(cmd), &off, def_path);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append(cmd, sizeof(cmd), &off, machine);
    kn_append(cmd, sizeof(cmd), &off, " /out:");
    kn_append_quoted(cmd, sizeof(cmd), &off, lib_path);

    if (kn_run_process(llvm_lib_tool, cmd) != 0 || !kn_file_exists(lib_path))
        return 0;
    kn_copy_str(out_lib, cap, lib_path);
    return 1;
}

static void kn_resolve_windows_system_libs(const KnLinkOptions *opts,
                                           char *k32, size_t kcap,
                                           char *u32, size_t ucap,
                                           char *g32, size_t gcap)
{
    const char *triple = opts->target_triple ? opts->target_triple : "";
    int has_k32 = kn_resolve_kernel32_lib(triple, k32, kcap);
    int has_u32 = kn_resolve_user32_lib(triple, u32, ucap);
    int has_g32 = kn_resolve_gdi32_lib(triple, g32, gcap);
    const char *machine = kn_triple_is_x86(triple) ? "/machine:x86" : "/machine:x64";

    if (!has_k32)
        has_k32 = kn_ensure_import_lib(opts->out_path, ".kernel32", machine, KN_KERNEL32_DEF, k32, kcap);
    if (!has_u32)
        has_u32 = kn_ensure_import_lib(opts->out_path, ".user32", machine, KN_USER32_DEF, u32, ucap);
    if (!has_g32)
        has_g32 = kn_ensure_import_lib(opts->out_path, ".gdi32", machine, KN_GDI32_DEF, g32, gcap);

    if (!has_k32)
        kn_copy_str(k32, kcap, "kernel32.lib");
    if (!has_u32)
        kn_copy_str(u32, ucap, "user32.lib");
    if (!has_g32)
        kn_copy_str(g32, gcap, "gdi32.lib");
}
