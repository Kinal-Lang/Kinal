static int kn_run_process(const char *exe, const char *cmdline)
{
    KN_STARTUPINFOA si;
    KN_PROCESS_INFORMATION pi;
    kn_memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    kn_memset(&pi, 0, sizeof(pi));

    size_t len = kn_strlen(cmdline);
    char *mutable_cmd = (char *)kn_malloc(len + 1);
    if (!mutable_cmd) return -1;
    kn_memcpy(mutable_cmd, cmdline, len);
    mutable_cmd[len] = 0;

    if (!CreateProcessA(exe, mutable_cmd, 0, 0, 0, 0, 0, 0, &si, &pi))
    {
        kn_free(mutable_cmd);
        return -1;
    }

    kn_free(mutable_cmd);
    WaitForSingleObject(pi.hProcess, KN_INFINITE);
    KN_DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

static void kn_append_quoted(char *buf, size_t cap, size_t *off, const char *s)
{
    kn_append(buf, cap, off, "\"");
    kn_append(buf, cap, off, s ? s : "");
    kn_append(buf, cap, off, "\"");
}

static void kn_copy_str(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = kn_strlen(src);
    if (n >= cap) n = cap - 1;
    kn_memcpy(dst, src, n);
    dst[n] = 0;
}

static int kn_has_sep(char c)
{
    return c == '\\' || c == '/';
}

static int kn_path_join(char *out, size_t cap, const char *a, const char *b)
{
    if (!out || !a || !b) return 0;
    char a_copy[1024];
    if (out == a)
    {
        kn_copy_str(a_copy, sizeof(a_copy), a);
        a = a_copy;
    }
    size_t la = kn_strlen(a);
    size_t lb = kn_strlen(b);
    size_t need = la + lb + 2;
    if (need > cap) return 0;

    size_t off = 0;
    kn_memcpy(out, a, la);
    off = la;
    if (off > 0 && !kn_has_sep(out[off - 1]))
    {
#if defined(_WIN32) || defined(_WIN64)
        out[off++] = '\\';
#else
        out[off++] = '/';
#endif
    }
    for (size_t i = 0; i < lb; i++)
    {
        char ch = b[i];
#if defined(_WIN32) || defined(_WIN64)
        out[off++] = (ch == '/') ? '\\' : ch;
#else
        out[off++] = (ch == '\\') ? '/' : ch;
#endif
    }
    out[off] = 0;
    return 1;
}

static int kn_path_with_suffix(char *out, size_t cap, const char *base, const char *suffix)
{
    if (!out || !base || !suffix) return 0;
    size_t lb = kn_strlen(base);
    size_t ls = kn_strlen(suffix);
    if (lb + ls + 1 > cap) return 0;
    kn_memcpy(out, base, lb);
    kn_memcpy(out + lb, suffix, ls);
    out[lb + ls] = 0;
    return 1;
}

static int kn_file_exists(const char *path)
{
    if (!path || !path[0]) return 0;
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_READ, KN_FILE_SHARE_READ | KN_FILE_SHARE_WRITE,
                              0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
        return 0;
    CloseHandle(h);
    return 1;
}

static int kn_write_text_file(const char *path, const char *text)
{
    if (!path || !text) return 0;
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_WRITE, 0, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
        return 0;
    size_t n = kn_strlen(text);
    KN_DWORD wrote = 0;
    int ok = WriteFile(h, text, (KN_DWORD)n, &wrote, 0) && wrote == (KN_DWORD)n;
    CloseHandle(h);
    return ok;
}

static int kn_get_exe_path(char *out, size_t cap)
{
    if (!out || cap == 0) return 0;
#if !defined(_WIN32) && !defined(_WIN64)
    {
        long n = readlink("/proc/self/exe", out, cap - 1);
        if (n > 0) { out[n] = 0; return 1; }
    }
#endif
    const char *cmd = GetCommandLineA();
    if (!cmd || !cmd[0]) return 0;

    size_t i = 0;
    size_t off = 0;
    while (cmd[i] == ' ' || cmd[i] == '\t') i++;
    if (!cmd[i]) return 0;

    if (cmd[i] == '"')
    {
        i++;
        while (cmd[i] && cmd[i] != '"' && off + 1 < cap)
            out[off++] = cmd[i++];
    }
    else
    {
        while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && off + 1 < cap)
            out[off++] = cmd[i++];
    }

    out[off] = 0;
    return off > 0;
}

static int kn_get_exe_dir(char *out, size_t cap)
{
    char exe[1024];
    if (!kn_get_exe_path(exe, sizeof(exe))) return 0;

    size_t n = kn_strlen(exe);
    while (n > 0)
    {
        char c = exe[n - 1];
        if (c == '\\' || c == '/') break;
        n--;
    }

    if (n == 0)
    {
        kn_copy_str(out, cap, ".");
        return 1;
    }

    if (n >= cap) n = cap - 1;
    kn_memcpy(out, exe, n);
    out[n] = 0;
    return 1;
}

static char kn_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int kn_str_contains_ci(const char *s, const char *needle)
{
    if (!s || !needle || !needle[0]) return 0;
    size_t ns = kn_strlen(s);
    size_t nn = kn_strlen(needle);
    if (nn > ns) return 0;
    for (size_t i = 0; i + nn <= ns; i++)
    {
        size_t j = 0;
        while (j < nn)
        {
            if (kn_to_lower(s[i + j]) != kn_to_lower(needle[j])) break;
            j++;
        }
        if (j == nn) return 1;
    }
    return 0;
}

static int kn_triple_is_windows(const char *triple)
{
    return kn_str_contains_ci(triple, "windows");
}

static int kn_triple_is_linux(const char *triple)
{
    return kn_str_contains_ci(triple, "linux");
}

static int kn_triple_is_macos(const char *triple)
{
    return kn_str_contains_ci(triple, "darwin") || kn_str_contains_ci(triple, "apple") || kn_str_contains_ci(triple, "macos");
}

static int kn_triple_is_x64(const char *triple)
{
    return kn_str_contains_ci(triple, "x86_64") || kn_str_contains_ci(triple, "amd64");
}

static int kn_triple_is_x86(const char *triple)
{
    if (kn_triple_is_x64(triple)) return 0;
    return kn_str_contains_ci(triple, "i386") || kn_str_contains_ci(triple, "i486") ||
           kn_str_contains_ci(triple, "i586") || kn_str_contains_ci(triple, "i686") ||
           kn_str_contains_ci(triple, "x86");
}

static int kn_triple_is_arm64(const char *triple)
{
    return kn_str_contains_ci(triple, "aarch64") || kn_str_contains_ci(triple, "arm64");
}

static int kn_path_has_ext(const char *path)
{
    if (!path || !path[0]) return 0;
    size_t n = kn_strlen(path);
    for (size_t i = n; i > 0; i--)
    {
        char c = path[i - 1];
        if (c == '.')
            return 1;
        if (kn_has_sep(c))
            return 0;
    }
    return 0;
}

static void kn_append_windows_lib_name(char *cmd, size_t cap, size_t *off, const char *name)
{
    char buf[1024];
    size_t n = 0;
    if (!name || !name[0]) return;
    if (kn_path_has_ext(name))
    {
        kn_append_quoted(cmd, cap, off, name);
        return;
    }
    buf[0] = 0;
    kn_append(buf, sizeof(buf), &n, name);
    kn_append(buf, sizeof(buf), &n, ".lib");
    kn_append_quoted(cmd, cap, off, buf);
}

static void kn_dump_link_search_header(void);
static void kn_dump_link_search_dir_value(const char *dir);
static void kn_dump_link_search_dirs_from_items(const KnLinkItem *items, int item_count);
static int kn_path_parent(char *out, size_t cap, const char *path);

static void kn_dump_link_search_dirs(const KnLinkOptions *opts)
{
    if (!opts || !opts->show_link_search) return;
    kn_dump_link_search_header();
    kn_dump_link_search_dirs_from_items(opts->items, opts->item_count);
}

static void kn_append_link_items_windows(char *cmd, size_t cap, size_t *off, const KnLinkItem *items, int item_count)
{
    for (int i = 0; i < item_count; i++)
    {
        const KnLinkItem *item = &items[i];
        const char *value = item->value;
        if (!value || !value[0]) continue;
        if (item->kind == KN_LINK_ITEM_FILE)
        {
            kn_append(cmd, cap, off, " ");
            kn_append_quoted(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_DIR)
        {
            kn_append(cmd, cap, off, " /libpath:");
            kn_append_quoted(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_NAME)
        {
            kn_append(cmd, cap, off, " ");
            kn_append_windows_lib_name(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_ARG)
        {
            kn_append(cmd, cap, off, " ");
            kn_append(cmd, cap, off, value);
        }
    }
}

static void kn_append_link_items_gnu(char *cmd, size_t cap, size_t *off, const KnLinkItem *items, int item_count)
{
    for (int i = 0; i < item_count; i++)
    {
        const KnLinkItem *item = &items[i];
        const char *value = item->value;
        if (!value || !value[0]) continue;
        if (item->kind == KN_LINK_ITEM_FILE)
        {
            kn_append(cmd, cap, off, " ");
            kn_append_quoted(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_DIR)
        {
            kn_append(cmd, cap, off, " -L");
            kn_append_quoted(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_NAME)
        {
            kn_append(cmd, cap, off, " -l");
            kn_append(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_ARG)
        {
            kn_append(cmd, cap, off, " -Xlinker ");
            kn_append_quoted(cmd, cap, off, value);
        }
    }
}

static int kn_get_env(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return 0;
    out[0] = 0;
    KN_DWORD n = GetEnvironmentVariableA(name, out, (KN_DWORD)cap);
    if (n == 0 || n >= (KN_DWORD)cap)
    {
        out[0] = 0;
        return 0;
    }
    return 1;
}

static int kn_tool_name_has_path(const char *name)
{
    if (!name || !name[0]) return 0;
    for (size_t i = 0; name[i]; i++)
    {
        if (name[i] == '\\' || name[i] == '/' || name[i] == ':')
            return 1;
    }
    return 0;
}

static int kn_tool_name_without_exe_suffix(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return 0;
    out[0] = 0;
#if defined(_WIN32) || defined(_WIN64)
    (void)name;
    return 0;
#else
    size_t n = kn_strlen(name);
    if (n <= 4 || n - 4 >= cap) return 0;
    if (kn_to_lower(name[n - 4]) != '.' || kn_to_lower(name[n - 3]) != 'e' ||
        kn_to_lower(name[n - 2]) != 'x' || kn_to_lower(name[n - 1]) != 'e')
        return 0;
    kn_memcpy(out, name, n - 4);
    out[n - 4] = 0;
    return 1;
#endif
}

static int kn_resolve_tool_in_dir(char *out, size_t cap, const char *dir, const char *name)
{
    char candidate[1024];
    char alt_name[1024];
    if (!out || cap == 0 || !dir || !dir[0] || !name || !name[0]) return 0;
    if (kn_path_join(candidate, sizeof(candidate), dir, name) && kn_file_exists(candidate))
    {
        kn_copy_str(out, cap, candidate);
        return 1;
    }
    if (kn_tool_name_without_exe_suffix(alt_name, sizeof(alt_name), name) &&
        kn_path_join(candidate, sizeof(candidate), dir, alt_name) && kn_file_exists(candidate))
    {
        kn_copy_str(out, cap, candidate);
        return 1;
    }
    return 0;
}

static char kn_path_env_sep(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ';';
#else
    return ':';
#endif
}

static int kn_try_resolve_tool_on_path(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return 0;
    out[0] = 0;

    if (kn_tool_name_has_path(name))
    {
        if (kn_file_exists(name))
        {
            kn_copy_str(out, cap, name);
            return 1;
        }
        return 0;
    }

    if (kn_file_exists(name))
    {
        kn_copy_str(out, cap, name);
        return 1;
    }

    char path_env[32768];
    if (!kn_get_env(path_env, sizeof(path_env), "PATH"))
        return 0;

    size_t start = 0;
    const char sep = kn_path_env_sep();
    while (path_env[start])
    {
        size_t end = start;
        while (path_env[end] && path_env[end] != sep)
            end++;

        char dir[1024];
        size_t len = end - start;
        if (len > 0 && len < sizeof(dir))
        {
            size_t off = 0;
            size_t src = start;
            if (path_env[src] == '"')
                src++;
            while (src < end && off + 1 < sizeof(dir))
            {
                char ch = path_env[src++];
                if (ch == '"')
                    continue;
                dir[off++] = ch;
            }
            dir[off] = 0;
            if (dir[0])
            {
                char candidate[1024];
                if (kn_path_join(candidate, sizeof(candidate), dir, name) && kn_file_exists(candidate))
                {
                    kn_copy_str(out, cap, candidate);
                    return 1;
                }
            }
        }

        start = path_env[end] ? (end + 1) : end;
    }
    return 0;
}

static int kn_try_env_tool(char *out, size_t cap, const char *name, const char *env_exe)
{
    char p[1024];
    if (env_exe && kn_get_env(p, sizeof(p), env_exe) && kn_file_exists(p))
    {
        kn_copy_str(out, cap, p);
        return 1;
    }
    if (kn_get_env(p, sizeof(p), "KINAL_LINKER_DIR"))
    {
        if (kn_resolve_tool_in_dir(out, cap, p, name))
            return 1;
    }
    return 0;
}

static void kn_resolve_tool(char *out, size_t cap, const char *name)
{
    char p[1024];
    char exe_dir[1024];
    const char *env_exe = 0;
    if (kn_strcmp(name, "lld-link.exe") == 0) env_exe = "KINAL_LINKER_LLD";
    else if (kn_strcmp(name, "link.exe") == 0) env_exe = "KINAL_LINKER_MSVC";
    else if (kn_strcmp(name, "clang.exe") == 0) env_exe = "KINAL_CLANG";
    else if (kn_strcmp(name, "llvm-lib.exe") == 0) env_exe = "KINAL_LLVM_LIB";

    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (kn_path_join(p, sizeof(p), exe_dir, "linker") &&
            kn_resolve_tool_in_dir(out, cap, p, name))
        {
            return;
        }
        if (env_exe && kn_try_env_tool(out, cap, name, env_exe))
            return;
        if (kn_path_join(p, sizeof(p), exe_dir, "llvm") &&
            kn_path_join(p, sizeof(p), p, "bin") &&
            kn_resolve_tool_in_dir(out, cap, p, name))
        {
            return;
        }
        if (kn_resolve_tool_in_dir(out, cap, exe_dir, name))
        {
            return;
        }
    }

    if (env_exe && kn_try_env_tool(out, cap, name, env_exe))
        return;

    if (KN_LLVM_BIN[0] && kn_resolve_tool_in_dir(out, cap, KN_LLVM_BIN, name))
    {
        return;
    }

    if (kn_try_resolve_tool_on_path(out, cap, name))
        return;

    {
        char alt_name[1024];
        if (kn_tool_name_without_exe_suffix(alt_name, sizeof(alt_name), name) &&
            kn_try_resolve_tool_on_path(out, cap, alt_name))
            return;
        if (alt_name[0])
        {
            kn_copy_str(out, cap, alt_name);
            return;
        }
    }

    kn_copy_str(out, cap, name);
}

static int kn_resolve_runtime_src(char *out, size_t cap)
{
    char p[1024];
    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\src\\kn_runtime.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\kn_runtime.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "..\\runtime\\src\\kn_runtime.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "kinal-compiler\\src\\kn_runtime.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "..\\kinal-compiler\\src\\kn_runtime.c") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
    }

    if (kn_file_exists("runtime/src/kn_runtime.c"))
    {
        kn_copy_str(out, cap, "runtime/src/kn_runtime.c");
        return 1;
    }
    if (kn_file_exists("libs/runtime/src/kn_runtime.c"))
    {
        kn_copy_str(out, cap, "libs/runtime/src/kn_runtime.c");
        return 1;
    }
    if (kn_file_exists("kinal-compiler/src/kn_runtime.c"))
    {
        kn_copy_str(out, cap, "kinal-compiler/src/kn_runtime.c");
        return 1;
    }
    if (kn_file_exists("runtime/kn_runtime.c"))
    {
        kn_copy_str(out, cap, "runtime/kn_runtime.c");
        return 1;
    }
    return 0;
}

static int kn_resolve_runtime_include(char *out, size_t cap)
{
    char p[1024];
    char hdr[1024];
    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\include") &&
            kn_path_join(hdr, sizeof(hdr), p, "kn\\platform.h") && kn_file_exists(hdr))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "..\\runtime\\include") &&
            kn_path_join(hdr, sizeof(hdr), p, "kn\\platform.h") && kn_file_exists(hdr))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "kinal-compiler\\include") &&
            kn_path_join(hdr, sizeof(hdr), p, "kn\\platform.h") && kn_file_exists(hdr))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "..\\kinal-compiler\\include") &&
            kn_path_join(hdr, sizeof(hdr), p, "kn\\platform.h") && kn_file_exists(hdr))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
    }
    if (kn_file_exists("runtime/include/kn/platform.h"))
    {
        kn_copy_str(out, cap, "runtime/include");
        return 1;
    }
    if (kn_file_exists("libs/runtime/include/kn/platform.h"))
    {
        kn_copy_str(out, cap, "libs/runtime/include");
        return 1;
    }
    if (kn_file_exists("kinal-compiler/include/kn/platform.h"))
    {
        kn_copy_str(out, cap, "kinal-compiler/include");
        return 1;
    }
    return 0;
}

static int kn_resolve_runtime_obj(const char *triple, char *out, size_t cap)
{
    char p[1024];
    const int is_linux = kn_triple_is_linux(triple ? triple : "");
    const int is_macos = kn_triple_is_macos(triple ? triple : "");
    const int is_windows = kn_triple_is_windows(triple ? triple : "");

    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (is_linux)
        {
            const int is_arm64 = kn_triple_is_arm64(triple ? triple : "");
            const char *rt_rel = is_arm64 ? "runtime\\linux-arm64\\kn_runtime.o" : "runtime\\linux-x64\\kn_runtime.o";
            if (kn_path_join(p, sizeof(p), exe_dir, rt_rel) && kn_file_exists(p))
            {
                kn_copy_str(out, cap, p);
                return 1;
            }
            return 0;
        }

        if (is_macos)
        {
            const int is_arm64 = kn_triple_is_arm64(triple ? triple : "");
            const char *rt_rel = is_arm64 ? "runtime\\macos-arm64\\kn_runtime.o" : "runtime\\macos-x64\\kn_runtime.o";
            if (kn_path_join(p, sizeof(p), exe_dir, rt_rel) && kn_file_exists(p))
            {
                kn_copy_str(out, cap, p);
                return 1;
            }
            return 0;
        }

        if (is_windows)
        {
            const int is_x86 = kn_triple_is_x86(triple ? triple : "");
            const char *rt_rel = is_x86 ? "runtime\\win-x86\\kn_runtime.obj" : "runtime\\win-x64\\kn_runtime.obj";
            if (kn_path_join(p, sizeof(p), exe_dir, rt_rel) && kn_file_exists(p))
            {
                kn_copy_str(out, cap, p);
                return 1;
            }
            if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\kn_runtime.obj") && kn_file_exists(p))
            {
                kn_copy_str(out, cap, p);
                return 1;
            }
            if (kn_path_join(p, sizeof(p), exe_dir, "kn_runtime.obj") && kn_file_exists(p))
            {
                kn_copy_str(out, cap, p);
                return 1;
            }
        }
    }

    if (!is_linux && KN_RUNTIME_OBJ[0] && kn_file_exists(KN_RUNTIME_OBJ))
    {
        kn_copy_str(out, cap, KN_RUNTIME_OBJ);
        return 1;
    }

    return 0;
}

static int kn_resolve_math_obj(const char *triple, char *out, size_t cap)
{
    char p[1024];
    char exe_dir[1024];
    if (!kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
        return 0;
    const int is_linux = kn_triple_is_linux(triple ? triple : "");
    const int is_macos = kn_triple_is_macos(triple ? triple : "");
    const int is_windows = kn_triple_is_windows(triple ? triple : "");
    const int is_x86 = kn_triple_is_x86(triple ? triple : "");
    if (is_linux)
    {
        const int is_arm64 = kn_triple_is_arm64(triple ? triple : "");
        const char *rel = is_arm64 ? "runtime\\linux-arm64\\kn_math.o" : "runtime\\linux-x64\\kn_math.o";
        if (kn_path_join(p, sizeof(p), exe_dir, rel) && kn_file_exists(p))
        { kn_copy_str(out, cap, p); return 1; }
        return 0;
    }
    else if (is_macos)
    {
        const int is_arm64 = kn_triple_is_arm64(triple ? triple : "");
        const char *rel = is_arm64 ? "runtime\\macos-arm64\\kn_math.o" : "runtime\\macos-x64\\kn_math.o";
        if (kn_path_join(p, sizeof(p), exe_dir, rel) && kn_file_exists(p))
        { kn_copy_str(out, cap, p); return 1; }
        return 0;
    }
    else if (is_windows)
    {
        const char *rel = is_x86 ? "runtime\\win-x86\\kn_math.obj" : "runtime\\win-x64\\kn_math.obj";
        if (kn_path_join(p, sizeof(p), exe_dir, rel) && kn_file_exists(p))
        { kn_copy_str(out, cap, p); return 1; }
    }
    return 0;
}

static int kn_resolve_ucrtbase_lib(const char *triple, char *out, size_t cap)
{
    char p[1024];
    char exe_dir[1024];
    if (!kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
        return 0;
    const int is_x86 = kn_triple_is_x86(triple ? triple : "");
    const char *rel = is_x86 ? "runtime\\win-x86\\ucrtbase.lib" : "runtime\\win-x64\\ucrtbase.lib";
    if (kn_path_join(p, sizeof(p), exe_dir, rel) && kn_file_exists(p))
    { kn_copy_str(out, cap, p); return 1; }
    return 0;
}

static int kn_resolve_kernel32_lib(const char *triple, char *out, size_t cap)
{
    char p[1024];

    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        const int is_x86 = kn_triple_is_x86(triple ? triple : "");
        const char *k32_rel = is_x86 ? "runtime\\win-x86\\kernel32.lib" : "runtime\\win-x64\\kernel32.lib";
        if (kn_path_join(p, sizeof(p), exe_dir, k32_rel) && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\kernel32.lib") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "kernel32.lib") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
    }

    if (KN_KERNEL32_LIB[0] && kn_file_exists(KN_KERNEL32_LIB))
    {
        kn_copy_str(out, cap, KN_KERNEL32_LIB);
        return 1;
    }

    return 0;
}

static int kn_resolve_user32_lib(const char *triple, char *out, size_t cap)
{
    char p[1024];

    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        const int is_x86 = kn_triple_is_x86(triple ? triple : "");
        const char *u32_rel = is_x86 ? "runtime\\win-x86\\user32.lib" : "runtime\\win-x64\\user32.lib";
        if (kn_path_join(p, sizeof(p), exe_dir, u32_rel) && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\user32.lib") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "user32.lib") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
    }

    if (KN_USER32_LIB[0] && kn_file_exists(KN_USER32_LIB))
    {
        kn_copy_str(out, cap, KN_USER32_LIB);
        return 1;
    }

    return 0;
}

static int kn_resolve_gdi32_lib(const char *triple, char *out, size_t cap)
{
    char p[1024];

    char exe_dir[1024];
    if (kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        const int is_x86 = kn_triple_is_x86(triple ? triple : "");
        const char *g32_rel = is_x86 ? "runtime\\win-x86\\gdi32.lib" : "runtime\\win-x64\\gdi32.lib";
        if (kn_path_join(p, sizeof(p), exe_dir, g32_rel) && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "runtime\\gdi32.lib") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (kn_path_join(p, sizeof(p), exe_dir, "gdi32.lib") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
    }

    if (KN_GDI32_LIB[0] && kn_file_exists(KN_GDI32_LIB))
    {
        kn_copy_str(out, cap, KN_GDI32_LIB);
        return 1;
    }

    return 0;
}

static int kn_get_default_toolchain_cache(char *out, size_t cap)
{
    char root[1024];
    if (kn_get_env(root, sizeof(root), "KINAL_TOOLCHAIN_CACHE"))
    {
        kn_copy_str(out, cap, root);
        return 1;
    }
#if defined(_WIN32) || defined(_WIN64)
    if (kn_get_env(root, sizeof(root), "LOCALAPPDATA"))
    {
        char p[1024];
        if (kn_path_join(p, sizeof(p), root, "Kinal") &&
            kn_path_join(out, cap, p, "toolchains"))
            return 1;
    }
    if (kn_get_env(root, sizeof(root), "USERPROFILE"))
    {
        char p[1024];
        if (kn_path_join(p, sizeof(p), root, "AppData\\Local\\Kinal") &&
            kn_path_join(out, cap, p, "toolchains"))
            return 1;
    }
#else
    if (kn_get_env(root, sizeof(root), "XDG_CACHE_HOME"))
    {
        char p[1024];
        if (kn_path_join(p, sizeof(p), root, "kinal") &&
            kn_path_join(out, cap, p, "toolchains"))
            return 1;
    }
    if (kn_get_env(root, sizeof(root), "HOME"))
    {
        char p[1024];
        if (kn_path_join(p, sizeof(p), root, ".cache/kinal") &&
            kn_path_join(out, cap, p, "toolchains"))
            return 1;
    }
#endif
    out[0] = 0;
    return 0;
}

static int kn_get_default_zig_cache_root(char *out, size_t cap)
{
    char cache_root[1024];
    if (!kn_get_default_toolchain_cache(cache_root, sizeof(cache_root)))
        return 0;
    char p[1024];
    if (!kn_path_join(p, sizeof(p), cache_root, "zig"))
        return 0;
#if defined(_WIN32) || defined(_WIN64)
    return kn_path_join(out, cap, p, "win-x64-" KN_ZIG_VERSION);
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    return kn_path_join(out, cap, p, "macos-arm64-" KN_ZIG_VERSION);
#else
    return kn_path_join(out, cap, p, "macos-x64-" KN_ZIG_VERSION);
#endif
#else
#if defined(__aarch64__) || defined(__arm64__)
    return kn_path_join(out, cap, p, "linux-arm64-" KN_ZIG_VERSION);
#else
    return kn_path_join(out, cap, p, "linux-x64-" KN_ZIG_VERSION);
#endif
#endif
}

static int kn_resolve_cached_zig(char *out, size_t cap)
{
    char root[1024];
    char p[1024];
    if (!kn_get_default_zig_cache_root(root, sizeof(root)))
        return 0;
#if defined(_WIN32) || defined(_WIN64)
    if (kn_path_join(p, sizeof(p), root, "zig.exe") && kn_file_exists(p))
#else
    if (kn_path_join(p, sizeof(p), root, "zig") && kn_file_exists(p))
#endif
    {
        kn_copy_str(out, cap, p);
        return 1;
    }
    return 0;
}

static int kn_resolve_python(char *out, size_t cap)
{
    char p[1024];
    if (kn_get_env(p, sizeof(p), "PYTHON") && kn_file_exists(p))
    {
        kn_copy_str(out, cap, p);
        return 1;
    }
#if defined(_WIN32) || defined(_WIN64)
    if (kn_try_resolve_tool_on_path(out, cap, "python.exe")) return 1;
    if (kn_try_resolve_tool_on_path(out, cap, "py.exe")) return 1;
#endif
    if (kn_try_resolve_tool_on_path(out, cap, "python")) return 1;
    if (kn_try_resolve_tool_on_path(out, cap, "python3")) return 1;
    out[0] = 0;
    return 0;
}

static int kn_resolve_setup_zig_script(char *out, size_t cap)
{
    char exe_dir[1024];
    char cur[1024];
    char p[1024];
    if (kn_file_exists("infra/toolchains/setup_zig.py"))
    {
        kn_copy_str(out, cap, "infra/toolchains/setup_zig.py");
        return 1;
    }
    if (!kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
        return 0;
    kn_copy_str(cur, sizeof(cur), exe_dir);
    for (int depth = 0; depth < 6; depth++)
    {
        if (kn_path_join(p, sizeof(p), cur, "infra/toolchains/setup_zig.py") && kn_file_exists(p))
        {
            kn_copy_str(out, cap, p);
            return 1;
        }
        if (!kn_path_parent(cur, sizeof(cur), cur))
            break;
    }
    out[0] = 0;
    return 0;
}

static int kn_ensure_zig_downloaded(void)
{
    char python[1024];
    char script[1024];
    if (!kn_resolve_python(python, sizeof(python)))
        return 0;
    if (!kn_resolve_setup_zig_script(script, sizeof(script)))
        return 0;

    char cmd[4096];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, python);
    kn_append(cmd, sizeof(cmd), &off, " ");
    kn_append_quoted(cmd, sizeof(cmd), &off, script);
    kn_append(cmd, sizeof(cmd), &off, " --fetch-prebuilt");
    return kn_run_process(python, cmd) == 0;
}

static int kn_resolve_zig(char *out, size_t cap, int allow_download)
{
    char p[1024];
    if (kn_get_env(p, sizeof(p), "KINAL_LINKER_ZIG") && kn_file_exists(p))
    {
        kn_copy_str(out, cap, p);
        return 1;
    }
    if (kn_get_env(p, sizeof(p), "KINAL_ZIG") && kn_file_exists(p))
    {
        kn_copy_str(out, cap, p);
        return 1;
    }
    if (kn_get_env(p, sizeof(p), "KINAL_LINKER_DIR"))
    {
        char q[1024];
#if defined(_WIN32) || defined(_WIN64)
        if (kn_path_join(q, sizeof(q), p, "zig.exe") && kn_file_exists(q))
        {
            kn_copy_str(out, cap, q);
            return 1;
        }
#else
        if (kn_path_join(q, sizeof(q), p, "zig") && kn_file_exists(q))
        {
            kn_copy_str(out, cap, q);
            return 1;
        }
#endif
    }
#if defined(_WIN32) || defined(_WIN64)
    if (kn_try_resolve_tool_on_path(out, cap, "zig.exe"))
#else
    if (kn_try_resolve_tool_on_path(out, cap, "zig"))
#endif
        return 1;
    if (kn_resolve_cached_zig(out, cap))
        return 1;
    if (allow_download)
    {
        kn_write_str("[Link] zig not found, bootstrapping to local toolchain cache ...\n");
        if (kn_ensure_zig_downloaded() && kn_resolve_cached_zig(out, cap))
            return 1;
    }
    out[0] = 0;
    return 0;
}

static int kn_check_tool_version(const char *tool)
{
    if (!tool || !tool[0]) return 0;
    char cmd[2048];
    size_t off = 0;
    cmd[0] = 0;
    kn_append_quoted(cmd, sizeof(cmd), &off, tool);
    kn_append(cmd, sizeof(cmd), &off, " version");
    return kn_run_process(tool, cmd) == 0;
}

static int kn_dir_exists(const char *path)
{
    if (!path || !path[0]) return 0;
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(path, &data);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
        return 0;
    int ok = (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY) != 0;
    FindClose(h);
    return ok;
}

static int kn_strcmp_ci(const char *a, const char *b)
{
    size_t i = 0;
    for (;; i++)
    {
        char ca = kn_to_lower(a ? a[i] : 0);
        char cb = kn_to_lower(b ? b[i] : 0);
        if (ca != cb)
            return (unsigned char)ca < (unsigned char)cb ? -1 : 1;
        if (ca == 0)
            return 0;
    }
}

static void kn_trim_trailing_seps(char *path)
{
    if (!path) return;
    size_t n = kn_strlen(path);
    while (n > 0 && kn_has_sep(path[n - 1]))
        n--;
    path[n] = 0;
}

static int kn_path_parent(char *out, size_t cap, const char *path)
{
    if (!out || cap == 0 || !path || !path[0]) return 0;
    kn_copy_str(out, cap, path);
    kn_trim_trailing_seps(out);
    size_t n = kn_strlen(out);
    while (n > 0 && !kn_has_sep(out[n - 1]))
        n--;
    while (n > 0 && kn_has_sep(out[n - 1]))
        n--;
    if (n == 0)
    {
        out[0] = 0;
        return 0;
    }
    out[n] = 0;
    return 1;
}

static const char *kn_path_base_name(const char *path)
{
    if (!path) return "";
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (kn_has_sep(*p))
            base = p + 1;
    return base;
}

static int kn_find_latest_child_dir(const char *root, char *out, size_t cap)
{
    if (!root || !root[0] || !out || cap == 0) return 0;
    char pattern[1024];
    if (!kn_path_join(pattern, sizeof(pattern), root, "*"))
        return 0;

    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(pattern, &data);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
        return 0;

    char best_name[KN_MAX_PATH];
    best_name[0] = 0;
    int best_is_version = 0;
    do
    {
        if ((data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY) == 0)
            continue;
        if (kn_strcmp(data.cFileName, ".") == 0 || kn_strcmp(data.cFileName, "..") == 0)
            continue;
        int is_version = 1;
        for (size_t i = 0; data.cFileName[i]; i++)
        {
            char c = data.cFileName[i];
            if (!((c >= '0' && c <= '9') || c == '.'))
            {
                is_version = 0;
                break;
            }
        }
        if (!best_name[0] ||
            (is_version && !best_is_version) ||
            (is_version == best_is_version && kn_strcmp_ci(data.cFileName, best_name) > 0))
        {
            kn_copy_str(best_name, sizeof(best_name), data.cFileName);
            best_is_version = is_version;
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);

    if (!best_name[0])
        return 0;
    return kn_path_join(out, cap, root, best_name) && kn_dir_exists(out);
}

static int kn_path_replace_ext(char *out, size_t cap, const char *path, const char *ext)
{
    if (!out || cap == 0 || !path || !ext) return 0;
    size_t n = kn_strlen(path);
    size_t dot = n;
    for (size_t i = n; i > 0; i--)
    {
        char c = path[i - 1];
        if (c == '.')
        {
            dot = i - 1;
            break;
        }
        if (kn_has_sep(c))
            break;
    }
    size_t le = kn_strlen(ext);
    if (dot + le + 1 > cap) return 0;
    kn_memcpy(out, path, dot);
    kn_memcpy(out + dot, ext, le);
    out[dot + le] = 0;
    return 1;
}

static void kn_dump_link_search_header(void)
{
    kn_write_str("[LinkSearch]\n");
}

static void kn_dump_link_search_dir_value(const char *dir)
{
    if (!dir || !dir[0]) return;
    kn_write_str("  ");
    kn_write_str(dir);
    kn_write_str("\n");
}

static void kn_dump_link_search_dirs_from_items(const KnLinkItem *items, int item_count)
{
    if (!items) return;
    for (int i = 0; i < item_count; i++)
    {
        const KnLinkItem *item = &items[i];
        if (item->kind != KN_LINK_ITEM_LIB_DIR || !item->value || !item->value[0])
            continue;
        kn_dump_link_search_dir_value(item->value);
    }
}

typedef struct
{
    char bundle_lib_dir[1024];
    char msvc_lib[1024];
    char sdk_um_lib[1024];
    char sdk_ucrt_lib[1024];
    int has_bundle_lib_dir;
    int has_bundle_system_libs;
    int has_bundle_dynamic_crt;
    int has_bundle_static_crt;
    int has_msvc_lib;
    int has_sdk_um_lib;
    int has_sdk_ucrt_lib;
} KnWindowsToolchainDirs;

static int kn_detect_windows_sdk_subdir(char *out, size_t cap, const char *version_root, const char *kind, const char *triple);
static int kn_windows_crt_mode(const KnLinkOptions *opts);

static const char *kn_windows_arch_dir(const char *triple)
{
    return kn_triple_is_x86(triple ? triple : "") ? "x86" : "x64";
}

static int kn_dir_has_named_lib(const char *dir, const char *name)
{
    char p[1024];
    if (!dir || !dir[0] || !name || !name[0])
        return 0;
    if (!kn_path_join(p, sizeof(p), dir, name))
        return 0;
    return kn_file_exists(p);
}

static int kn_detect_windows_bundle_lib_dir(const char *triple, char *out, size_t cap)
{
    char exe_dir[1024];
    if (!out || cap == 0)
        return 0;
    out[0] = 0;
    if (!kn_get_exe_dir(exe_dir, sizeof(exe_dir)))
        return 0;
    if (!kn_path_join(out, cap, exe_dir, kn_triple_is_x86(triple ? triple : "") ? "runtime\\win-x86" : "runtime\\win-x64"))
        return 0;
    return kn_dir_exists(out);
}

static int kn_try_env_dir(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return 0;
    if (!kn_get_env(out, cap, name))
        return 0;
    kn_trim_trailing_seps(out);
    return kn_dir_exists(out);
}

static int kn_drive_root(char *out, size_t cap, char letter)
{
    if (!out || cap < 4) return 0;
    out[0] = letter;
    out[1] = ':';
    out[2] = '\\';
    out[3] = 0;
    return 1;
}

static int kn_find_vs_layout_dir(const char *vs_root, const char *triple, char *out, size_t cap)
{
    static const char *years[] = { "2022", "2019", "2017" };
    static const char *editions[] = { "Enterprise", "Professional", "Community", "BuildTools" };
    if (!vs_root || !vs_root[0] || !out || cap == 0)
        return 0;
    for (int yi = 0; yi < (int)(sizeof(years) / sizeof(years[0])); yi++)
    {
        for (int ei = 0; ei < (int)(sizeof(editions) / sizeof(editions[0])); ei++)
        {
            char tools[1024];
            char latest[1024];
            if (!kn_path_join(tools, sizeof(tools), vs_root, years[yi])) continue;
            if (!kn_path_join(tools, sizeof(tools), tools, editions[ei])) continue;
            if (!kn_path_join(tools, sizeof(tools), tools, "VC\\Tools\\MSVC")) continue;
            if (!kn_find_latest_child_dir(tools, latest, sizeof(latest))) continue;
            if (!kn_path_join(out, cap, latest, "lib")) continue;
            if (!kn_path_join(out, cap, out, kn_windows_arch_dir(triple))) continue;
            if (kn_dir_exists(out))
                return 1;
        }
    }
    out[0] = 0;
    return 0;
}

static int kn_find_vs_link_exe(const char *vs_root, const char *arch, char *out, size_t cap)
{
    static const char *years[] = { "2022", "2019", "2017" };
    static const char *editions[] = { "Enterprise", "Professional", "Community", "BuildTools" };
    if (!vs_root || !vs_root[0] || !out || cap == 0 || !arch || !arch[0])
        return 0;
    for (int yi = 0; yi < (int)(sizeof(years) / sizeof(years[0])); yi++)
    {
        for (int ei = 0; ei < (int)(sizeof(editions) / sizeof(editions[0])); ei++)
        {
            char tools[1024];
            char latest[1024];
            if (!kn_path_join(tools, sizeof(tools), vs_root, years[yi])) continue;
            if (!kn_path_join(tools, sizeof(tools), tools, editions[ei])) continue;
            if (!kn_path_join(tools, sizeof(tools), tools, "VC\\Tools\\MSVC")) continue;
            if (!kn_find_latest_child_dir(tools, latest, sizeof(latest))) continue;
            if (!kn_path_join(out, cap, latest, "bin\\Hostx64")) continue;
            if (!kn_path_join(out, cap, out, arch)) continue;
            if (!kn_path_join(out, cap, out, "link.exe")) continue;
            if (kn_file_exists(out))
                return 1;
        }
    }
    out[0] = 0;
    return 0;
}

static int kn_scan_drive_roots_for_vs_layout(const char *triple, char *out, size_t cap)
{
    if (!out || cap == 0)
        return 0;
    for (char drive = 'C'; drive <= 'Z'; drive++)
    {
        char root[8];
        char vs_root[1024];
        if (!kn_drive_root(root, sizeof(root), drive)) continue;
        if (!kn_path_join(vs_root, sizeof(vs_root), root, "Microsoft Visual Studio")) continue;
        if (kn_find_vs_layout_dir(vs_root, triple, out, cap))
            return 1;
    }
    out[0] = 0;
    return 0;
}

static int kn_scan_drive_roots_for_vs_link(const char *arch, char *out, size_t cap)
{
    if (!out || cap == 0 || !arch || !arch[0])
        return 0;
    for (char drive = 'C'; drive <= 'Z'; drive++)
    {
        char root[8];
        char vs_root[1024];
        if (!kn_drive_root(root, sizeof(root), drive)) continue;
        if (!kn_path_join(vs_root, sizeof(vs_root), root, "Microsoft Visual Studio")) continue;
        if (kn_find_vs_link_exe(vs_root, arch, out, cap))
            return 1;
    }
    out[0] = 0;
    return 0;
}

static int kn_scan_drive_roots_for_windows_sdk(const char *kind, const char *triple, char *out, size_t cap)
{
    if (!kind || !kind[0] || !out || cap == 0)
        return 0;
    for (char drive = 'C'; drive <= 'Z'; drive++)
    {
        char root[8];
        char lib_root[1024];
        if (!kn_drive_root(root, sizeof(root), drive)) continue;
        if (!kn_path_join(lib_root, sizeof(lib_root), root, "Windows Kits\\10\\Lib")) continue;
        if (kn_detect_windows_sdk_subdir(out, cap, lib_root, kind, triple))
            return 1;
    }
    out[0] = 0;
    return 0;
}

static int kn_detect_msvc_lib_dir_from_linker(const char *linker_path, const char *triple, char *out, size_t cap)
{
    if (!linker_path || !linker_path[0] || !out || cap == 0)
        return 0;
    char cur[1024];
    if (!kn_path_parent(cur, sizeof(cur), linker_path))
        return 0;
    for (int i = 0; i < 8; i++)
    {
        const char *base = kn_path_base_name(cur);
        if (kn_strcmp_ci(base, "bin") == 0)
        {
            char ver_dir[1024];
            if (!kn_path_parent(ver_dir, sizeof(ver_dir), cur))
                return 0;
            char lib_dir[1024];
            if (!kn_path_join(lib_dir, sizeof(lib_dir), ver_dir, "lib"))
                return 0;
            if (!kn_path_join(lib_dir, sizeof(lib_dir), lib_dir, kn_windows_arch_dir(triple)))
                return 0;
            if (kn_dir_exists(lib_dir))
            {
                kn_copy_str(out, cap, lib_dir);
                return 1;
            }
            return 0;
        }
        if (!kn_path_parent(cur, sizeof(cur), cur))
            break;
    }
    return 0;
}

static int kn_detect_msvc_lib_dir(const KnLinkOptions *opts, char *out, size_t cap)
{
    const char *triple = opts && opts->target_triple ? opts->target_triple : "";
    if (kn_try_env_dir(out, cap, "KINAL_MSVC_LIB"))
        return 1;

    char root[1024];
    if (kn_get_env(root, sizeof(root), "VCToolsInstallDir"))
    {
        kn_trim_trailing_seps(root);
        if (kn_path_join(out, cap, root, "lib") &&
            kn_path_join(out, cap, out, kn_windows_arch_dir(triple)) &&
            kn_dir_exists(out))
            return 1;
    }

    if (kn_get_env(root, sizeof(root), "VCINSTALLDIR"))
    {
        kn_trim_trailing_seps(root);
        char tools[1024];
        char latest[1024];
        if (kn_path_join(tools, sizeof(tools), root, "Tools\\MSVC") &&
            kn_find_latest_child_dir(tools, latest, sizeof(latest)) &&
            kn_path_join(out, cap, latest, "lib") &&
            kn_path_join(out, cap, out, kn_windows_arch_dir(triple)) &&
            kn_dir_exists(out))
            return 1;
    }

    if (opts && opts->linker_path && kn_detect_msvc_lib_dir_from_linker(opts->linker_path, triple, out, cap))
        return 1;

    if (kn_get_env(root, sizeof(root), "ProgramFiles(x86)") || kn_get_env(root, sizeof(root), "ProgramFiles"))
    {
        char vs_root[1024];
        if (kn_path_join(vs_root, sizeof(vs_root), root, "Microsoft Visual Studio") &&
            kn_find_vs_layout_dir(vs_root, triple, out, cap))
            return 1;
    }

    if (kn_scan_drive_roots_for_vs_layout(triple, out, cap))
        return 1;

    out[0] = 0;
    return 0;
}

static int kn_detect_msvc_link_exe(const KnLinkOptions *opts, char *out, size_t cap)
{
    const char *triple = opts && opts->target_triple ? opts->target_triple : "";
    const char *arch = kn_windows_arch_dir(triple);
    if (kn_get_env(out, cap, "KINAL_LINKER_MSVC") && kn_file_exists(out))
        return 1;

    char root[1024];
    if (kn_get_env(root, sizeof(root), "VCToolsInstallDir"))
    {
        kn_trim_trailing_seps(root);
        if (kn_path_join(out, cap, root, "bin\\Hostx64") &&
            kn_path_join(out, cap, out, arch) &&
            kn_path_join(out, cap, out, "link.exe") &&
            kn_file_exists(out))
            return 1;
    }

    if (kn_get_env(root, sizeof(root), "ProgramFiles(x86)") || kn_get_env(root, sizeof(root), "ProgramFiles"))
    {
        char vs_root[1024];
        if (kn_path_join(vs_root, sizeof(vs_root), root, "Microsoft Visual Studio") &&
            kn_find_vs_link_exe(vs_root, arch, out, cap))
            return 1;
    }

    if (kn_scan_drive_roots_for_vs_link(arch, out, cap))
        return 1;

    out[0] = 0;
    return 0;
}

static int kn_detect_windows_sdk_subdir(char *out, size_t cap, const char *version_root, const char *kind, const char *triple)
{
    char latest[1024];
    if (!kn_find_latest_child_dir(version_root, latest, sizeof(latest)))
        return 0;
    if (!kn_path_join(out, cap, latest, kind))
        return 0;
    if (!kn_path_join(out, cap, out, kn_windows_arch_dir(triple)))
        return 0;
    return kn_dir_exists(out);
}

static int kn_detect_windows_um_lib_dir(const KnLinkOptions *opts, char *out, size_t cap)
{
    const char *triple = opts && opts->target_triple ? opts->target_triple : "";
    if (kn_try_env_dir(out, cap, "KINAL_WINDOWS_SDK_UM_LIB"))
        return 1;

    char root[1024];
    char version[1024];
    if (kn_get_env(root, sizeof(root), "WindowsSdkDir"))
    {
        kn_trim_trailing_seps(root);
        char lib_root[1024];
        if (kn_path_join(lib_root, sizeof(lib_root), root, "Lib"))
        {
            if (kn_get_env(version, sizeof(version), "WindowsSDKLibVersion"))
            {
                kn_trim_trailing_seps(version);
                if (kn_path_join(out, cap, lib_root, version) &&
                    kn_path_join(out, cap, out, "um") &&
                    kn_path_join(out, cap, out, kn_windows_arch_dir(triple)) &&
                    kn_dir_exists(out))
                    return 1;
            }
            if (kn_detect_windows_sdk_subdir(out, cap, lib_root, "um", triple))
                return 1;
        }
    }

    if (kn_get_env(root, sizeof(root), "ProgramFiles(x86)") || kn_get_env(root, sizeof(root), "ProgramFiles"))
    {
        char lib_root[1024];
        if (kn_path_join(lib_root, sizeof(lib_root), root, "Windows Kits\\10\\Lib") &&
            kn_detect_windows_sdk_subdir(out, cap, lib_root, "um", triple))
            return 1;
    }

    if (kn_scan_drive_roots_for_windows_sdk("um", triple, out, cap))
        return 1;

    out[0] = 0;
    return 0;
}

static int kn_detect_windows_ucrt_lib_dir(const KnLinkOptions *opts, char *out, size_t cap)
{
    const char *triple = opts && opts->target_triple ? opts->target_triple : "";
    if (kn_try_env_dir(out, cap, "KINAL_WINDOWS_SDK_UCRT_LIB"))
        return 1;

    char root[1024];
    char version[1024];
    if (kn_get_env(root, sizeof(root), "UniversalCRTSdkDir"))
    {
        kn_trim_trailing_seps(root);
        char lib_root[1024];
        if (kn_path_join(lib_root, sizeof(lib_root), root, "Lib"))
        {
            if (kn_get_env(version, sizeof(version), "UCRTVersion"))
            {
                kn_trim_trailing_seps(version);
                if (kn_path_join(out, cap, lib_root, version) &&
                    kn_path_join(out, cap, out, "ucrt") &&
                    kn_path_join(out, cap, out, kn_windows_arch_dir(triple)) &&
                    kn_dir_exists(out))
                    return 1;
            }
            if (kn_detect_windows_sdk_subdir(out, cap, lib_root, "ucrt", triple))
                return 1;
        }
    }

    if (kn_get_env(root, sizeof(root), "WindowsSdkDir"))
    {
        kn_trim_trailing_seps(root);
        char lib_root[1024];
        if (kn_path_join(lib_root, sizeof(lib_root), root, "Lib"))
        {
            if (kn_get_env(version, sizeof(version), "WindowsSDKLibVersion"))
            {
                kn_trim_trailing_seps(version);
                if (kn_path_join(out, cap, lib_root, version) &&
                    kn_path_join(out, cap, out, "ucrt") &&
                    kn_path_join(out, cap, out, kn_windows_arch_dir(triple)) &&
                    kn_dir_exists(out))
                    return 1;
            }
            if (kn_detect_windows_sdk_subdir(out, cap, lib_root, "ucrt", triple))
                return 1;
        }
    }

    if (kn_get_env(root, sizeof(root), "ProgramFiles(x86)") || kn_get_env(root, sizeof(root), "ProgramFiles"))
    {
        char lib_root[1024];
        if (kn_path_join(lib_root, sizeof(lib_root), root, "Windows Kits\\10\\Lib") &&
            kn_detect_windows_sdk_subdir(out, cap, lib_root, "ucrt", triple))
            return 1;
    }

    if (kn_scan_drive_roots_for_windows_sdk("ucrt", triple, out, cap))
        return 1;

    out[0] = 0;
    return 0;
}

static void kn_detect_windows_toolchain_dirs(const KnLinkOptions *opts, KnWindowsToolchainDirs *dirs)
{
    if (!dirs) return;
    kn_memset(dirs, 0, sizeof(*dirs));
    dirs->has_bundle_lib_dir = kn_detect_windows_bundle_lib_dir(
        opts && opts->target_triple ? opts->target_triple : "",
        dirs->bundle_lib_dir,
        sizeof(dirs->bundle_lib_dir));
    if (dirs->has_bundle_lib_dir)
    {
        dirs->has_bundle_system_libs =
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "kernel32.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "user32.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "gdi32.lib");
        dirs->has_bundle_dynamic_crt =
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "oldnames.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "msvcrt.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "vcruntime.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "ucrt.lib");
        dirs->has_bundle_static_crt =
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "oldnames.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "libcmt.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "libvcruntime.lib") &&
            kn_dir_has_named_lib(dirs->bundle_lib_dir, "libucrt.lib");
        if (dirs->has_bundle_system_libs &&
            ((kn_windows_crt_mode(opts) == KN_CRT_STATIC && dirs->has_bundle_static_crt) ||
             (kn_windows_crt_mode(opts) != KN_CRT_STATIC && dirs->has_bundle_dynamic_crt)))
            return;
    }
    dirs->has_msvc_lib = kn_detect_msvc_lib_dir(opts, dirs->msvc_lib, sizeof(dirs->msvc_lib));
    dirs->has_sdk_um_lib = kn_detect_windows_um_lib_dir(opts, dirs->sdk_um_lib, sizeof(dirs->sdk_um_lib));
    dirs->has_sdk_ucrt_lib = kn_detect_windows_ucrt_lib_dir(opts, dirs->sdk_ucrt_lib, sizeof(dirs->sdk_ucrt_lib));
}

static int kn_windows_crt_mode(const KnLinkOptions *opts)
{
    if (!opts) return KN_CRT_AUTO;
    if (opts->crt_mode == KN_CRT_STATIC)
        return KN_CRT_STATIC;
    if (opts->crt_mode == KN_CRT_DYNAMIC)
        return KN_CRT_DYNAMIC;
    return KN_CRT_DYNAMIC;
}

static void kn_dump_windows_toolchain_search_dirs(const KnLinkOptions *opts, const KnWindowsToolchainDirs *dirs)
{
    if (!opts || !opts->show_link_search) return;
    kn_dump_link_search_header();
    kn_dump_link_search_dirs_from_items(opts->items, opts->item_count);
    if (dirs)
    {
        if (dirs->has_bundle_lib_dir) kn_dump_link_search_dir_value(dirs->bundle_lib_dir);
        if (dirs->has_sdk_um_lib) kn_dump_link_search_dir_value(dirs->sdk_um_lib);
        if (dirs->has_sdk_ucrt_lib) kn_dump_link_search_dir_value(dirs->sdk_ucrt_lib);
        if (dirs->has_msvc_lib) kn_dump_link_search_dir_value(dirs->msvc_lib);
    }
}
