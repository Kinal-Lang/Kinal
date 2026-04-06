static bool strlist_contains(const StrList *l, const char *s)
{
    if (!l || !s) return false;
    for (int i = 0; i < l->count; i++)
    {
        if (kn_strcmp(l->items[i], s) == 0)
            return true;
    }
    return false;
}

static void strlist_add(StrList *l, const char *s)
{
    if (!l || !s) return;
    if (strlist_contains(l, s)) return;
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        const char **n = (const char **)kn_malloc(sizeof(char *) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(char *) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = s;
}

static bool link_item_contains(const KnLinkItem *items, int count, int kind, const char *value)
{
    if (!items || !value) return false;
    for (int i = 0; i < count; i++)
    {
        if (items[i].kind != kind) continue;
        if (items[i].value && kn_strcmp(items[i].value, value) == 0)
            return true;
    }
    return false;
}

static bool link_item_add(KnLinkItem *items, int *count, int cap, int kind, const char *value)
{
    if (!items || !count || !value || !value[0]) return false;
    if (*count >= cap) return false;
    items[*count].kind = kind;
    items[*count].value = value;
    (*count)++;
    return true;
}

static ModulePath *modulemap_find(ModuleMap *m, const char *module)
{
    if (!m || !module) return 0;
    for (int i = 0; i < m->count; i++)
    {
        if (kn_strcmp(m->items[i].module, module) == 0)
            return &m->items[i];
    }
    return 0;
}

static void modulemap_add(ModuleMap *m, const char *module, const char *path)
{
    if (!m || !module || !path) return;
    ModulePath *entry = modulemap_find(m, module);
    if (entry)
    {
        strlist_add(&entry->paths, path);
        return;
    }
    if (m->count + 1 > m->cap)
    {
        int new_cap = m->cap ? m->cap * 2 : 8;
        ModulePath *n = (ModulePath *)kn_malloc(sizeof(ModulePath) * (size_t)new_cap);
        if (m->items)
        {
            kn_memcpy(n, m->items, sizeof(ModulePath) * (size_t)m->count);
            kn_free(m->items);
        }
        m->items = n;
        m->cap = new_cap;
    }
    m->items[m->count].module = module;
    kn_memset(&m->items[m->count].paths, 0, sizeof(StrList));
    strlist_add(&m->items[m->count].paths, path);
    m->count++;
}

static void sourcelist_add(SourceList *l, KnSource *src)
{
    if (!l || !src) return;
    if (l->count + 1 > l->cap)
    {
        int new_cap = l->cap ? l->cap * 2 : 8;
        KnSource **n = (KnSource **)kn_malloc(sizeof(KnSource *) * (size_t)new_cap);
        if (l->items)
        {
            kn_memcpy(n, l->items, sizeof(KnSource *) * (size_t)l->count);
            kn_free(l->items);
        }
        l->items = n;
        l->cap = new_cap;
    }
    l->items[l->count++] = src;
}

static char *dup_str(const char *s)
{
    size_t n = kn_strlen(s);
    char *out = (char *)kn_malloc(n + 1);
    if (!out) return 0;
    kn_memcpy(out, s, n);
    out[n] = 0;
    return out;
}

static bool is_path_sep(char c)
{
    return c == '\\' || c == '/';
}

static void path_dirname(const char *path, char *out, size_t cap)
{
    size_t len = kn_strlen(path);
    size_t i = len;
    while (i > 0 && !is_path_sep(path[i - 1]))
        i--;
    if (i == 0)
    {
        if (cap > 2)
        {
            out[0] = '.';
            out[1] = 0;
        }
        return;
    }
    if (i >= cap) i = cap - 1;
    kn_memcpy(out, path, i);
    out[i] = 0;
}

static bool path_join(char *out, size_t cap, const char *a, const char *b)
{
    if (!out || !a || !b) return false;
    char a_copy[KN_MAX_PATH];
    if (out == a)
    {
        size_t n = kn_strlen(a);
        if (n >= sizeof(a_copy)) n = sizeof(a_copy) - 1;
        kn_memcpy(a_copy, a, n);
        a_copy[n] = 0;
        a = a_copy;
    }
    size_t la = kn_strlen(a);
    size_t lb = kn_strlen(b);
    size_t off = 0;
    if (la + lb + 2 > cap) return false;
    kn_memcpy(out, a, la);
    off = la;
    if (off > 0 && !is_path_sep(out[off - 1]))
        out[off++] = '\\';
    kn_memcpy(out + off, b, lb);
    off += lb;
    out[off] = 0;
    return true;
}

static bool file_exists(const char *path)
{
    if (!path || !path[0]) return false;
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_READ, KN_FILE_SHARE_READ | KN_FILE_SHARE_WRITE,
                              0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(h);
    return true;
}

static void try_delete_file(const char *path)
{
    if (!path || !path[0]) return;
    DeleteFileA(path);
}

static bool copy_file_overwrite(const char *src, const char *dst)
{
    if (!src || !dst) return false;
    size_t sz = 0;
    char *data = kn_read_file(src, &sz);
    if (!data) return false;
    KN_HANDLE h = CreateFileA(dst, KN_GENERIC_WRITE, 0, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
    {
        kn_free(data);
        return false;
    }
    KN_DWORD wrote = 0;
    int ok = WriteFile(h, data, (KN_DWORD)sz, &wrote, 0) && wrote == (KN_DWORD)sz;
    CloseHandle(h);
    kn_free(data);
    return ok ? true : false;
}

static bool dir_exists(const char *path)
{
    if (!path || !path[0]) return false;
    char pattern[KN_MAX_PATH];
    if (!path_join(pattern, sizeof(pattern), path, "*"))
        return false;
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE)
        return false;
    FindClose(h);
    return true;
}

static bool path_full(char *out, size_t cap, const char *path)
{
    if (!out || cap == 0 || !path || !path[0]) return false;
    KN_DWORD n = GetFullPathNameA(path, (KN_DWORD)cap, out, 0);
    if (!n || n >= (KN_DWORD)cap)
        return false;
    return true;
}

static void copy_str(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = kn_strlen(src);
    if (n >= cap) n = cap - 1;
    kn_memcpy(dst, src, n);
    dst[n] = 0;
}

static char lower_char(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static bool str_eq_ci(const char *a, const char *b)
{
    if (!a || !b) return false;
    size_t na = kn_strlen(a);
    size_t nb = kn_strlen(b);
    if (na != nb) return false;
    for (size_t i = 0; i < na; i++)
        if (lower_char(a[i]) != lower_char(b[i]))
            return false;
    return true;
}

static bool path_eq(const char *a, const char *b)
{
    char full_a[KN_MAX_PATH];
    char full_b[KN_MAX_PATH];
    const char *ca = a;
    const char *cb = b;
    if (!a || !b) return false;
    if (path_full(full_a, sizeof(full_a), a))
        ca = full_a;
    if (path_full(full_b, sizeof(full_b), b))
        cb = full_b;
#if defined(_WIN32) || defined(_WIN64)
    return str_eq_ci(ca, cb);
#else
    return kn_strcmp(ca, cb) == 0;
#endif
}

static bool strlist_contains_path(const StrList *l, const char *path)
{
    if (!l || !path) return false;
    for (int i = 0; i < l->count; i++)
    {
        if (path_eq(l->items[i], path))
            return true;
    }
    return false;
}

static bool str_contains_ci(const char *s, const char *needle)
{
    if (!s || !needle || !needle[0]) return false;
    size_t ns = kn_strlen(s);
    size_t nn = kn_strlen(needle);
    if (nn > ns) return false;
    for (size_t i = 0; i + nn <= ns; i++)
    {
        size_t j = 0;
        while (j < nn)
        {
            if (lower_char(s[i + j]) != lower_char(needle[j])) break;
            j++;
        }
        if (j == nn) return true;
    }
    return false;
}

static bool path_relative_module(const char *root, const char *full, char *out, size_t cap)
{
    if (!root || !full || !out || cap == 0) return false;
    size_t root_len = kn_strlen(root);
    if (kn_strncmp(root, full, root_len) != 0)
        return false;
    const char *rel = full + root_len;
    while (*rel && is_path_sep(*rel))
        rel++;
    if (!*rel)
        return false;

    size_t off = 0;
    for (size_t i = 0; rel[i]; i++)
    {
        char c = rel[i];
        if (c == '.')
        {
            const char *ext = rel + i;
            if (str_eq_ci(ext, ".kn") || str_eq_ci(ext, ".fx"))
                break;
        }
        if (is_path_sep(c))
            c = '.';
        if (off + 1 >= cap)
            return false;
        out[off++] = c;
    }
    if (off == 0)
        return false;
    out[off] = 0;
    return true;
}

static bool path_has_ext(const char *path)
{
    if (!path || !path[0]) return false;
    size_t n = kn_strlen(path);
    for (size_t i = n; i > 0; i--)
    {
        char c = path[i - 1];
        if (c == '.')
            return true;
        if (is_path_sep(c))
            return false;
    }
    return false;
}

static bool target_is_windows(const char *triple);
static bool target_is_linux(const char *triple);
static bool target_is_macos(const char *triple);
static bool target_is_freestanding(const char *triple);
static bool target_is_x86(const char *triple);
static bool target_is_arm64(const char *triple);

static const char *target_platform_dir_alias(const char *triple)
{
    if (target_is_windows(triple))
    {
        if (target_is_x86(triple)) return "win-x86";
        if (target_is_arm64(triple)) return "win-arm64";
        return "win-x64";
    }
    if (target_is_linux(triple))
    {
        if (target_is_arm64(triple)) return "linux-arm64";
        return "linux-x64";
    }
    if (target_is_macos(triple))
    {
        if (target_is_arm64(triple)) return "macos-arm64";
        return "macos-x64";
    }
    if (target_is_freestanding(triple))
    {
        if (target_is_arm64(triple)) return "bare-arm64";
        if (target_is_x86(triple)) return "bare-x86";
        return "bare64";
    }
    return 0;
}

static bool link_root_add_dir(KnLinkItem *items, int *count, int cap,
                              char storage[][KN_MAX_PATH], int *storage_count, int storage_cap,
                              const char *path)
{
    if (!items || !count || !storage || !storage_count || !path || !path[0])
        return false;
    if (!dir_exists(path))
        return true;

    char resolved[KN_MAX_PATH];
    const char *store = path;
    if (path_full(resolved, sizeof(resolved), path))
        store = resolved;
    if (link_item_contains(items, *count, KN_LINK_ITEM_LIB_DIR, store))
        return true;
    if (*storage_count >= storage_cap || *count >= cap)
        return false;
    copy_str(storage[*storage_count], KN_MAX_PATH, store);
    items[*count].kind = KN_LINK_ITEM_LIB_DIR;
    items[*count].value = storage[*storage_count];
    (*storage_count)++;
    (*count)++;
    return true;
}

static bool expand_link_root(KnLinkItem *items, int *count, int cap,
                             char storage[][KN_MAX_PATH], int *storage_count, int storage_cap,
                             const char *root, const char *target)
{
    if (!root || !root[0]) return false;

    char lib_dir[KN_MAX_PATH];
    char candidate[KN_MAX_PATH];
    const char *alias = target_platform_dir_alias(target);

    if (path_join(lib_dir, sizeof(lib_dir), root, "lib"))
    {
        if (path_join(candidate, sizeof(candidate), lib_dir, target))
        {
            if (!link_root_add_dir(items, count, cap, storage, storage_count, storage_cap, candidate))
                return false;
        }
        if (alias && alias[0] && path_join(candidate, sizeof(candidate), lib_dir, alias))
        {
            if (!link_root_add_dir(items, count, cap, storage, storage_count, storage_cap, candidate))
                return false;
        }
        if (!link_root_add_dir(items, count, cap, storage, storage_count, storage_cap, lib_dir))
            return false;
    }

    if (!link_root_add_dir(items, count, cap, storage, storage_count, storage_cap, root))
        return false;
    return true;
}

static void dump_link_search_dirs(const KnLinkItem *items, int count)
{
    kn_write_str("[LinkSearch]\n");
    for (int i = 0; i < count; i++)
    {
        if (items[i].kind != KN_LINK_ITEM_LIB_DIR) continue;
        kn_write_str("  ");
        kn_write_str(items[i].value ? items[i].value : "<null>");
        kn_write_str("\n");
    }
}

static const char *default_target_triple(void)
{
#if defined(_WIN64)
    return "x86_64-pc-windows-msvc";
#elif defined(_WIN32)
    return "i686-pc-windows-msvc";
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    return "aarch64-apple-darwin";
#elif defined(__APPLE__)
    return "x86_64-apple-darwin";
#elif defined(__linux__) && defined(__aarch64__)
    return "aarch64-unknown-linux-gnu";
#elif defined(__linux__)
    return "x86_64-pc-linux-gnu";
#else
    return "x86_64-pc-windows-msvc";
#endif
}

static const char *normalize_target_triple(const char *raw)
{
    if (!raw || !raw[0] || str_eq_ci(raw, "host"))
        return default_target_triple();
    if (str_eq_ci(raw, "win86") || str_eq_ci(raw, "win32") || str_eq_ci(raw, "x86"))
        return "i686-pc-windows-msvc";
    if (str_eq_ci(raw, "win64") || str_eq_ci(raw, "x64") || str_eq_ci(raw, "windows"))
        return "x86_64-pc-windows-msvc";
    if (str_eq_ci(raw, "linux64") || str_eq_ci(raw, "linux-x64"))
        return "x86_64-pc-linux-gnu";
    if (str_eq_ci(raw, "linux-arm64") || str_eq_ci(raw, "linuxarm64") || str_eq_ci(raw, "aarch64-linux"))
        return "aarch64-unknown-linux-gnu";
    if (str_eq_ci(raw, "macos") || str_eq_ci(raw, "macos-x64") || str_eq_ci(raw, "mac64") ||
        str_eq_ci(raw, "darwin") || str_eq_ci(raw, "darwin-x64"))
        return "x86_64-apple-darwin";
    if (str_eq_ci(raw, "macos-arm64") || str_eq_ci(raw, "macarm64") ||
        str_eq_ci(raw, "darwin-arm64") || str_eq_ci(raw, "aarch64-darwin"))
        return "aarch64-apple-darwin";
    if (str_eq_ci(raw, "bare64") || str_eq_ci(raw, "freestanding64") || str_eq_ci(raw, "kernel64"))
        return "x86_64-unknown-none-elf";
    if (str_eq_ci(raw, "bare-arm64") || str_eq_ci(raw, "freestanding-arm64") || str_eq_ci(raw, "kernel-arm64"))
        return "aarch64-unknown-none-elf";
    return raw;
}

static bool target_is_windows(const char *triple)
{
    return str_contains_ci(triple, "windows");
}

static bool target_is_linux(const char *triple)
{
    return str_contains_ci(triple, "linux");
}

static bool target_is_macos(const char *triple)
{
    return str_contains_ci(triple, "darwin") || str_contains_ci(triple, "apple") || str_contains_ci(triple, "macos");
}

static bool target_is_freestanding(const char *triple)
{
    if (!triple || !triple[0]) return false;
    return str_contains_ci(triple, "unknown-none") || str_contains_ci(triple, "none-elf");
}

static int default_hosted_linker_kind(const char *triple)
{
    return (target_is_linux(triple) || target_is_macos(triple)) ? KN_LINKER_ZIG : KN_LINKER_LLD;
}

static bool target_is_x86(const char *triple)
{
    if (str_contains_ci(triple, "x86_64") || str_contains_ci(triple, "amd64"))
        return false;
    return str_contains_ci(triple, "i386") || str_contains_ci(triple, "i486") ||
           str_contains_ci(triple, "i586") || str_contains_ci(triple, "i686") ||
           str_eq_ci(triple, "x86");
}

static bool target_is_arm64(const char *triple)
{
    return str_contains_ci(triple, "aarch64") || str_contains_ci(triple, "arm64");
}

static bool target_is_x86_64(const char *triple)
{
    return str_contains_ci(triple, "x86_64") || str_contains_ci(triple, "amd64");
}

static bool derive_obj_path(const char *base_out, const char *target, char *out, size_t cap)
{
    if (!base_out || !out || cap == 0) return false;
    size_t n = kn_strlen(base_out);
    if (n + 5 >= cap) return false;
    kn_memcpy(out, base_out, n);
    out[n] = 0;
    for (size_t i = n; i > 0; i--)
    {
        char c = out[i - 1];
        if (is_path_sep(c))
            break;
        if (c == '.')
        {
            out[i - 1] = 0;
            break;
        }
    }
    n = kn_strlen(out);
    if (target_is_windows(target))
        kn_append(out, cap, &n, ".obj");
    else
        kn_append(out, cap, &n, ".o");
    return true;
}

static bool derive_temp_obj_path(const char *base_out, const char *target, int index, char *out, size_t cap)
{
    if (!base_out || !out || cap == 0) return false;
    char base[KN_MAX_PATH];
    copy_str(base, sizeof(base), base_out);
    for (size_t i = kn_strlen(base); i > 0; i--)
    {
        char c = base[i - 1];
        if (is_path_sep(c))
            break;
        if (c == '.')
        {
            base[i - 1] = 0;
            break;
        }
    }

    size_t off = 0;
    out[0] = 0;
    kn_append(out, cap, &off, base);
    kn_append(out, cap, &off, ".in");
    char num[24];
    int n = 0;
    int v = index < 0 ? 0 : index;
    if (v == 0) num[n++] = '0';
    while (v > 0 && n < (int)sizeof(num))
    {
        num[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = n - 1; i >= 0; i--)
    {
        char t[2];
        t[0] = num[i];
        t[1] = 0;
        kn_append(out, cap, &off, t);
    }
    if (target_is_windows(target))
        kn_append(out, cap, &off, ".obj");
    else
        kn_append(out, cap, &off, ".o");
    return true;
}

static bool derive_temp_ir_path(const char *base_out, char *out, size_t cap)
{
    if (!base_out || !out || cap == 0) return false;
    char base[KN_MAX_PATH];
    copy_str(base, sizeof(base), base_out);
    for (size_t i = kn_strlen(base); i > 0; i--)
    {
        char c = base[i - 1];
        if (is_path_sep(c))
            break;
        if (c == '.')
        {
            base[i - 1] = 0;
            break;
        }
    }
    size_t off = 0;
    out[0] = 0;
    kn_append(out, cap, &off, base);
    kn_append(out, cap, &off, ".kn.lto.ll");
    return true;
}

static bool derive_temp_bc_path(const char *base_out, char *out, size_t cap)
{
    if (!base_out || !out || cap == 0) return false;
    char base[KN_MAX_PATH];
    copy_str(base, sizeof(base), base_out);
    for (size_t i = kn_strlen(base); i > 0; i--)
    {
        char c = base[i - 1];
        if (is_path_sep(c))
            break;
        if (c == '.')
        {
            base[i - 1] = 0;
            break;
        }
    }
    size_t off = 0;
    out[0] = 0;
    kn_append(out, cap, &off, base);
    kn_append(out, cap, &off, ".kn.lto.bc");
    return true;
}

static bool derive_temp_bc_input_path(const char *base_out, int index, char *out, size_t cap)
{
    if (!base_out || !out || cap == 0) return false;
    char base[KN_MAX_PATH];
    copy_str(base, sizeof(base), base_out);
    for (size_t i = kn_strlen(base); i > 0; i--)
    {
        char c = base[i - 1];
        if (is_path_sep(c))
            break;
        if (c == '.')
        {
            base[i - 1] = 0;
            break;
        }
    }

    size_t off = 0;
    out[0] = 0;
    kn_append(out, cap, &off, base);
    kn_append(out, cap, &off, ".in");
    char num[24];
    int n = 0;
    int v = index < 0 ? 0 : index;
    if (v == 0) num[n++] = '0';
    while (v > 0 && n < (int)sizeof(num))
    {
        num[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = n - 1; i >= 0; i--)
    {
        char t[2];
        t[0] = num[i];
        t[1] = 0;
        kn_append(out, cap, &off, t);
    }
    kn_append(out, cap, &off, ".bc");
    return true;
}

static bool get_exe_path(char *out, size_t cap)
{
    char raw[KN_MAX_PATH];
    if (!out || cap == 0) return false;
#if !defined(_WIN32) && !defined(_WIN64)
    {
        long n = readlink("/proc/self/exe", out, cap - 1);
        if (n > 0) { out[n] = 0; return true; }
    }
#endif
#if defined(__APPLE__)
    {
        uint32_t bufsize = (uint32_t)cap;
        if (_NSGetExecutablePath(out, &bufsize) == 0)
        {
            char resolved[KN_MAX_PATH];
            if (realpath(out, resolved))
            {
                copy_str(out, cap, resolved);
            }
            return true;
        }
    }
#endif
    const char *cmd = GetCommandLineA();
    if (!cmd || !cmd[0]) return false;

    size_t i = 0;
    size_t off = 0;
    while (cmd[i] == ' ' || cmd[i] == '\t') i++;
    if (!cmd[i]) return false;

    if (cmd[i] == '"')
    {
        i++;
        while (cmd[i] && cmd[i] != '"' && off + 1 < sizeof(raw))
            raw[off++] = cmd[i++];
    }
    else
    {
        while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && off + 1 < sizeof(raw))
            raw[off++] = cmd[i++];
    }

    raw[off] = 0;
    if (off == 0) return false;
    if (path_full(out, cap, raw))
        return true;
    copy_str(out, cap, raw);
    return true;
}

static bool get_exe_dir(char *out, size_t cap)
{
    char exe[KN_MAX_PATH];
    if (!get_exe_path(exe, sizeof(exe)))
        return false;
    path_dirname(exe, out, cap);
    return true;
}

static bool has_source_extension(const char *name)
{
    size_t n = kn_strlen(name);
    if (n >= 3 &&
        name[n - 3] == '.' &&
        (name[n - 2] == 'f' || name[n - 2] == 'F') &&
        (name[n - 1] == 'x' || name[n - 1] == 'X'))
        return true;
    if (n >= 3 &&
        name[n - 3] == '.' &&
        (name[n - 2] == 'k' || name[n - 2] == 'K') &&
        (name[n - 1] == 'n' || name[n - 1] == 'N'))
        return true;
    if (n >= 6 &&
        name[n - 6] == '.' &&
        (name[n - 5] == 'k' || name[n - 5] == 'K') &&
        (name[n - 4] == 'i' || name[n - 4] == 'I') &&
        (name[n - 3] == 'n' || name[n - 3] == 'N') &&
        (name[n - 2] == 'a' || name[n - 2] == 'A') &&
        (name[n - 1] == 'l' || name[n - 1] == 'L'))
        return true;
    return false;
}

static bool has_ext_ci(const char *name, const char *ext)
{
    if (!name || !ext) return false;
    size_t nn = kn_strlen(name);
    size_t ne = kn_strlen(ext);
    if (nn < ne) return false;
    return str_eq_ci(name + (nn - ne), ext);
}

static KnInputKind classify_input(const char *path)
{
    if (!path || !path[0]) return KN_INPUT_UNKNOWN;
    if (has_source_extension(path)) return KN_INPUT_FX;
    if (has_ext_ci(path, ".obj") || has_ext_ci(path, ".o")) return KN_INPUT_OBJ;
    if (has_ext_ci(path, ".s") || has_ext_ci(path, ".asm")) return KN_INPUT_ASM;
    if (has_ext_ci(path, ".ll")) return KN_INPUT_LL;
    if (has_ext_ci(path, ".knc")) return KN_INPUT_KNC;
    return KN_INPUT_UNKNOWN;
}

static void path_stem_local(const char *path, char *out, size_t cap)
{
    const char *base = path;
    size_t off = 0;
    if (!out || cap == 0)
        return;
    out[0] = 0;
    if (!path || !path[0])
        return;

    for (const char *p = path; *p; p++)
    {
        if (is_path_sep(*p))
            base = p + 1;
    }
    while (base[off] && base[off] != '.' && off + 1 < cap)
    {
        out[off] = base[off];
        off++;
    }
    out[off] = 0;
    if (off == 0 && cap > 5)
        copy_str(out, cap, "main");
}

static bool derive_temp_knc_path(const char *input_path, char *out, size_t cap)
{
    char temp_dir[KN_MAX_PATH];
    char stem[KN_MAX_PATH];
    char file_name[KN_MAX_PATH];
    size_t off = 0;
    unsigned long long tick = 0;
    char digits[32];
    int dn = 0;

    if (!out || cap == 0)
        return false;
    out[0] = 0;

    temp_dir[0] = 0;
    {
        KN_DWORD n = GetEnvironmentVariableA("TMP", temp_dir, (KN_DWORD)sizeof(temp_dir));
        if (n == 0 || n >= (KN_DWORD)sizeof(temp_dir))
            n = GetEnvironmentVariableA("TEMP", temp_dir, (KN_DWORD)sizeof(temp_dir));
        if (n == 0 || n >= (KN_DWORD)sizeof(temp_dir))
            copy_str(temp_dir, sizeof(temp_dir), ".");
    }

    path_stem_local(input_path, stem, sizeof(stem));
    tick = (unsigned long long)GetTickCount64();

    file_name[0] = 0;
    kn_append(file_name, sizeof(file_name), &off, "kinal-vm-");
    kn_append(file_name, sizeof(file_name), &off, stem[0] ? stem : "main");
    kn_append(file_name, sizeof(file_name), &off, "-");
    if (tick == 0)
    {
        digits[dn++] = '0';
    }
    else
    {
        while (tick > 0 && dn < (int)sizeof(digits))
        {
            digits[dn++] = (char)('0' + (tick % 10ULL));
            tick /= 10ULL;
        }
    }
    for (int i = dn - 1; i >= 0; i--)
    {
        char one[2];
        one[0] = digits[i];
        one[1] = 0;
        kn_append(file_name, sizeof(file_name), &off, one);
    }
    kn_append(file_name, sizeof(file_name), &off, ".knc");

    return path_join(out, cap, temp_dir, file_name);
}

/* Derive a temp path for a native run-exe: %TMP%/kinal-run-<stem>-<tick>[.exe] */
static bool derive_temp_run_exe_path(const char *input_path, char *out, size_t cap)
{
    char temp_dir[KN_MAX_PATH];
    char stem[KN_MAX_PATH];
    char file_name[KN_MAX_PATH];
    size_t off = 0;
    unsigned long long tick = 0;
    char digits[32];
    int dn = 0;

    if (!out || cap == 0)
        return false;
    out[0] = 0;

    temp_dir[0] = 0;
    {
        KN_DWORD n = GetEnvironmentVariableA("TMP", temp_dir, (KN_DWORD)sizeof(temp_dir));
        if (n == 0 || n >= (KN_DWORD)sizeof(temp_dir))
            n = GetEnvironmentVariableA("TEMP", temp_dir, (KN_DWORD)sizeof(temp_dir));
        if (n == 0 || n >= (KN_DWORD)sizeof(temp_dir))
            n = GetEnvironmentVariableA("TMPDIR", temp_dir, (KN_DWORD)sizeof(temp_dir));
        if (n == 0 || n >= (KN_DWORD)sizeof(temp_dir))
            copy_str(temp_dir, sizeof(temp_dir), "/tmp");
    }

    path_stem_local(input_path, stem, sizeof(stem));
    tick = (unsigned long long)GetTickCount64();

    file_name[0] = 0;
    kn_append(file_name, sizeof(file_name), &off, "kinal-run-");
    kn_append(file_name, sizeof(file_name), &off, stem[0] ? stem : "main");
    kn_append(file_name, sizeof(file_name), &off, "-");
    if (tick == 0)
    {
        digits[dn++] = '0';
    }
    else
    {
        while (tick > 0 && dn < (int)sizeof(digits))
        {
            digits[dn++] = (char)('0' + (tick % 10ULL));
            tick /= 10ULL;
        }
    }
    for (int i = dn - 1; i >= 0; i--)
    {
        char one[2];
        one[0] = digits[i];
        one[1] = 0;
        kn_append(file_name, sizeof(file_name), &off, one);
    }
#if defined(_WIN32) || defined(_WIN64)
    kn_append(file_name, sizeof(file_name), &off, ".exe");
#endif

    return path_join(out, cap, temp_dir, file_name);
}

static void append_quoted(char *buf, size_t cap, size_t *off, const char *s)
{
    kn_append(buf, cap, off, "\"");
    kn_append(buf, cap, off, s ? s : "");
    kn_append(buf, cap, off, "\"");
}

/*
 * Spawn a compiled native program and forward all stdio to/from the parent.
 * prog_argv[0..prog_argc-1] are appended after exe on the command line.
 * Returns the process exit code, or -1 on launch failure.
 */
static int run_native_program(const char *exe, int prog_argc, const char **prog_argv)
{
    if (!exe || !exe[0]) return -1;

    /* Build command line: "exe" ["arg1"] ["arg2"] ... */
    char cmd[32768];
    size_t off = 0;
    cmd[0] = 0;
    append_quoted(cmd, sizeof(cmd), &off, exe);
    for (int i = 0; i < prog_argc; i++)
    {
        kn_append(cmd, sizeof(cmd), &off, " ");
        append_quoted(cmd, sizeof(cmd), &off, prog_argv[i] ? prog_argv[i] : "");
    }

    KN_STARTUPINFOA si;
    KN_PROCESS_INFORMATION pi;
    kn_memset(&si, 0, sizeof(si));
    kn_memset(&pi, 0, sizeof(pi));
    si.cb = (uint32_t)sizeof(si);
    /* Forward stdin/stdout/stderr so piped I/O works correctly. */
    si.dwFlags = KN_STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(KN_STDIN_HANDLE);
    si.hStdOutput = GetStdHandle(KN_STDOUT_HANDLE);
    si.hStdError  = GetStdHandle(KN_STDERR_HANDLE);

    size_t len = kn_strlen(cmd);
    char *mutable_cmd = (char *)kn_malloc(len + 1);
    if (!mutable_cmd) return -1;
    kn_memcpy(mutable_cmd, cmd, len);
    mutable_cmd[len] = 0;

    if (!CreateProcessA(exe, mutable_cmd, 0, 0, 1 /*bInheritHandles*/, 0, 0, 0, &si, &pi))
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

static int run_process(const char *exe, const char *cmdline)
{
    KN_STARTUPINFOA si;
    KN_PROCESS_INFORMATION pi;
    kn_memset(&si, 0, sizeof(si));
    kn_memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    size_t len = kn_strlen(cmdline ? cmdline : "");
    char *mutable_cmd = (char *)kn_malloc(len + 1);
    if (!mutable_cmd) return -1;
    if (cmdline && len > 0)
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

static int tool_name_has_path(const char *name)
{
    if (!name || !name[0]) return 0;
    for (size_t i = 0; name[i]; i++)
    {
        if (name[i] == '\\' || name[i] == '/' || name[i] == ':')
            return 1;
    }
    return 0;
}

static int tool_name_without_exe_suffix(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return 0;
    out[0] = 0;
#if defined(_WIN32) || defined(_WIN64)
    (void)name;
    return 0;
#else
    size_t n = kn_strlen(name);
    if (n <= 4 || n - 4 >= cap) return 0;
    if (lower_char(name[n - 4]) != '.' || lower_char(name[n - 3]) != 'e' ||
        lower_char(name[n - 2]) != 'x' || lower_char(name[n - 1]) != 'e')
        return 0;
    kn_memcpy(out, name, n - 4);
    out[n - 4] = 0;
    return 1;
#endif
}

static int resolve_tool_in_dir(char *out, size_t cap, const char *dir, const char *name)
{
    char candidate[KN_MAX_PATH];
    char alt_name[KN_MAX_PATH];
    if (!out || cap == 0 || !dir || !dir[0] || !name || !name[0]) return 0;
    if (path_join(candidate, sizeof(candidate), dir, name) && file_exists(candidate))
    {
        copy_str(out, cap, candidate);
        return 1;
    }
    if (tool_name_without_exe_suffix(alt_name, sizeof(alt_name), name) &&
        path_join(candidate, sizeof(candidate), dir, alt_name) && file_exists(candidate))
    {
        copy_str(out, cap, candidate);
        return 1;
    }
    return 0;
}

static char path_env_sep(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ';';
#else
    return ':';
#endif
}

static int try_resolve_tool_on_path(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return 0;
    out[0] = 0;

    if (tool_name_has_path(name))
    {
        if (file_exists(name))
        {
            copy_str(out, cap, name);
            return 1;
        }
        return 0;
    }

    if (file_exists(name))
    {
        copy_str(out, cap, name);
        return 1;
    }

    char path_env[32768];
    KN_DWORD n = GetEnvironmentVariableA("PATH", path_env, (KN_DWORD)sizeof(path_env));
    if (n == 0 || n >= (KN_DWORD)sizeof(path_env))
        return 0;

    size_t start = 0;
    const char sep = path_env_sep();
    while (path_env[start])
    {
        size_t end = start;
        while (path_env[end] && path_env[end] != sep)
            end++;

        char dir[KN_MAX_PATH];
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
                char candidate[KN_MAX_PATH];
                if (path_join(candidate, sizeof(candidate), dir, name) && file_exists(candidate))
                {
                    copy_str(out, cap, candidate);
                    return 1;
                }
            }
        }

        start = path_env[end] ? (end + 1) : end;
    }
    return 0;
}

static void resolve_tool(char *out, size_t cap, const char *name)
{
    if (!out || cap == 0 || !name || !name[0]) return;
    out[0] = 0;

    char exe_dir[KN_MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        char p[KN_MAX_PATH];
        if (path_join(p, sizeof(p), exe_dir, "linker") && resolve_tool_in_dir(out, cap, p, name))
        {
            return;
        }
        if (path_join(p, sizeof(p), exe_dir, "llvm") && path_join(p, sizeof(p), p, "bin") && resolve_tool_in_dir(out, cap, p, name))
        {
            return;
        }
    }

    if (KN_LLVM_BIN[0] != 0)
    {
        char p[KN_MAX_PATH];
        copy_str(p, sizeof(p), KN_LLVM_BIN);
#if defined(_WIN32) || defined(_WIN64)
        for (size_t i = 0; p[i]; i++)
            if (p[i] == '/') p[i] = '\\';
#endif
        if (resolve_tool_in_dir(out, cap, p, name))
        {
            return;
        }
    }

    // Runtime KN_LLVM_BIN environment variable (e.g. set by x.py test).
    {
        char envp[KN_MAX_PATH];
        KN_DWORD n = GetEnvironmentVariableA("KN_LLVM_BIN", envp, (KN_DWORD)sizeof(envp));
        if (n > 0 && n < (KN_DWORD)sizeof(envp) && envp[0])
        {
#if defined(_WIN32) || defined(_WIN64)
            for (size_t i = 0; envp[i]; i++)
                if (envp[i] == '/') envp[i] = '\\';
#endif
            if (resolve_tool_in_dir(out, cap, envp, name))
            {
                return;
            }
        }
    }

    if (try_resolve_tool_on_path(out, cap, name))
        return;

    {
        char alt_name[KN_MAX_PATH];
        if (tool_name_without_exe_suffix(alt_name, sizeof(alt_name), name) &&
            try_resolve_tool_on_path(out, cap, alt_name))
            return;
        if (alt_name[0])
        {
            copy_str(out, cap, alt_name);
            return;
        }
    }

    copy_str(out, cap, name);
}

static int resolve_zig_tool(char *out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;

    char p[KN_MAX_PATH];
    char exe_dir[KN_MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (path_join(p, sizeof(p), exe_dir, "linker") && path_join(p, sizeof(p), p, "zig"))
        {
            if (resolve_tool_in_dir(out, cap, p, "zig.exe"))
                return 1;
            if (path_join(p, sizeof(p), p, "bin") && resolve_tool_in_dir(out, cap, p, "zig.exe"))
                return 1;
        }
    }

    // Prefer explicit env override.
    char envp[KN_MAX_PATH];
    KN_DWORD n = GetEnvironmentVariableA("KINAL_ZIG", envp, (KN_DWORD)sizeof(envp));
    if (n > 0 && n < (KN_DWORD)sizeof(envp) && file_exists(envp))
    {
        copy_str(out, cap, envp);
        return 1;
    }
    n = GetEnvironmentVariableA("ZIG", envp, (KN_DWORD)sizeof(envp));
    if (n > 0 && n < (KN_DWORD)sizeof(envp) && file_exists(envp))
    {
        copy_str(out, cap, envp);
        return 1;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (try_resolve_tool_on_path(out, cap, "zig.exe"))
#else
    if (try_resolve_tool_on_path(out, cap, "zig"))
#endif
        return 1;

    out[0] = 0;
    return 0;
}

static int resolve_vm_executable(char *out, size_t cap, const char *explicit_path)
{
    if (!out || cap == 0)
        return 0;
    out[0] = 0;

    if (explicit_path && explicit_path[0])
    {
        if (file_exists(explicit_path))
        {
            copy_str(out, cap, explicit_path);
            return 1;
        }
        return 0;
    }

    char exe_dir[KN_MAX_PATH];
    char p[KN_MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
#if defined(_WIN32)
        if (path_join(p, sizeof(p), exe_dir, "kinalvm.exe") && file_exists(p))
        {
            copy_str(out, cap, p);
            return 1;
        }
#endif
        if (path_join(p, sizeof(p), exe_dir, "kinalvm") && file_exists(p))
        {
            copy_str(out, cap, p);
            return 1;
        }
    }

#if defined(_WIN32)
    if (try_resolve_tool_on_path(out, cap, "kinalvm.exe"))
        return 1;
#endif
    if (try_resolve_tool_on_path(out, cap, "kinalvm"))
        return 1;
    out[0] = 0;
    return 0;
}

static int run_knc_vm_ex(const char *vm_exe, const char *knc_path, int prog_argc, const char **prog_argv)
{
    if (!vm_exe || !vm_exe[0] || !knc_path || !knc_path[0])
        return -1;

    const char *argv_local[257];
    if (prog_argc < 0) prog_argc = 0;
    if (prog_argc > 255) prog_argc = 255;
    argv_local[0] = knc_path;
    for (int i = 0; i < prog_argc; i++)
        argv_local[i + 1] = prog_argv[i];
    return run_native_program(vm_exe, prog_argc + 1, argv_local);
}

static int run_knc_vm(const char *vm_exe, const char *knc_path)
{
    return run_knc_vm_ex(vm_exe, knc_path, 0, 0);
}

static int resolve_vm_main_source(char *out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;

    char exe_dir[KN_MAX_PATH];
    char p[KN_MAX_PATH];

    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        // Bundle layout: <exe_dir>/../apps/kinalvm/src/Main.kn
        char parent[KN_MAX_PATH];
        path_dirname(exe_dir, parent, sizeof(parent));
        if (path_join(p, sizeof(p), parent, "apps\\kinalvm\\src\\Main.kn") && file_exists(p))
        {
            copy_str(out, cap, p);
            return 1;
        }
        // Same directory: apps/kinalvm/src/Main.kn relative to exe
        if (path_join(p, sizeof(p), exe_dir, "apps\\kinalvm\\src\\Main.kn") && file_exists(p))
        {
            copy_str(out, cap, p);
            return 1;
        }
    }

    // Dev tree: walk upward from exe dir looking for the kinalvm source
    if (exe_dir[0])
    {
        char cur[KN_MAX_PATH];
        copy_str(cur, sizeof(cur), exe_dir);
        for (int depth = 0; depth < 5; depth++)
        {
            char parent[KN_MAX_PATH];
            path_dirname(cur, parent, sizeof(parent));
            if (kn_strcmp(parent, cur) == 0)
                break;
            copy_str(cur, sizeof(cur), parent);
            if (path_join(p, sizeof(p), cur, "apps\\kinalvm\\src\\Main.kn") && file_exists(p))
            {
                copy_str(out, cap, p);
                return 1;
            }
        }
    }

    // CWD fallback
    if (file_exists("apps\\kinalvm\\src\\Main.kn"))
    {
        copy_str(out, cap, "apps\\kinalvm\\src\\Main.kn");
        return 1;
    }

    return 0;
}

static int embed_knc_into_exe(const char *exe_path, const char *knc_path, const char *out_path)
{
    if (!exe_path || !knc_path || !out_path)
        return 1;

    size_t exe_size = 0;
    char *exe_data = kn_read_file(exe_path, &exe_size);
    if (!exe_data)
    {
        kn_write_str("failed to read VM exe: ");
        kn_write_str(exe_path);
        kn_write_str("\n");
        return 1;
    }

    size_t knc_size = 0;
    char *knc_data = kn_read_file(knc_path, &knc_size);
    if (!knc_data)
    {
        kn_free(exe_data);
        kn_write_str("failed to read .knc: ");
        kn_write_str(knc_path);
        kn_write_str("\n");
        return 1;
    }

    // Format: exe_bytes + knc_bytes + knc_size(4 bytes LE) + "KNCE"(4 bytes)
    KN_HANDLE h = CreateFileA(out_path, KN_GENERIC_WRITE, 0, 0,
                              KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
    {
        kn_free(exe_data);
        kn_free(knc_data);
        kn_write_str("failed to create output: ");
        kn_write_str(out_path);
        kn_write_str("\n");
        return 1;
    }

    KN_DWORD wrote = 0;
    int ok = 1;

    // Write exe bytes
    if (!WriteFile(h, exe_data, (KN_DWORD)exe_size, &wrote, 0) || wrote != (KN_DWORD)exe_size)
        ok = 0;

    // Write knc bytes
    if (ok && (!WriteFile(h, knc_data, (KN_DWORD)knc_size, &wrote, 0) || wrote != (KN_DWORD)knc_size))
        ok = 0;

    // Write knc size as 4-byte LE
    if (ok)
    {
        unsigned char size_le[4];
        size_le[0] = (unsigned char)(knc_size & 0xFF);
        size_le[1] = (unsigned char)((knc_size >> 8) & 0xFF);
        size_le[2] = (unsigned char)((knc_size >> 16) & 0xFF);
        size_le[3] = (unsigned char)((knc_size >> 24) & 0xFF);
        if (!WriteFile(h, size_le, 4, &wrote, 0) || wrote != 4)
            ok = 0;
    }

    // Write magic "KNCE"
    if (ok)
    {
        const char magic[4] = { 'K', 'N', 'C', 'E' };
        if (!WriteFile(h, magic, 4, &wrote, 0) || wrote != 4)
            ok = 0;
    }

    CloseHandle(h);
    kn_free(exe_data);
    kn_free(knc_data);

    if (!ok)
    {
        kn_write_str("failed to write knc-exe output\n");
        try_delete_file(out_path);
        return 1;
    }

    return 0;
}

static void to_zig_target(const char *triple, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!triple || !triple[0])
    {
        copy_str(out, cap, "x86_64-windows-msvc");
        return;
    }
    if (target_is_windows(triple))
    {
        if (target_is_x86(triple))
            copy_str(out, cap, "i386-windows-msvc");
        else
            copy_str(out, cap, "x86_64-windows-msvc");
        return;
    }
    if (target_is_linux(triple))
    {
        if (target_is_arm64(triple))
            copy_str(out, cap, "aarch64-linux-gnu");
        else
            copy_str(out, cap, "x86_64-linux-gnu");
        return;
    }
    if (target_is_macos(triple))
    {
        if (target_is_arm64(triple))
            copy_str(out, cap, "aarch64-macos");
        else
            copy_str(out, cap, "x86_64-macos");
        return;
    }
    if (target_is_freestanding(triple))
    {
        if (target_is_arm64(triple))
            copy_str(out, cap, "aarch64-none");
        else
            copy_str(out, cap, "x86_64-none");
        return;
    }
    copy_str(out, cap, triple);
}

static int compile_intermediate_to_obj(const char *in_path, const char *out_path, const char *target_triple, int lto_mode)
{
    const char *triple = (target_triple && target_triple[0]) ? target_triple : default_target_triple();
    KnInputKind kind = classify_input(in_path);
    if (kind == KN_INPUT_OBJ)
    {
        if (str_eq_ci(in_path, out_path))
            return 0;
        if (!copy_file_overwrite(in_path, out_path))
            return -1;
        return 0;
    }

    if (kind != KN_INPUT_ASM && kind != KN_INPUT_LL)
        return -1;

    if (kind == KN_INPUT_LL && lto_mode == KN_LTO_OFF)
    {
        char llc[KN_MAX_PATH];
        resolve_tool(llc, sizeof(llc), "llc.exe");
        if (llc[0] && file_exists(llc))
        {
            char cmd[8192];
            size_t off = 0;
            cmd[0] = 0;
            append_quoted(cmd, sizeof(cmd), &off, llc);
            kn_append(cmd, sizeof(cmd), &off, " -mtriple=");
            kn_append(cmd, sizeof(cmd), &off, triple);
            kn_append(cmd, sizeof(cmd), &off, " -filetype=obj -o ");
            append_quoted(cmd, sizeof(cmd), &off, out_path);
            kn_append(cmd, sizeof(cmd), &off, " ");
            append_quoted(cmd, sizeof(cmd), &off, in_path);
            if (run_process(llc, cmd) == 0)
                return 0;
        }
    }

    if (kind == KN_INPUT_ASM)
    {
        char mc[KN_MAX_PATH];
        resolve_tool(mc, sizeof(mc), "llvm-mc.exe");
        if (mc[0] && file_exists(mc))
        {
            char cmd[8192];
            size_t off = 0;
            cmd[0] = 0;
            append_quoted(cmd, sizeof(cmd), &off, mc);
            kn_append(cmd, sizeof(cmd), &off, " -triple=");
            kn_append(cmd, sizeof(cmd), &off, triple);
            kn_append(cmd, sizeof(cmd), &off, " -filetype=obj -o ");
            append_quoted(cmd, sizeof(cmd), &off, out_path);
            kn_append(cmd, sizeof(cmd), &off, " ");
            append_quoted(cmd, sizeof(cmd), &off, in_path);
            if (run_process(mc, cmd) == 0)
                return 0;
        }
    }

    char clang[KN_MAX_PATH];
    resolve_tool(clang, sizeof(clang), "clang.exe");

    if (clang[0] && kind == KN_INPUT_LL)
    {
        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, clang);
        kn_append(cmd, sizeof(cmd), &off, " --target=");
        kn_append(cmd, sizeof(cmd), &off, triple);
        kn_append(cmd, sizeof(cmd), &off, " -Wno-override-module -c -x ir ");
        if (lto_mode == KN_LTO_FULL)
            kn_append(cmd, sizeof(cmd), &off, " -flto ");
        else if (lto_mode == KN_LTO_THIN)
            kn_append(cmd, sizeof(cmd), &off, " -flto=thin ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        kn_append(cmd, sizeof(cmd), &off, " -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);

        int rc = run_process(clang, cmd);
        if (rc == 0)
            return 0;
        if (lto_mode != KN_LTO_OFF)
            return rc;
    }

    // Prefer Zig toolchain (portable), fall back to clang if available.
    char zig[KN_MAX_PATH];
    (void)resolve_zig_tool(zig, sizeof(zig));
    if (zig[0])
    {
        char zt[256];
        to_zig_target(triple, zt, sizeof(zt));

        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, zig);
        kn_append(cmd, sizeof(cmd), &off, " cc -target ");
        kn_append(cmd, sizeof(cmd), &off, zt);
        kn_append(cmd, sizeof(cmd), &off, " -c ");
        if (lto_mode == KN_LTO_FULL)
            kn_append(cmd, sizeof(cmd), &off, " -flto ");
        else if (lto_mode == KN_LTO_THIN)
            kn_append(cmd, sizeof(cmd), &off, " -flto=thin ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        kn_append(cmd, sizeof(cmd), &off, " -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);

        int rc = run_process(zig, cmd);
        if (rc == 0)
            return rc;
    }

    if (clang[0])
    {
        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, clang);
        kn_append(cmd, sizeof(cmd), &off, " --target=");
        kn_append(cmd, sizeof(cmd), &off, triple);
        kn_append(cmd, sizeof(cmd), &off, " -c ");
        if (lto_mode == KN_LTO_FULL)
            kn_append(cmd, sizeof(cmd), &off, " -flto ");
        else if (lto_mode == KN_LTO_THIN)
            kn_append(cmd, sizeof(cmd), &off, " -flto=thin ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        kn_append(cmd, sizeof(cmd), &off, " -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);
        return run_process(clang, cmd);
    }

    return -1;
}

static int compile_ir_to_bc(const char *in_path, const char *out_path)
{
    char llvm_as[KN_MAX_PATH];
    resolve_tool(llvm_as, sizeof(llvm_as), "llvm-as.exe");
    if (llvm_as[0] && file_exists(llvm_as))
    {
        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, llvm_as);
        kn_append(cmd, sizeof(cmd), &off, " -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);
        kn_append(cmd, sizeof(cmd), &off, " ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        if (run_process(llvm_as, cmd) == 0)
            return 0;
    }

    char clang[KN_MAX_PATH];
    resolve_tool(clang, sizeof(clang), "clang.exe");
    if (clang[0] && file_exists(clang))
    {
        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, clang);
        kn_append(cmd, sizeof(cmd), &off, " -Wno-override-module -emit-llvm -c -x ir ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        kn_append(cmd, sizeof(cmd), &off, " -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);
        return run_process(clang, cmd);
    }

    return -1;
}

static int compile_ll_to_asm(const char *in_path, const char *out_path, const char *target_triple)
{
    const char *triple = (target_triple && target_triple[0]) ? target_triple : default_target_triple();

    char llc[KN_MAX_PATH];
    resolve_tool(llc, sizeof(llc), "llc.exe");
    if (llc[0] && file_exists(llc))
    {
        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, llc);
        kn_append(cmd, sizeof(cmd), &off, " -mtriple=");
        kn_append(cmd, sizeof(cmd), &off, triple);
        kn_append(cmd, sizeof(cmd), &off, " -filetype=asm -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);
        kn_append(cmd, sizeof(cmd), &off, " ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        if (run_process(llc, cmd) == 0)
            return 0;
    }

    char clang[KN_MAX_PATH];
    resolve_tool(clang, sizeof(clang), "clang.exe");

    if (clang[0] && file_exists(clang))
    {
        char cmd[8192];
        size_t off = 0;
        cmd[0] = 0;
        append_quoted(cmd, sizeof(cmd), &off, clang);
        kn_append(cmd, sizeof(cmd), &off, " --target=");
        kn_append(cmd, sizeof(cmd), &off, triple);
        kn_append(cmd, sizeof(cmd), &off, " -Wno-override-module -S -x ir ");
        append_quoted(cmd, sizeof(cmd), &off, in_path);
        kn_append(cmd, sizeof(cmd), &off, " -o ");
        append_quoted(cmd, sizeof(cmd), &off, out_path);
        if (run_process(clang, cmd) == 0)
            return 0;
    }

    return -1;
}

static int resolve_link_input_path(char *out, size_t cap, const char *macro_path,
                                   const char *fallback_rel, const char *exe_rel1, const char *exe_rel2)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (macro_path && macro_path[0] && file_exists(macro_path))
    {
        copy_str(out, cap, macro_path);
        return 1;
    }
    if (fallback_rel && fallback_rel[0] && file_exists(fallback_rel))
    {
        copy_str(out, cap, fallback_rel);
        return 1;
    }

    char exe_dir[KN_MAX_PATH];
    char p[KN_MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (exe_rel1 && exe_rel1[0] && path_join(p, sizeof(p), exe_dir, exe_rel1) && file_exists(p))
        {
            copy_str(out, cap, p);
            return 1;
        }
        if (exe_rel2 && exe_rel2[0] && path_join(p, sizeof(p), exe_dir, exe_rel2) && file_exists(p))
        {
            copy_str(out, cap, p);
            return 1;
        }
    }
    return 0;
}

static void append_windows_driver_lib_name(char *cmd, size_t cap, size_t *off, const char *name)
{
    char buf[KN_MAX_PATH];
    size_t n = 0;
    if (!name || !name[0]) return;
    if (path_has_ext(name))
    {
        append_quoted(cmd, cap, off, name);
        return;
    }
    buf[0] = 0;
    kn_append(buf, sizeof(buf), &n, name);
    kn_append(buf, sizeof(buf), &n, ".lib");
    append_quoted(cmd, cap, off, buf);
}

static void append_windows_driver_link_items(char *cmd, size_t cap, size_t *off, const KnLinkItem *items, int item_count)
{
    for (int i = 0; i < item_count; i++)
    {
        const KnLinkItem *item = &items[i];
        const char *value = item->value;
        if (!value || !value[0]) continue;
        if (item->kind == KN_LINK_ITEM_FILE)
        {
            kn_append(cmd, cap, off, " ");
            append_quoted(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_DIR)
        {
            kn_append(cmd, cap, off, " /libpath:");
            append_quoted(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_LIB_NAME)
        {
            kn_append(cmd, cap, off, " ");
            append_windows_driver_lib_name(cmd, cap, off, value);
            continue;
        }
        if (item->kind == KN_LINK_ITEM_ARG)
        {
            kn_append(cmd, cap, off, " ");
            kn_append(cmd, cap, off, value);
        }
    }
}

static int link_intermediate_windows_lld(const KnCompileConfig *cfg, const char *obj_path, const KnLinkItem *items, int item_count)
{
    if (!cfg || !obj_path || !obj_path[0]) return -1;
    if (cfg->env_kind == KN_ENV_FREESTANDING) return -1;
    if (!target_is_windows(cfg->target_triple)) return -1;
    if (cfg->artifact_kind != KN_ARTIFACT_EXE) return -1;
    if (cfg->linker_kind != KN_LINKER_LLD) return -1;
    KnLinkOptions opts;
    kn_memset(&opts, 0, sizeof(opts));
    opts.obj_path = obj_path;
    opts.out_path = cfg->out_path;
    opts.linker_path = cfg->linker_path;
    opts.items = items;
    opts.item_count = item_count;
    opts.target_triple = cfg->target_triple;
    opts.linker_kind = cfg->linker_kind;
    opts.artifact_kind = cfg->artifact_kind;
    opts.include_runtime = 1;
    opts.lto_mode = cfg->lto_mode;
    opts.env_kind = cfg->env_kind;
    opts.runtime_mode = cfg->runtime_mode;
    opts.panic_mode = cfg->panic_mode;
    opts.crt_mode = cfg->crt_mode;
    opts.no_crt = cfg->no_crt;
    opts.no_default_libs = cfg->no_default_libs;
    opts.entry_symbol = cfg->entry_symbol;
    opts.link_script = cfg->link_script;
    opts.trace = cfg->trace;
    opts.show_link = cfg->show_link;
    opts.show_link_search = cfg->show_link_search;
    return kn_link(&opts);
}

static int invoke_linker(const KnLinkOptions *opts)
{
    if (!opts) return -1;
    return kn_link(opts);
}

static bool is_ignored_dir(const char *name)
{
    if (!name) return true;
    if (kn_strcmp(name, ".") == 0 || kn_strcmp(name, "..") == 0) return true;
    if (kn_strcmp(name, ".git") == 0 || kn_strcmp(name, ".github") == 0) return true;
    if (kn_strcmp(name, ".kinal-cache") == 0) return true;
    if (kn_strcmp(name, "build") == 0 || kn_strcmp(name, "third_party") == 0) return true;
    if (kn_strcmp(name, "tests") == 0) return true;
    return false;
}

static void append_token(char **buf, size_t *cap, size_t *off, Token t)
{
    if (!buf || !cap || !off) return;
    if (t.length == 0) return;
    size_t need = *off + t.length + 1;
    if (need > *cap)
    {
        size_t new_cap = *cap ? *cap * 2 : 64;
        while (new_cap < need) new_cap *= 2;
        char *n = (char *)kn_malloc(new_cap);
        if (!n) return;
        if (*buf)
        {
            kn_memcpy(n, *buf, *cap);
            kn_free(*buf);
        }
        *buf = n;
        *cap = new_cap;
    }
    kn_memcpy(*buf + *off, t.start, t.length);
    *off += t.length;
    (*buf)[*off] = 0;
}

static char *parse_unit_name(const char *text)
{
    Lexer l;
    lex_init(&l, text);
    Token t = lex_next(&l);
    if (t.type != TOK_UNIT)
        return 0;
    Token id = lex_next(&l);
    if (id.type != TOK_ID)
        return 0;

    size_t cap = id.length + 8;
    char *buf = (char *)kn_malloc(cap);
    if (!buf) return 0;
    size_t off = 0;
    buf[0] = 0;
    append_token(&buf, &cap, &off, id);

    for (;;)
    {
        Token dot = lex_next(&l);
        if (dot.type != TOK_DOT) break;
        Token part = lex_next(&l);
        if (part.type != TOK_ID) break;
        Token sep;
        sep.start = ".";
        sep.length = 1;
        append_token(&buf, &cap, &off, sep);
        append_token(&buf, &cap, &off, part);
    }
    return buf;
}

static char *read_unit_name_from_file(const char *path)
{
    size_t sz = 0;
    char *text = kn_read_file(path, &sz);
    if (!text) return 0;
    char *unit = parse_unit_name(text);
    kn_free(text);
    return unit;
}

static void scan_dir_modules(const char *root_dir, const char *dir, ModuleMap *map)
{
    char pattern[KN_MAX_PATH];
    if (!path_join(pattern, sizeof(pattern), dir, "*")) return;

    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE) return;

    do
    {
        const char *name = data.cFileName;
        if (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY)
        {
            if (is_ignored_dir(name)) continue;
            char subdir[KN_MAX_PATH];
            if (!path_join(subdir, sizeof(subdir), dir, name)) continue;
            scan_dir_modules(root_dir, subdir, map);
            continue;
        }

        if (!has_source_extension(name)) continue;
        char full[KN_MAX_PATH];
        if (!path_join(full, sizeof(full), dir, name)) continue;
        char *unit = read_unit_name_from_file(full);
        if (!unit) continue;
        char canonical[KN_MAX_PATH];
        const char *store_path = full;
        if (path_full(canonical, sizeof(canonical), full))
            store_path = canonical;
        char *path = dup_str(store_path);
        if (path)
        {
            ModulePath *entry = modulemap_find(map, unit);
            if (entry)
            {
                strlist_add(&entry->paths, path);
                kn_free(unit);
            }
            else
            {
                modulemap_add(map, unit, path);
            }

            char rel_module[KN_MAX_PATH];
            if (path_relative_module(root_dir, store_path, rel_module, sizeof(rel_module)))
            {
                if (kn_strcmp(rel_module, unit) != 0)
                {
                    ModulePath *alias_entry = modulemap_find(map, rel_module);
                    if (alias_entry)
                    {
                        strlist_add(&alias_entry->paths, path);
                    }
                    else
                    {
                        modulemap_add(map, dup_str(rel_module), path);
                    }
                }
            }
        }
    } while (FindNextFileA(h, &data));

    FindClose(h);
}

static bool scan_stdpkg_sources_from_root(const char *stdpkg_root, ModuleMap *map)
{
    if (!stdpkg_root || !stdpkg_root[0] || !map) return false;
    if (!dir_exists(stdpkg_root)) return false;

    char pattern[KN_MAX_PATH];
    if (!path_join(pattern, sizeof(pattern), stdpkg_root, "*")) return false;

    KN_WIN32_FIND_DATAA pkg_data;
    KN_HANDLE pkg_h = FindFirstFileA(pattern, &pkg_data);
    if (pkg_h == KN_INVALID_HANDLE_VALUE) return false;

    bool any = false;
    do
    {
        if (!(pkg_data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY))
            continue;
        if (kn_strcmp(pkg_data.cFileName, ".") == 0 || kn_strcmp(pkg_data.cFileName, "..") == 0)
            continue;

        char pkg_dir[KN_MAX_PATH];
        char version_pattern[KN_MAX_PATH];
        if (!path_join(pkg_dir, sizeof(pkg_dir), stdpkg_root, pkg_data.cFileName))
            continue;
        if (!path_join(version_pattern, sizeof(version_pattern), pkg_dir, "*"))
            continue;

        KN_WIN32_FIND_DATAA ver_data;
        KN_HANDLE ver_h = FindFirstFileA(version_pattern, &ver_data);
        if (ver_h == KN_INVALID_HANDLE_VALUE)
            continue;

        do
        {
            if (!(ver_data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY))
                continue;
            if (kn_strcmp(ver_data.cFileName, ".") == 0 || kn_strcmp(ver_data.cFileName, "..") == 0)
                continue;

            char src_dir[KN_MAX_PATH];
            if (!path_join(src_dir, sizeof(src_dir), pkg_dir, ver_data.cFileName))
                continue;
            if (!path_join(src_dir, sizeof(src_dir), src_dir, "src"))
                continue;
            if (!dir_exists(src_dir))
                continue;

            char root[KN_MAX_PATH];
            const char *scan_root = src_dir;
            if (path_full(root, sizeof(root), src_dir))
                scan_root = root;
            scan_dir_modules(scan_root, scan_root, map);
            any = true;
        } while (FindNextFileA(ver_h, &ver_data));

        FindClose(ver_h);
    } while (FindNextFileA(pkg_h, &pkg_data));

    FindClose(pkg_h);
    return any;
}

static void scan_default_stdlib_modules(ModuleMap *map)
{
    if (!map) return;

    // Current working tree layout: official Kinal-written std modules live under
    // stdpkg/<Package>/<Version>/src and are scanned first when explicit package
    // roots are not available.
    if (scan_stdpkg_sources_from_root("stdpkg", map))
        return;

    char exe_dir[KN_MAX_PATH];
    if (!get_exe_dir(exe_dir, sizeof(exe_dir)))
        exe_dir[0] = 0;

    char candidate[KN_MAX_PATH];
    if (exe_dir[0] && path_join(candidate, sizeof(candidate), exe_dir, "stdpkg"))
    {
        if (scan_stdpkg_sources_from_root(candidate, map))
            return;
    }

    // Dev tree fallback: walk a few parents so local binaries can still find
    // the official stdpkg source roots without requiring a staged bundle.
    if (exe_dir[0])
    {
        char cur[KN_MAX_PATH];
        copy_str(cur, sizeof(cur), exe_dir);
        for (int depth = 0; depth < 4; depth++)
        {
            char parent[KN_MAX_PATH];
            path_dirname(cur, parent, sizeof(parent));
            if (kn_strcmp(parent, cur) == 0)
                break;
            copy_str(cur, sizeof(cur), parent);
            if (path_join(candidate, sizeof(candidate), cur, "stdpkg"))
            {
                if (scan_stdpkg_sources_from_root(candidate, map))
                    return;
            }
        }
    }

    // Legacy compatibility fallback: older worktrees bundled official Kinal
    // sources under stdlib/src before stdpkg became the primary layout.
    if (dir_exists("stdlib\\src"))
    {
        char root[KN_MAX_PATH];
        const char *scan_root = "stdlib\\src";
        if (path_full(root, sizeof(root), scan_root))
            scan_root = root;
        scan_dir_modules(scan_root, scan_root, map);
        return;
    }

    if (exe_dir[0] && path_join(candidate, sizeof(candidate), exe_dir, "stdlib\\src"))
    {
        if (dir_exists(candidate))
        {
            char root[KN_MAX_PATH];
            const char *scan_root = candidate;
            if (path_full(root, sizeof(root), candidate))
                scan_root = root;
            scan_dir_modules(scan_root, scan_root, map);
            return;
        }
    }

    if (!exe_dir[0])
        return;

    char cur[KN_MAX_PATH];
    copy_str(cur, sizeof(cur), exe_dir);
    for (int depth = 0; depth < 4; depth++)
    {
        char parent[KN_MAX_PATH];
        path_dirname(cur, parent, sizeof(parent));
        if (kn_strcmp(parent, cur) == 0)
            break;
        copy_str(cur, sizeof(cur), parent);
        if (path_join(candidate, sizeof(candidate), cur, "stdlib\\src"))
        {
            if (dir_exists(candidate))
            {
                char root[KN_MAX_PATH];
                const char *scan_root = candidate;
                if (path_full(root, sizeof(root), candidate))
                    scan_root = root;
                scan_dir_modules(scan_root, scan_root, map);
                return;
            }
        }
    }
}

static void trace_log(int trace, const char *msg)
{
    if (!trace || !msg) return;
    kn_write_str("[Trace] ");
    kn_write_str(msg);
    kn_write_str("\n");
}

static uint64_t now_ms(void)
{
    return GetTickCount64();
}

static void write_u64(uint64_t v)
{
    char buf[32];
    int n = 0;
    if (v == 0)
    {
        kn_write_str("0");
        return;
    }
    while (v > 0 && n < (int)sizeof(buf))
    {
        buf[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = n - 1; i >= 0; i--)
    {
        char c[2];
        c[0] = buf[i];
        c[1] = 0;
        kn_write_str(c);
    }
}

static void profile_phase(int profile, const char *name, uint64_t ms)
{
    if (!profile) return;
    kn_write_str("[Profile] ");
    kn_write_str(name);
    kn_write_str(": ");
    write_u64(ms);
    kn_write_str(" ms\n");
}
