typedef enum
{
    KN_PROJ_BACKEND_UNSET = 0,
    KN_PROJ_BACKEND_NATIVE = 1,
    KN_PROJ_BACKEND_VM = 2
} KnProjBackend;

typedef enum
{
    KN_PROJ_SOURCE_MODE_UNSET = 0,
    KN_PROJ_SOURCE_MODE_FILE_ONLY = 1,
    KN_PROJ_SOURCE_MODE_ENTRY_UNIT = 2,
    KN_PROJ_SOURCE_MODE_REACHABLE_UNITS = 3,
    KN_PROJ_SOURCE_MODE_ALL_SOURCES = 4
} KnProjSourceMode;

typedef struct
{
    char name[64];
    char roots[32][KN_MAX_PATH];
    int root_count;
    char files[64][KN_MAX_PATH];
    int file_count;
    char includes[32][KN_MAX_PATH];
    int include_count;
    char excludes[32][KN_MAX_PATH];
    int exclude_count;
    int has_require_unit;
    int require_unit;
} KnProjSourceSet;

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
    int has_source_mode;
    int source_mode;
    int has_superloop;
    int knc_superloop;

    char source_sets[32][64];
    int source_set_count;

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
    char lsp_extra_sets[32][64];
    int lsp_extra_set_count;
    int has_lsp_strict_project_scope;
    int lsp_strict_project_scope;

    char workspace_ignores[64][KN_MAX_PATH];
    int workspace_ignore_count;

    char package_roots[32][KN_MAX_PATH];
    int package_root_count;
    char official_package_roots[32][KN_MAX_PATH];
    int official_package_root_count;

    KnProjSourceSet source_sets[32];
    int source_set_count;

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

static KN_DRIVER_THREAD_LOCAL char g_knproj_error[256];
static KN_DRIVER_THREAD_LOCAL char g_knproj_runtime_error[512];

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

static bool knproj_parse_source_mode(KnProjParser *p, int *out)
{
    if (p->cur.type != TOK_ID && p->cur.type != TOK_STRING)
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "expected source mode");
        p->failed = 1;
        return false;
    }
    if (knproj_text_eq_ci(p->cur, "FileOnly"))
        *out = KN_PROJ_SOURCE_MODE_FILE_ONLY;
    else if (knproj_text_eq_ci(p->cur, "EntryUnit"))
        *out = KN_PROJ_SOURCE_MODE_ENTRY_UNIT;
    else if (knproj_text_eq_ci(p->cur, "ReachableUnits"))
        *out = KN_PROJ_SOURCE_MODE_REACHABLE_UNITS;
    else if (knproj_text_eq_ci(p->cur, "AllSources"))
        *out = KN_PROJ_SOURCE_MODE_ALL_SOURCES;
    else
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "unknown source mode");
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
        else if (knproj_text_eq_ci(key, "Sets"))
        {
            if (!knproj_parse_string_list(p, (char *)profile->source_sets, 64, 64, &profile->source_set_count,
                                          32, "too many source set names"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Mode"))
        {
            if (!knproj_parse_source_mode(p, &profile->source_mode))
                return false;
            profile->has_source_mode = 1;
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

static bool knproj_parse_workspace_block(KnProjParser *p, KnProjectConfig *cfg)
{
    if (!cfg)
        return false;
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected Workspace field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Ignore"))
        {
            if (!knproj_parse_string_list(p, (char *)cfg->workspace_ignores, KN_MAX_PATH, KN_MAX_PATH,
                                          &cfg->workspace_ignore_count, 64, "too many Workspace.Ignore patterns"))
                return false;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown Workspace field");
            p->failed = 1;
            return false;
        }
        if (!knproj_parse_semi(p))
            return false;
    }
    return knproj_expect(p, TOK_RBRACE, "expected '}'");
}

static bool knproj_parse_sourceset_block(KnProjParser *p, KnProjectConfig *cfg)
{
    KnProjSourceSet *set = 0;
    if (!cfg || cfg->source_set_count >= (int)(sizeof(cfg->source_sets) / sizeof(cfg->source_sets[0])))
    {
        knproj_set_error(p->path, p->cur.line, p->cur.col, "too many source sets");
        p->failed = 1;
        return false;
    }
    set = &cfg->source_sets[cfg->source_set_count++];
    kn_memset(set, 0, sizeof(*set));
    if (!knproj_parse_name(p, set->name, sizeof(set->name), "expected source set name"))
        return false;
    if (!knproj_expect(p, TOK_LBRACE, "expected '{'"))
        return false;
    while (!p->failed && p->cur.type != TOK_RBRACE && p->cur.type != TOK_EOF)
    {
        Token key = p->cur;
        if (!knproj_expect(p, TOK_ID, "expected SourceSet field name"))
            return false;
        if (!knproj_parse_assign(p))
            return false;
        if (knproj_text_eq_ci(key, "Roots"))
        {
            if (!knproj_parse_string_list(p, (char *)set->roots, KN_MAX_PATH, KN_MAX_PATH, &set->root_count,
                                          32, "too many source roots"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Files"))
        {
            if (!knproj_parse_string_list(p, (char *)set->files, KN_MAX_PATH, KN_MAX_PATH, &set->file_count,
                                          64, "too many source files"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Include"))
        {
            if (!knproj_parse_string_list(p, (char *)set->includes, KN_MAX_PATH, KN_MAX_PATH, &set->include_count,
                                          32, "too many include patterns"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "Exclude"))
        {
            if (!knproj_parse_string_list(p, (char *)set->excludes, KN_MAX_PATH, KN_MAX_PATH, &set->exclude_count,
                                          32, "too many exclude patterns"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "RequireUnit"))
        {
            if (!knproj_parse_bool(p, &set->require_unit, "expected true or false"))
                return false;
            set->has_require_unit = 1;
        }
        else
        {
            knproj_set_error(p->path, key.line, key.col, "unknown SourceSet field");
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
        else if (knproj_text_eq_ci(key, "ExtraSets"))
        {
            if (!knproj_parse_string_list(p, (char *)cfg->lsp_extra_sets, 64, 64, &cfg->lsp_extra_set_count,
                                          32, "too many Lsp.ExtraSets names"))
                return false;
        }
        else if (knproj_text_eq_ci(key, "StrictProjectScope"))
        {
            if (!knproj_parse_bool(p, &cfg->lsp_strict_project_scope, "expected true or false"))
                return false;
            cfg->has_lsp_strict_project_scope = 1;
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
        if (knproj_text_eq_ci(key, "SourceSet"))
        {
            if (!knproj_parse_sourceset_block(&p, out))
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
        if (knproj_text_eq_ci(key, "Workspace"))
        {
            if (!knproj_parse_workspace_block(&p, out))
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

static const KnProjSourceSet *project_source_set_by_name(const KnProjectConfig *cfg, const char *name)
{
    if (!cfg || !name || !name[0])
        return 0;
    for (int i = 0; i < cfg->source_set_count; i++)
    {
        if (str_eq_ci(cfg->source_sets[i].name, name))
            return &cfg->source_sets[i];
    }
    return 0;
}

static int project_profile_source_mode(const KnProjectConfig *cfg, const KnProjProfile *profile)
{
    if (profile && profile->has_source_mode)
        return profile->source_mode;
    if (profile && profile->has_auto_link)
        return profile->auto_link ? KN_PROJ_SOURCE_MODE_REACHABLE_UNITS : KN_PROJ_SOURCE_MODE_FILE_ONLY;
    if (cfg && cfg->source_set_count > 0)
        return KN_PROJ_SOURCE_MODE_REACHABLE_UNITS;
    return KN_PROJ_SOURCE_MODE_UNSET;
}

static void knproj_runtime_set_error(const char *path, const char *msg)
{
    size_t off = 0;
    g_knproj_runtime_error[0] = 0;
    if (path && path[0])
    {
        kn_append(g_knproj_runtime_error, sizeof(g_knproj_runtime_error), &off, path);
        kn_append(g_knproj_runtime_error, sizeof(g_knproj_runtime_error), &off, ": ");
    }
    kn_append(g_knproj_runtime_error, sizeof(g_knproj_runtime_error), &off, msg ? msg : "invalid project source set");
}

static void knproj_normalize_slashes(char *s)
{
    if (!s) return;
    for (size_t i = 0; s[i]; i++)
    {
        if (s[i] == '\\')
            s[i] = '/';
    }
}

static bool knproj_path_relative_text(const char *base, const char *full, char *out, size_t cap)
{
    char full_base[KN_MAX_PATH];
    char full_path[KN_MAX_PATH];
    const char *b = base;
    const char *f = full;
    size_t base_len = 0;
    if (!base || !full || !out || cap == 0)
        return false;
    out[0] = 0;
    if (path_full(full_base, sizeof(full_base), base))
        b = full_base;
    if (path_full(full_path, sizeof(full_path), full))
        f = full_path;
    base_len = kn_strlen(b);
    if (kn_strncmp(b, f, base_len) != 0)
        return false;
    f += base_len;
    while (*f && is_path_sep(*f))
        f++;
    copy_str(out, cap, f);
    knproj_normalize_slashes(out);
    return out[0] != 0;
}

static bool knproj_glob_match_ci(const char *pattern, const char *text)
{
    char pc = 0;
    char sc = 0;
    if (!pattern || !text)
        return false;
    if (!pattern[0])
        return !text[0];

    pc = pattern[0];
    if (pc == '\\')
        pc = '/';
    sc = text[0];
    if (sc == '\\')
        sc = '/';

    if (pc == '*')
    {
        if (pattern[1] == '*')
        {
            const char *next = pattern + 2;
            while (*next == '/' || *next == '\\')
                next++;
            if (knproj_glob_match_ci(next, text))
                return true;
            while (*text)
            {
                if (knproj_glob_match_ci(pattern, text + 1))
                    return true;
                text++;
            }
            return false;
        }
        if (knproj_glob_match_ci(pattern + 1, text))
            return true;
        while (*text)
        {
            char tc = *text;
            if (tc == '\\')
                tc = '/';
            if (tc == '/')
                break;
            if (knproj_glob_match_ci(pattern + 1, text + 1))
                return true;
            text++;
        }
        return false;
    }

    if (pc == '?')
    {
        if (!sc || sc == '/')
            return false;
        return knproj_glob_match_ci(pattern + 1, text + 1);
    }

    if (lower_char(pc) != lower_char(sc))
        return false;
    return knproj_glob_match_ci(pattern + 1, text + 1);
}

static bool knproj_patterns_match_any(char patterns[][KN_MAX_PATH], int pattern_count,
                                      const char *a, const char *b)
{
    for (int i = 0; i < pattern_count; i++)
    {
        if ((a && knproj_glob_match_ci(patterns[i], a)) ||
            (b && knproj_glob_match_ci(patterns[i], b)))
            return true;
    }
    return false;
}

static bool knproj_source_file_allowed(const KnProjectConfig *cfg, const KnProjSourceSet *set,
                                       const char *root_rel, const char *project_rel)
{
    int include_ok = 0;
    if (cfg && cfg->workspace_ignore_count > 0 &&
        knproj_patterns_match_any((char (*)[KN_MAX_PATH])cfg->workspace_ignores, cfg->workspace_ignore_count, root_rel, project_rel))
        return false;
    if (set && set->exclude_count > 0 &&
        knproj_patterns_match_any((char (*)[KN_MAX_PATH])set->excludes, set->exclude_count, root_rel, project_rel))
        return false;
    if (!set || set->include_count == 0)
        return true;
    include_ok = knproj_patterns_match_any((char (*)[KN_MAX_PATH])set->includes, set->include_count, root_rel, project_rel) ? 1 : 0;
    return include_ok ? true : false;
}

static bool knproj_add_source_file(StrList *out, const char *path)
{
    char canonical[KN_MAX_PATH];
    const char *store = path;
    if (!out || !path || !path[0])
        return false;
    if (path_full(canonical, sizeof(canonical), path))
        store = canonical;
    if (strlist_contains_path(out, store))
        return true;
    strlist_add(out, dup_str(store));
    return true;
}

static bool knproj_validate_source_file_unit(const KnProjSourceSet *set, const char *path)
{
    int require_unit = 1;
    char *unit = 0;
    if (!path || !path[0])
        return false;
    if (set && set->has_require_unit)
        require_unit = set->require_unit;
    if (!require_unit)
        return true;
    unit = read_unit_name_from_file(path);
    if (!unit)
    {
        knproj_runtime_set_error(path, "project source files must declare Unit and cannot use Unsafe Alias");
        return false;
    }
    kn_free(unit);
    return true;
}

static bool knproj_collect_root_sources_recursive(const KnProjectConfig *cfg, const KnProjSourceSet *set,
                                                  const char *root_abs, const char *dir_abs, StrList *out)
{
    char pattern[KN_MAX_PATH];
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h;
    if (!cfg || !set || !root_abs || !dir_abs || !out)
        return false;
    if (!path_join(pattern, sizeof(pattern), dir_abs, "*"))
        return false;
    h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE)
        return false;
    do
    {
        char full[KN_MAX_PATH];
        if (!path_join(full, sizeof(full), dir_abs, data.cFileName))
            continue;
        if (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY)
        {
            if (kn_strcmp(data.cFileName, ".") == 0 || kn_strcmp(data.cFileName, "..") == 0)
                continue;
            if (kn_strcmp(data.cFileName, ".git") == 0 || kn_strcmp(data.cFileName, ".github") == 0 || kn_strcmp(data.cFileName, ".kinal-cache") == 0)
                continue;
            (void)knproj_collect_root_sources_recursive(cfg, set, root_abs, full, out);
            continue;
        }
        if (!has_source_extension(data.cFileName))
            continue;
        char root_rel[KN_MAX_PATH];
        char project_rel[KN_MAX_PATH];
        root_rel[0] = 0;
        project_rel[0] = 0;
        (void)knproj_path_relative_text(root_abs, full, root_rel, sizeof(root_rel));
        (void)knproj_path_relative_text(cfg->project_dir, full, project_rel, sizeof(project_rel));
        if (!knproj_source_file_allowed(cfg, set, root_rel[0] ? root_rel : 0, project_rel[0] ? project_rel : 0))
            continue;
        if (!knproj_validate_source_file_unit(set, full))
        {
            FindClose(h);
            return false;
        }
        if (!knproj_add_source_file(out, full))
        {
            FindClose(h);
            return false;
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);
    return true;
}

static bool collect_project_local_sources(const KnProjectConfig *cfg, const KnProjProfile *profile, StrList *out)
{
    int set_count = 0;
    g_knproj_runtime_error[0] = 0;
    if (!cfg || !profile || !out)
        return false;

    if (profile->source_set_count > 0)
        set_count = profile->source_set_count;
    else if (cfg->source_set_count > 0)
        set_count = cfg->source_set_count;
    else
        return true;

    for (int si = 0; si < set_count; si++)
    {
        const KnProjSourceSet *set = 0;
        char fallback_name[64];
        const char *set_name = 0;
        if (profile->source_set_count > 0)
            set_name = profile->source_sets[si];
        else
        {
            copy_str(fallback_name, sizeof(fallback_name), cfg->source_sets[si].name);
            set_name = fallback_name;
        }
        set = project_source_set_by_name(cfg, set_name);
        if (!set)
        {
            knproj_runtime_set_error(set_name, "unknown SourceSet name referenced by profile");
            return false;
        }
        for (int ri = 0; ri < set->root_count; ri++)
        {
            char resolved_root[KN_MAX_PATH];
            if (!knproj_resolve_path(cfg, set->roots[ri], resolved_root, sizeof(resolved_root)))
            {
                knproj_runtime_set_error(set->roots[ri], "failed to resolve source root");
                return false;
            }
            if (!dir_exists(resolved_root))
                continue;
            if (!knproj_collect_root_sources_recursive(cfg, set, resolved_root, resolved_root, out))
                return false;
        }
        for (int fi = 0; fi < set->file_count; fi++)
        {
            char resolved_file[KN_MAX_PATH];
            char project_rel[KN_MAX_PATH];
            if (!knproj_resolve_path(cfg, set->files[fi], resolved_file, sizeof(resolved_file)))
            {
                knproj_runtime_set_error(set->files[fi], "failed to resolve source file");
                return false;
            }
            if (!file_exists(resolved_file))
            {
                knproj_runtime_set_error(resolved_file, "project source file was not found");
                return false;
            }
            if (!has_source_extension(resolved_file))
                continue;
            project_rel[0] = 0;
            (void)knproj_path_relative_text(cfg->project_dir, resolved_file, project_rel, sizeof(project_rel));
            if (!knproj_source_file_allowed(cfg, set, 0, project_rel[0] ? project_rel : 0))
                continue;
            if (!knproj_validate_source_file_unit(set, resolved_file))
                return false;
            if (!knproj_add_source_file(out, resolved_file))
                return false;
        }
    }
    return true;
}

static bool prepare_project_source_graph(const KnProjectConfig *cfg, const KnProjProfile *profile,
                                         const char *entry_path, StrList *out_sources, int *out_mode)
{
    int explicit_graph = 0;
    int mode = KN_PROJ_SOURCE_MODE_UNSET;
    g_knproj_runtime_error[0] = 0;
    if (!cfg || !profile)
        return false;
    explicit_graph = (cfg->source_set_count > 0) || (profile->source_set_count > 0) || profile->has_source_mode;
    mode = project_profile_source_mode(cfg, profile);
    if (!explicit_graph)
    {
        if (out_mode) *out_mode = mode;
        return true;
    }
    if (!out_sources)
    {
        knproj_runtime_set_error(cfg->project_path, "internal error: missing project source output list");
        return false;
    }
    if (!collect_project_local_sources(cfg, profile, out_sources))
        return false;
    if (out_sources->count == 0)
    {
        knproj_runtime_set_error(cfg->project_path, "active SourceSet selection produced no local source files");
        return false;
    }
    if (entry_path && entry_path[0] && !strlist_contains_path(out_sources, entry_path))
    {
        knproj_runtime_set_error(entry_path, "profile entry is not included by the active SourceSet selection");
        return false;
    }
    if (out_mode) *out_mode = (mode == KN_PROJ_SOURCE_MODE_UNSET) ? KN_PROJ_SOURCE_MODE_REACHABLE_UNITS : mode;
    return true;
}
