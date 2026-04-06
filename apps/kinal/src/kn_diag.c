#include "kn/diag.h"
#include "kn/lexer.h"
#include "kn/util.h"

typedef enum
{
    DIAG_SEV_ERROR = 0,
    DIAG_SEV_WARNING = 1
} DiagSeverity;

typedef struct
{
    char code[16]; // E-SYN-01234 / W-SAF-00001
    const char *title;
    const char *detail;
} DiagTemplate;

typedef struct
{
    const char *key;
    const char *en;
} UiTemplate;

typedef struct
{
    KnDiagStage stage;
    DiagSeverity sev;
    int line;
    int col;
    char code[16];
    char title[64];
} EmittedDiag;

static int g_error_count = 0;
static int g_warning_count = 0;
static int g_warn_as_error = 0;
static int g_warn_level = 1;
static int g_color_mode = KN_COLOR_AUTO;
static const char *g_lang = "en";
static const char *g_locale_path = 0;
static char *g_locale_text = 0;
static int g_max_errors = 128;
static int g_quiet = 0;
static KnDiagSink g_sink = 0;
static void *g_sink_user_data = 0;

static DiagTemplate g_templates[512];
static int g_template_count = 0;

static char g_locale_buf[8][2048];
static int g_locale_buf_index = 0;

static int g_color_probe_done = 0;
static int g_color_auto_enabled = 0;
static EmittedDiag g_emitted[2048];
static int g_emitted_count = 0;

// Registered in kn_diag_registry.c
void kn_diag_register_builtin_templates(void);

static const char g_help_full_en[] =
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
    "                     aliases: win86, win64, win-arm64, linux64, linux-arm64, bare64, bare-arm64\n"
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
    "  --no-module-discovery  Disable source/module auto-discovery for single-input builds\n"
    "  --keep-temps     Keep intermediate outputs generated for linking\n"
    "  --lto[=off|full|thin]  Enable link-time optimization (default: off)\n"
    "\n"
    "Diagnostics:\n"
    "  --color <auto|always|never>  Diagnostic color mode\n"
    "  --lang <en|zh>   Diagnostic language\n"
    "  --locale-file <file>  External locale JSON map by code\n"
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

static const UiTemplate g_ui_templates[] = {
    { "ui.severity.error", "error" },
    { "ui.severity.warning", "warning" },
    { "ui.diag.found_eof", " (found end of file)" },
    { "ui.diag.found_token", " (found token '{0}')" },
    { "ui.diag.location", "in <{0}> at line <{1}>, column <{2}>" },
    { "ui.diag.summary", "[Diag] {0} error(s), {1} warning(s)" },
    { "ui.diag.too_many_parser_errors", "[Diag] too many parser errors, stopping early" },
    { "ui.diag.too_many_errors", "[Diag] too many errors, stopping early" },
    { "ui.diag.arg_count", "Argument count mismatch: {0} expects {1} argument(s)" },
    { "ui.diag.type_mismatch", "Type mismatch: expected {0}, got {1}" },
    { "ui.diag.type_mismatch.must_be", "Type mismatch: {0} must be {1}" },
    { "ui.diag.type_mismatch.requires", "Type mismatch: {0} requires {1}" },
    { "ui.diag.type_mismatch.cast", "Type mismatch: cannot cast {0} to {1}" },
    { "ui.diag.unused_var", "Unused variable: {0}" },
    { "ui.diag.unreachable", "Unreachable code: {0}" },
    { "ui.stage.parser", "Parser" },
    { "ui.stage.sema", "Sema" },
    { "ui.stage.codegen", "Codegen" },
    { "ui.help.full", g_help_full_en }
};

static const char *canonical_detail_for_code(const char *title, const char *detail)
{
    const char *safe_title = title ? title : "";
    const char *safe_detail = detail ? detail : "";
    if (!safe_title[0])
        return safe_detail;
    // Keep one stable code for argument-count diagnostics, while detail is rendered dynamically.
    if (kn_strcmp(safe_title, "Argument Count") == 0)
        return "Wrong number of arguments";
    // Keep one stable code for all type-mismatch diagnostics.
    if (kn_strcmp(safe_title, "Type Mismatch") == 0)
        return "Type mismatch";
    if (kn_strcmp(safe_title, "Unknown Parameter") == 0)
        return "Unknown named argument";
    if (kn_strcmp(safe_title, "Duplicate Named Argument") == 0)
        return "Duplicate named argument";
    if (kn_strcmp(safe_title, "Invalid Named Argument") == 0)
    {
        if (kn_strncmp(safe_detail, "Named arguments are not supported for constructors", 48) == 0)
            return "Named arguments are not supported for constructors";
        if (kn_strncmp(safe_detail, "Positional argument cannot follow named argument", 46) == 0)
            return "Positional argument cannot follow named argument";
        return "Named arguments are not supported for this callable";
    }
    if (kn_strcmp(safe_title, "Unused Variable") == 0)
        return "Variable is never used";
    if (kn_strcmp(safe_title, "Unreachable Code") == 0)
        return "Unreachable code";
    // Keep concrete details exportable/translatable.
    return safe_detail;
}

static const char *stage_name(KnDiagStage stage)
{
    switch (stage)
    {
    case KN_STAGE_PARSER: return "Parser";
    case KN_STAGE_SEMA: return "Sema";
    case KN_STAGE_CODEGEN: return "Codegen";
    default: return "Unknown";
    }
}

static const char *stage_locale_key(KnDiagStage stage)
{
    switch (stage)
    {
    case KN_STAGE_PARSER: return "ui.stage.parser";
    case KN_STAGE_SEMA: return "ui.stage.sema";
    case KN_STAGE_CODEGEN: return "ui.stage.codegen";
    default: return "ui.stage.unknown";
    }
}

static int is_zh_lang(void)
{
    return g_lang &&
           (kn_strcmp(g_lang, "zh") == 0 || kn_strcmp(g_lang, "zh-CN") == 0);
}

static int starts_with(const char *s, const char *prefix)
{
    if (!s || !prefix) return 0;
    size_t np = kn_strlen(prefix);
    return kn_strncmp(s, prefix, np) == 0;
}

static int contains_substr(const char *s, const char *needle)
{
    if (!s || !needle || !needle[0]) return 0;
    size_t ns = kn_strlen(s);
    size_t nn = kn_strlen(needle);
    if (nn > ns) return 0;
    for (size_t i = 0; i + nn <= ns; i++)
    {
        if (kn_strncmp(s + i, needle, nn) == 0)
            return 1;
    }
    return 0;
}

static int parse_code_number(const char *code)
{
    if (!code || !code[0]) return -1;
    const char *last_dash = 0;
    for (const char *p = code; *p; p++)
        if (*p == '-') last_dash = p;
    if (!last_dash || !last_dash[1]) return -1;

    int v = 0;
    for (const char *p = last_dash + 1; *p; p++)
    {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
    }
    return v;
}

static const char *group_code(const char *title, const char *detail, DiagSeverity sev)
{
    if (sev == DIAG_SEV_WARNING)
    {
        if (title && kn_strcmp(title, "Entry Unsafe") == 0)
            return "SAF";
        return "WRN";
    }

    if (starts_with(title, "Expected") || starts_with(title, "Unexpected") ||
        starts_with(title, "Unterminated") || starts_with(detail, "Missing "))
        return "SYN";
    if (starts_with(title, "Type ") || starts_with(title, "Invalid Type") ||
        starts_with(title, "Unsupported Type") || starts_with(title, "Invalid Cast"))
        return "TYP";
    if (starts_with(title, "Unknown") || starts_with(title, "Ambiguous"))
        return "RES";
    if (starts_with(title, "Duplicate") || starts_with(title, "Name Conflict"))
        return "NAM";
    if (starts_with(title, "Argument Count"))
        return "ARG";
    if (starts_with(title, "Entry"))
        return "ENT";
    if (contains_substr(title, "Return") || contains_substr(title, "Throw") ||
        kn_strcmp(title ? title : "", "Invalid Control Flow") == 0)
        return "CTL";
    if (starts_with(title, "Invalid") || starts_with(title, "Unsafe") ||
        starts_with(title, "Access Violation") || starts_with(title, "Missing"))
        return "SEM";
    return "GEN";
}

static void u32_to_text(uint32_t v, char out[16])
{
    char tmp[16];
    int n = 0;
    if (v == 0)
    {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    while (v > 0 && n < (int)sizeof(tmp))
    {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    int off = 0;
    for (int i = n - 1; i >= 0; i--)
        out[off++] = tmp[i];
    out[off] = 0;
}

static void make_code(char out[16], DiagSeverity sev, const char *title, const char *detail)
{
    const char *safe_title = title ? title : "";
    const char *safe_detail = canonical_detail_for_code(safe_title, detail);
    const char *grp = group_code(safe_title, safe_detail, sev);
    char sev_ch = (sev == DIAG_SEV_WARNING) ? 'W' : 'E';

    // Keep code stable for already-registered templates.
    for (int i = 0; i < g_template_count; i++)
    {
        DiagTemplate *t = &g_templates[i];
        const char *tmpl_detail = canonical_detail_for_code(t->title, t->detail);
        if (t->code[0] == sev_ch &&
            kn_strcmp(t->title ? t->title : "", safe_title) == 0 &&
            kn_strcmp(tmpl_detail, safe_detail) == 0)
        {
            size_t n = kn_strlen(t->code);
            if (n >= 15) n = 15;
            kn_memcpy(out, t->code, n);
            out[n] = 0;
            return;
        }
    }

    // Human-readable sequential numbering by severity+group.
    // Example: E-SYN-00001, E-SYN-00002 ...
    char prefix[8];
    prefix[0] = sev_ch;
    prefix[1] = '-';
    prefix[2] = grp[0];
    prefix[3] = grp[1];
    prefix[4] = grp[2];
    prefix[5] = '-';
    prefix[6] = 0;

    int next = 1;
    for (int i = 0; i < g_template_count; i++)
    {
        DiagTemplate *t = &g_templates[i];
        if (kn_strncmp(t->code, prefix, 6) != 0)
            continue;
        int n = parse_code_number(t->code);
        if (n >= next)
            next = n + 1;
    }
    if (next > 99999) next = 99999;

    out[0] = sev_ch;
    out[1] = '-';
    out[2] = grp[0];
    out[3] = grp[1];
    out[4] = grp[2];
    out[5] = '-';
    out[6] = (char)('0' + ((next / 10000) % 10));
    out[7] = (char)('0' + ((next / 1000) % 10));
    out[8] = (char)('0' + ((next / 100) % 10));
    out[9] = (char)('0' + ((next / 10) % 10));
    out[10] = (char)('0' + (next % 10));
    out[11] = 0;

    // If collisions happen (manual overrides), linearly probe.
    for (int step = 1; step < 100000; step++)
    {
        int conflict = 0;
        for (int i = 0; i < g_template_count; i++)
        {
            DiagTemplate *t = &g_templates[i];
            if (kn_strcmp(t->code, out) != 0)
                continue;
            conflict = 1;
            break;
        }
        if (!conflict)
            return;

        int v = (next + step) % 100000;
        out[6] = (char)('0' + ((v / 10000) % 10));
        out[7] = (char)('0' + ((v / 1000) % 10));
        out[8] = (char)('0' + ((v / 100) % 10));
        out[9] = (char)('0' + ((v / 10) % 10));
        out[10] = (char)('0' + (v % 10));
    }
}

static int use_color(void)
{
    if (g_color_mode == KN_COLOR_ALWAYS) return 1;
    if (g_color_mode == KN_COLOR_NEVER) return 0;
    if (g_color_probe_done)
        return g_color_auto_enabled;

    g_color_probe_done = 1;
    g_color_auto_enabled = 0;

    KN_HANDLE h = GetStdHandle(KN_STDERR_HANDLE);
    KN_DWORD mode = 0;
    if (!h || h == KN_INVALID_HANDLE_VALUE) return 0;
    if (!GetConsoleMode(h, &mode)) return 0;
    // Try enable ANSI VT processing on Windows console.
    // If SetConsoleMode fails, keep existing mode and still try ANSI output.
    (void)SetConsoleMode(h, mode | 0x0004u);
    g_color_auto_enabled = 1;
    return 1;
}

static void write_color(const char *ansi)
{
    if (!ansi || !ansi[0]) return;
    if (!use_color()) return;
    kn_write_str(ansi);
}

static void write_spaces(size_t n)
{
    for (size_t i = 0; i < n; i++)
        kn_write_str(" ");
}

static void template_add(const char code[16], const char *title, const char *detail)
{
    if (!code || !code[0]) return;
    for (int i = 0; i < g_template_count; i++)
    {
        if (kn_strcmp(g_templates[i].code, code) == 0)
            return;
    }
    if (g_template_count >= (int)(sizeof(g_templates) / sizeof(g_templates[0])))
        return;
    DiagTemplate *t = &g_templates[g_template_count++];
    kn_memset(t, 0, sizeof(*t));
    size_t n = kn_strlen(code);
    if (n >= sizeof(t->code))
        n = sizeof(t->code) - 1;
    kn_memcpy(t->code, code, n);
    t->code[n] = 0;
    t->title = title ? title : "";
    t->detail = canonical_detail_for_code(title, detail);
}

void kn_diag_register_seed_code(const char *code, int severity, const char *title, const char *detail)
{
    if (!code || !code[0])
    {
        kn_diag_register_seed(severity, title, detail);
        return;
    }

    char stable_code[16];
    size_t n = kn_strlen(code);
    if (n >= sizeof(stable_code))
        n = sizeof(stable_code) - 1;
    kn_memcpy(stable_code, code, n);
    stable_code[n] = 0;
    template_add(stable_code, title ? title : "", detail ? detail : "");
}

void kn_diag_register_seed(int severity, const char *title, const char *detail)
{
    DiagSeverity sev = (severity == DIAG_SEV_WARNING) ? DIAG_SEV_WARNING : DIAG_SEV_ERROR;
    char code[16];
    make_code(code, sev, title ? title : "", detail ? detail : "");
    template_add(code, title ? title : "", detail ? detail : "");
}

static void seed_builtin_templates(void)
{
    kn_diag_register_builtin_templates();
}

static void ensure_locale_loaded(void)
{
    if (g_locale_text || !g_locale_path || !g_locale_path[0])
        return;
    size_t sz = 0;
    g_locale_text = kn_read_file(g_locale_path, &sz);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_u16_escape(const char **pp, uint32_t *out)
{
    if (!pp || !*pp || !out) return 0;
    const char *p = *pp;
    uint32_t cp = 0;
    for (int i = 0; i < 4; i++)
    {
        int hv = hex_value(p[i]);
        if (hv < 0) return 0;
        cp = (cp << 4) | (uint32_t)hv;
    }
    *pp = p + 4;
    *out = cp;
    return 1;
}

static void append_utf8_cp(char *out, size_t cap, size_t *off, uint32_t cp)
{
    if (!out || !off || cap == 0) return;

    if (cp <= 0x7Fu)
    {
        if (*off + 1 < cap)
            out[(*off)++] = (char)cp;
        return;
    }
    if (cp <= 0x7FFu)
    {
        if (*off + 2 < cap)
        {
            out[(*off)++] = (char)(0xC0u | (cp >> 6));
            out[(*off)++] = (char)(0x80u | (cp & 0x3Fu));
        }
        return;
    }
    if (cp <= 0xFFFFu)
    {
        if (*off + 3 < cap)
        {
            out[(*off)++] = (char)(0xE0u | (cp >> 12));
            out[(*off)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
            out[(*off)++] = (char)(0x80u | (cp & 0x3Fu));
        }
        return;
    }
    if (cp <= 0x10FFFFu)
    {
        if (*off + 4 < cap)
        {
            out[(*off)++] = (char)(0xF0u | (cp >> 18));
            out[(*off)++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
            out[(*off)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
            out[(*off)++] = (char)(0x80u | (cp & 0x3Fu));
        }
    }
}

static int parse_json_string(const char *start, char *out, size_t cap)
{
    if (!start || !out || cap == 0 || start[0] != '"')
        return 0;
    size_t off = 0;
    const char *p = start + 1;
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
            else if (e == 'u')
            {
                uint32_t cp = 0;
                if (!parse_u16_escape(&p, &cp))
                    return 0;
                if (cp >= 0xD800u && cp <= 0xDBFFu)
                {
                    if (p[0] == '\\' && p[1] == 'u')
                    {
                        const char *q = p + 2;
                        uint32_t lo = 0;
                        if (parse_u16_escape(&q, &lo) && lo >= 0xDC00u && lo <= 0xDFFFu)
                        {
                            cp = 0x10000u + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                            p = q;
                        }
                    }
                }
                append_utf8_cp(out, cap, &off, cp);
                continue;
            }
            else c = e;
        }
        if (off + 1 < cap)
            out[off++] = c;
    }
    out[off] = 0;
    return *p == '"';
}

static const char *find_quoted_key(const char *text, const char *key)
{
    if (!text || !key || !key[0]) return 0;
    size_t kn = kn_strlen(key);
    for (const char *p = text; *p; p++)
    {
        if (*p != '"') continue;
        if (kn_strncmp(p + 1, key, kn) != 0) continue;
        if (p[1 + kn] != '"') continue;
        return p;
    }
    return 0;
}

static const char *find_key_in_range(const char *start, const char *end, const char *key)
{
    if (!start || !end || !key || !key[0] || start >= end) return 0;
    size_t kn = kn_strlen(key);
    for (const char *p = start; p < end && *p; p++)
    {
        if (*p != '"') continue;
        size_t rem = (size_t)(end - p);
        if (rem < kn + 2) continue;
        if (kn_strncmp(p + 1, key, kn) != 0) continue;
        if (p[1 + kn] != '"') continue;
        return p;
    }
    return 0;
}

static const char *next_locale_buf(void)
{
    g_locale_buf_index = (g_locale_buf_index + 1) % (int)(sizeof(g_locale_buf) / sizeof(g_locale_buf[0]));
    g_locale_buf[g_locale_buf_index][0] = 0;
    return g_locale_buf[g_locale_buf_index];
}

static const char *lookup_locale_field(const char *entry_key, const char *field)
{
    ensure_locale_loaded();
    if (!g_locale_text || !entry_key || !entry_key[0] || !field || !field[0])
        return 0;

    const char *entry = find_quoted_key(g_locale_text, entry_key);
    if (!entry) return 0;

    const char *obj_start = 0;
    for (const char *p = entry; *p; p++)
    {
        if (*p == '{') { obj_start = p; break; }
        if (*p == '\n' && p > entry + 2) break;
    }
    if (!obj_start) return 0;

    int depth = 0;
    const char *obj_end = 0;
    for (const char *p = obj_start; *p; p++)
    {
        if (*p == '{') depth++;
        else if (*p == '}')
        {
            depth--;
            if (depth == 0)
            {
                obj_end = p;
                break;
            }
        }
    }
    if (!obj_end) return 0;

    const char *field_hit = find_key_in_range(obj_start, obj_end, field);
    if (!field_hit) return 0;

    const char *colon = 0;
    for (const char *p = field_hit; p < obj_end; p++)
    {
        if (*p == ':') { colon = p; break; }
    }
    if (!colon) return 0;

    const char *q = 0;
    for (const char *p = colon + 1; p < obj_end; p++)
    {
        if (*p == '"') { q = p; break; }
    }
    if (!q) return 0;

    char *out = (char *)next_locale_buf();
    if (!parse_json_string(q, out, 2048))
        return 0;
    return out;
}

static const char *locale_or_fallback(const char *key, const char *fallback)
{
    // Locale entry format:
    // { "key": { "text": "..." } }
    const char *v = lookup_locale_field(key, "text");
    if (v && v[0]) return v;
    return fallback ? fallback : "";
}

const char *kn_diag_localize(const char *key, const char *fallback)
{
    return locale_or_fallback(key, fallback);
}

static void append_json_escaped(char *buf, size_t cap, size_t *off, const char *s)
{
    if (!buf || !off || !s) return;
    for (size_t i = 0; s[i]; i++)
    {
        char c = s[i];
        if (c == '\\')
            kn_append(buf, cap, off, "\\\\");
        else if (c == '"')
            kn_append(buf, cap, off, "\\\"");
        else if (c == '\n')
            kn_append(buf, cap, off, "\\n");
        else if (c == '\r')
            kn_append(buf, cap, off, "\\r");
        else if (c == '\t')
            kn_append(buf, cap, off, "\\t");
        else
        {
            char one[2];
            one[0] = c;
            one[1] = 0;
            kn_append(buf, cap, off, one);
        }
    }
}

static void render_fmt(char *out, size_t cap, const char *fmt,
                       const char *a0, const char *a1, const char *a2, const char *a3)
{
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!fmt) return;
    size_t off = 0;
    for (size_t i = 0; fmt[i] && off + 1 < cap; i++)
    {
        if (fmt[i] == '{' && fmt[i + 2] == '}')
        {
            const char *rep = 0;
            if (fmt[i + 1] == '0') rep = a0;
            else if (fmt[i + 1] == '1') rep = a1;
            else if (fmt[i + 1] == '2') rep = a2;
            else if (fmt[i + 1] == '3') rep = a3;
            if (rep)
            {
                kn_append(out, cap, &off, rep);
                i += 2;
                continue;
            }
        }
        char one[2];
        one[0] = fmt[i];
        one[1] = 0;
        kn_append(out, cap, &off, one);
    }
}

static int ascii_tolower_i(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int starts_with_ci(const char *s, const char *prefix)
{
    if (!s || !prefix) return 0;
    size_t i = 0;
    while (prefix[i])
    {
        if (!s[i]) return 0;
        if (ascii_tolower_i((unsigned char)s[i]) != ascii_tolower_i((unsigned char)prefix[i]))
            return 0;
        i++;
    }
    return 1;
}

static const char *dynamic_title_fallback(const char *title)
{
    (void)title;
    return 0;
}

static int parse_arg_count_detail(const char *detail, const char **name_start, size_t *name_len, int *argc_out)
{
    if (name_start) *name_start = 0;
    if (name_len) *name_len = 0;
    if (argc_out) *argc_out = -1;
    if (!detail || !detail[0]) return 0;

    const char *p = detail;
    while (*p && *p != '(') p++;
    if (!(*p == '(' && p[1] == ')' && p[2] == ' '))
        return 0;
    const char *tail = p + 3;
    if (!starts_with_ci(tail, "takes "))
        return 0;
    tail += 6;

    int n = -1;
    if (starts_with_ci(tail, "no arguments"))
    {
        n = 0;
    }
    else
    {
        const char *q = tail;
        n = 0;
        while (*q >= '0' && *q <= '9')
        {
            n = n * 10 + (*q - '0');
            q++;
        }
        if (q == tail)
        {
            if (starts_with_ci(tail, "one argument")) n = 1;
            else if (starts_with_ci(tail, "two arguments")) n = 2;
            else if (starts_with_ci(tail, "three arguments")) n = 3;
            else if (starts_with_ci(tail, "four arguments")) n = 4;
            else if (starts_with_ci(tail, "five arguments")) n = 5;
            else return 0;
        }
        else if (!starts_with_ci(q, " argument"))
            return 0;
    }

    if (name_start) *name_start = detail;
    if (name_len) *name_len = (size_t)(p - detail) + 2; // include "()"
    if (argc_out) *argc_out = n;
    return 1;
}

static int parse_expected_got_detail(const char *detail,
                                     const char **exp_start, size_t *exp_len,
                                     const char **got_start, size_t *got_len)
{
    if (exp_start) *exp_start = 0;
    if (exp_len) *exp_len = 0;
    if (got_start) *got_start = 0;
    if (got_len) *got_len = 0;
    if (!detail || !detail[0]) return 0;
    if (!starts_with_ci(detail, "Expected ")) return 0;

    const char *exp = detail + 9; // after "Expected "
    const char *sep = 0;
    for (const char *p = exp; *p; p++)
    {
        if (p[0] == ',' && p[1] == ' ' &&
            ascii_tolower_i((unsigned char)p[2]) == 'g' &&
            ascii_tolower_i((unsigned char)p[3]) == 'o' &&
            ascii_tolower_i((unsigned char)p[4]) == 't' &&
            p[5] == ' ')
        {
            sep = p;
            break;
        }
    }
    if (!sep) return 0;

    const char *got = sep + 6; // skip ", got "
    size_t en = (size_t)(sep - exp);
    size_t gn = kn_strlen(got);
    while (en > 0 && exp[en - 1] == ' ') en--;
    while (gn > 0 && got[gn - 1] == ' ') gn--;
    if (en == 0 || gn == 0) return 0;

    if (exp_start) *exp_start = exp;
    if (exp_len) *exp_len = en;
    if (got_start) *got_start = got;
    if (got_len) *got_len = gn;
    return 1;
}

static int parse_must_be_detail(const char *detail,
                                const char **lhs_start, size_t *lhs_len,
                                const char **rhs_start, size_t *rhs_len)
{
    if (lhs_start) *lhs_start = 0;
    if (lhs_len) *lhs_len = 0;
    if (rhs_start) *rhs_start = 0;
    if (rhs_len) *rhs_len = 0;
    if (!detail || !detail[0]) return 0;

    const char *sep = 0;
    for (const char *p = detail; *p; p++)
    {
        if (p[0] == ' ' &&
            ascii_tolower_i((unsigned char)p[1]) == 'm' &&
            ascii_tolower_i((unsigned char)p[2]) == 'u' &&
            ascii_tolower_i((unsigned char)p[3]) == 's' &&
            ascii_tolower_i((unsigned char)p[4]) == 't' &&
            p[5] == ' ' &&
            ascii_tolower_i((unsigned char)p[6]) == 'b' &&
            ascii_tolower_i((unsigned char)p[7]) == 'e' &&
            p[8] == ' ')
        {
            sep = p;
            break;
        }
    }
    if (!sep) return 0;

    const char *lhs = detail;
    size_t ln = (size_t)(sep - lhs);
    const char *rhs = sep + 9; // " must be "
    size_t rn = kn_strlen(rhs);
    while (ln > 0 && lhs[ln - 1] == ' ') ln--;
    while (rn > 0 && rhs[rn - 1] == ' ') rn--;
    if (ln == 0 || rn == 0) return 0;

    if (lhs_start) *lhs_start = lhs;
    if (lhs_len) *lhs_len = ln;
    if (rhs_start) *rhs_start = rhs;
    if (rhs_len) *rhs_len = rn;
    return 1;
}

static int parse_requires_detail(const char *detail,
                                 const char **lhs_start, size_t *lhs_len,
                                 const char **rhs_start, size_t *rhs_len)
{
    if (lhs_start) *lhs_start = 0;
    if (lhs_len) *lhs_len = 0;
    if (rhs_start) *rhs_start = 0;
    if (rhs_len) *rhs_len = 0;
    if (!detail || !detail[0]) return 0;

    const char *sep = 0;
    for (const char *p = detail; *p; p++)
    {
        if (p[0] == ' ' &&
            ascii_tolower_i((unsigned char)p[1]) == 'r' &&
            ascii_tolower_i((unsigned char)p[2]) == 'e' &&
            ascii_tolower_i((unsigned char)p[3]) == 'q' &&
            ascii_tolower_i((unsigned char)p[4]) == 'u' &&
            ascii_tolower_i((unsigned char)p[5]) == 'i' &&
            ascii_tolower_i((unsigned char)p[6]) == 'r' &&
            ascii_tolower_i((unsigned char)p[7]) == 'e' &&
            ascii_tolower_i((unsigned char)p[8]) == 's' &&
            p[9] == ' ')
        {
            sep = p;
            break;
        }
    }
    if (!sep) return 0;

    const char *lhs = detail;
    size_t ln = (size_t)(sep - lhs);
    const char *rhs = sep + 10; // " requires "
    size_t rn = kn_strlen(rhs);
    while (ln > 0 && lhs[ln - 1] == ' ') ln--;
    while (rn > 0 && rhs[rn - 1] == ' ') rn--;
    if (ln == 0 || rn == 0) return 0;

    if (lhs_start) *lhs_start = lhs;
    if (lhs_len) *lhs_len = ln;
    if (rhs_start) *rhs_start = rhs;
    if (rhs_len) *rhs_len = rn;
    return 1;
}

static int parse_cast_to_detail(const char *detail,
                                const char **src_start, size_t *src_len,
                                const char **dst_start, size_t *dst_len)
{
    if (src_start) *src_start = 0;
    if (src_len) *src_len = 0;
    if (dst_start) *dst_start = 0;
    if (dst_len) *dst_len = 0;
    if (!detail || !detail[0]) return 0;
    if (!starts_with_ci(detail, "Cannot cast ")) return 0;

    const char *src = detail + 12;
    const char *sep = 0;
    for (const char *p = src; *p; p++)
    {
        if (p[0] == ' ' &&
            ascii_tolower_i((unsigned char)p[1]) == 't' &&
            ascii_tolower_i((unsigned char)p[2]) == 'o' &&
            p[3] == ' ')
        {
            sep = p;
            break;
        }
    }
    if (!sep) return 0;

    const char *dst = sep + 4; // " to "
    size_t sn = (size_t)(sep - src);
    size_t dn = kn_strlen(dst);
    while (sn > 0 && src[sn - 1] == ' ') sn--;
    while (dn > 0 && dst[dn - 1] == ' ') dn--;
    if (sn == 0 || dn == 0) return 0;

    if (src_start) *src_start = src;
    if (src_len) *src_len = sn;
    if (dst_start) *dst_start = dst;
    if (dst_len) *dst_len = dn;
    return 1;
}

static int parse_unused_var_detail(const char *detail, const char **name_start, size_t *name_len)
{
    if (name_start) *name_start = 0;
    if (name_len) *name_len = 0;
    if (!detail || !detail[0]) return 0;
    if (!starts_with_ci(detail, "Variable '")) return 0;

    const char *name = detail + 10; // after "Variable '"
    const char *end = 0;
    for (const char *p = name; *p; p++)
    {
        if (p[0] == '\'' &&
            p[1] == ' ' &&
            ascii_tolower_i((unsigned char)p[2]) == 'i' &&
            ascii_tolower_i((unsigned char)p[3]) == 's' &&
            p[4] == ' ' &&
            ascii_tolower_i((unsigned char)p[5]) == 'n' &&
            ascii_tolower_i((unsigned char)p[6]) == 'e' &&
            ascii_tolower_i((unsigned char)p[7]) == 'v' &&
            ascii_tolower_i((unsigned char)p[8]) == 'e' &&
            ascii_tolower_i((unsigned char)p[9]) == 'r' &&
            p[10] == ' ' &&
            ascii_tolower_i((unsigned char)p[11]) == 'u' &&
            ascii_tolower_i((unsigned char)p[12]) == 's' &&
            ascii_tolower_i((unsigned char)p[13]) == 'e' &&
            ascii_tolower_i((unsigned char)p[14]) == 'd' &&
            p[15] == 0)
        {
            end = p;
            break;
        }
    }
    if (!end || end <= name) return 0;

    if (name_start) *name_start = name;
    if (name_len) *name_len = (size_t)(end - name);
    return 1;
}

static int parse_unreachable_detail(const char *detail, const char **reason_start, size_t *reason_len)
{
    if (reason_start) *reason_start = 0;
    if (reason_len) *reason_len = 0;
    if (!detail || !detail[0]) return 0;
    if (!starts_with_ci(detail, "Unreachable code")) return 0;

    const char *p = detail + 16; // len("Unreachable code")
    if (p[0] == 0) return 1;
    if (!(p[0] == ':' && p[1] == ' ')) return 0;
    p += 2;
    size_t n = kn_strlen(p);
    while (n > 0 && p[n - 1] == ' ') n--;
    if (reason_start) *reason_start = p;
    if (reason_len) *reason_len = n;
    return 1;
}

static const char *dynamic_detail_fallback(const char *detail)
{
    if (!detail || !detail[0]) return 0;

    const char *name_s = 0;
    size_t name_n = 0;
    if (parse_unused_var_detail(detail, &name_s, &name_n))
    {
        char name_buf[512];
        char out_buf[2048];
        size_t nn = name_n < sizeof(name_buf) - 1 ? name_n : sizeof(name_buf) - 1;
        kn_memcpy(name_buf, name_s, nn);
        name_buf[nn] = 0;

        const char *fmt = locale_or_fallback("ui.diag.unused_var", "Unused variable: {0}");
        render_fmt(out_buf, sizeof(out_buf), fmt, name_buf, "", "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    const char *reason_s = 0;
    size_t reason_n = 0;
    if (parse_unreachable_detail(detail, &reason_s, &reason_n))
    {
        char reason_buf[1024];
        char out_buf[2048];
        if (!reason_s || reason_n == 0)
        {
            reason_buf[0] = 0;
        }
        else
        {
            size_t rn = reason_n < sizeof(reason_buf) - 1 ? reason_n : sizeof(reason_buf) - 1;
            kn_memcpy(reason_buf, reason_s, rn);
            reason_buf[rn] = 0;
        }

        const char *fmt = locale_or_fallback("ui.diag.unreachable", "Unreachable code: {0}");
        render_fmt(out_buf, sizeof(out_buf), fmt, reason_buf, "", "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    // "<Name>() takes N argument(s)" => one generic localized template.
    int n = -1;
    if (parse_arg_count_detail(detail, &name_s, &name_n, &n))
    {
        char name_buf[512];
        char argc_buf[32];
        char out_buf[2048];
        size_t copy_n = name_n < sizeof(name_buf) - 1 ? name_n : sizeof(name_buf) - 1;
        kn_memcpy(name_buf, name_s, copy_n);
        name_buf[copy_n] = 0;
        u32_to_text((uint32_t)n, argc_buf);

        const char *fmt = locale_or_fallback("ui.diag.arg_count",
                                             "Argument count mismatch: {0} expects {1} argument(s)");
        render_fmt(out_buf, sizeof(out_buf), fmt, name_buf, argc_buf, "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    // "Expected X, got Y" => one generic localized template.
    const char *exp_s = 0;
    const char *got_s = 0;
    size_t exp_n = 0;
    size_t got_n = 0;
    if (parse_expected_got_detail(detail, &exp_s, &exp_n, &got_s, &got_n))
    {
        char exp_buf[512];
        char got_buf[512];
        char out_buf[2048];
        size_t en = exp_n < sizeof(exp_buf) - 1 ? exp_n : sizeof(exp_buf) - 1;
        size_t gn = got_n < sizeof(got_buf) - 1 ? got_n : sizeof(got_buf) - 1;
        kn_memcpy(exp_buf, exp_s, en);
        exp_buf[en] = 0;
        kn_memcpy(got_buf, got_s, gn);
        got_buf[gn] = 0;

        const char *fmt = locale_or_fallback("ui.diag.type_mismatch",
                                             "Type mismatch: expected {0}, got {1}");
        render_fmt(out_buf, sizeof(out_buf), fmt, exp_buf, got_buf, "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    // "<lhs> must be <rhs>" => one generic localized template.
    const char *lhs_s = 0;
    const char *rhs_s = 0;
    size_t lhs_n = 0;
    size_t rhs_n = 0;
    if (parse_must_be_detail(detail, &lhs_s, &lhs_n, &rhs_s, &rhs_n))
    {
        char lhs_buf[512];
        char rhs_buf[512];
        char out_buf[2048];
        size_t ln = lhs_n < sizeof(lhs_buf) - 1 ? lhs_n : sizeof(lhs_buf) - 1;
        size_t rn = rhs_n < sizeof(rhs_buf) - 1 ? rhs_n : sizeof(rhs_buf) - 1;
        kn_memcpy(lhs_buf, lhs_s, ln);
        lhs_buf[ln] = 0;
        kn_memcpy(rhs_buf, rhs_s, rn);
        rhs_buf[rn] = 0;

        const char *fmt = locale_or_fallback("ui.diag.type_mismatch.must_be",
                                             "Type mismatch: {0} must be {1}");
        render_fmt(out_buf, sizeof(out_buf), fmt, lhs_buf, rhs_buf, "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    // "<lhs> requires <rhs>" => one generic localized template.
    if (parse_requires_detail(detail, &lhs_s, &lhs_n, &rhs_s, &rhs_n))
    {
        char lhs_buf[512];
        char rhs_buf[512];
        char out_buf[2048];
        size_t ln = lhs_n < sizeof(lhs_buf) - 1 ? lhs_n : sizeof(lhs_buf) - 1;
        size_t rn = rhs_n < sizeof(rhs_buf) - 1 ? rhs_n : sizeof(rhs_buf) - 1;
        kn_memcpy(lhs_buf, lhs_s, ln);
        lhs_buf[ln] = 0;
        kn_memcpy(rhs_buf, rhs_s, rn);
        rhs_buf[rn] = 0;

        const char *fmt = locale_or_fallback("ui.diag.type_mismatch.requires",
                                             "Type mismatch: {0} requires {1}");
        render_fmt(out_buf, sizeof(out_buf), fmt, lhs_buf, rhs_buf, "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    // "Cannot cast <src> to <dst>" => one generic localized template.
    const char *src_s = 0;
    const char *dst_s = 0;
    size_t src_n = 0;
    size_t dst_n = 0;
    if (parse_cast_to_detail(detail, &src_s, &src_n, &dst_s, &dst_n))
    {
        char src_buf[512];
        char dst_buf[512];
        char out_buf[2048];
        size_t sn = src_n < sizeof(src_buf) - 1 ? src_n : sizeof(src_buf) - 1;
        size_t dn = dst_n < sizeof(dst_buf) - 1 ? dst_n : sizeof(dst_buf) - 1;
        kn_memcpy(src_buf, src_s, sn);
        src_buf[sn] = 0;
        kn_memcpy(dst_buf, dst_s, dn);
        dst_buf[dn] = 0;

        const char *fmt = locale_or_fallback("ui.diag.type_mismatch.cast",
                                             "Type mismatch: cannot cast {0} to {1}");
        render_fmt(out_buf, sizeof(out_buf), fmt, src_buf, dst_buf, "", "");
        char *out = (char *)next_locale_buf();
        size_t off = 0;
        kn_memset(out, 0, 2048);
        kn_append(out, 2048, &off, out_buf);
        return out;
    }

    return 0;
}

static const char *localize_diag_field(const char *code, const char *field, const char *raw)
{
    // New code-driven format:
    // {
    //   "E-SYN-00001": { "title": "...", "detail": "..." }
    // }
    const char *v = lookup_locale_field(code, field);
    if (v && v[0]) return v;

    return raw ? raw : "";
}

static void print_header(KnDiagStage stage, DiagSeverity sev, const char code[16],
                         const char *title, const char *detail, const char *got)
{
    const char *sev_color = (sev == DIAG_SEV_WARNING) ? "\x1b[33m" : "\x1b[31m";
    const char *sev_label_key = (sev == DIAG_SEV_WARNING) ? "ui.severity.warning" : "ui.severity.error";
    const char *sev_label = locale_or_fallback(sev_label_key, (sev == DIAG_SEV_WARNING) ? "warning" : "error");
    const char *stage_label = locale_or_fallback(stage_locale_key(stage), stage_name(stage));

    write_color(sev_color);
    kn_write_str("[");
    kn_write_str(stage_label);
    kn_write_str("][");
    kn_write_str(code);
    kn_write_str("][");
    kn_write_str(sev_label);
    kn_write_str("] ");
    write_color("\x1b[0m");
    if (use_color()) write_color(sev_color);

    const char *raw_title = title ? title : "Error";
    const char *raw_detail = detail ? detail : "";
    const char *msg_title = localize_diag_field(code, "title", raw_title);
    const char *forced_dyn_detail = dynamic_detail_fallback(raw_detail);
    const char *canon = canonical_detail_for_code(raw_title, raw_detail);
    int prefer_raw_detail = (kn_strcmp(canon ? canon : "", raw_detail ? raw_detail : "") != 0);
    const char *msg_detail = forced_dyn_detail && forced_dyn_detail[0]
                           ? forced_dyn_detail
                           : (prefer_raw_detail ? raw_detail : localize_diag_field(code, "detail", raw_detail));
    if (msg_title == raw_title)
    {
        const char *dyn_title = dynamic_title_fallback(raw_title);
        if (dyn_title && dyn_title[0]) msg_title = dyn_title;
    }
    if (!forced_dyn_detail && msg_detail == raw_detail)
    {
        const char *dyn_detail = dynamic_detail_fallback(raw_detail);
        if (dyn_detail && dyn_detail[0]) msg_detail = dyn_detail;
    }
    if (msg_title && msg_title[0] && msg_detail && msg_detail[0])
    {
        if (starts_with(msg_detail, msg_title))
        {
            kn_write_str(msg_detail);
        }
        else
        {
            kn_write_str(msg_title);
            kn_write_str(": ");
            kn_write_str(msg_detail);
        }
    }
    else if (msg_detail && msg_detail[0])
    {
        kn_write_str(msg_detail);
    }
    else
    {
        kn_write_str(msg_title);
    }

    if (got && got[0])
    {
        if (kn_strcmp(got, "<end-of-file>") == 0)
        {
            kn_write_str(locale_or_fallback("ui.diag.found_eof", " (found end of file)"));
        }
        else
        {
            char tmp[512];
            const char *fmt = locale_or_fallback("ui.diag.found_token", " (found token '{0}')");
            render_fmt(tmp, sizeof(tmp), fmt, got, "", "", "");
            kn_write_str(tmp);
        }
    }

    write_color("\x1b[0m");
    kn_write_str("\n");
}

static void write_span(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        char one[2];
        one[0] = s[i];
        one[1] = 0;
        kn_write_str(one);
    }
}

static int tok_is_keyword(TokenType t)
{
    switch (t)
    {
    case TOK_UNIT:
    case TOK_GET:
    case TOK_SET:
    case TOK_BY:
    case TOK_PROPERTY:
    case TOK_FUNCTION:
    case TOK_RETURN:
    case TOK_IF:
    case TOK_ELSE:
    case TOK_ELSEIF:
    case TOK_WHILE:
    case TOK_FOR:
    case TOK_SWITCH:
    case TOK_CASE:
    case TOK_BREAK:
    case TOK_CONTINUE:
    case TOK_TRY:
    case TOK_CATCH:
    case TOK_THROW:
    case TOK_CONST:
    case TOK_VAR:
    case TOK_SAFE:
    case TOK_TRUSTED:
    case TOK_UNSAFE:
    case TOK_EXTERN:
    case TOK_CLASS:
    case TOK_INTERFACE:
    case TOK_STRUCT:
    case TOK_ENUM:
    case TOK_PUBLIC:
    case TOK_PRIVATE:
    case TOK_PROTECTED:
    case TOK_INTERNAL:
    case TOK_STATIC:
    case TOK_VIRTUAL:
    case TOK_OVERRIDE:
    case TOK_ABSTRACT:
    case TOK_SEALED:
    case TOK_CONSTRUCTOR:
    case TOK_NEW:
    case TOK_THIS:
    case TOK_BASE:
    case TOK_BLOCK:
    case TOK_RECORD:
    case TOK_JUMP:
    case TOK_DELEGATE:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_IS:
    case TOK_DEFAULT:
    case TOK_NULL:
        return 1;
    default:
        return 0;
    }
}

static int tok_is_type(TokenType t)
{
    return t >= TOK_TYPE_VOID && t <= TOK_TYPE_ANY;
}

static int tok_is_name(TokenType t)
{
    return t == TOK_ID;
}

static int tok_is_bracket(TokenType t)
{
    return t == TOK_LPAREN || t == TOK_RPAREN ||
           t == TOK_LBRACE || t == TOK_RBRACE ||
           t == TOK_LBRACKET || t == TOK_RBRACKET;
}

static int tok_is_literal(TokenType t)
{
    return t == TOK_STRING || t == TOK_BAD_STRING || t == TOK_NUMBER || t == TOK_TRUE || t == TOK_FALSE || t == TOK_NULL;
}

static const char *tok_color(TokenType t)
{
    // 5 groups: keyword/type/name/bracket/literal
    if (tok_is_keyword(t)) return "\x1b[38;5;39m";   // blue
    if (tok_is_type(t)) return "\x1b[38;5;45m";      // cyan
    if (tok_is_name(t)) return "\x1b[38;5;222m";     // warm yellow
    if (tok_is_bracket(t)) return "\x1b[38;5;177m";  // magenta
    if (tok_is_literal(t)) return "\x1b[38;5;114m";  // green
    return 0;
}

static void print_colored_source_line(const char *line_text, size_t line_len)
{
    if (!line_text || line_len == 0 || !use_color())
    {
        if (line_text && line_len) write_span(line_text, line_len);
        return;
    }

    char *tmp = (char *)kn_malloc(line_len + 1);
    if (!tmp)
    {
        write_span(line_text, line_len);
        return;
    }
    kn_memcpy(tmp, line_text, line_len);
    tmp[line_len] = 0;

    Lexer lx;
    lex_init(&lx, tmp);
    size_t pos = 0;
    for (;;)
    {
        Token t = lex_next(&lx);
        if (t.type == TOK_EOF)
            break;
        size_t tok_off = (size_t)(t.start - tmp);
        if (tok_off > pos)
            write_span(tmp + pos, tok_off - pos);
        const char *c = tok_color(t.type);
        if (c) write_color(c);
        write_span(t.start, t.length);
        if (c) write_color("\x1b[0m");
        pos = tok_off + t.length;
    }
    if (pos < line_len)
        write_span(tmp + pos, line_len - pos);
    kn_free(tmp);
}

static void print_location(const KnSource *src, int line, int col, int len)
{
    char line_s[16];
    char col_s[16];
    u32_to_text((uint32_t)(line > 0 ? line : 0), line_s);
    u32_to_text((uint32_t)(col > 0 ? col : 1), col_s);

    char loc_msg[1024];
    const char *loc_fmt = locale_or_fallback("ui.diag.location", "in <{0}> at line <{1}>, column <{2}>");
    render_fmt(loc_msg, sizeof(loc_msg), loc_fmt, (src && src->path) ? src->path : "", line_s, col_s, "");
    kn_write_str(loc_msg);
    kn_write_str("\n\n");

    size_t line_len = 0;
    const char *line_text = kn_source_get_line(src, line, &line_len);

    kn_write_str(line_s);
    kn_write_str(" | ");
    if (line_text && line_len)
        print_colored_source_line(line_text, line_len);
    kn_write_str("\n");

    size_t prefix = kn_strlen(line_s) + 3;
    if (col < 1) col = 1;
    size_t tabw = 4;
    size_t visual = 0;
    size_t limit = (size_t)(col - 1);
    if (line_text && line_len)
    {
        if (limit > line_len) limit = line_len;
        for (size_t i = 0; i < limit; i++)
        {
            if (line_text[i] == '\t')
            {
                size_t off = visual % tabw;
                visual += (tabw - off);
            }
            else
            {
                visual++;
            }
        }
    }
    size_t caret_pos = prefix + visual;
    write_spaces(caret_pos);
    int caret_len = len > 0 ? len : 1;
    write_color("\x1b[36m");
    for (int i = 0; i < caret_len; i++)
        kn_write_str("^");
    write_color("\x1b[0m");
    kn_write_str("\n");
}

static int diag_already_emitted(KnDiagStage stage, DiagSeverity sev, int line, int col, const char code[16], const char *title)
{
    const char *safe_title = title ? title : "";
    for (int i = 0; i < g_emitted_count; i++)
    {
        EmittedDiag *e = &g_emitted[i];
        if (e->stage != stage || e->sev != sev || e->line != line || e->col != col)
            continue;
        if (kn_strcmp(e->code, code) == 0)
            return 1;
        if (safe_title[0] && kn_strcmp(e->title, safe_title) == 0)
            return 1;
    }
    if (g_emitted_count < (int)(sizeof(g_emitted) / sizeof(g_emitted[0])))
    {
        EmittedDiag *e = &g_emitted[g_emitted_count++];
        kn_memset(e, 0, sizeof(*e));
        e->stage = stage;
        e->sev = sev;
        e->line = line;
        e->col = col;
        size_t n = kn_strlen(code);
        if (n >= sizeof(e->code)) n = sizeof(e->code) - 1;
        kn_memcpy(e->code, code, n);
        e->code[n] = 0;
        size_t tn = kn_strlen(safe_title);
        if (tn >= sizeof(e->title)) tn = sizeof(e->title) - 1;
        if (tn > 0) kn_memcpy(e->title, safe_title, tn);
        e->title[tn] = 0;
    }
    return 0;
}

static int emit_diag(const KnSource *src, KnDiagStage stage, DiagSeverity sev, int line, int col, int len,
                     const char *title, const char *detail, const char *got)
{
    char code[16];
    make_code(code, sev, title ? title : "", detail ? detail : "");
    if (diag_already_emitted(stage, sev, line, col, code, title))
        return 0;
    template_add(code, title ? title : "", detail ? detail : "");

    if (g_sink)
    {
        const char *raw_title = title ? title : "Error";
        const char *raw_detail = detail ? detail : "";
        const char *msg_title = localize_diag_field(code, "title", raw_title);
        const char *forced_dyn_detail = dynamic_detail_fallback(raw_detail);
        const char *canon = canonical_detail_for_code(raw_title, raw_detail);
        int prefer_raw_detail = (kn_strcmp(canon ? canon : "", raw_detail ? raw_detail : "") != 0);
        const char *msg_detail = forced_dyn_detail && forced_dyn_detail[0]
                               ? forced_dyn_detail
                               : (prefer_raw_detail ? raw_detail : localize_diag_field(code, "detail", raw_detail));
        if (msg_title == raw_title)
        {
            const char *dyn_title = dynamic_title_fallback(raw_title);
            if (dyn_title && dyn_title[0]) msg_title = dyn_title;
        }
        if (!forced_dyn_detail && msg_detail == raw_detail)
        {
            const char *dyn_detail = dynamic_detail_fallback(raw_detail);
            if (dyn_detail && dyn_detail[0]) msg_detail = dyn_detail;
        }

        KnDiagEvent ev;
        ev.src = src;
        ev.stage = stage;
        ev.severity = (sev == DIAG_SEV_WARNING) ? KN_DIAG_SEV_WARNING : KN_DIAG_SEV_ERROR;
        ev.line = line;
        ev.col = col;
        ev.len = len;
        ev.code = code;
        ev.title = msg_title ? msg_title : raw_title;
        ev.detail = msg_detail ? msg_detail : raw_detail;
        ev.got = got ? got : "";
        g_sink(&ev, g_sink_user_data);
    }

    if (!g_quiet)
    {
        print_header(stage, sev, code, title, detail, got);
        print_location(src, line, col, len);
    }
    return 1;
}

void kn_diag_reset(void)
{
    g_error_count = 0;
    g_warning_count = 0;
    g_warn_as_error = 0;
    g_warn_level = 1;
    g_color_mode = KN_COLOR_AUTO;
    g_lang = "en";
    g_locale_path = 0;
    g_max_errors = 128;
    g_quiet = 0;
    g_sink = 0;
    g_sink_user_data = 0;
    g_color_probe_done = 0;
    g_color_auto_enabled = 0;
    g_emitted_count = 0;
    if (g_locale_text)
    {
        kn_free(g_locale_text);
        g_locale_text = 0;
    }
    g_template_count = 0;
    seed_builtin_templates();
}

void kn_diag_set_color_mode(int mode)
{
    g_color_mode = mode;
    g_color_probe_done = 0;
}

void kn_diag_set_language(const char *lang)
{
    g_lang = (lang && lang[0]) ? lang : "en";
    if (is_zh_lang())
    {
        SetConsoleOutputCP(65001u);
        SetConsoleCP(65001u);
    }
}

void kn_diag_set_locale_file(const char *path)
{
    g_locale_path = (path && path[0]) ? path : 0;
    if (g_locale_text)
    {
        kn_free(g_locale_text);
        g_locale_text = 0;
    }
}

void kn_diag_set_sink(KnDiagSink sink, void *user_data)
{
    g_sink = sink;
    g_sink_user_data = user_data;
}

void kn_diag_set_quiet(int quiet)
{
    g_quiet = quiet ? 1 : 0;
}

void kn_diag_set_max_errors(int max_errors)
{
    if (max_errors < 1)
        max_errors = 1;
    g_max_errors = max_errors;
}

void kn_diag_set_warn_as_error(int enabled) { g_warn_as_error = enabled ? 1 : 0; }
void kn_diag_set_warning_level(int level) { g_warn_level = level < 0 ? 0 : level; }
int kn_diag_error_count(void) { return g_error_count; }
int kn_diag_warning_count(void) { return g_warning_count; }
int kn_diag_warning_enabled(int level) { return level <= g_warn_level; }

void kn_diag_print_summary(void)
{
    if (g_error_count == 0 && g_warning_count == 0)
        return;

    char ebuf[16];
    char wbuf[16];
    u32_to_text((uint32_t)g_error_count, ebuf);
    u32_to_text((uint32_t)g_warning_count, wbuf);

    char msg[256];
    const char *fmt = locale_or_fallback("ui.diag.summary", "[Diag] {0} error(s), {1} warning(s)");
    render_fmt(msg, sizeof(msg), fmt, ebuf, wbuf, "", "");
    kn_write_str(msg);
    kn_write_str("\n");
}

int kn_diag_export_english_template(const char *out_path)
{
    if (!out_path || !out_path[0]) return -1;

    // Always export from a full seed set.
    g_template_count = 0;
    seed_builtin_templates();

    char *buf = (char *)kn_malloc(262144);
    if (!buf) return -1;
    size_t off = 0;
    buf[0] = 0;

    kn_append(buf, 262144, &off, "{\n");

    int emitted = 0;

    // Export code-driven diagnostics:
    // {
    //   "E-SYN-00001": { "title": "...", "detail": "..." }
    // }
    for (int i = 0; i < g_template_count; i++)
    {
        DiagTemplate *t = &g_templates[i];
        if (emitted > 0) kn_append(buf, 262144, &off, ",\n");
        kn_append(buf, 262144, &off, "  \"");
        kn_append(buf, 262144, &off, t->code);
        kn_append(buf, 262144, &off, "\": {\n");
        kn_append(buf, 262144, &off, "    \"title\": \"");
        append_json_escaped(buf, 262144, &off, t->title ? t->title : "");
        kn_append(buf, 262144, &off, "\",\n");
        kn_append(buf, 262144, &off, "    \"detail\": \"");
        append_json_escaped(buf, 262144, &off, t->detail ? t->detail : "");
        kn_append(buf, 262144, &off, "\"\n");
        kn_append(buf, 262144, &off, "  }");
        emitted++;
    }

    for (size_t i = 0; i < sizeof(g_ui_templates) / sizeof(g_ui_templates[0]); i++)
    {
        if (emitted > 0) kn_append(buf, 262144, &off, ",\n");
        kn_append(buf, 262144, &off, "  \"");
        kn_append(buf, 262144, &off, g_ui_templates[i].key);
        kn_append(buf, 262144, &off, "\": { \"text\": \"");
        append_json_escaped(buf, 262144, &off, g_ui_templates[i].en ? g_ui_templates[i].en : "");
        kn_append(buf, 262144, &off, "\" }");
        emitted++;
    }

    kn_append(buf, 262144, &off, "\n}\n");

    KN_HANDLE h = CreateFileA(out_path, KN_GENERIC_WRITE, 0, 0, KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == KN_INVALID_HANDLE_VALUE)
    {
        kn_free(buf);
        return -1;
    }
    KN_DWORD wrote = 0;
    int ok = WriteFile(h, buf, (KN_DWORD)off, &wrote, 0) && wrote == (KN_DWORD)off;
    CloseHandle(h);
    kn_free(buf);
    return ok ? 0 : -1;
}

void kn_diag_report(const KnSource *src, KnDiagStage stage, int line, int col, int len,
                    const char *title, const char *detail, const char *got)
{
    if (!emit_diag(src, stage, DIAG_SEV_ERROR, line, col, len, title, detail, got))
        return;
    g_error_count++;

    if (g_error_count >= g_max_errors)
    {
        if (!g_quiet)
        {
            const char *msg = 0;
            if (stage == KN_STAGE_PARSER)
                msg = locale_or_fallback("ui.diag.too_many_parser_errors",
                                         "[Diag] too many parser errors, stopping early");
            else
                msg = locale_or_fallback("ui.diag.too_many_errors",
                                         "[Diag] too many errors, stopping early");
            kn_write_str(msg);
            kn_write_str("\n");
            kn_diag_print_summary();
            ExitProcess(1);
        }
    }
}

void kn_diag_warn(const KnSource *src, KnDiagStage stage, int warn_level, int line, int col, int len,
                  const char *title, const char *detail)
{
    if (!kn_diag_warning_enabled(warn_level))
        return;

    if (g_warn_as_error)
    {
        kn_diag_report(src, stage, line, col, len, title, detail, 0);
        return;
    }

    if (!emit_diag(src, stage, DIAG_SEV_WARNING, line, col, len, title, detail, 0))
        return;
    g_warning_count++;
}



