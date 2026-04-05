typedef struct
{
    int is_project;
    char manifest_path[KN_MAX_PATH];
    char manifest_dir[KN_MAX_PATH];
    char name[128];
    char version[64];
    char summary[256];
    char url[512];
    char entry[KN_MAX_PATH];
    char out_path[KN_MAX_PATH];
    char source_root[KN_MAX_PATH];
    char klib_path[KN_MAX_PATH];
    char modules[128][128];
    int module_count;
    char dependencies[128][128];
    int dependency_count;
    char package_roots[32][KN_MAX_PATH];
    int package_root_count;
    char source_files[256][KN_MAX_PATH];
    int source_file_count;
} KnPkgManifest;

typedef struct
{
    char compiler_path[KN_MAX_PATH];
    char manifest_text[65536];
    uint32_t source_count;
    uint64_t source_bytes;
} KnKlibInfo;

typedef struct
{
    char name[128];
    char version[64];
    char manifest_path[KN_MAX_PATH];
} KnPkgSelectedManifest;

#define KN_KLIB_MAGIC "KNKLIB1"
#define KN_KLIB_VERSION 1u

static int g_pkg_error = 0;

static void pkg_fail(const char *msg, const char *detail)
{
    g_pkg_error = 1;
    kn_write_str("[Package] ");
    kn_write_str(msg ? msg : "error");
    if (detail && detail[0])
    {
        kn_write_str(": ");
        kn_write_str(detail);
    }
    kn_write_str("\n");
}

static bool is_json_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *json_skip_ws_local(const char *p)
{
    while (p && *p && is_json_ws(*p))
        p++;
    return p;
}

static bool parse_json_string_local(const char **pp, char *out, size_t cap)
{
    const char *p = json_skip_ws_local(pp ? *pp : 0);
    size_t off = 0;
    if (!p || *p != '"' || !out || cap == 0)
        return false;
    p++;
    while (*p && *p != '"')
    {
        char c = *p++;
        if (c == '\\' && *p)
        {
            char e = *p++;
            if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else if (e == 'b') c = '\b';
            else if (e == 'f') c = '\f';
            else if (e == '\\') c = '\\';
            else if (e == '"') c = '"';
            else if (e == '/') c = '/';
            else c = e;
        }
        if (off + 1 < cap)
            out[off++] = c;
    }
    if (*p != '"')
        return false;
    out[off] = 0;
    if (pp) *pp = p + 1;
    return true;
}

static bool json_skip_value_local(const char **pp)
{
    const char *p = json_skip_ws_local(pp ? *pp : 0);
    int depth = 0;
    if (!p || !*p)
        return false;

    if (*p == '"')
    {
        char sink[2];
        return parse_json_string_local(&p, sink, sizeof(sink)) ? ((*pp = p), true) : false;
    }
    if (*p == '{')
    {
        depth = 1;
        p++;
        while (*p && depth > 0)
        {
            if (*p == '"')
            {
                char sink[2];
                if (!parse_json_string_local(&p, sink, sizeof(sink)))
                    return false;
                continue;
            }
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
        if (pp) *pp = p;
        return depth == 0;
    }
    if (*p == '[')
    {
        depth = 1;
        p++;
        while (*p && depth > 0)
        {
            if (*p == '"')
            {
                char sink[2];
                if (!parse_json_string_local(&p, sink, sizeof(sink)))
                    return false;
                continue;
            }
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            p++;
        }
        if (pp) *pp = p;
        return depth == 0;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']')
        p++;
    if (pp) *pp = p;
    return true;
}

static bool parse_json_string_array_local(const char **pp, char *items, int stride, size_t item_cap, int *count, int cap)
{
    const char *p = json_skip_ws_local(pp ? *pp : 0);
    if (!p || *p != '[')
        return false;
    p++;
    p = json_skip_ws_local(p);
    while (*p && *p != ']')
    {
        if (*count >= cap)
            return false;
        if (!parse_json_string_local(&p, items + ((size_t)(*count) * (size_t)stride), item_cap))
            return false;
        (*count)++;
        p = json_skip_ws_local(p);
        if (*p == ',')
        {
            p++;
            p = json_skip_ws_local(p);
            continue;
        }
        break;
    }
    p = json_skip_ws_local(p);
    if (*p != ']')
        return false;
    if (pp) *pp = p + 1;
    return true;
}

static bool pkg_manifest_file_name(const char *name)
{
    return name && (str_eq_ci(name, "package.knpkg.json") || str_eq_ci(name, "package.knpkg"));
}

static bool project_manifest_file_name(const char *name)
{
    return name && (str_eq_ci(name, "kinal.pkg.json") || str_eq_ci(name, "kinal.pkg"));
}

static const char *path_basename_local(const char *path);

static bool manifest_path_exists(const char *path)
{
    return path && path[0] && file_exists(path);
}

static bool resolve_manifest_input_path(const char *arg, int want_project, char *out, size_t cap)
{
    char candidate[KN_MAX_PATH];
    if (!arg || !arg[0] || !out || cap == 0)
        return false;
    if (manifest_path_exists(arg))
    {
        copy_str(out, cap, arg);
        return true;
    }
    if (!dir_exists(arg))
        return false;
    if (want_project)
    {
        if (path_join(candidate, sizeof(candidate), arg, "kinal.pkg.json") && manifest_path_exists(candidate))
        {
            copy_str(out, cap, candidate);
            return true;
        }
        if (path_join(candidate, sizeof(candidate), arg, "kinal.pkg") && manifest_path_exists(candidate))
        {
            copy_str(out, cap, candidate);
            return true;
        }
    }
    else
    {
        if (path_join(candidate, sizeof(candidate), arg, "package.knpkg.json") && manifest_path_exists(candidate))
        {
            copy_str(out, cap, candidate);
            return true;
        }
        if (path_join(candidate, sizeof(candidate), arg, "package.knpkg") && manifest_path_exists(candidate))
        {
            copy_str(out, cap, candidate);
            return true;
        }
    }
    return false;
}

static bool path_is_absolute_pkg(const char *path)
{
    if (!path || !path[0])
        return false;
    if (is_path_sep(path[0]))
        return true;
    if (kn_strlen(path) < 3)
        return false;
    return path[1] == ':' &&
           (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) &&
           is_path_sep(path[2]);
}

static bool path_relative_file(const char *root, const char *full, char *out, size_t cap)
{
    char a[KN_MAX_PATH];
    char b[KN_MAX_PATH];
    const char *ra = root;
    const char *rb = full;
    size_t n = 0;
    if (!root || !full || !out || cap == 0)
        return false;
    if (path_full(a, sizeof(a), root))
        ra = a;
    if (path_full(b, sizeof(b), full))
        rb = b;
    n = kn_strlen(ra);
    if (kn_strncmp(ra, rb, n) != 0)
        return false;
    rb += n;
    while (*rb && is_path_sep(*rb))
        rb++;
    if (!*rb)
        return false;
    copy_str(out, cap, rb);
    for (size_t i = 0; out[i]; i++)
        if (out[i] == '\\') out[i] = '/';
    return true;
}

static bool ensure_dir_recursive(const char *path)
{
    char tmp[KN_MAX_PATH];
    size_t n = 0;
    if (!path || !path[0])
        return false;
    copy_str(tmp, sizeof(tmp), path);
    n = kn_strlen(tmp);
    if (n == 0)
        return false;
    for (size_t i = 0; i < n; i++)
    {
        if (!is_path_sep(tmp[i]))
            continue;
        if (i == 0)
            continue;
        if (i == 2 && tmp[1] == ':')
            continue;
        {
            char hold = tmp[i];
            tmp[i] = 0;
            if (tmp[0] && !dir_exists(tmp))
                CreateDirectoryA(tmp, 0);
            tmp[i] = hold;
        }
    }
    if (!dir_exists(tmp))
        CreateDirectoryA(tmp, 0);
    return dir_exists(path);
}

static bool write_binary_file(const char *path, const void *data, size_t size)
{
    KN_HANDLE h;
    KN_DWORD wrote = 0;
    if (!path || !data)
        return false;
    h = CreateFileA(path, KN_GENERIC_WRITE, 0, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
        return false;
    if (!WriteFile(h, data, (KN_DWORD)size, &wrote, 0) || wrote != (KN_DWORD)size)
    {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);
    return true;
}

static bool write_text_file_local(const char *path, const char *text)
{
    return write_binary_file(path, text ? text : "", kn_strlen(text ? text : ""));
}

static void sanitize_cache_component(char *out, size_t cap, const char *text)
{
    size_t off = 0;
    if (!out || cap == 0)
        return;
    out[0] = 0;
    if (!text || !text[0])
    {
        copy_str(out, cap, "pkg");
        return;
    }
    for (size_t i = 0; text[i] && off + 1 < cap; i++)
    {
        char c = text[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9'))
            out[off++] = c;
        else
            out[off++] = '_';
    }
    out[off] = 0;
    if (!out[0])
        copy_str(out, cap, "pkg");
}

static bool default_klib_cache_root(char *out, size_t cap)
{
    char temp[KN_MAX_PATH];
    if (!out || cap == 0)
        return false;
    out[0] = 0;
    {
        KN_DWORD n = GetEnvironmentVariableA("TMP", temp, (KN_DWORD)sizeof(temp));
        if (n == 0 || n >= (KN_DWORD)sizeof(temp))
            n = GetEnvironmentVariableA("TEMP", temp, (KN_DWORD)sizeof(temp));
        if (n > 0 && n < (KN_DWORD)sizeof(temp))
        {
            if (!path_join(out, cap, temp, "kinal-cache"))
                return false;
            if (!path_join(out, cap, out, "klib"))
                return false;
            return true;
        }
    }
    if (!path_join(out, cap, ".", ".kinal-cache"))
        return false;
    if (!path_join(out, cap, out, "klib"))
        return false;
    return true;
}

static void append_json_escaped_local(char *buf, size_t cap, size_t *off, const char *s)
{
    if (!buf || !off || !s)
        return;
    for (size_t i = 0; s[i]; i++)
    {
        char ch = s[i];
        switch (ch)
        {
        case '\\': kn_append(buf, cap, off, "\\\\"); break;
        case '"': kn_append(buf, cap, off, "\\\""); break;
        case '\n': kn_append(buf, cap, off, "\\n"); break;
        case '\r': kn_append(buf, cap, off, "\\r"); break;
        case '\t': kn_append(buf, cap, off, "\\t"); break;
        default:
        {
            char tmp[2];
            tmp[0] = ch;
            tmp[1] = 0;
            kn_append(buf, cap, off, tmp);
            break;
        }
        }
    }
}

static bool read_u32_at(const unsigned char *data, size_t size, size_t *off, uint32_t *out)
{
    if (!data || !off || !out || *off + 4 > size)
        return false;
    *out = (uint32_t)data[*off] |
           ((uint32_t)data[*off + 1] << 8) |
           ((uint32_t)data[*off + 2] << 16) |
           ((uint32_t)data[*off + 3] << 24);
    *off += 4;
    return true;
}

static bool read_u64_at(const unsigned char *data, size_t size, size_t *off, uint64_t *out)
{
    uint64_t v = 0;
    if (!data || !off || !out || *off + 8 > size)
        return false;
    for (int i = 0; i < 8; i++)
        v |= ((uint64_t)data[*off + (size_t)i]) << (unsigned)(i * 8);
    *off += 8;
    *out = v;
    return true;
}

static bool parse_pkg_manifest(const char *path, KnPkgManifest *out)
{
    size_t size = 0;
    char *text = 0;
    const char *p = 0;
    if (!path || !out)
        return false;
    kn_memset(out, 0, sizeof(*out));
    copy_str(out->manifest_path, sizeof(out->manifest_path), path);
    path_dirname(path, out->manifest_dir, sizeof(out->manifest_dir));
    text = kn_read_file(path, &size);
    if (!text)
        return false;

    p = json_skip_ws_local(text);
    if (!p || *p != '{')
    {
        kn_free(text);
        return false;
    }
    p++;
    while (true)
    {
        char key[128];
        p = json_skip_ws_local(p);
        if (*p == '}')
        {
            p++;
            break;
        }
        if (!parse_json_string_local(&p, key, sizeof(key)))
        {
            kn_free(text);
            return false;
        }
        p = json_skip_ws_local(p);
        if (*p != ':')
        {
            kn_free(text);
            return false;
        }
        p++;
        if (str_eq_ci(key, "kind"))
        {
            char value[64];
            if (!parse_json_string_local(&p, value, sizeof(value)))
            {
                kn_free(text);
                return false;
            }
            out->is_project = str_eq_ci(value, "project") ? 1 : 0;
        }
        else if (str_eq_ci(key, "name"))
        {
            if (!parse_json_string_local(&p, out->name, sizeof(out->name)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "version"))
        {
            if (!parse_json_string_local(&p, out->version, sizeof(out->version)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "summary"))
        {
            if (!parse_json_string_local(&p, out->summary, sizeof(out->summary)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "url"))
        {
            if (!parse_json_string_local(&p, out->url, sizeof(out->url)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "entry"))
        {
            if (!parse_json_string_local(&p, out->entry, sizeof(out->entry)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "out"))
        {
            if (!parse_json_string_local(&p, out->out_path, sizeof(out->out_path)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "source_root"))
        {
            if (!parse_json_string_local(&p, out->source_root, sizeof(out->source_root)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "klib"))
        {
            if (!parse_json_string_local(&p, out->klib_path, sizeof(out->klib_path)))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "source_files"))
        {
            if (!parse_json_string_array_local(&p, (char *)out->source_files, KN_MAX_PATH, KN_MAX_PATH, &out->source_file_count,
                                               (int)(sizeof(out->source_files) / sizeof(out->source_files[0]))))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "modules"))
        {
            if (!parse_json_string_array_local(&p, (char *)out->modules, 128, 128, &out->module_count,
                                               (int)(sizeof(out->modules) / sizeof(out->modules[0]))))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "dependencies"))
        {
            if (!parse_json_string_array_local(&p, (char *)out->dependencies, 128, 128, &out->dependency_count,
                                               (int)(sizeof(out->dependencies) / sizeof(out->dependencies[0]))))
            {
                kn_free(text);
                return false;
            }
        }
        else if (str_eq_ci(key, "package_roots"))
        {
            if (!parse_json_string_array_local(&p, (char *)out->package_roots, KN_MAX_PATH, KN_MAX_PATH, &out->package_root_count,
                                               (int)(sizeof(out->package_roots) / sizeof(out->package_roots[0]))))
            {
                kn_free(text);
                return false;
            }
        }
        else
        {
            if (!json_skip_value_local(&p))
            {
                kn_free(text);
                return false;
            }
        }
        p = json_skip_ws_local(p);
        if (*p == ',')
        {
            p++;
            continue;
        }
        if (*p == '}')
        {
            p++;
            break;
        }
    }

    if (!out->is_project && !out->name[0])
    {
        kn_free(text);
        return false;
    }
    if (!out->source_root[0] && out->source_file_count == 0 && !out->klib_path[0] && !out->is_project)
    {
        kn_free(text);
        return false;
    }
    if (!out->entry[0] && out->source_file_count > 0)
        copy_str(out->entry, sizeof(out->entry), out->source_files[0]);
    kn_free(text);
    return true;
}

static int compare_version_text(const char *a, const char *b)
{
    const char *pa = a ? a : "";
    const char *pb = b ? b : "";
    while (*pa || *pb)
    {
        char ta[64];
        char tb[64];
        size_t oa = 0;
        size_t ob = 0;
        int da = 1;
        int db = 1;
        uint64_t va = 0;
        uint64_t vb = 0;

        while (*pa && *pa != '.')
        {
            if (*pa < '0' || *pa > '9')
                da = 0;
            if (oa + 1 < sizeof(ta))
                ta[oa++] = *pa;
            pa++;
        }
        while (*pb && *pb != '.')
        {
            if (*pb < '0' || *pb > '9')
                db = 0;
            if (ob + 1 < sizeof(tb))
                tb[ob++] = *pb;
            pb++;
        }
        ta[oa] = 0;
        tb[ob] = 0;
        if (*pa == '.')
            pa++;
        if (*pb == '.')
            pb++;

        if (da && db)
        {
            for (size_t i = 0; i < oa; i++)
                va = (va * 10u) + (uint64_t)(ta[i] - '0');
            for (size_t i = 0; i < ob; i++)
                vb = (vb * 10u) + (uint64_t)(tb[i] - '0');
            if (va < vb) return -1;
            if (va > vb) return 1;
        }
        else
        {
            int cmp = kn_strcmp(ta, tb);
            if (cmp < 0) return -1;
            if (cmp > 0) return 1;
        }
    }
    return 0;
}

static bool is_reserved_official_namespace(const char *name)
{
    return name && kn_strncmp(name, "IO.", 3) == 0;
}

static bool register_module_file_path(const char *path, ModuleMap *map)
{
    char *unit = 0;
    char canonical[KN_MAX_PATH];
    const char *store_path = path;
    char *owned = 0;
    if (!path || !path[0] || !map || !file_exists(path))
        return false;
    unit = read_unit_name_from_file(path);
    if (!unit)
        return false;
    if (path_full(canonical, sizeof(canonical), path))
        store_path = canonical;
    owned = dup_str(store_path);
    if (!owned)
    {
        kn_free(unit);
        return false;
    }
    modulemap_add(map, unit, owned);
    return true;
}

static bool should_skip_manifest_payload_file(const char *name)
{
    const char *ext = 0;
    size_t len;
    if (!name || !name[0])
        return true;
    if (str_eq_ci(name, "package.knpkg.json") || str_eq_ci(name, "package.knpkg"))
        return true;
    len = kn_strlen(name);
    for (size_t i = len; i > 0; i--)
    {
        if (name[i - 1] == '.')
        {
            ext = name + i - 1;
            break;
        }
        if (name[i - 1] == '\\' || name[i - 1] == '/')
            break;
    }
    return ext && str_eq_ci(ext, ".klib");
}

static bool collect_manifest_payload_files_recursive(const char *manifest_dir, const char *dir,
                                                     char files[][KN_MAX_PATH], int *count, int cap)
{
    char pattern[KN_MAX_PATH];
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h;
    if (!manifest_dir || !dir || !files || !count)
        return false;
    if (!path_join(pattern, sizeof(pattern), dir, "*"))
        return false;
    h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE)
        return false;
    do
    {
        char full[KN_MAX_PATH];
        if (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY)
        {
            if (kn_strcmp(data.cFileName, ".") == 0 || kn_strcmp(data.cFileName, "..") == 0)
                continue;
            if (kn_strcmp(data.cFileName, ".git") == 0 || kn_strcmp(data.cFileName, ".kinal-cache") == 0)
                continue;
            if (!path_join(full, sizeof(full), dir, data.cFileName))
                continue;
            if (!collect_manifest_payload_files_recursive(manifest_dir, full, files, count, cap))
                continue;
            continue;
        }
        if (should_skip_manifest_payload_file(data.cFileName))
            continue;
        if (*count >= cap)
        {
            FindClose(h);
            return false;
        }
        if (!path_join(full, sizeof(full), dir, data.cFileName))
            continue;
        if (!path_relative_file(manifest_dir, full, files[*count], KN_MAX_PATH))
            continue;
        (*count)++;
    } while (FindNextFileA(h, &data));
    FindClose(h);
    return true;
}

static void write_u32_le(KN_HANDLE h, uint32_t v)
{
    unsigned char buf[4];
    KN_DWORD wrote = 0;
    buf[0] = (unsigned char)(v & 0xFFu);
    buf[1] = (unsigned char)((v >> 8) & 0xFFu);
    buf[2] = (unsigned char)((v >> 16) & 0xFFu);
    buf[3] = (unsigned char)((v >> 24) & 0xFFu);
    WriteFile(h, buf, 4, &wrote, 0);
}

static void write_u64_le(KN_HANDLE h, uint64_t v)
{
    unsigned char buf[8];
    KN_DWORD wrote = 0;
    for (int i = 0; i < 8; i++)
        buf[i] = (unsigned char)((v >> (unsigned)(i * 8)) & 0xFFu);
    WriteFile(h, buf, 8, &wrote, 0);
}

static int klib_pack_from_manifest(const char *manifest_path, const char *out_path)
{
    KnPkgManifest manifest;
    char compiler_path[KN_MAX_PATH];
    char manifest_full[KN_MAX_PATH];
    char manifest_dir[KN_MAX_PATH];
    char files[256][KN_MAX_PATH];
    int file_count = 0;
    char *manifest_text = 0;
    size_t manifest_size = 0;
    KN_HANDLE h;
    KN_DWORD wrote = 0;
    if (!manifest_path || !out_path)
        return -1;
    if (!parse_pkg_manifest(manifest_path, &manifest))
    {
        pkg_fail("invalid package manifest", manifest_path);
        return -1;
    }
    if (manifest.is_project)
    {
        pkg_fail("expected a package manifest, got project manifest", manifest_path);
        return -1;
    }
    manifest_text = kn_read_file(manifest_path, &manifest_size);
    if (!manifest_text)
    {
        pkg_fail("failed to read package manifest", manifest_path);
        return -1;
    }
    if (!path_full(manifest_full, sizeof(manifest_full), manifest_path))
        copy_str(manifest_full, sizeof(manifest_full), manifest_path);
    path_dirname(manifest_full, manifest_dir, sizeof(manifest_dir));

    if (!collect_manifest_payload_files_recursive(manifest_dir, manifest_dir, files, &file_count,
                                                  (int)(sizeof(files) / sizeof(files[0]))))
    {
        kn_free(manifest_text);
        pkg_fail("failed to enumerate package payload files", manifest_dir);
        return -1;
    }
    if (file_count == 0)
    {
        kn_free(manifest_text);
        pkg_fail("package has no files to pack", manifest_path);
        return -1;
    }

    if (!get_exe_path(compiler_path, sizeof(compiler_path)))
        compiler_path[0] = 0;

    {
        char out_dir[KN_MAX_PATH];
        path_dirname(out_path, out_dir, sizeof(out_dir));
        if (out_dir[0] && !dir_exists(out_dir))
            ensure_dir_recursive(out_dir);
    }

    h = CreateFileA(out_path, KN_GENERIC_WRITE, 0, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
    {
        kn_free(manifest_text);
        pkg_fail("failed to create klib", out_path);
        return -1;
    }

    WriteFile(h, KN_KLIB_MAGIC, 8, &wrote, 0);
    write_u32_le(h, KN_KLIB_VERSION);
    write_u32_le(h, (uint32_t)kn_strlen(compiler_path));
    write_u32_le(h, (uint32_t)manifest_size);
    write_u32_le(h, (uint32_t)file_count);
    if (compiler_path[0])
        WriteFile(h, compiler_path, (KN_DWORD)kn_strlen(compiler_path), &wrote, 0);
    if (manifest_size > 0)
        WriteFile(h, manifest_text, (KN_DWORD)manifest_size, &wrote, 0);

    for (int i = 0; i < file_count; i++)
    {
        char full_path[KN_MAX_PATH];
        char *src = 0;
        size_t src_size = 0;
        if (!path_join(full_path, sizeof(full_path), manifest_dir, files[i]))
        {
            CloseHandle(h);
            kn_free(manifest_text);
            pkg_fail("failed to resolve package file", files[i]);
            return -1;
        }
        src = kn_read_file(full_path, &src_size);
        if (!src)
        {
            CloseHandle(h);
            kn_free(manifest_text);
            pkg_fail("failed to read package file", full_path);
            return -1;
        }
        write_u32_le(h, (uint32_t)kn_strlen(files[i]));
        write_u64_le(h, (uint64_t)src_size);
        WriteFile(h, files[i], (KN_DWORD)kn_strlen(files[i]), &wrote, 0);
        if (src_size > 0)
            WriteFile(h, src, (KN_DWORD)src_size, &wrote, 0);
        kn_free(src);
    }

    CloseHandle(h);
    kn_free(manifest_text);
    return 0;
}

static int emit_local_package_from_manifest(const char *manifest_path, const char *package_root)
{
    KnPkgManifest manifest;
    char root_dir[KN_MAX_PATH];
    char package_dir[KN_MAX_PATH];
    char lib_dir[KN_MAX_PATH];
    char klib_path[KN_MAX_PATH];
    char manifest_out[KN_MAX_PATH];
    char rel_klib[KN_MAX_PATH];
    char klib_name[256];
    char json[65536];
    size_t off = 0;
    if (!manifest_path || !package_root)
        return -1;
    if (!parse_pkg_manifest(manifest_path, &manifest))
    {
        pkg_fail("invalid package manifest", manifest_path);
        return -1;
    }
    if (manifest.is_project)
    {
        pkg_fail("expected a package manifest, got project manifest", manifest_path);
        return -1;
    }
    if (!path_full(root_dir, sizeof(root_dir), package_root))
        copy_str(root_dir, sizeof(root_dir), package_root);
    if (!path_join(package_dir, sizeof(package_dir), root_dir, manifest.name) ||
        !path_join(package_dir, sizeof(package_dir), package_dir, manifest.version[0] ? manifest.version : "0.0.0"))
    {
        pkg_fail("failed to resolve output package directory", package_root);
        return -1;
    }
    copy_str(klib_name, sizeof(klib_name), manifest.name);
    {
        size_t n = kn_strlen(klib_name);
        if (n + 5 >= sizeof(klib_name))
        {
            pkg_fail("package name is too long for klib emission", manifest.name);
            return -1;
        }
        kn_memcpy(klib_name + n, ".klib", 6);
    }
    if (!path_join(lib_dir, sizeof(lib_dir), package_dir, "lib") ||
        !path_join(klib_path, sizeof(klib_path), lib_dir, klib_name))
    {
        pkg_fail("failed to resolve output klib path", package_root);
        return -1;
    }
    if (!path_join(manifest_out, sizeof(manifest_out), package_dir, "package.knpkg.json"))
    {
        pkg_fail("failed to resolve output manifest path", package_root);
        return -1;
    }
    if (!dir_exists(lib_dir))
        ensure_dir_recursive(lib_dir);
    if (klib_pack_from_manifest(manifest_path, klib_path) != 0)
        return -1;

    copy_str(rel_klib, sizeof(rel_klib), "lib/");
    {
        size_t n = kn_strlen(rel_klib);
        const char *base = path_basename_local(klib_path);
        for (size_t i = 0; base[i] && n + 1 < sizeof(rel_klib); i++)
            rel_klib[n++] = (base[i] == '\\') ? '/' : base[i];
        rel_klib[n] = 0;
    }

    json[0] = 0;
    kn_append(json, sizeof(json), &off, "{\n");
    kn_append(json, sizeof(json), &off, "  \"kind\": \"package\",\n");
    kn_append(json, sizeof(json), &off, "  \"name\": \"");
    append_json_escaped_local(json, sizeof(json), &off, manifest.name);
    kn_append(json, sizeof(json), &off, "\",\n");
    kn_append(json, sizeof(json), &off, "  \"version\": \"");
    append_json_escaped_local(json, sizeof(json), &off, manifest.version[0] ? manifest.version : "0.0.0");
    kn_append(json, sizeof(json), &off, "\"");
    if (manifest.summary[0])
    {
        kn_append(json, sizeof(json), &off, ",\n  \"summary\": \"");
        append_json_escaped_local(json, sizeof(json), &off, manifest.summary);
        kn_append(json, sizeof(json), &off, "\"");
    }
    if (manifest.url[0])
    {
        kn_append(json, sizeof(json), &off, ",\n  \"url\": \"");
        append_json_escaped_local(json, sizeof(json), &off, manifest.url);
        kn_append(json, sizeof(json), &off, "\"");
    }
    if (manifest.entry[0])
    {
        kn_append(json, sizeof(json), &off, ",\n  \"entry\": \"");
        append_json_escaped_local(json, sizeof(json), &off, manifest.entry);
        kn_append(json, sizeof(json), &off, "\"");
    }
    kn_append(json, sizeof(json), &off, ",\n  \"klib\": \"");
    append_json_escaped_local(json, sizeof(json), &off, rel_klib);
    kn_append(json, sizeof(json), &off, "\"");
    if (manifest.module_count > 0)
    {
        kn_append(json, sizeof(json), &off, ",\n  \"modules\": [");
        for (int i = 0; i < manifest.module_count; i++)
        {
            if (i > 0) kn_append(json, sizeof(json), &off, ", ");
            kn_append(json, sizeof(json), &off, "\"");
            append_json_escaped_local(json, sizeof(json), &off, manifest.modules[i]);
            kn_append(json, sizeof(json), &off, "\"");
        }
        kn_append(json, sizeof(json), &off, "]");
    }
    if (manifest.dependency_count > 0)
    {
        kn_append(json, sizeof(json), &off, ",\n  \"dependencies\": [");
        for (int i = 0; i < manifest.dependency_count; i++)
        {
            if (i > 0) kn_append(json, sizeof(json), &off, ", ");
            kn_append(json, sizeof(json), &off, "\"");
            append_json_escaped_local(json, sizeof(json), &off, manifest.dependencies[i]);
            kn_append(json, sizeof(json), &off, "\"");
        }
        kn_append(json, sizeof(json), &off, "]");
    }
    kn_append(json, sizeof(json), &off, "\n}\n");
    if (!write_text_file_local(manifest_out, json))
    {
        pkg_fail("failed to write emitted package manifest", manifest_out);
        return -1;
    }
    kn_write_str("[Package] emitted local package: ");
    kn_write_str(package_dir);
    kn_write_str("\n");
    return 0;
}

static int klib_read_info(const char *path, KnKlibInfo *info)
{
    size_t size = 0;
    unsigned char *data = 0;
    size_t off = 0;
    uint32_t version = 0;
    uint32_t compiler_len = 0;
    uint32_t manifest_len = 0;
    uint32_t raw_compiler_len = 0;
    uint32_t raw_manifest_len = 0;
    uint32_t source_count = 0;
    if (!path || !info)
        return -1;
    kn_memset(info, 0, sizeof(*info));
    data = (unsigned char *)kn_read_file(path, &size);
    if (!data || size < 20)
        return -1;
    if (kn_strncmp((const char *)data, KN_KLIB_MAGIC, 7) != 0)
    {
        kn_free(data);
        return -1;
    }
    off = 8;
    if (!read_u32_at(data, size, &off, &version) ||
        !read_u32_at(data, size, &off, &compiler_len) ||
        !read_u32_at(data, size, &off, &manifest_len) ||
        !read_u32_at(data, size, &off, &source_count))
    {
        kn_free(data);
        return -1;
    }
    raw_compiler_len = compiler_len;
    raw_manifest_len = manifest_len;
    if (version != KN_KLIB_VERSION || off + raw_compiler_len + raw_manifest_len > size)
    {
        kn_free(data);
        return -1;
    }
    if (compiler_len >= sizeof(info->compiler_path))
        compiler_len = (uint32_t)sizeof(info->compiler_path) - 1;
    if (compiler_len > 0)
        kn_memcpy(info->compiler_path, data + off, compiler_len);
    info->compiler_path[compiler_len] = 0;
    off += raw_compiler_len;
    if (manifest_len >= sizeof(info->manifest_text))
        manifest_len = (uint32_t)sizeof(info->manifest_text) - 1;
    if (manifest_len > 0)
        kn_memcpy(info->manifest_text, data + off, manifest_len);
    info->manifest_text[manifest_len] = 0;
    off += raw_manifest_len;
    info->source_count = source_count;
    for (uint32_t i = 0; i < source_count; i++)
    {
        uint32_t rel_len = 0;
        uint64_t src_size = 0;
        if (!read_u32_at(data, size, &off, &rel_len) ||
            !read_u64_at(data, size, &off, &src_size) ||
            off + rel_len + src_size > size)
        {
            kn_free(data);
            return -1;
        }
        off += rel_len;
        off += (size_t)src_size;
        info->source_bytes += src_size;
    }
    kn_free(data);
    return 0;
}

static int klib_extract_to_dir(const char *path, const char *out_dir)
{
    size_t size = 0;
    unsigned char *data = 0;
    size_t off = 0;
    uint32_t version = 0;
    uint32_t compiler_len = 0;
    uint32_t manifest_len = 0;
    uint32_t source_count = 0;
    char manifest_path[KN_MAX_PATH];
    if (!path || !out_dir)
        return -1;
    data = (unsigned char *)kn_read_file(path, &size);
    if (!data || size < 20)
        return -1;
    if (kn_strncmp((const char *)data, KN_KLIB_MAGIC, 7) != 0)
    {
        kn_free(data);
        return -1;
    }
    off = 8;
    if (!read_u32_at(data, size, &off, &version) ||
        !read_u32_at(data, size, &off, &compiler_len) ||
        !read_u32_at(data, size, &off, &manifest_len) ||
        !read_u32_at(data, size, &off, &source_count))
    {
        kn_free(data);
        return -1;
    }
    if (version != KN_KLIB_VERSION || off + compiler_len + manifest_len > size)
    {
        kn_free(data);
        return -1;
    }
    ensure_dir_recursive(out_dir);
    if (!path_join(manifest_path, sizeof(manifest_path), out_dir, "package.knpkg.json"))
    {
        kn_free(data);
        return -1;
    }
    off += compiler_len;
    {
        KN_HANDLE h = CreateFileA(manifest_path, KN_GENERIC_WRITE, 0, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
        KN_DWORD wrote = 0;
        if (!h || h == KN_INVALID_HANDLE_VALUE)
        {
            kn_free(data);
            return -1;
        }
        if (manifest_len > 0)
            WriteFile(h, data + off, manifest_len, &wrote, 0);
        CloseHandle(h);
        off += manifest_len;
    }

    for (uint32_t i = 0; i < source_count; i++)
    {
        uint32_t rel_len = 0;
        uint64_t src_size = 0;
        char rel[KN_MAX_PATH];
        char out_path[KN_MAX_PATH];
        char out_parent[KN_MAX_PATH];
        if (!read_u32_at(data, size, &off, &rel_len) ||
            !read_u64_at(data, size, &off, &src_size) ||
            rel_len >= sizeof(rel) ||
            off + rel_len + src_size > size)
        {
            kn_free(data);
            return -1;
        }
        kn_memcpy(rel, data + off, rel_len);
        rel[rel_len] = 0;
        off += rel_len;
        for (size_t j = 0; rel[j]; j++)
            if (rel[j] == '/') rel[j] = '\\';
        if (!path_join(out_path, sizeof(out_path), out_dir, rel))
        {
            kn_free(data);
            return -1;
        }
        path_dirname(out_path, out_parent, sizeof(out_parent));
        if (out_parent[0] && !dir_exists(out_parent))
            ensure_dir_recursive(out_parent);
        if (!write_binary_file(out_path, data + off, (size_t)src_size))
        {
            kn_free(data);
            return -1;
        }
        off += (size_t)src_size;
    }
    kn_free(data);
    return 0;
}

static bool scan_package_manifest_tree(const char *dir, StrList *manifests)
{
    char pattern[KN_MAX_PATH];
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h;
    if (!dir || !manifests)
        return false;
    if (!path_join(pattern, sizeof(pattern), dir, "*"))
        return false;
    h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE)
        return false;
    do
    {
        char full[KN_MAX_PATH];
        if (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY)
        {
            if (kn_strcmp(data.cFileName, ".") == 0 || kn_strcmp(data.cFileName, "..") == 0)
                continue;
            if (kn_strcmp(data.cFileName, ".git") == 0 || kn_strcmp(data.cFileName, ".kinal-cache") == 0 ||
                kn_strcmp(data.cFileName, "build") == 0 || kn_strcmp(data.cFileName, "out") == 0)
                continue;
            if (!path_join(full, sizeof(full), dir, data.cFileName))
                continue;
            scan_package_manifest_tree(full, manifests);
            continue;
        }
        if (!pkg_manifest_file_name(data.cFileName))
            continue;
        if (!path_join(full, sizeof(full), dir, data.cFileName))
            continue;
        {
            char canonical[KN_MAX_PATH];
            const char *store = full;
            if (path_full(canonical, sizeof(canonical), full))
                store = canonical;
            strlist_add(manifests, dup_str(store));
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);
    return true;
}

static bool manifest_resolve_path(const KnPkgManifest *manifest, const char *rel, char *out, size_t cap)
{
    if (!manifest || !rel || !rel[0] || !out || cap == 0)
        return false;
    if (path_is_absolute_pkg(rel))
    {
        copy_str(out, cap, rel);
        return true;
    }
    return path_join(out, cap, manifest->manifest_dir, rel);
}

static bool register_manifest_into_module_map(const KnPkgManifest *manifest, ModuleMap *map, int official_root)
{
    if (!manifest || !map)
        return false;
    if (!official_root && is_reserved_official_namespace(manifest->name))
    {
        pkg_fail("reserved package namespace 'IO.*' is only allowed in stdpkg", manifest->name);
        return false;
    }
    if (manifest->klib_path[0])
    {
        char klib[KN_MAX_PATH];
        char cache_root[KN_MAX_PATH];
        char cache_dir[KN_MAX_PATH];
        char embedded_manifest[KN_MAX_PATH];
        char cache_name[256];
        char safe_name[160];
        char safe_version[64];
        KnPkgManifest extracted;
        if (!manifest_resolve_path(manifest, manifest->klib_path, klib, sizeof(klib)))
            return false;
        if (!file_exists(klib))
        {
            klib[0] = 0;
        }
        if (klib[0])
        {
            if (!default_klib_cache_root(cache_root, sizeof(cache_root)))
                return false;
            if (!dir_exists(cache_root))
                ensure_dir_recursive(cache_root);
            sanitize_cache_component(safe_name, sizeof(safe_name), manifest->name);
            sanitize_cache_component(safe_version, sizeof(safe_version), manifest->version[0] ? manifest->version : "0");
            copy_str(cache_name, sizeof(cache_name), safe_name);
            {
                size_t off = kn_strlen(cache_name);
                if (off + 1 < sizeof(cache_name))
                    cache_name[off++] = '-';
                cache_name[off] = 0;
                kn_append(cache_name, sizeof(cache_name), &off, safe_version);
            }
            if (!path_join(cache_dir, sizeof(cache_dir), cache_root, cache_name))
                return false;
            if (!path_join(embedded_manifest, sizeof(embedded_manifest), cache_dir, "package.knpkg.json"))
                return false;
            if (!dir_exists(cache_dir))
                ensure_dir_recursive(cache_dir);
            if (klib_extract_to_dir(klib, cache_dir) != 0)
            {
                pkg_fail("failed to extract klib", klib);
                return false;
            }
            if (!parse_pkg_manifest(embedded_manifest, &extracted))
            {
                pkg_fail("failed to parse embedded package manifest", embedded_manifest);
                return false;
            }
            return register_manifest_into_module_map(&extracted, map, official_root);
        }
    }
    if (manifest->source_file_count > 0)
    {
        for (int i = 0; i < manifest->source_file_count; i++)
        {
            char full[KN_MAX_PATH];
            if (!manifest_resolve_path(manifest, manifest->source_files[i], full, sizeof(full)))
                return false;
            if (!register_module_file_path(full, map))
            {
                pkg_fail("failed to register package source file", full);
                return false;
            }
        }
        return true;
    }
    if (manifest->source_root[0])
    {
        char root[KN_MAX_PATH];
        if (!manifest_resolve_path(manifest, manifest->source_root, root, sizeof(root)))
            return false;
        if (!dir_exists(root))
        {
            pkg_fail("package source_root does not exist", root);
            return false;
        }
        scan_dir_modules(root, root, map);
        return true;
    }
    return true;
}

static bool scan_package_roots_into_module_map(const char **roots, int root_count, ModuleMap *map, int official_root)
{
    StrList manifests = {0};
    KnPkgSelectedManifest selected[256];
    int selected_count = 0;
    if (!roots || root_count <= 0 || !map)
        return true;
    for (int i = 0; i < root_count; i++)
    {
        const char *root = roots[i];
        if (!root || !root[0] || !dir_exists(root))
            continue;
        scan_package_manifest_tree(root, &manifests);
    }
    for (int i = 0; i < manifests.count; i++)
    {
        KnPkgManifest manifest;
        const char *path = manifests.items[i];
        if (!path || !path[0])
            continue;
        if (!parse_pkg_manifest(path, &manifest))
        {
            pkg_fail("invalid package manifest", path);
            return false;
        }
        if (manifest.is_project)
            continue;
        if (!official_root && is_reserved_official_namespace(manifest.name))
        {
            pkg_fail("reserved package namespace 'IO.*' is only allowed in stdpkg", manifest.name);
            return false;
        }
        {
            int existing = -1;
            for (int j = 0; j < selected_count; j++)
            {
                if (kn_strcmp(selected[j].name, manifest.name) == 0)
                {
                    existing = j;
                    break;
                }
            }
            if (existing < 0)
            {
                if (selected_count >= (int)(sizeof(selected) / sizeof(selected[0])))
                {
                    pkg_fail("too many packages in package roots", path);
                    return false;
                }
                copy_str(selected[selected_count].name, sizeof(selected[selected_count].name), manifest.name);
                copy_str(selected[selected_count].version, sizeof(selected[selected_count].version), manifest.version);
                copy_str(selected[selected_count].manifest_path, sizeof(selected[selected_count].manifest_path), path);
                selected_count++;
            }
            else if (compare_version_text(manifest.version, selected[existing].version) > 0)
            {
                copy_str(selected[existing].version, sizeof(selected[existing].version), manifest.version);
                copy_str(selected[existing].manifest_path, sizeof(selected[existing].manifest_path), path);
            }
        }
    }
    for (int i = 0; i < selected_count; i++)
    {
        KnPkgManifest manifest;
        if (!parse_pkg_manifest(selected[i].manifest_path, &manifest))
        {
            pkg_fail("invalid package manifest", selected[i].manifest_path);
            return false;
        }
        if (!register_manifest_into_module_map(&manifest, map, official_root))
            return false;
    }
    return !g_pkg_error;
}

static int append_root_list(StrList *roots, const char *path)
{
    char full[KN_MAX_PATH];
    const char *store = path;
    if (!roots || !path || !path[0])
        return 0;
    if (path_full(full, sizeof(full), path))
        store = full;
    if (dir_exists(store) && !strlist_contains_path(roots, store))
    {
        strlist_add(roots, dup_str(store));
        return 1;
    }
    return 0;
}

static void collect_default_official_package_roots(StrList *roots)
{
    char exe_dir[KN_MAX_PATH];
    char candidate[KN_MAX_PATH];
    if (!roots)
        return;
    if (append_root_list(roots, "stdpkg"))
        return;
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (path_join(candidate, sizeof(candidate), exe_dir, "stdpkg"))
        {
            if (append_root_list(roots, candidate))
                return;
        }
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
                    if (append_root_list(roots, candidate))
                        return;
                }
            }
        }
    }
}

static const char *path_basename_local(const char *path)
{
    const char *name = path;
    if (!path) return "";
    for (size_t i = kn_strlen(path); i > 0; i--)
    {
        if (is_path_sep(path[i - 1]))
        {
            name = path + i;
            break;
        }
    }
    return name;
}

static void print_klib_info(const char *path)
{
    KnKlibInfo info;
    if (!path || klib_read_info(path, &info) != 0)
    {
        kn_write_str("invalid klib: ");
        kn_write_str(path ? path : "<null>");
        kn_write_str("\n");
        return;
    }
    kn_write_str("[Klib]\n");
    kn_write_str("  File: ");
    kn_write_str(path);
    kn_write_str("\n");
    kn_write_str("  Compiler: ");
    kn_write_str(info.compiler_path[0] ? info.compiler_path : "<unknown>");
    kn_write_str("\n");
    kn_write_str("  Sources: ");
    kn_write_u32(info.source_count);
    kn_write_str("\n");
    kn_write_str("  SourceBytes: ");
    write_u64(info.source_bytes);
    kn_write_str("\n");
}
