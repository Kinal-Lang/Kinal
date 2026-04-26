typedef enum
{
    KN_PROJ_BACKEND_UNSET = 0,
    KN_PROJ_BACKEND_NATIVE = 1,
    KN_PROJ_BACKEND_VM = 2
} KnProjBackend;

typedef struct
{
    char name[64];
    char entry[KN_MAX_PATH];
    char out_path[KN_MAX_PATH];
    char target[128];
    char entry_symbol[128];
    char linker_path[KN_MAX_PATH];
    char link_script[KN_MAX_PATH];

    int has_backend;
    int backend;
    int has_env;
    int env_kind;
    int has_runtime;
    int runtime_mode;
    int has_panic;
    int panic_mode;
    int has_linker_kind;
    int linker_kind;
    int has_no_crt;
    int no_crt;
    int has_no_default_libs;
    int no_default_libs;
    int has_auto_link;
    int auto_link;
    int has_superloop;
    int knc_superloop;

    char package_roots[32][KN_MAX_PATH];
    int package_root_count;
    char official_package_roots[32][KN_MAX_PATH];
    int official_package_root_count;

    char lib_dirs[32][KN_MAX_PATH];
    int lib_dir_count;
    char libs[32][128];
    int lib_count;
    char link_files[32][KN_MAX_PATH];
    int link_file_count;
    char link_args[32][256];
    int link_arg_count;
    char link_roots[32][KN_MAX_PATH];
    int link_root_count;
} KnProjProfile;

typedef struct
{
    char project_path[KN_MAX_PATH];
    char project_dir[KN_MAX_PATH];
    char name[128];
    char default_profile[64];
    char lsp_profile[64];

    char package_roots[32][KN_MAX_PATH];
    int package_root_count;
    char official_package_roots[32][KN_MAX_PATH];
    int official_package_root_count;

    KnProjProfile profiles[16];
    int profile_count;
} KnProjectConfig;

static KnProjectConfig *knproj_alloc_config(void)
{
    KnProjectConfig *cfg = (KnProjectConfig *)kn_malloc(sizeof(KnProjectConfig));
    if (!cfg)
        return 0;
    kn_memset(cfg, 0, sizeof(*cfg));
    return cfg;
}

static void knproj_free_config(KnProjectConfig *cfg)
{
    if (cfg)
        kn_free(cfg);
}

typedef struct
{
    Lexer lex;
    Token cur;
    const char *path;
    int failed;
} KnProjParser;

static char g_knproj_error[256];

static void knproj_append_int(char *buf, size_t cap, size_t *off, int v)
{
    char tmp[32];
    size_t n = 0;
    unsigned int x = 0;
    if (!buf || cap == 0 || !off)
        return;
    if (v == 0)
    {
        kn_append(buf, cap, off, "0");
        return;
    }
    if (v < 0)
        return;
    x = (unsigned int)v;
    while (x > 0 && n < sizeof(tmp))
    {
        tmp[n++] = (char)('0' + (x % 10u));
        x /= 10u;
    }
    while (n > 0)
    {
        char one[2];
        one[0] = tmp[--n];
        one[1] = 0;
        kn_append(buf, cap, off, one);
    }
}

static void knproj_set_error(const char *path, int line, int col, const char *msg)
{
    size_t off = 0;
    g_knproj_error[0] = 0;
    if (path && path[0])
    {
        kn_append(g_knproj_error, sizeof(g_knproj_error), &off, path);
        kn_append(g_knproj_error, sizeof(g_knproj_error), &off, ":");
    }
    if (line > 0)
    {
        knproj_append_int(g_knproj_error, sizeof(g_knproj_error), &off, line);
        if (col > 0)
        {
            kn_append(g_knproj_error, sizeof(g_knproj_error), &off, ":");
            knproj_append_int(g_knproj_error, sizeof(g_knproj_error), &off, col);
        }
        kn_append(g_knproj_error, sizeof(g_knproj_error), &off, ": ");
    }
    kn_append(g_knproj_error, sizeof(g_knproj_error), &off, msg ? msg : "invalid knproj");
}

static void knproj_next(KnProjParser *p)
{
    if (!p)
        return;
    p->cur = lex_next(&p->lex);
}

static bool knproj_text_eq(Token tok, const char *text)
{
    size_t n = 0;
    if (!text)
        return false;
    n = kn_strlen(text);
    return tok.length == n && kn_strncmp(tok.start, text, n) == 0;
}

static bool knproj_text_eq_ci(Token tok, const char *text)
{
    size_t n = 0;
    if (!text)
        return false;
    n = kn_strlen(text);
    if (tok.length != n)
        return false;
    for (size_t i = 0; i < n; i++)
    {
        char a = tok.start[i];
        char b = text[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}

static bool knproj_accept(KnProjParser *p, TokenType type)
{
    if (!p || p->cur.type != type)
        return false;
    knproj_next(p);
    return true;
}

static bool knproj_expect(KnProjParser *p, TokenType type, const char *msg)
{
    if (!p)
        return false;
    if (p->cur.type == type)
    {
        knproj_next(p);
        return true;
    }
    knproj_set_error(p->path, p->cur.line, p->cur.col, msg);
    p->failed = 1;
    return false;
}

static bool knproj_copy_string_token(Token tok, char *out, size_t cap)
{
    size_t off = 0;
    if (!out || cap == 0)
        return false;
    out[0] = 0;
    if (tok.type != TOK_STRING || tok.length < 2 || tok.start[0] != '"' || tok.start[tok.length - 1] != '"')
        return false;
    for (size_t i = 1; i + 1 < tok.length; i++)
    {
        char c = tok.start[i];
        if (c == '\\' && i + 1 < tok.length - 1)
        {
            char e = tok.start[++i];
            if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else if (e == 'b') c = '\b';
            else if (e == 'f') c = '\f';
            else if (e == '\\') c = '\\';
            else if (e == '"') c = '"';
            else c = e;
        }
        if (off + 1 < cap)
            out[off++] = c;
    }
    out[off] = 0;
    return true;
}

static bool knproj_copy_text_token(Token tok, char *out, size_t cap)
{
    size_t len = 0;
    if (!out || cap == 0)
        return false;
    out[0] = 0;
    if (tok.type == TOK_STRING)
        return knproj_copy_string_token(tok, out, cap);
    len = tok.length < cap - 1 ? tok.length : cap - 1;
    kn_memcpy(out, tok.start, len);
    out[len] = 0;
    return len > 0;
}

static bool knproj_parse_name(KnProjParser *p, char *out, size_t cap, const char *msg)
{
    if (!p || !out || cap == 0)
        return false;
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, msg);
        p->failed = 1;
        return false;
    }
    if (!knproj_copy_text_token(p->cur, out, cap))
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, msg);
        p->failed = 1;
        return false;
    }
    knproj_next(p);
    return true;
}

static bool knproj_parse_bool(KnProjParser *p, int *out, const char *msg)
{
    if (!p || !out)
        return false;
    if (p->cur.type == TOK_TRUE)
    {
        *out = 1;
        knproj_next(p);
        return true;
    }
    if (p->cur.type == TOK_FALSE)
    {
        *out = 0;
        knproj_next(p);
        return true;
    }
    knproj_set_error(p->path, p->cur.line, p->cur.col, msg);
    p->failed = 1;
    return false;
}

static bool knproj_parse_string_list(KnProjParser *p, char *items, int stride, size_t item_cap, int *count, int cap, const char *msg)
{
    if (!knproj_expect(p, TOK_LBRACKET, "expected '['"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACKET && p->cur.type != TOK_EOF)
    {
        if (*count >= cap)
        {
            knproj_set_error(p->path, p->cur.line, p->cur.col, msg);
            p->failed = 1;
            return false;
        }
        if (p->cur.type != TOK_STRING && p->cur.type != TOK_ID)
        {
            knproj_set_error(p->path, p->cur.line, p->cur.col, "expected string or identifier");
            p->failed = 1;
            return false;
        }
        if (!knproj_copy_text_token(p->cur, items + ((size_t)(*count) * (size_t)stride), item_cap))
        {
            knproj_set_error(p->path, p->cur.line, p->cur.col, msg);
            p->failed = 1;
            return false;
        }
        (*count)++;
        knproj_next(p);
        if (p->cur.type == TOK_COMMA)
        {
            knproj_next(p);
            continue;
        }
        break;
    }
    return knproj_expect(p, TOK_RBRACKET, "expected ']'");
}

static bool knproj_parse_assign(KnProjParser *p)
{
    return knproj_expect(p, TOK_ASSIGN, "expected '='");
}

static bool knproj_parse_semi(KnProjParser *p)
{
    return knproj_expect(p, TOK_SEMI, "expected ';'");
}

static bool knproj_parse_backend(KnProjParser *p, int *out)
{
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "expected backend name");
        p->failed = 1;
        return false;
    }
    if (knproj_text_eq_ci(p->cur, "Native"))
        *out = KN_PROJ_BACKEND_NATIVE;
    else if (knproj_text_eq_ci(p->cur, "VM"))
        *out = KN_PROJ_BACKEND_VM;
    else
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "unknown backend");
        p->failed = 1;
        return false;
    }
    knproj_next(p);
    return true;
}

static bool knproj_parse_env(KnProjParser *p, int *out)
{
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "expected environment name");
        p->failed = 1;
        return false;
    }
    if (knproj_text_eq_ci(p->cur, "Hosted"))
        *out = KN_ENV_HOSTED;
    else if (knproj_text_eq_ci(p->cur, "Freestanding"))
        *out = KN_ENV_FREESTANDING;
    else
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "unknown environment");
        p->failed = 1;
        return false;
    }
    knproj_next(p);
    return true;
}

static bool knproj_parse_runtime(KnProjParser *p, int *out)
{
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "expected runtime name");
        p->failed = 1;
        return false;
    }
    if (knproj_text_eq_ci(p->cur, "None"))
        *out = KN_RUNTIME_NONE;
    else if (knproj_text_eq_ci(p->cur, "Alloc"))
        *out = KN_RUNTIME_ALLOC;
    else if (knproj_text_eq_ci(p->cur, "GC"))
        *out = KN_RUNTIME_GC;
    else
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "unknown runtime");
        p->failed = 1;
        return false;
    }
    knproj_next(p);
    return true;
}

static bool knproj_parse_panic(KnProjParser *p, int *out)
{
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "expected panic mode");
        p->failed = 1;
        return false;
    }
    if (knproj_text_eq_ci(p->cur, "Trap"))
        *out = KN_PANIC_TRAP;
    else if (knproj_text_eq_ci(p->cur, "Loop"))
        *out = KN_PANIC_LOOP;
    else
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "unknown panic mode");
        p->failed = 1;
        return false;
    }
    knproj_next(p);
    return true;
}

static bool knproj_parse_linker(KnProjParser *p, int *out)
{
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "expected linker name");
        p->failed = 1;
        return false;
    }
    if (knproj_text_eq_ci(p->cur, "LLD"))
        *out = KN_LINKER_LLD;
    else if (knproj_text_eq_ci(p->cur, "Zig"))
        *out = KN_LINKER_ZIG;
    else if (knproj_text_eq_ci(p->cur, "MSVC"))
        *out = KN_LINKER_MSVC;
    else
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "unknown linker");
        p->failed = 1;
        return false;
    }
    knproj_next(p);
    return true;
}

static bool knproj_parse_packages_block(KnProjParser *p, char roots[32][KN_MAX_PATH], int *root_count,
                                        char official_roots[32][KN_MAX_PATH], int *official_root_count)
{
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected package field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Roots"))
        {
            if (!knproj_parse_string_list(p, (char *)roots, KN_MAX_PATH, KN_MAX_PATH, root_count,
                                          32, "too many package roots"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "OfficialRoots"))
        {
            if (!knproj_parse_string_list(p, (char *)official_roots, KN_MAX_PATH, KN_MAX_PATH, official_root_count,
                                          32, "too many official package roots"))
                return false;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown Packages field");
            p->failed = 1;
            return false;
        }
        if (!knproj_parse_semi(p))
            return false;
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool knproj_parse_source_block(KnProjParser *p, KnProjProfile *profile)
{
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected source field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Entry"))
        {
            if (!knproj_parse_name(p, profile->entry, sizeof(profile->entry), "expected entry path"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "AutoDiscovery"))
        {
            if (!knproj_parse_bool(p, &profile->auto_link, "expected true or false"))
                return false;
            profile->has_auto_link = 1;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown Source field");
            p->failed = 1;
            return false;
        }
        if (!knproj_parse_semi(p))
            return false;
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool knproj_parse_build_block(KnProjParser *p, KnProjProfile *profile)
{
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected build field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Backend"))
        {
            if (!knproj_parse_backend(p, &profile->backend))
                return false;
            profile->has_backend = 1;
        }
        else if (knproj_text_eq_ci(key, "Environment"))
        {
            if (!knproj_parse_env(p, &profile->env_kind))
                return false;
            profile->has_env = 1;
        }
        else if (knproj_text_eq_ci(key, "Runtime"))
        {
            if (!knproj_parse_runtime(p, &profile->runtime_mode))
                return false;
            profile->has_runtime = 1;
        }
        else if (knproj_text_eq_ci(key, "Panic"))
        {
            if (!knproj_parse_panic(p, &profile->panic_mode))
                return false;
            profile->has_panic = 1;
        }
        else if (knproj_text_eq_ci(key, "Target"))
        {
            if (!knproj_parse_name(p, profile->target, sizeof(profile->target), "expected target"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Output"))
        {
            if (!knproj_parse_name(p, profile->out_path, sizeof(profile->out_path), "expected output path"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "EntrySymbol"))
        {
            if (!knproj_parse_name(p, profile->entry_symbol, sizeof(profile->entry_symbol), "expected entry symbol"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Linker"))
        {
            if (!knproj_parse_linker(p, &profile->linker_kind))
                return false;
            profile->has_linker_kind = 1;
        }
        else if (knproj_text_eq_ci(key, "LinkerPath"))
        {
            if (!knproj_parse_name(p, profile->linker_path, sizeof(profile->linker_path), "expected linker path"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Superloop"))
        {
            if (!knproj_parse_bool(p, &profile->knc_superloop, "expected true or false"))
                return false;
            profile->has_superloop = 1;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown Build field");
            p->failed = 1;
            return false;
        }
        if (!knproj_parse_semi(p))
            return false;
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool knproj_parse_link_block(KnProjParser *p, KnProjProfile *profile)
{
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected link field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Script"))
        {
            if (!knproj_parse_name(p, profile->link_script, sizeof(profile->link_script), "expected linker script path"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "NoCRT"))
        {
            if (!knproj_parse_bool(p, &profile->no_crt, "expected true or false"))
                return false;
            profile->has_no_crt = 1;
        }
        else if (knproj_text_eq_ci(key, "NoDefaultLibs"))
        {
            if (!knproj_parse_bool(p, &profile->no_default_libs, "expected true or false"))
                return false;
            profile->has_no_default_libs = 1;
        }
        else if (knproj_text_eq_ci(key, "LibDirs"))
        {
            if (!knproj_parse_string_list(p, (char *)profile->lib_dirs, KN_MAX_PATH, KN_MAX_PATH, &profile->lib_dir_count,
                                          32, "too many library directories"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Libs"))
        {
            if (!knproj_parse_string_list(p, (char *)profile->libs, 128, 128, &profile->lib_count,
                                          32, "too many libraries"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "LinkFiles"))
        {
            if (!knproj_parse_string_list(p, (char *)profile->link_files, KN_MAX_PATH, KN_MAX_PATH, &profile->link_file_count,
                                          32, "too many link files"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "LinkArgs"))
        {
            if (!knproj_parse_string_list(p, (char *)profile->link_args, 256, 256, &profile->link_arg_count,
                                          32, "too many link arguments"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "LinkRoots"))
        {
            if (!knproj_parse_string_list(p, (char *)profile->link_roots, KN_MAX_PATH, KN_MAX_PATH, &profile->link_root_count,
                                          32, "too many link roots"))
                return false;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown Link field");
            p->failed = 1;
            return false;
        }
        if (!knproj_parse_semi(p))
            return false;
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool knproj_parse_lsp_block(KnProjParser *p, KnProjectConfig *cfg)
{
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected Lsp field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Profile"))
        {
            if (!knproj_parse_name(p, cfg->lsp_profile, sizeof(cfg->lsp_profile), "expected profile name"))
                return false;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown Lsp field");
            p->failed = 1;
            return false;
        }
        if (!knproj_parse_semi(p))
            return false;
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool knproj_parse_profile_block(KnProjParser *p, KnProjectConfig *cfg)
{
    KnProjProfile *profile = 0;
    if (!cfg || cfg->profile_count >= (int)(sizeof(cfg->profiles) / sizeof(cfg->profiles[0])))
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "too many profiles");
        p->failed = 1;
        return false;
    }
    profile = &cfg->profiles[cfg->profile_count++];
    kn_memset(profile, 0, sizeof(*profile));
    if (!knproj_parse_name(p, profile->name, sizeof(profile->name), "expected profile name"))
        return false;
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token sec = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected profile section"))
            return false;
        if (knproj_text_eq_ci(sec, "Source"))
        {
            if (!knproj_parse_source_block(p, profile))
                return false;
        }
        else if (knproj_text_eq_ci(sec, "Build"))
        {
            if (!knproj_parse_build_block(p, profile))
                return false;
        }
        else if (knproj_text_eq_ci(sec, "Link"))
        {
            if (!knproj_parse_link_block(p, profile))
                return false;
        }
        else if (knproj_text_eq_ci(sec, "Packages"))
        {
            if (!knproj_parse_packages_block(p, profile->package_roots, &profile->package_root_count,
                                             profile->official_package_roots, &profile->official_package_root_count))
                return false;
        }
        else
        {
            knproj_set_error(p->path, sec.line, sec.col, "unknown profile section");
            p->failed = 1;
            return false;
        }
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool parse_knproj_config(const char *path, KnProjectConfig *out)
{
    size_t size = 0;
    char *text = 0;
    KnProjParser p;
    if (!path || !out)
        return false;
    g_knproj_error[0] = 0;
    kn_memset(out, 0, sizeof(*out));
    copy_str(out->project_path, sizeof(out->project_path), path);
    path_dirname(path, out->project_dir, sizeof(out->project_dir));

    text = kn_read_file(path, &size);
    if (!text)
    {
        knproj_set_error(path, 0, 0, "failed to read project file");
        return false;
    }

    kn_memset(&p, 0, sizeof(p));
    p.path = path;
    lex_init(&p.lex, text);
    knproj_next(&p);

    if (p.cur.type != TOK_ID || !knproj_text_eq_ci(p.cur, "Project"))
    {
        knproj_set_error(path, p.cur.line, p.cur.col, "expected 'Project'");
        kn_free(text);
        return false;
    }
    knproj_next(&p);
    if (!knproj_parse_name(&p, out->name, sizeof(out->name), "expected project name"))
    {
        kn_free(text);
        return false;
    }
    if (!knproj_expect(&p, TOK_LBRACE, "expected '{'"))
    {
        kn_free(text);
        return false;
    }

    while (!p.failed && p.cur.type != TOK_RBRACE && p.cur.type != TOK_EOF)
    {
        Token key = p.cur;
        if (!knproj_expect(&p, TOK_ID, "expected project field"))
            break;
        if (knproj_text_eq_ci(key, "DefaultProfile"))
        {
            if (!knproj_parse_assign(&p) ||
                !knproj_parse_name(&p, out->default_profile, sizeof(out->default_profile), "expected profile name") ||
                !knproj_parse_semi(&p))
            {
                kn_free(text);
                return false;
            }
            continue;
        }
        if (knproj_text_eq_ci(key, "Profile"))
        {
            if (!knproj_parse_profile_block(&p, out))
            {
                kn_free(text);
                return false;
            }
            continue;
        }
        if (knproj_text_eq_ci(key, "Packages"))
        {
            if (!knproj_parse_packages_block(&p, out->package_roots, &out->package_root_count,
                                             out->official_package_roots, &out->official_package_root_count))
            {
                kn_free(text);
                return false;
            }
            continue;
        }
        if (knproj_text_eq_ci(key, "Lsp"))
        {
            if (!knproj_parse_lsp_block(&p, out))
            {
                kn_free(text);
                return false;
            }
            continue;
        }
        knproj_set_error(path, key.line, key.col, "unknown project field");
        kn_free(text);
        return false;
    }

    if (!knproj_expect(&p, TOK_RBRACE, "expected '}'"))
    {
        kn_free(text);
        return false;
    }
    if (p.cur.type != TOK_EOF)
    {
        knproj_set_error(path, p.cur.line, p.cur.col, "unexpected token after project block");
        kn_free(text);
        return false;
    }
    if (out->profile_count == 0)
    {
        knproj_set_error(path, 0, 0, "project file must declare at least one profile");
        kn_free(text);
        return false;
    }
    if (!out->default_profile[0])
        copy_str(out->default_profile, sizeof(out->default_profile), out->profiles[0].name);

    kn_free(text);
    return true;
}

static bool project_manifest_is_knproj(const char *path)
{
    const char *base = path_basename_local(path);
    size_t len = 0;
    if (!base || !*base)
        base = path;
    if (!base)
        return false;
    len = kn_strlen(base);
    if (str_eq_ci(base, "kinal.knproj"))
        return true;
    return len > 7 && str_eq_ci(base + (len - 7), ".knproj");
}

static const KnProjProfile *project_profile_by_name(const KnProjectConfig *cfg, const char *name)
{
    if (!cfg || !name || !name[0])
        return 0;
    for (int i = 0; i < cfg->profile_count; i++)
    {
        if (str_eq_ci(cfg->profiles[i].name, name))
            return &cfg->profiles[i];
    }
    return 0;
}

static const KnProjProfile *load_selected_project_profile(const char *path, const char *profile_name,
                                                          KnProjectConfig *cfg, char *selected_name, size_t selected_cap)
{
    const KnProjProfile *profile = 0;
    const char *name = profile_name;
    if (!parse_knproj_config(path, cfg))
        return 0;
    if (!name || !name[0])
        name = cfg->default_profile;
    profile = project_profile_by_name(cfg, name);
    if (!profile)
    {
        knproj_set_error(path, 0, 0, "requested profile was not found");
        return 0;
    }
    if (selected_name && selected_cap > 0)
        copy_str(selected_name, selected_cap, profile->name);
    return profile;
}

static bool knproj_resolve_path(const KnProjectConfig *cfg, const char *rel, char *out, size_t cap)
{
    if (!cfg || !rel || !rel[0] || !out || cap == 0)
        return false;
    if (path_is_absolute_pkg(rel))
    {
        copy_str(out, cap, rel);
        return true;
    }
    return path_join(out, cap, cfg->project_dir, rel);
}

static void append_project_package_roots(StrList *roots, const KnProjectConfig *cfg, const KnProjProfile *profile, int official)
{
    int count = 0;
    char (*items)[KN_MAX_PATH] = 0;
    if (!roots || !cfg)
        return;

    count = official ? cfg->official_package_root_count : cfg->package_root_count;
    items = official ? (char (*)[KN_MAX_PATH])cfg->official_package_roots : (char (*)[KN_MAX_PATH])cfg->package_roots;
    for (int i = 0; i < count; i++)
    {
        char resolved[KN_MAX_PATH];
        if (knproj_resolve_path(cfg, items[i], resolved, sizeof(resolved)))
            append_root_list(roots, resolved);
    }
    if (profile)
    {
        count = official ? profile->official_package_root_count : profile->package_root_count;
        items = official ? (char (*)[KN_MAX_PATH])profile->official_package_roots : (char (*)[KN_MAX_PATH])profile->package_roots;
        for (int i = 0; i < count; i++)
        {
            char resolved[KN_MAX_PATH];
            if (knproj_resolve_path(cfg, items[i], resolved, sizeof(resolved)))
                append_root_list(roots, resolved);
        }
    }
}

static int append_project_link_items(const KnProjectConfig *cfg, const KnProjProfile *profile,
                                     KnLinkItem *items, int *item_count, int item_cap)
{
    int i = 0;
    char resolved[KN_MAX_PATH];
    if (!cfg || !profile || !items || !item_count)
        return 1;
    for (i = 0; i < profile->lib_dir_count; i++)
    {
        if (!knproj_resolve_path(cfg, profile->lib_dirs[i], resolved, sizeof(resolved)))
            return 0;
        if (!link_item_add(items, item_count, item_cap, KN_LINK_ITEM_LIB_DIR, resolved))
            return 0;
    }
    for (i = 0; i < profile->lib_count; i++)
    {
        if (!link_item_add(items, item_count, item_cap, KN_LINK_ITEM_LIB_NAME, profile->libs[i]))
            return 0;
    }
    for (i = 0; i < profile->link_file_count; i++)
    {
        if (!knproj_resolve_path(cfg, profile->link_files[i], resolved, sizeof(resolved)))
            return 0;
        if (!link_item_add(items, item_count, item_cap, KN_LINK_ITEM_FILE, resolved))
            return 0;
    }
    for (i = 0; i < profile->link_arg_count; i++)
    {
        if (!link_item_add(items, item_count, item_cap, KN_LINK_ITEM_ARG, profile->link_args[i]))
            return 0;
    }
    for (i = 0; i < profile->link_root_count; i++)
    {
        if (!knproj_resolve_path(cfg, profile->link_roots[i], resolved, sizeof(resolved)))
            return 0;
        if (!link_item_add(items, item_count, item_cap, KN_CLI_LINK_ROOT, resolved))
            return 0;
    }
    return 1;
}
