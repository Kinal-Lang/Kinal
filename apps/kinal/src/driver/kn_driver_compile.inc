static bool func_attr_name_is(const Attribute *a, const char *name)
{
    return a && a->name && name && kn_strcmp(a->name, name) == 0;
}

static bool path_is_absolute_host(const char *path)
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

static bool resolve_source_relative_link_file(char *out, size_t cap, const Func *f, const char *value)
{
    if (!out || cap == 0 || !value || !value[0])
        return false;

    char joined[KN_MAX_PATH];
    const char *candidate = value;
    if (!path_is_absolute_host(value) && f && f->src && f->src->path && f->src->path[0])
    {
        char base_dir[KN_MAX_PATH];
        path_dirname(f->src->path, base_dir, sizeof(base_dir));
        if (path_join(joined, sizeof(joined), base_dir, value))
            candidate = joined;
    }

    if (path_full(out, cap, candidate))
        return true;
    copy_str(out, cap, candidate);
    return true;
}

static bool link_target_spec_matches(const char *spec, const char *triple)
{
    if (!spec || !spec[0] || !triple || !triple[0])
        return false;

    if (str_eq_ci(spec, triple))
        return true;

    if (str_eq_ci(spec, "windows"))
        return target_is_windows(triple);
    if (str_eq_ci(spec, "linux"))
        return target_is_linux(triple);
    if (str_eq_ci(spec, "macos") || str_eq_ci(spec, "darwin"))
        return target_is_macos(triple);
    if (str_eq_ci(spec, "freestanding") || str_eq_ci(spec, "bare"))
        return target_is_freestanding(triple);

    if (str_eq_ci(spec, "win64") || str_eq_ci(spec, "win-x64"))
        return target_is_windows(triple) && !target_is_x86(triple) && !target_is_arm64(triple);
    if (str_eq_ci(spec, "win86") || str_eq_ci(spec, "win-x86"))
        return target_is_windows(triple) && target_is_x86(triple);
    if (str_eq_ci(spec, "win-arm64"))
        return target_is_windows(triple) && target_is_arm64(triple);

    if (str_eq_ci(spec, "linux64") || str_eq_ci(spec, "linux-x64"))
        return target_is_linux(triple) && !target_is_arm64(triple);
    if (str_eq_ci(spec, "linux-arm64"))
        return target_is_linux(triple) && target_is_arm64(triple);
    if (str_eq_ci(spec, "mac64") || str_eq_ci(spec, "macos-x64") || str_eq_ci(spec, "darwin-x64"))
        return target_is_macos(triple) && !target_is_arm64(triple);
    if (str_eq_ci(spec, "macos-arm64") || str_eq_ci(spec, "darwin-arm64"))
        return target_is_macos(triple) && target_is_arm64(triple);

    if (str_eq_ci(spec, "bare64"))
        return target_is_freestanding(triple) && !target_is_arm64(triple) && !target_is_x86(triple);
    if (str_eq_ci(spec, "bare-arm64"))
        return target_is_freestanding(triple) && target_is_arm64(triple);
    if (str_eq_ci(spec, "bare-x86"))
        return target_is_freestanding(triple) && target_is_x86(triple);

    return false;
}

static bool collect_extern_attr_link_items(const FuncList *funcs,
                                           KnLinkItem *items, int *count, int cap,
                                           char resolved_paths[][KN_MAX_PATH], int *resolved_path_count, int resolved_path_cap,
                                           const char *target_triple)
{
    if (!funcs || !items || !count || !resolved_paths || !resolved_path_count)
        return false;

    for (int i = 0; i < funcs->count; i++)
    {
        const Func *f = &funcs->items[i];
        if (!f->is_extern)
            continue;
        for (int a = 0; a < f->attrs.count; a++)
        {
            const Attribute *attr = &f->attrs.items[a];
            if (func_attr_name_is(attr, "LinkLib"))
            {
                for (int ai = 0; ai < attr->args.count; ai++)
                {
                    Expr *arg = attr->args.items[ai];
                    const char *value = (arg && arg->kind == EXPR_STRING) ? arg->v.str_val.ptr : 0;
                    if (!value || !value[0])
                        continue;
                    if (link_item_contains(items, *count, KN_LINK_ITEM_LIB_NAME, value))
                        continue;
                    if (!link_item_add(items, count, cap, KN_LINK_ITEM_LIB_NAME, value))
                        return false;
                }
                continue;
            }
            if (func_attr_name_is(attr, "LinkLibTarget"))
            {
                Expr *target_arg = attr->args.count > 0 ? attr->args.items[0] : 0;
                const char *target_spec = (target_arg && target_arg->kind == EXPR_STRING) ? target_arg->v.str_val.ptr : 0;
                if (!link_target_spec_matches(target_spec, target_triple))
                    continue;
                for (int ai = 1; ai < attr->args.count; ai++)
                {
                    Expr *arg = attr->args.items[ai];
                    const char *value = (arg && arg->kind == EXPR_STRING) ? arg->v.str_val.ptr : 0;
                    if (!value || !value[0])
                        continue;
                    if (link_item_contains(items, *count, KN_LINK_ITEM_LIB_NAME, value))
                        continue;
                    if (!link_item_add(items, count, cap, KN_LINK_ITEM_LIB_NAME, value))
                        return false;
                }
                continue;
            }
            if (func_attr_name_is(attr, "LinkFile"))
            {
                for (int ai = 0; ai < attr->args.count; ai++)
                {
                    Expr *arg = attr->args.items[ai];
                    const char *value = (arg && arg->kind == EXPR_STRING) ? arg->v.str_val.ptr : 0;
                    char resolved[KN_MAX_PATH];
                    if (!value || !value[0])
                        continue;
                    if (!resolve_source_relative_link_file(resolved, sizeof(resolved), f, value))
                        return false;
                    if (link_item_contains(items, *count, KN_LINK_ITEM_FILE, resolved))
                        continue;
                    if (*resolved_path_count >= resolved_path_cap)
                        return false;
                    copy_str(resolved_paths[*resolved_path_count], KN_MAX_PATH, resolved);
                    if (!link_item_add(items, count, cap, KN_LINK_ITEM_FILE, resolved_paths[*resolved_path_count]))
                        return false;
                    (*resolved_path_count)++;
                }
                continue;
            }
            if (func_attr_name_is(attr, "LinkFileTarget"))
            {
                Expr *target_arg = attr->args.count > 0 ? attr->args.items[0] : 0;
                const char *target_spec = (target_arg && target_arg->kind == EXPR_STRING) ? target_arg->v.str_val.ptr : 0;
                if (!link_target_spec_matches(target_spec, target_triple))
                    continue;
                for (int ai = 1; ai < attr->args.count; ai++)
                {
                    Expr *arg = attr->args.items[ai];
                    const char *value = (arg && arg->kind == EXPR_STRING) ? arg->v.str_val.ptr : 0;
                    char resolved[KN_MAX_PATH];
                    if (!value || !value[0])
                        continue;
                    if (!resolve_source_relative_link_file(resolved, sizeof(resolved), f, value))
                        return false;
                    if (link_item_contains(items, *count, KN_LINK_ITEM_FILE, resolved))
                        continue;
                    if (*resolved_path_count >= resolved_path_cap)
                        return false;
                    copy_str(resolved_paths[*resolved_path_count], KN_MAX_PATH, resolved);
                    if (!link_item_add(items, count, cap, KN_LINK_ITEM_FILE, resolved_paths[*resolved_path_count]))
                        return false;
                    (*resolved_path_count)++;
                }
            }
        }
    }
    return true;
}

static int sema_info_os_from_triple(const char *triple)
{
    if (target_is_windows(triple))
        return KN_INFO_OS_WINDOWS;
    if (target_is_linux(triple))
        return KN_INFO_OS_LINUX;
    if (target_is_macos(triple))
        return KN_INFO_OS_MACOS;
    return KN_INFO_OS_UNKNOWN;
}

static int sema_info_arch_from_triple(const char *triple)
{
    if (target_is_arm64(triple))
        return KN_INFO_ARCH_ARM64;
    if (target_is_x86(triple))
        return KN_INFO_ARCH_X86;
    if (triple && (str_contains_ci(triple, "x86_64") || str_contains_ci(triple, "amd64")))
        return KN_INFO_ARCH_X64;
    return KN_INFO_ARCH_UNKNOWN;
}

static int sema_info_runtime_backend_from_cfg(const KnCompileConfig *cfg)
{
    if (!cfg) return KN_INFO_RUNTIME_NATIVE;
    if (cfg->emit_mode == KN_EMIT_KNC || cfg->emit_mode == KN_EMIT_KNC_EXE)
        return KN_INFO_RUNTIME_VM;
    return KN_INFO_RUNTIME_NATIVE;
}

static int sema_info_env_from_cfg(int env_kind)
{
    return env_kind == KN_ENV_FREESTANDING ? KN_INFO_ENV_FREESTANDING : KN_INFO_ENV_HOSTED;
}

static int sema_info_pointer_bits_from_triple(const char *triple)
{
    if (target_is_x86(triple))
        return 32;
    if (target_is_windows(triple) || target_is_linux(triple) || target_is_arm64(triple) ||
        (triple && (str_contains_ci(triple, "x86_64") || str_contains_ci(triple, "amd64") ||
                    str_contains_ci(triple, "aarch64"))))
        return 64;
    return 0;
}

static const char *sema_info_path_separator_from_triple(const char *triple)
{
    return target_is_windows(triple) ? "\\" : "/";
}

static const char *sema_info_new_line_from_triple(const char *triple)
{
    return target_is_windows(triple) ? "\r\n" : "\n";
}

static const char *sema_info_exe_suffix_from_triple(const char *triple)
{
    return target_is_windows(triple) ? ".exe" : "";
}

static const char *sema_info_object_suffix_from_triple(const char *triple)
{
    return target_is_windows(triple) ? ".obj" : ".o";
}

static const char *sema_info_dynamic_library_suffix_from_triple(const char *triple)
{
    if (target_is_windows(triple)) return ".dll";
    if (target_is_macos(triple)) return ".dylib";
    return ".so";
}

static const char *sema_info_static_library_suffix_from_triple(const char *triple)
{
    return target_is_windows(triple) ? ".lib" : ".a";
}

static int sema_info_host_os(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return KN_INFO_OS_WINDOWS;
#elif defined(__linux__)
    return KN_INFO_OS_LINUX;
#elif defined(__APPLE__)
    return KN_INFO_OS_MACOS;
#else
    return KN_INFO_OS_UNKNOWN;
#endif
}

static int sema_info_host_arch(void)
{
#if defined(_M_IX86) || defined(__i386__)
    return KN_INFO_ARCH_X86;
#elif defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return KN_INFO_ARCH_X64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return KN_INFO_ARCH_ARM64;
#else
    return KN_INFO_ARCH_UNKNOWN;
#endif
}

static int sema_info_host_pointer_bits(void)
{
#if defined(_M_IX86) || defined(__i386__)
    return 32;
#elif defined(_WIN64) || defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) || defined(_M_ARM64)
    return 64;
#else
    return (int)(sizeof(void *) * 8);
#endif
}

static const char *sema_info_host_path_separator(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return "\\";
#else
    return "/";
#endif
}

static const char *sema_info_host_new_line(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return "\r\n";
#else
    return "\n";
#endif
}

static const char *sema_info_host_exe_suffix(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ".exe";
#else
    return "";
#endif
}

static const char *sema_info_host_object_suffix(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ".obj";
#else
    return ".o";
#endif
}

static const char *sema_info_host_dynamic_library_suffix(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ".dll";
#else
    return ".so";
#endif
}

static const char *sema_info_host_static_library_suffix(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return ".lib";
#else
    return ".a";
#endif
}

static void compile_init_link_options(KnLinkOptions *out,
                                      const KnCompileConfig *cfg,
                                      const char *obj_path,
                                      const KnLinkItem *items,
                                      int item_count,
                                      int include_runtime,
                                      const char *entry_symbol)
{
    kn_memset(out, 0, sizeof(*out));
    out->obj_path = obj_path;
    out->out_path = cfg->out_path;
    out->linker_path = cfg->linker_path;
    out->items = items;
    out->item_count = item_count;
    out->target_triple = cfg->target_triple;
    out->linker_kind = cfg->linker_kind;
    out->artifact_kind = cfg->artifact_kind;
    out->include_runtime = include_runtime;
    out->lto_mode = cfg->lto_mode;
    out->env_kind = cfg->env_kind;
    out->runtime_mode = cfg->runtime_mode;
    out->panic_mode = cfg->panic_mode;
    out->crt_mode = cfg->crt_mode;
    out->no_crt = cfg->no_crt;
    out->no_default_libs = cfg->no_default_libs;
    out->entry_symbol = entry_symbol;
    out->link_script = cfg->link_script;
    out->trace = cfg->trace;
    out->show_link = cfg->show_link;
    out->show_link_search = cfg->show_link_search;
}

static void compile_init_sema_options(KnSemaOptions *out,
                                      const KnCompileConfig *cfg,
                                      int require_entry,
                                      const char *entry_name)
{
    kn_memset(out, 0, sizeof(*out));
    out->require_entry = require_entry;
    out->env_kind = cfg->env_kind;
    out->entry_name = entry_name;
    out->target_os = sema_info_os_from_triple(cfg->target_triple);
    out->target_arch = sema_info_arch_from_triple(cfg->target_triple);
    out->target_env = sema_info_env_from_cfg(cfg->env_kind);
    out->target_pointer_bits = sema_info_pointer_bits_from_triple(cfg->target_triple);
    out->target_path_separator = sema_info_path_separator_from_triple(cfg->target_triple);
    out->target_new_line = sema_info_new_line_from_triple(cfg->target_triple);
    out->target_exe_suffix = sema_info_exe_suffix_from_triple(cfg->target_triple);
    out->target_object_suffix = sema_info_object_suffix_from_triple(cfg->target_triple);
    out->target_dynamic_library_suffix = sema_info_dynamic_library_suffix_from_triple(cfg->target_triple);
    out->target_static_library_suffix = sema_info_static_library_suffix_from_triple(cfg->target_triple);
    out->host_os = sema_info_host_os();
    out->host_arch = sema_info_host_arch();
    out->host_env = KN_INFO_ENV_HOSTED;
    out->host_pointer_bits = sema_info_host_pointer_bits();
    out->host_path_separator = sema_info_host_path_separator();
    out->host_new_line = sema_info_host_new_line();
    out->host_exe_suffix = sema_info_host_exe_suffix();
    out->host_object_suffix = sema_info_host_object_suffix();
    out->host_dynamic_library_suffix = sema_info_host_dynamic_library_suffix();
    out->host_static_library_suffix = sema_info_host_static_library_suffix();
    out->runtime_backend = sema_info_runtime_backend_from_cfg(cfg);
}

static void compile_init_codegen_options(KnCodegenOptions *out,
                                         const KnCompileConfig *cfg,
                                         int emit_kind,
                                         int emit_entry,
                                         int emit_pic,
                                         const char *entry_name)
{
    kn_memset(out, 0, sizeof(*out));
    out->target_triple = cfg->target_triple;
    out->emit_kind = emit_kind;
    out->emit_entry = emit_entry;
    out->emit_pic = emit_pic;
    out->env_kind = cfg->env_kind;
    out->runtime_mode = cfg->runtime_mode;
    out->panic_mode = cfg->panic_mode;
    out->entry_name = entry_name;
}

static void compile_append_uint(char *buf, size_t cap, size_t *off, unsigned value)
{
    char digits[16];
    int count = 0;
    do
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && count < (int)sizeof(digits));
    while (count > 0)
    {
        char one[2] = { digits[--count], 0 };
        kn_append(buf, cap, off, one);
    }
}

static void compile_append_summary_count(char *buf, size_t cap, size_t *off,
                                         const char *name, int value)
{
    kn_append(buf, cap, off, name);
    kn_append(buf, cap, off, "=");
    compile_append_uint(buf, cap, off, (unsigned)(value > 0 ? value : 0));
    kn_append(buf, cap, off, "\n");
}

typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
    int failed;
} CompileTextBuffer;

static int compile_text_reserve(CompileTextBuffer *buf, size_t extra)
{
    if (!buf || buf->failed)
        return 0;
    size_t required = buf->length + extra + 1;
    if (required <= buf->capacity)
        return 1;
    size_t capacity = buf->capacity ? buf->capacity : 4096;
    while (capacity < required)
    {
        if (capacity > ((size_t)-1) / 2)
        {
            buf->failed = 1;
            return 0;
        }
        capacity *= 2;
    }
    char *next = (char *)kn_malloc(capacity);
    if (!next)
    {
        buf->failed = 1;
        return 0;
    }
    if (buf->data && buf->length)
        kn_memcpy(next, buf->data, buf->length);
    if (buf->data)
        kn_free(buf->data);
    buf->data = next;
    buf->capacity = capacity;
    buf->data[buf->length] = 0;
    return 1;
}

static void compile_text_append_n(CompileTextBuffer *buf, const char *text, size_t length)
{
    if (!text || !length || !compile_text_reserve(buf, length))
        return;
    kn_memcpy(buf->data + buf->length, text, length);
    buf->length += length;
    buf->data[buf->length] = 0;
}

static void compile_text_append(CompileTextBuffer *buf, const char *text)
{
    if (text)
        compile_text_append_n(buf, text, kn_strlen(text));
}

static void compile_text_append_uint(CompileTextBuffer *buf, unsigned value)
{
    char digits[16];
    int count = 0;
    do
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && count < (int)sizeof(digits));
    while (count > 0)
        compile_text_append_n(buf, &digits[--count], 1);
}

static void compile_text_append_hex(CompileTextBuffer *buf, const char *text, size_t length)
{
    static const char hex[] = "0123456789ABCDEF";
    if (!compile_text_reserve(buf, length * 2))
        return;
    for (size_t i = 0; i < length; i++)
    {
        unsigned value = (unsigned)(unsigned char)text[i];
        buf->data[buf->length++] = hex[(value >> 4) & 15u];
        buf->data[buf->length++] = hex[value & 15u];
    }
    buf->data[buf->length] = 0;
}

static int compile_write_token_summary(const char *out_path, const StrList *inputs)
{
    CompileTextBuffer output;
    kn_memset(&output, 0, sizeof(output));
    compile_text_append(&output, "format=kinal-tokens-v1\n");
    compile_text_append(&output, "sources=");
    compile_text_append_uint(&output, (unsigned)(inputs ? inputs->count : 0));
    compile_text_append(&output, "\n");

    for (int i = 0; inputs && i < inputs->count && !output.failed; i++)
    {
        size_t source_size = 0;
        char *source = kn_read_file(inputs->items[i], &source_size);
        if (!source)
        {
            output.failed = 1;
            break;
        }
        compile_text_append(&output, "source=");
        compile_text_append_uint(&output, (unsigned)i);
        compile_text_append(&output, "\n");

        Lexer lexer;
        lex_init(&lexer, source);
        for (;;)
        {
            Token token = lex_next(&lexer);
            compile_text_append_uint(&output, (unsigned)token.type);
            compile_text_append(&output, "\t");
            compile_text_append_uint(&output, (unsigned)token.line);
            compile_text_append(&output, "\t");
            compile_text_append_uint(&output, (unsigned)token.col);
            compile_text_append(&output, "\t");
            compile_text_append_hex(&output, token.start, token.length);
            compile_text_append(&output, "\n");
            if (token.type == TOK_EOF)
                break;
        }
        kn_free(source);
    }

    int result = -1;
    if (!output.failed)
    {
        KN_HANDLE handle = CreateFileA(out_path, KN_GENERIC_WRITE, 0, 0,
                                       KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
        if (handle && handle != KN_INVALID_HANDLE_VALUE)
        {
            KN_DWORD written = 0;
            int ok = WriteFile(handle, output.data, (KN_DWORD)output.length, &written, 0) &&
                     written == (KN_DWORD)output.length;
            CloseHandle(handle);
            result = ok ? 0 : -1;
        }
    }
    if (output.data)
        kn_free(output.data);
    return result;
}

static const char *compile_syntax_unit(const FuncList *funcs,
                                       const ImportList *imports,
                                       const ClassList *classes,
                                       const InterfaceList *interfaces,
                                       const StructList *structs,
                                       const EnumList *enums,
                                       const StmtList *globals)
{
    if (funcs && funcs->count > 0) return funcs->items[0].unit;
    if (imports && imports->count > 0) return imports->items[0].unit;
    if (classes && classes->count > 0) return classes->items[0].unit;
    if (interfaces && interfaces->count > 0) return interfaces->items[0].unit;
    if (structs && structs->count > 0) return structs->items[0].unit;
    if (enums && enums->count > 0) return enums->items[0].unit;
    if (globals && globals->count > 0 && globals->items[0] && globals->items[0]->kind == ST_VAR)
        return globals->items[0]->v.var.unit;
    return "";
}

static ModulePath *compile_find_import_module(ModuleMap *map, const char *qualified)
{
    if (!map || !qualified || !qualified[0])
        return 0;
    ModulePath *found = modulemap_find(map, qualified);
    if (found)
        return found;

    size_t length = kn_strlen(qualified);
    char *candidate = (char *)kn_malloc(length + 1);
    if (!candidate)
        kn_die("out of memory");
    kn_memcpy(candidate, qualified, length + 1);
    while (length > 0)
    {
        while (length > 0 && candidate[length - 1] != '.')
            length--;
        if (length == 0)
            break;
        candidate[length - 1] = 0;
        found = modulemap_find(map, candidate);
        if (found)
            break;
        length--;
    }
    kn_free(candidate);
    return found;
}

static int compile_program_has_unit(const char *unit,
                                    const MetaList *metas,
                                    const FuncList *funcs,
                                    const ClassList *classes,
                                    const InterfaceList *interfaces,
                                    const StructList *structs,
                                    const EnumList *enums,
                                    const StmtList *globals)
{
    if (!unit || !unit[0])
        return 0;
    for (int i = 0; metas && i < metas->count; i++)
        if (metas->items[i].unit && kn_strcmp(metas->items[i].unit, unit) == 0) return 1;
    for (int i = 0; funcs && i < funcs->count; i++)
        if (funcs->items[i].unit && kn_strcmp(funcs->items[i].unit, unit) == 0) return 1;
    for (int i = 0; classes && i < classes->count; i++)
        if (classes->items[i].unit && kn_strcmp(classes->items[i].unit, unit) == 0) return 1;
    for (int i = 0; interfaces && i < interfaces->count; i++)
        if (interfaces->items[i].unit && kn_strcmp(interfaces->items[i].unit, unit) == 0) return 1;
    for (int i = 0; structs && i < structs->count; i++)
        if (structs->items[i].unit && kn_strcmp(structs->items[i].unit, unit) == 0) return 1;
    for (int i = 0; enums && i < enums->count; i++)
        if (enums->items[i].unit && kn_strcmp(enums->items[i].unit, unit) == 0) return 1;
    for (int i = 0; globals && i < globals->count; i++)
    {
        Stmt *global = globals->items[i];
        if (global && global->kind == ST_VAR && global->v.var.unit &&
            kn_strcmp(global->v.var.unit, unit) == 0) return 1;
    }
    return 0;
}

static int compile_program_has_type_qname(const char *qname,
                                          const ClassList *classes,
                                          const InterfaceList *interfaces,
                                          const StructList *structs,
                                          const EnumList *enums)
{
    if (!qname || !qname[0])
        return 0;
    for (int i = 0; classes && i < classes->count; i++)
        if ((classes->items[i].qname && kn_strcmp(classes->items[i].qname, qname) == 0) ||
            (classes->items[i].name && kn_strcmp(classes->items[i].name, qname) == 0)) return 1;
    for (int i = 0; interfaces && i < interfaces->count; i++)
        if ((interfaces->items[i].qname && kn_strcmp(interfaces->items[i].qname, qname) == 0) ||
            (interfaces->items[i].name && kn_strcmp(interfaces->items[i].name, qname) == 0)) return 1;
    for (int i = 0; structs && i < structs->count; i++)
        if ((structs->items[i].qname && kn_strcmp(structs->items[i].qname, qname) == 0) ||
            (structs->items[i].name && kn_strcmp(structs->items[i].name, qname) == 0)) return 1;
    for (int i = 0; enums && i < enums->count; i++)
        if ((enums->items[i].qname && kn_strcmp(enums->items[i].qname, qname) == 0) ||
            (enums->items[i].name && kn_strcmp(enums->items[i].name, qname) == 0)) return 1;
    return 0;
}

static void compile_resolve_package_symbol_imports(ImportList *imports,
                                                   const MetaList *metas,
                                                   const FuncList *funcs,
                                                   const ClassList *classes,
                                                   const InterfaceList *interfaces,
                                                   const StructList *structs,
                                                   const EnumList *enums,
                                                   const StmtList *globals)
{
    for (int i = 0; imports && i < imports->count; i++)
    {
        ImportMap *import = &imports->items[i];
        if ((import->kind != IMPORT_OPEN && import->kind != IMPORT_ALIAS) ||
            !import->module || !import->module[0])
            continue;
        if (compile_program_has_unit(import->module, metas, funcs, classes, interfaces,
                                     structs, enums, globals) ||
            compile_program_has_type_qname(import->module, classes, interfaces, structs, enums) ||
            kn_std_module_has(import->module))
            continue;

        const char *last = 0;
        for (const char *p = import->module; *p; p++)
            if (*p == '.') last = p;
        if (!last || last == import->module || !last[1])
            continue;

        size_t module_length = (size_t)(last - import->module);
        char *module = (char *)kn_malloc(module_length + 1);
        if (!module)
            kn_die("out of memory");
        kn_memcpy(module, import->module, module_length);
        module[module_length] = 0;
        if (!compile_program_has_unit(module, metas, funcs, classes, interfaces,
                                      structs, enums, globals) &&
            !kn_std_module_has(module))
        {
            kn_free(module);
            continue;
        }

        const char *member = last + 1;
        if (import->kind == IMPORT_OPEN)
        {
            import->local = member;
            import->remote = member;
            import->global = 0;
        }
        else
        {
            import->remote = member;
            import->global = 1;
        }
        import->kind = IMPORT_SYMBOL;
        import->module = module;
    }
}

static int compile_write_syntax_summary(const char *out_path,
                                        const SourceList *sources,
                                        const FuncList *funcs,
                                        const ImportList *imports,
                                        const ClassList *classes,
                                        const InterfaceList *interfaces,
                                        const StructList *structs,
                                        const EnumList *enums,
                                        const StmtList *globals)
{
    char text[1024];
    size_t off = 0;
    text[0] = 0;
    kn_append(text, sizeof(text), &off, "format=kinal-syntax-v1\n");
    compile_append_summary_count(text, sizeof(text), &off, "sources", sources ? sources->count : 0);
    kn_append(text, sizeof(text), &off, "unit=");
    kn_append(text, sizeof(text), &off,
              compile_syntax_unit(funcs, imports, classes, interfaces, structs, enums, globals));
    kn_append(text, sizeof(text), &off, "\n");
    compile_append_summary_count(text, sizeof(text), &off, "imports", imports ? imports->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "functions", funcs ? funcs->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "types",
                                 (classes ? classes->count : 0) +
                                 (interfaces ? interfaces->count : 0) +
                                 (structs ? structs->count : 0) +
                                 (enums ? enums->count : 0));
    int field_count = 0;
    int method_count = 0;
    if (classes)
    {
        for (int i = 0; i < classes->count; i++)
        {
            field_count += classes->items[i].fields.count;
            method_count += classes->items[i].methods.count;
        }
    }
    if (interfaces)
    {
        for (int i = 0; i < interfaces->count; i++)
            method_count += interfaces->items[i].methods.count;
    }
    if (structs)
    {
        for (int i = 0; i < structs->count; i++)
            field_count += structs->items[i].fields.count;
    }
    compile_append_summary_count(text, sizeof(text), &off, "fields", field_count);
    compile_append_summary_count(text, sizeof(text), &off, "methods", method_count);
    compile_append_summary_count(text, sizeof(text), &off, "globals", globals ? globals->count : 0);

    KN_HANDLE handle = CreateFileA(out_path, KN_GENERIC_WRITE, 0, 0,
                                   KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!handle || handle == KN_INVALID_HANDLE_VALUE)
        return -1;
    KN_DWORD written = 0;
    int ok = WriteFile(handle, text, (KN_DWORD)off, &written, 0) && written == (KN_DWORD)off;
    CloseHandle(handle);
    return ok ? 0 : -1;
}

static int compile_write_sema_summary(const char *out_path,
                                      const KnCompileConfig *cfg,
                                      const SourceList *sources,
                                      const MetaList *metas,
                                      const FuncList *funcs,
                                      const ImportList *imports,
                                      const ClassList *classes,
                                      const InterfaceList *interfaces,
                                      const StructList *structs,
                                      const EnumList *enums,
                                      const StmtList *globals)
{
    char text[4096];
    size_t off = 0;
    text[0] = 0;
    kn_append(text, sizeof(text), &off, "format=kinal-sema-v1\n");
    kn_append(text, sizeof(text), &off, "target=");
    kn_append(text, sizeof(text), &off, cfg->target_triple ? cfg->target_triple : "");
    kn_append(text, sizeof(text), &off, "\n");
    compile_append_summary_count(text, sizeof(text), &off, "pointer_bits", sema_info_pointer_bits_from_triple(cfg->target_triple));
    kn_append(text, sizeof(text), &off, "environment=");
    kn_append(text, sizeof(text), &off, cfg->env_kind == KN_ENV_FREESTANDING ? "freestanding\n" : "hosted\n");
    compile_append_summary_count(text, sizeof(text), &off, "sources", sources ? sources->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "metas", metas ? metas->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "functions", funcs ? funcs->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "imports", imports ? imports->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "classes", classes ? classes->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "interfaces", interfaces ? interfaces->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "structs", structs ? structs->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "enums", enums ? enums->count : 0);
    compile_append_summary_count(text, sizeof(text), &off, "globals", globals ? globals->count : 0);
    {
        KnHirStats hir;
        kn_hir_collect_stats(funcs, classes, interfaces, globals, &hir);
        kn_append(text, sizeof(text), &off, "hir_format=kinal-call-hir-v1\n");
        kn_append(text, sizeof(text), &off, "binary_hir_format=kinal-binary-hir-v1\n");
        compile_append_summary_count(text, sizeof(text), &off, "hir_expressions", (int)hir.expressions);
        compile_append_summary_count(text, sizeof(text), &off, "hir_typed_expressions", (int)hir.typed_expressions);
        compile_append_summary_count(text, sizeof(text), &off, "hir_calls", (int)hir.calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_unresolved_calls", (int)hir.unresolved_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_builtin_calls", (int)hir.builtin_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_function_calls", (int)hir.function_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_delegate_calls", (int)hir.delegate_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_static_method_calls", (int)hir.static_method_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_direct_method_calls", (int)hir.direct_method_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_virtual_method_calls", (int)hir.virtual_method_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_interface_method_calls", (int)hir.interface_method_calls);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binaries", (int)hir.binaries);
        compile_append_summary_count(text, sizeof(text), &off, "hir_unresolved_binaries", (int)hir.unresolved_binaries);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_numeric_arithmetic", (int)hir.binary_numeric_arithmetic);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_string_concat", (int)hir.binary_string_concat);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_pointer_arithmetic", (int)hir.binary_pointer_arithmetic);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_bitwise", (int)hir.binary_bitwise);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_string_equality", (int)hir.binary_string_equality);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_reference_equality", (int)hir.binary_reference_equality);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_scalar_equality", (int)hir.binary_scalar_equality);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_numeric_comparison", (int)hir.binary_numeric_comparison);
        compile_append_summary_count(text, sizeof(text), &off, "hir_binary_logical_short_circuit", (int)hir.binary_logical_short_circuit);
    }

    KN_HANDLE handle = CreateFileA(out_path, KN_GENERIC_WRITE, 0, 0,
                                   KN_CREATE_ALWAYS, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!handle || handle == KN_INVALID_HANDLE_VALUE)
        return -1;
    KN_DWORD written = 0;
    int ok = WriteFile(handle, text, (KN_DWORD)off, &written, 0) && written == (KN_DWORD)off;
    CloseHandle(handle);
    return ok ? 0 : -1;
}

static void compile_free_sources(SourceList *sources)
{
    if (!sources)
        return;
    for (int i = 0; i < sources->count; i++)
    {
        KnSource *source = sources->items[i];
        if (!source) continue;
        kn_source_free(source);
        if (source->text)
            kn_free((void *)source->text);
        kn_free(source);
    }
    sources->count = 0;
}

static int compile_non_kinal_inputs(const StrList *inputs,
                                    const KnCompileConfig *cfg,
                                    int include_runtime,
                                    const char *link_entry_symbol)
{
    const char *out_path = cfg->out_path;
    if (cfg->emit_mode == KN_EMIT_CHECK || cfg->emit_mode == KN_EMIT_TOKENS ||
        cfg->emit_mode == KN_EMIT_AST)
    {
        kn_write_str("frontend emit requires at least one Kinal source input\n");
        return -1;
    }
    if (cfg->emit_mode == KN_EMIT_OBJECT)
    {
        if (inputs->count != 1)
        {
            kn_write_str("object emit from non-.fx inputs requires exactly one input\n");
            return -1;
        }
        if (compile_intermediate_to_obj(inputs->items[0], out_path, cfg->target_triple, cfg->lto_mode,
                                        cfg->artifact_kind == KN_ARTIFACT_SHARED_LIB) != 0)
        {
            kn_write_str("failed to compile intermediate input to object\n");
            return -1;
        }
        return 0;
    }
    if (cfg->emit_mode == KN_EMIT_ASSEMBLY)
    {
        if (inputs->count != 1 || classify_input(inputs->items[0]) != KN_INPUT_LL)
        {
            kn_write_str("assembly emit from non-.fx inputs currently supports only one .ll input\n");
            return -1;
        }
        if (compile_ll_to_asm(inputs->items[0], out_path, cfg->target_triple) != 0)
        {
            kn_write_str("failed to compile .ll to assembly\n");
            return -1;
        }
        return 0;
    }
    if (cfg->emit_mode == KN_EMIT_LLVM_IR)
    {
        if (inputs->count != 1 || classify_input(inputs->items[0]) != KN_INPUT_LL)
        {
            kn_write_str("llvm ir emit from non-.fx inputs currently supports only one .ll input\n");
            return -1;
        }
        if (!copy_file_overwrite(inputs->items[0], out_path))
        {
            kn_write_str("failed to copy .ll output\n");
            return -1;
        }
        return 0;
    }

    const char *link_inputs[256];
    int link_inputs_count = 0;
    char temp_objs[64][KN_MAX_PATH];
    int temp_count = 0;
    for (int i = 0; i < inputs->count; i++)
    {
        const char *path = inputs->items[i];
        KnInputKind kind = classify_input(path);
        if (kind == KN_INPUT_OBJ)
        {
            if (link_inputs_count < (int)(sizeof(link_inputs) / sizeof(link_inputs[0])))
                link_inputs[link_inputs_count++] = path;
            continue;
        }
        if (kind != KN_INPUT_ASM && kind != KN_INPUT_LL)
            continue;
        if (temp_count >= (int)(sizeof(temp_objs) / sizeof(temp_objs[0])))
        {
            kn_write_str("too many intermediate inputs\n");
            return -1;
        }
        int use_bc = (kind == KN_INPUT_LL && cfg->lto_mode != KN_LTO_OFF && cfg->linker_kind == KN_LINKER_LLD);
        if (use_bc)
        {
            if (!derive_temp_bc_input_path(out_path, temp_count + 1, temp_objs[temp_count], sizeof(temp_objs[temp_count])))
                return -1;
            if (compile_ir_to_bc(path, temp_objs[temp_count]) != 0)
            {
                kn_write_str("failed to compile intermediate llvm ir to bitcode\n");
                return -1;
            }
        }
        else
        {
            if (!derive_temp_obj_path(out_path, cfg->target_triple, temp_count + 1, temp_objs[temp_count], sizeof(temp_objs[temp_count])))
                return -1;
            if (compile_intermediate_to_obj(path, temp_objs[temp_count], cfg->target_triple, cfg->lto_mode,
                                            cfg->artifact_kind == KN_ARTIFACT_SHARED_LIB) != 0)
            {
                kn_write_str("failed to compile intermediate input to object\n");
                return -1;
            }
        }
        if (link_inputs_count < (int)(sizeof(link_inputs) / sizeof(link_inputs[0])))
            link_inputs[link_inputs_count++] = temp_objs[temp_count];
        temp_count++;
    }

    if (link_inputs_count == 0)
    {
        kn_write_str("no object-like inputs available for linking\n");
        return -1;
    }
    KnLinkItem extra_items[512];
    int extra_count = 0;
    for (int i = 1; i < link_inputs_count && extra_count < (int)(sizeof(extra_items) / sizeof(extra_items[0])); i++)
        link_item_add(extra_items, &extra_count, (int)(sizeof(extra_items) / sizeof(extra_items[0])), KN_LINK_ITEM_FILE, link_inputs[i]);
    for (int i = 0; i < cfg->link_item_count && extra_count < (int)(sizeof(extra_items) / sizeof(extra_items[0])); i++)
        link_item_add(extra_items, &extra_count, (int)(sizeof(extra_items) / sizeof(extra_items[0])), cfg->link_items[i].kind, cfg->link_items[i].value);

    if (cfg->show_link_search)
        dump_link_search_dirs(extra_items, extra_count);

    if (target_is_windows(cfg->target_triple) &&
        cfg->artifact_kind == KN_ARTIFACT_EXE &&
        cfg->linker_kind == KN_LINKER_LLD)
    {
        int direct_rc = link_intermediate_windows_lld(cfg, link_inputs[0], extra_items, extra_count);
        if (direct_rc == 0)
        {
            if (!cfg->keep_temps)
            {
                for (int i = 0; i < temp_count; i++)
                    try_delete_file(temp_objs[i]);
            }
            return 0;
        }
        if (direct_rc != -1)
        {
            kn_write_str("link failed\n");
            return -1;
        }
    }

    KnLinkOptions link_opts;
    compile_init_link_options(&link_opts, cfg, link_inputs[0], extra_items,
                              extra_count, include_runtime, link_entry_symbol);
    trace_log(cfg->trace, "compile:intermediate_link");
    if (cfg->trace)
    {
        kn_write_str("[Trace] intermediate obj: ");
        kn_write_str(link_opts.obj_path ? link_opts.obj_path : "<null>");
        kn_write_str("\n");
        kn_write_str("[Trace] intermediate out: ");
        kn_write_str(link_opts.out_path ? link_opts.out_path : "<null>");
        kn_write_str("\n");
    }
    if (invoke_linker(&link_opts) != 0)
    {
        kn_write_str("link failed\n");
        return -1;
    }
    if (!cfg->keep_temps)
    {
        for (int i = 0; i < temp_count; i++)
            try_delete_file(temp_objs[i]);
    }
    return 0;
}

static int kn_compile(int in_count, const char **in_paths, const KnCompileConfig *cfg)
{
    if (!cfg || !cfg->out_path || !cfg->target_triple)
        return -1;
    if (!ensure_output_parent(cfg->out_path))
    {
        kn_write_str("failed to create output directory for: ");
        kn_write_str(cfg->out_path);
        kn_write_str("\n");
        return -1;
    }

    // Keep the entry wrapper in executable intermediates as well so emitted
    // .ll/.s/.obj artifacts can be resumed and linked later without re-running
    // the frontend.
    const int need_entry = (cfg->artifact_kind == KN_ARTIFACT_EXE &&
                            cfg->emit_mode != KN_EMIT_CHECK &&
                            cfg->emit_mode != KN_EMIT_TOKENS &&
                            cfg->emit_mode != KN_EMIT_AST) ? 1 : 0;
    const int include_runtime = (cfg->env_kind == KN_ENV_HOSTED) ? 1 : 0;
    const char *entry_name = (cfg->entry_symbol && cfg->entry_symbol[0])
                                 ? cfg->entry_symbol
                                 : ((cfg->env_kind == KN_ENV_FREESTANDING) ? "KMain" : "Main");
    const char *link_entry_symbol = (cfg->env_kind == KN_ENV_FREESTANDING && need_entry)
                                        ? "__kn_entry"
                                        : entry_name;

    // Fast path for direct object linking to avoid unnecessary frontend work.
    if (cfg->emit_mode == KN_EMIT_LINK && in_count == 1 && in_paths && in_paths[0] &&
        classify_input(in_paths[0]) == KN_INPUT_OBJ && cfg->link_item_count == 0)
    {
        if (target_is_windows(cfg->target_triple) &&
            cfg->artifact_kind == KN_ARTIFACT_EXE &&
            cfg->linker_kind == KN_LINKER_LLD)
        {
            int direct_rc = link_intermediate_windows_lld(cfg, in_paths[0], 0, 0);
            if (direct_rc == 0)
                return 0;
            if (direct_rc != -1)
            {
                kn_write_str("link failed\n");
                return -1;
            }
        }

        KnLinkOptions link_opts;
        compile_init_link_options(&link_opts, cfg, in_paths[0], cfg->link_items,
                                  cfg->link_item_count, include_runtime, link_entry_symbol);
        if (invoke_linker(&link_opts) != 0)
        {
            kn_write_str("link failed\n");
            return -1;
        }
        return 0;
    }

    const char *out_path = cfg->out_path;
    int auto_link = cfg->auto_link;
    int trace = cfg->trace;
    int profile = cfg->profile;
    const KnLinkItem *cfg_link_items = cfg->link_items;
    int cfg_link_item_count = cfg->link_item_count;

    uint64_t t_all = now_ms();
    uint64_t p_parse = 0;
    uint64_t p_sema = 0;
    uint64_t p_codegen = 0;
    uint64_t p_link = 0;

    trace_log(trace, "compile:begin");

    StrList all_inputs = {0};
    StrList kn_inputs = {0};
    StrList intermediate_inputs = {0};
    for (int i = 0; i < in_count; i++)
    {
        const char *p = in_paths[i];
        if (!p || !p[0]) continue;
        strlist_add(&all_inputs, p);
        KnInputKind k = classify_input(p);
        if (k == KN_INPUT_FX)
            strlist_add(&kn_inputs, p);
        else if (k == KN_INPUT_OBJ || k == KN_INPUT_ASM || k == KN_INPUT_LL)
            strlist_add(&intermediate_inputs, p);
        else
        {
            kn_write_str("unsupported input file type: ");
            kn_write_str(p);
            kn_write_str("\n");
            return -1;
        }
    }
    trace_log(trace, "compile:inputs_ready");

    if (kn_inputs.count == 0)
    {
        trace_log(trace, "compile:nonfx");
        return compile_non_kinal_inputs(&all_inputs, cfg, include_runtime, link_entry_symbol);
    }

    if (cfg->emit_mode != KN_EMIT_LINK && intermediate_inputs.count > 0)
    {
        kn_write_str("mixing .fx with non-.fx inputs is only supported in link mode\n");
        return -1;
    }

    MetaList metas = {0};
    FuncList funcs = {0};
    ImportList imports = {0};
    ClassList classes = {0};
    InterfaceList interfaces = {0};
    StructList structs = {0};
    EnumList enums = {0};
    StmtList globals = {0};

    StrList inputs = {0};
    for (int i = 0; i < kn_inputs.count; i++)
        strlist_add(&inputs, kn_inputs.items[i]);

    ModuleMap modmap = {0};
    ModuleMap local_modmap = {0};
    ModuleMap dep_modmap = {0};
    ModuleMap official_modmap = {0};
    StrList known_standard_modules = {0};
    StrList module_seen = {0};
    const int project_source_mode = cfg->project_source_mode;
    const int use_project_local_map = cfg->project_local_sources && cfg->project_local_source_count > 0;
    const int resolve_import_modules =
        cfg->emit_mode != KN_EMIT_TOKENS && cfg->emit_mode != KN_EMIT_AST;
    /* Explicit standard-library imports remain available when local module
       auto-discovery is disabled. Standard modules are packages, not compiler
       builtins, so --no-module-discovery must not erase their declarations. */
    if (resolve_import_modules)
    {
        g_pkg_error = 0;
        if (!scan_package_roots_into_module_map(cfg->official_package_roots,
                                                cfg->official_package_root_count,
                                                &official_modmap, 1))
            return -1;
        if (cfg->official_package_root_count == 0)
            scan_default_stdlib_modules(&official_modmap);
        for (int module_index = 0; module_index < official_modmap.count; module_index++)
            strlist_add(&known_standard_modules, official_modmap.items[module_index].module);
    }
    if (auto_link && kn_inputs.count == 1 && inputs.count > 0)
    {
        if (use_project_local_map)
        {
            for (int psi = 0; psi < cfg->project_local_source_count; psi++)
            {
                if (!register_module_file_path(cfg->project_local_sources[psi], &local_modmap))
                    return -1;
            }
            if (!scan_package_roots_into_module_map(cfg->package_roots, cfg->package_root_count, &dep_modmap, 0))
                return -1;

            if (project_source_mode == KN_PROJ_SOURCE_MODE_ALL_SOURCES)
            {
                for (int psi = 0; psi < cfg->project_local_source_count; psi++)
                {
                    const char *path2 = cfg->project_local_sources[psi];
                    if (path2 && !strlist_contains_path(&inputs, path2))
                        strlist_add(&inputs, path2);
                }
            }
            else if (project_source_mode == KN_PROJ_SOURCE_MODE_ENTRY_UNIT ||
                     project_source_mode == KN_PROJ_SOURCE_MODE_REACHABLE_UNITS)
            {
                char *input_unit = read_unit_name_from_file(inputs.items[0]);
                if (input_unit)
                {
                    ModulePath *same_unit = modulemap_find(&local_modmap, input_unit);
                    if (same_unit)
                    {
                        for (int mp = 0; mp < same_unit->paths.count; mp++)
                        {
                            const char *path2 = same_unit->paths.items[mp];
                            if (path2 && !path_eq(inputs.items[0], path2) && !strlist_contains_path(&inputs, path2))
                                strlist_add(&inputs, path2);
                        }
                    }
                    kn_free(input_unit);
                }
            }
        }
        else
        {
            char base_dir[KN_MAX_PATH];
            path_dirname(inputs.items[0], base_dir, sizeof(base_dir));
            scan_dir_modules(base_dir, base_dir, &modmap);
            if (!scan_package_roots_into_module_map(cfg->package_roots, cfg->package_root_count, &modmap, 0))
                return -1;

            char *input_unit = read_unit_name_from_file(inputs.items[0]);
            if (input_unit)
            {
                ModulePath *same_unit = modulemap_find(&modmap, input_unit);
                if (same_unit)
                {
                    for (int mp = 0; mp < same_unit->paths.count; mp++)
                    {
                        const char *path2 = same_unit->paths.items[mp];
                        if (path2 && !path_eq(inputs.items[0], path2) && !strlist_contains_path(&inputs, path2))
                            strlist_add(&inputs, path2);
                    }
                }
                kn_free(input_unit);
            }
        }
    }

    if (cfg->emit_mode == KN_EMIT_TOKENS)
    {
        if (compile_write_token_summary(out_path, &inputs) != 0)
        {
            kn_write_str("failed to write token summary\n");
            return -1;
        }
        trace_log(trace, "compile:tokens");
        profile_phase(profile, "total", now_ms() - t_all);
        return 0;
    }

    SourceList sources = {0};
    uint64_t t_parse = now_ms();
    for (int i = 0; i < inputs.count; i++)
    {
        trace_log(trace, "parse:source");
        const char *path = inputs.items[i];
        size_t sz = 0;
        char *src = kn_read_file(path, &sz);
        if (!src)
        {
            kn_write_str("failed to read input\n");
            return -1;
        }
        if (cfg->project_mode && source_has_unsafe_alias_prefix(src))
        {
            KnSource temp_src;
            kn_memset(&temp_src, 0, sizeof(temp_src));
            kn_source_init(&temp_src, path, src, sz);
            kn_diag_report(&temp_src, KN_STAGE_PARSER, 1, 1, 1,
                           "Unsafe Alias Project File",
                           "Files that use Unsafe Alias cannot be built through kinal.knproj/project mode",
                           0);
            kn_source_free(&temp_src);
            kn_free(src);
            return -1;
        }
        KnSource *fsrc = (KnSource *)kn_malloc(sizeof(KnSource));
        if (!fsrc)
        {
            kn_write_str("out of memory\n");
            return -1;
        }
        kn_memset(fsrc, 0, sizeof(KnSource));
        kn_source_init(fsrc, path, src, sz);
        parse_program(fsrc, &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals);
        sourcelist_add(&sources, fsrc);

        /* Explicit official-package imports are resolved for both single-file
           and multi-file compilations. Local source discovery remains gated
           by the single-input auto_link setup above. */
        if (auto_link || resolve_import_modules)
        {
            for (int j = 0; j < imports.count; j++)
            {
                ImportMap *m = &imports.items[j];
                if (!m->module) continue;
                if (strlist_contains(&module_seen, m->module)) continue;
                strlist_add(&module_seen, m->module);
                if (kn_std_module_has(m->module))
                    continue;
                ModulePath *mod = 0;
                if (auto_link)
                {
                    if (use_project_local_map && project_source_mode == KN_PROJ_SOURCE_MODE_REACHABLE_UNITS)
                        mod = compile_find_import_module(&local_modmap, m->module);
                    if (!mod)
                        mod = use_project_local_map ? compile_find_import_module(&dep_modmap, m->module) : compile_find_import_module(&modmap, m->module);
                }
                if (!mod && resolve_import_modules)
                    mod = compile_find_import_module(&official_modmap, m->module);
                if (!mod) continue;
                for (int mp = 0; mp < mod->paths.count; mp++)
                {
                    const char *path2 = mod->paths.items[mp];
                    if (path2 && !strlist_contains_path(&inputs, path2))
                        strlist_add(&inputs, path2);
                }
            }
        }
    }
    if (resolve_import_modules)
        compile_resolve_package_symbol_imports(&imports, &metas, &funcs, &classes,
                                               &interfaces, &structs, &enums, &globals);
    p_parse = now_ms() - t_parse;
    if (kn_diag_error_count() > 0)
    {
        trace_log(trace, "sema:skipped_parser_errors");
        return -1;
    }

    if (cfg->emit_mode == KN_EMIT_AST)
    {
        if (compile_write_syntax_summary(out_path, &sources, &funcs, &imports,
                                         &classes, &interfaces, &structs, &enums, &globals) != 0)
        {
            kn_write_str("failed to write syntax summary\n");
            return -1;
        }
        compile_free_sources(&sources);
        trace_log(trace, "compile:syntax");
        profile_phase(profile, "parse", p_parse);
        profile_phase(profile, "total", now_ms() - t_all);
        return 0;
    }

    kn_inject_builtins(&classes);

    trace_log(trace, "sema:start");
    uint64_t t_sema = now_ms();
    const KnSource *first_src = sources.count > 0 ? sources.items[0] : 0;
    KnSemaOptions sema_opts;
    compile_init_sema_options(&sema_opts, cfg, need_entry, entry_name);
    sema_opts.known_standard_modules = known_standard_modules.items;
    sema_opts.known_standard_module_count = known_standard_modules.count;
    if (kn_sema_check(first_src, &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals, &sema_opts) != 0)
        return -1;
    if (kn_diag_error_count() > 0)
        return -1;
    p_sema = now_ms() - t_sema;
    trace_log(trace, "sema:ok");

    if (cfg->emit_mode == KN_EMIT_CHECK)
    {
        if (compile_write_sema_summary(out_path, cfg, &sources, &metas, &funcs, &imports,
                                       &classes, &interfaces, &structs, &enums, &globals) != 0)
        {
            kn_write_str("failed to write semantic summary\n");
            return -1;
        }
        compile_free_sources(&sources);
        trace_log(trace, "compile:checked");
        profile_phase(profile, "parse", p_parse);
        profile_phase(profile, "sema", p_sema);
        profile_phase(profile, "total", now_ms() - t_all);
        return 0;
    }

    if (cfg->emit_mode == KN_EMIT_KNC)
    {
        if (cfg->artifact_kind != KN_ARTIFACT_EXE)
        {
            kn_diag_report(first_src, KN_STAGE_CODEGEN, 1, 1, 1,
                           "Invalid KNC Artifact",
                           "The bootstrap KNC emitter currently supports executable programs only",
                           0);
            return -1;
        }
        trace_log(trace, "knc:start");
        uint64_t t_knc = now_ms();
        KnKncOptions knc_opts;
        kn_memset(&knc_opts, 0, sizeof(knc_opts));
        knc_opts.entry_name = entry_name;
        knc_opts.enable_superloop = cfg->knc_superloop;
        knc_opts.listing_path = cfg->knc_listing_path;
        if (kn_emit_knc(out_path, &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals, &knc_opts) != 0)
            return -1;
        p_codegen = now_ms() - t_knc;
        trace_log(trace, "knc:ok");
        profile_phase(profile, "knc", p_codegen);
        profile_phase(profile, "total", now_ms() - t_all);
        return 0;
    }

    char cg_out[KN_MAX_PATH];
    char cg_obj[KN_MAX_PATH];
    int emit_kind = KN_CODEGEN_OBJECT;
    int emit_lto_bitcode = 0;
    if (cfg->emit_mode == KN_EMIT_LINK)
    {
        if (cfg->lto_mode == KN_LTO_OFF)
        {
            if (!derive_obj_path(out_path, cfg->target_triple, cg_out, sizeof(cg_out)))
                return -1;
            emit_kind = KN_CODEGEN_OBJECT;
        }
        else
        {
            emit_lto_bitcode = (cfg->linker_kind == KN_LINKER_LLD);
            if (!(emit_lto_bitcode
                      ? derive_temp_bc_path(out_path, cg_out, sizeof(cg_out))
                      : derive_temp_ir_path(out_path, cg_out, sizeof(cg_out))))
                return -1;
            emit_kind = emit_lto_bitcode ? KN_CODEGEN_LLVM_BC : KN_CODEGEN_LLVM_IR;
        }
    }
    else if (cfg->emit_mode == KN_EMIT_OBJECT)
    {
        copy_str(cg_out, sizeof(cg_out), out_path);
        emit_kind = KN_CODEGEN_OBJECT;
    }
    else if (cfg->emit_mode == KN_EMIT_ASSEMBLY)
    {
        copy_str(cg_out, sizeof(cg_out), out_path);
        emit_kind = KN_CODEGEN_ASSEMBLY;
    }
    else
    {
        copy_str(cg_out, sizeof(cg_out), out_path);
        emit_kind = KN_CODEGEN_LLVM_IR;
    }

    const int emit_pic = (cfg->artifact_kind == KN_ARTIFACT_SHARED_LIB) ||
                         (cfg->artifact_kind == KN_ARTIFACT_STATIC_LIB && cfg->env_kind == KN_ENV_FREESTANDING);

    trace_log(trace, "codegen:start");
    uint64_t t_codegen = now_ms();
    KnCodegenOptions cg_opts;
    // Emit entry wrapper for executable artifacts so .ll/.s/.obj intermediates
    // can be resumed and linked later without re-running frontend.
    compile_init_codegen_options(&cg_opts, cfg, emit_kind, need_entry, emit_pic, entry_name);
    if (kn_codegen(cg_out, &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals, &cg_opts) != 0)
        return -1;
    p_codegen = now_ms() - t_codegen;
    trace_log(trace, "codegen:ok");

    if (cfg->emit_mode == KN_EMIT_LINK)
    {
        const char *primary_obj = cg_out;
        if (cfg->lto_mode != KN_LTO_OFF && !emit_lto_bitcode)
        {
            if (!derive_obj_path(out_path, cfg->target_triple, cg_obj, sizeof(cg_obj)))
                return -1;
            if (compile_intermediate_to_obj(cg_out, cg_obj, cfg->target_triple, cfg->lto_mode,
                                            emit_pic) != 0)
            {
                kn_write_str("failed to compile generated llvm ir to object for lto\n");
                return -1;
            }
            primary_obj = cg_obj;
        }

        KnLinkItem extra_items[512];
        int extra_count = 0;
        char attr_link_paths[128][KN_MAX_PATH];
        int attr_link_path_count = 0;
        char temp_objs[64][KN_MAX_PATH];
        int temp_count = 0;
        for (int i = 0; i < intermediate_inputs.count; i++)
        {
            const char *p = intermediate_inputs.items[i];
            KnInputKind k = classify_input(p);
            if (k == KN_INPUT_OBJ)
            {
                link_item_add(extra_items, &extra_count, (int)(sizeof(extra_items) / sizeof(extra_items[0])), KN_LINK_ITEM_FILE, p);
                continue;
            }
            if (k == KN_INPUT_ASM || k == KN_INPUT_LL)
            {
                if (temp_count >= (int)(sizeof(temp_objs) / sizeof(temp_objs[0])))
                {
                    kn_write_str("too many intermediate inputs\n");
                    return -1;
                }
                int use_bc = (k == KN_INPUT_LL && cfg->lto_mode != KN_LTO_OFF && cfg->linker_kind == KN_LINKER_LLD);
                if (use_bc)
                {
                    if (!derive_temp_bc_input_path(out_path, temp_count + 20, temp_objs[temp_count], sizeof(temp_objs[temp_count])))
                        return -1;
                    if (compile_ir_to_bc(p, temp_objs[temp_count]) != 0)
                    {
                        kn_write_str("failed to compile intermediate llvm ir to bitcode\n");
                        return -1;
                    }
                }
                else
                {
                    if (!derive_temp_obj_path(out_path, cfg->target_triple, temp_count + 20, temp_objs[temp_count], sizeof(temp_objs[temp_count])))
                        return -1;
                    if (compile_intermediate_to_obj(p, temp_objs[temp_count], cfg->target_triple, cfg->lto_mode,
                                                    emit_pic) != 0)
                    {
                        kn_write_str("failed to compile intermediate input to object\n");
                        return -1;
                    }
                }
                link_item_add(extra_items, &extra_count, (int)(sizeof(extra_items) / sizeof(extra_items[0])), KN_LINK_ITEM_FILE, temp_objs[temp_count]);
                temp_count++;
            }
        }
        if (!collect_extern_attr_link_items(&funcs, extra_items, &extra_count,
                                            (int)(sizeof(extra_items) / sizeof(extra_items[0])),
                                            attr_link_paths, &attr_link_path_count,
                                            (int)(sizeof(attr_link_paths) / sizeof(attr_link_paths[0])),
                                            cfg->target_triple))
        {
            kn_write_str("too many link items\n");
            return -1;
        }
        for (int i = 0; i < cfg_link_item_count && extra_count < (int)(sizeof(extra_items) / sizeof(extra_items[0])); i++)
            link_item_add(extra_items, &extra_count, (int)(sizeof(extra_items) / sizeof(extra_items[0])), cfg_link_items[i].kind, cfg_link_items[i].value);

        if (cfg->show_link_search)
            dump_link_search_dirs(extra_items, extra_count);

        const char *exports[256];
        int export_count = 0;
        if (cfg->artifact_kind == KN_ARTIFACT_SHARED_LIB)
        {
            if (export_count < (int)(sizeof(exports) / sizeof(exports[0])))
                exports[export_count++] = "__kn_meta_get_module";
            for (int i = 0; i < funcs.count; i++)
            {
                Func *f = &funcs.items[i];
                if (f->is_extern || f->is_static) continue;
                const char *sym = f->qname ? f->qname : f->name;
                if (!sym || !sym[0]) continue;
                if (export_count < (int)(sizeof(exports) / sizeof(exports[0])))
                    exports[export_count++] = sym;
            }
        }

        trace_log(trace, "link:start");
        uint64_t t_link = now_ms();
        KnLinkOptions link_opts;
        compile_init_link_options(&link_opts, cfg, primary_obj, extra_items,
                                  extra_count, include_runtime, link_entry_symbol);
        link_opts.exports = exports;
        link_opts.export_count = export_count;
        if (invoke_linker(&link_opts) != 0)
        {
            kn_write_str("link failed\n");
            return -1;
        }
        if (!cfg->keep_temps)
        {
            for (int ti = 0; ti < temp_count; ti++)
                try_delete_file(temp_objs[ti]);
            if (cfg->lto_mode == KN_LTO_OFF)
            {
                try_delete_file(cg_out);
            }
            else
            {
                try_delete_file(cg_out);
                try_delete_file(cg_obj);
            }
        }
        p_link = now_ms() - t_link;
        trace_log(trace, "link:ok");
    }

    compile_free_sources(&sources);
    trace_log(trace, "compile:done");
    profile_phase(profile, "parse", p_parse);
    profile_phase(profile, "sema", p_sema);
    profile_phase(profile, "codegen", p_codegen);
    if (cfg->emit_mode == KN_EMIT_LINK)
        profile_phase(profile, "link", p_link);
    profile_phase(profile, "total", now_ms() - t_all);
    return 0;
}

#ifdef KN_HOSTLIB
#ifndef KN_LLVM_LIBDIR
#define KN_LLVM_LIBDIR ""
#endif

static int split_path_list(const char *text, const char **out, int cap, char **owned_buf)
{
    if (owned_buf) *owned_buf = 0;
    if (!text || !text[0] || !out || cap <= 0) return 0;

    size_t len = kn_strlen(text);
    char *buf = (char *)kn_malloc(len + 1);
    if (!buf) return 0;
    kn_memcpy(buf, text, len);
    buf[len] = 0;

    int count = 0;
    char *p = buf;
    while (*p && count < cap)
    {
        while (*p == '\r' || *p == '\n' || *p == ';' || *p == ' ' || *p == '\t')
            p++;
        if (!*p) break;
        out[count++] = p;
        while (*p && *p != '\r' && *p != '\n' && *p != ';')
            p++;
        if (*p)
        {
            *p = 0;
            p++;
        }
    }

    if (owned_buf) *owned_buf = buf;
    return count;
}

int __kn_host_compile_ex(const char *input_paths, const char *output_path, int auto_link, const char *extra_link_paths)
{
    if (!input_paths || !input_paths[0] || !output_path) return -1;
    kn_diag_reset();

    const char *inputs[64];
    KnLinkItem links[64];
    char *inputs_buf = 0;
    char *links_buf = 0;

    int in_count = split_path_list(input_paths, inputs, (int)(sizeof(inputs) / sizeof(inputs[0])), &inputs_buf);
    if (in_count <= 0)
    {
        if (inputs_buf) kn_free(inputs_buf);
        return -1;
    }

    int link_count = 0;
    char hostlib_local[KN_MAX_PATH];
    char llvmc_local[KN_MAX_PATH];
    bool has_hostlib_local = false;
    bool has_llvmc_local = false;

    char exe_dir[KN_MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        if (path_join(hostlib_local, sizeof(hostlib_local), exe_dir, "build\\kinal-host.lib") &&
            file_exists(hostlib_local))
        {
            has_hostlib_local = true;
        }
        else if (path_join(hostlib_local, sizeof(hostlib_local), exe_dir, "kinal-host.lib") &&
                 file_exists(hostlib_local))
        {
            has_hostlib_local = true;
        }

        if (path_join(llvmc_local, sizeof(llvmc_local), exe_dir, "llvm\\lib\\LLVM-C.lib") &&
            file_exists(llvmc_local))
        {
            has_llvmc_local = true;
        }
    }

    if (link_count < (int)(sizeof(links) / sizeof(links[0])))
    {
        links[link_count].kind = KN_LINK_ITEM_FILE;
        links[link_count++].value = has_hostlib_local ? hostlib_local : "kinal-host.lib";
    }
    if (has_llvmc_local)
    {
        if (link_count < (int)(sizeof(links) / sizeof(links[0])))
        {
            links[link_count].kind = KN_LINK_ITEM_FILE;
            links[link_count++].value = llvmc_local;
        }
    }
    else if (KN_LLVM_LIBDIR[0] != 0)
    {
        if (link_count < (int)(sizeof(links) / sizeof(links[0])))
        {
            links[link_count].kind = KN_LINK_ITEM_FILE;
            links[link_count++].value = KN_LLVM_LIBDIR "/LLVM-C.lib";
        }
    }

    const char *extra[32];
    int extra_count = split_path_list(extra_link_paths, extra, (int)(sizeof(extra) / sizeof(extra[0])), &links_buf);
    for (int i = 0; i < extra_count && link_count < (int)(sizeof(links) / sizeof(links[0])); i++)
    {
        if (!extra[i] || !extra[i][0]) continue;
        links[link_count].kind = KN_LINK_ITEM_FILE;
        links[link_count++].value = extra[i];
    }

    KnCompileConfig cfg;
    kn_memset(&cfg, 0, sizeof(cfg));
    cfg.out_path = output_path;
    cfg.auto_link = auto_link ? 1 : 0;
    cfg.trace = 0;
    cfg.profile = 0;
    cfg.link_items = links;
    cfg.link_item_count = link_count;
    cfg.target_triple = default_target_triple();
    cfg.linker_kind = default_hosted_linker_kind(cfg.target_triple);
    cfg.linker_path = 0;
    cfg.artifact_kind = KN_ARTIFACT_EXE;
    cfg.emit_mode = KN_EMIT_LINK;
    cfg.lto_mode = KN_LTO_OFF;
    cfg.show_link = 0;
    cfg.show_link_search = 0;
    cfg.knc_superloop = 1;
    int rc = kn_compile(in_count, inputs, &cfg);

    if (inputs_buf) kn_free(inputs_buf);
    if (links_buf) kn_free(links_buf);
    return rc;
}

int __kn_host_compile(const char *input_path, const char *output_path)
{
    return __kn_host_compile_ex(input_path, output_path, 1, "");
}
#endif
