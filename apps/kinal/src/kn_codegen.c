#include "kn/codegen.h"
#include "kn/util.h"
#include "kn/lexer.h"
#include "kn/std.h"
#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"

// ----------------------
// Codegen
// ----------------------

typedef struct VarSymbol VarSymbol;
struct VarSymbol
{
    const char *name;
    Type type;
    LLVMValueRef value;
    VarSymbol *next;
};

typedef struct Scope Scope;
struct Scope
{
    VarSymbol *vars;
    Scope *parent;
};

typedef struct
{
    const char *qname;
    Func *func;
    LLVMValueRef fn;
    LLVMTypeRef fn_ty;
    LLVMValueRef async_body_fn;
    LLVMTypeRef async_body_fn_ty;
    LLVMValueRef async_runner_fn;
    LLVMTypeRef async_runner_fn_ty;
    LLVMTypeRef async_env_ty;
    LLVMValueRef wrapper_fn;
    LLVMTypeRef wrapper_fn_ty;
    LLVMValueRef delegate_global;   // global variable for delegate function pointer (byte*)
    LLVMValueRef delegate_bridge;   // bridge thunk for By Kinal delegates (any(byte*,any*,i64))
} FuncSymbol;

typedef struct
{
    FuncSymbol *items;
    int count;
    int cap;
} FuncTable;

typedef struct
{
    ClassDecl *owner;
    int method_index;
} VSlot;

typedef struct
{
    ClassDecl *decl;
    LLVMTypeRef struct_ty;
    LLVMTypeRef ptr_ty;
    LLVMValueRef *methods;
    LLVMTypeRef *method_tys;
    int *method_vslots;
    VSlot *vtable_slots;
    int vtable_count;
    LLVMValueRef vtable_global;
    int vtable_built;
} ClassSymbol;

typedef struct
{
    ClassSymbol *items;
    int count;
    int cap;
} ClassTable;

typedef struct
{
    StructDecl *decl;
    LLVMTypeRef struct_ty;
    LLVMTypeRef ptr_ty;
} StructSymbol;

typedef struct
{
    StructSymbol *items;
    int count;
    int cap;
} StructTable;

typedef struct
{
    Type elem;
    LLVMTypeRef struct_ty;
} ArrayType;

typedef struct
{
    ArrayType *items;
    int count;
    int cap;
} ArrayTypeTable;

typedef struct
{
    const char *name;
    Type ret_type;
    ParamList *params;
} VSlotSig;

typedef struct
{
    VSlotSig *items;
    int count;
    int cap;
} VSlotTable;

typedef struct
{
    int id;
    int is_block;
    Expr *expr;
    ClassDecl *lexical_class;
    int capture_this;
    const char **capture_names;
    Type *capture_types;
    int capture_count;
    int capture_cap;
    LLVMValueRef body_fn;
    LLVMValueRef wrapper_fn;
    LLVMTypeRef wrapper_fn_ty;
} ClosureInfo;

typedef struct
{
    ClosureInfo *items;
    int count;
    int cap;
} ClosureTable;

static void functable_add(FuncTable *t, FuncSymbol s)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 8;
        FuncSymbol *n = (FuncSymbol *)kn_malloc(sizeof(FuncSymbol) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(FuncSymbol) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = s;
}

static FuncSymbol *functable_find(FuncTable *t, const char *name)
{
    for (int i = 0; i < t->count; i++)
        if (kn_strcmp(t->items[i].qname, name) == 0) return &t->items[i];
    return 0;
}

static const char *cg_class_qname(const ClassDecl *c)
{
    return c ? (c->qname ? c->qname : c->name) : 0;
}

static const char *cg_struct_qname(const StructDecl *st)
{
    return st ? (st->qname ? st->qname : st->name) : 0;
}

static const char *cg_interface_qname(const InterfaceDecl *it)
{
    return it ? (it->qname ? it->qname : it->name) : 0;
}

static const char *cg_enum_qname(const EnumDecl *en)
{
    return en ? (en->qname ? en->qname : en->name) : 0;
}

static void classtable_add(ClassTable *t, ClassSymbol s)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 8;
        ClassSymbol *n = (ClassSymbol *)kn_malloc(sizeof(ClassSymbol) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(ClassSymbol) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = s;
}

static ClassSymbol *classtable_find(ClassTable *t, const char *name)
{
    if (!t || !name) return 0;
    for (int i = 0; i < t->count; i++)
        if (kn_strcmp(t->items[i].decl->name, name) == 0 ||
            (t->items[i].decl->qname && kn_strcmp(t->items[i].decl->qname, name) == 0))
            return &t->items[i];
    return 0;
}

static void structtable_add(StructTable *t, StructSymbol s)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 8;
        StructSymbol *n = (StructSymbol *)kn_malloc(sizeof(StructSymbol) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(StructSymbol) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = s;
}

static StructSymbol *structtable_find(StructTable *t, const char *name)
{
    if (!t || !name) return 0;
    for (int i = 0; i < t->count; i++)
        if (kn_strcmp(t->items[i].decl->name, name) == 0 ||
            (t->items[i].decl->qname && kn_strcmp(t->items[i].decl->qname, name) == 0))
            return &t->items[i];
    return 0;
}

static void arraytable_add(ArrayTypeTable *t, ArrayType a)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 8;
        ArrayType *n = (ArrayType *)kn_malloc(sizeof(ArrayType) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(ArrayType) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = a;
}

static ArrayType *arraytable_find(ArrayTypeTable *t, Type elem)
{
    if (!t) return 0;
    for (int i = 0; i < t->count; i++)
    {
        Type a = t->items[i].elem;
        if (a.kind != elem.kind) continue;
        if (a.kind == TY_PTR)
        {
            if (a.elem != elem.elem) continue;
            if ((a.elem == TY_CLASS || a.elem == TY_STRUCT || a.elem == TY_ENUM) &&
                ((a.name == 0 && elem.name == 0) || (a.name && elem.name && kn_strcmp(a.name, elem.name) == 0)))
                return &t->items[i];
            if (!(a.elem == TY_CLASS || a.elem == TY_STRUCT || a.elem == TY_ENUM))
                return &t->items[i];
            continue;
        }
        if (a.kind == TY_CLASS || a.kind == TY_STRUCT || a.kind == TY_ENUM)
        {
            if (a.name && elem.name && kn_strcmp(a.name, elem.name) == 0)
                return &t->items[i];
            continue;
        }
        return &t->items[i];
    }
    return 0;
}

static void vslottable_add(VSlotTable *t, VSlotSig s)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 16;
        VSlotSig *n = (VSlotSig *)kn_malloc(sizeof(VSlotSig) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(VSlotSig) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = s;
}

static void closuretable_add(ClosureTable *t, ClosureInfo c)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 8;
        ClosureInfo *n = (ClosureInfo *)kn_malloc(sizeof(ClosureInfo) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(ClosureInfo) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = c;
}

static ClosureInfo *closuretable_find(ClosureTable *t, int id, int is_block)
{
    if (!t) return 0;
    for (int i = 0; i < t->count; i++)
    {
        if (t->items[i].id == id && t->items[i].is_block == is_block)
            return &t->items[i];
    }
    return 0;
}

static LLVMValueRef ensure_function(
    LLVMModuleRef mod,
    const char* name,
    LLVMTypeRef type)
{
    LLVMValueRef fn = LLVMGetNamedFunction(mod, name);
    if (!fn)
        fn = LLVMAddFunction(mod, name, type);
    return fn;
}

typedef struct
{
    LLVMContextRef ctx;
    LLVMModuleRef mod;
    LLVMBuilderRef builder;
    LLVMTargetDataRef target_data;
    LLVMTypeRef i1;
    LLVMTypeRef i8;
    LLVMTypeRef i16;
    LLVMTypeRef i32;
    LLVMTypeRef i64;
    LLVMTypeRef f16;
    LLVMTypeRef f32;
    LLVMTypeRef f64;
    LLVMTypeRef f128;
    LLVMTypeRef voidt;
    LLVMTypeRef i8ptr;
    LLVMTypeRef any_ty;
    FuncTable funcs;
    ClassTable classes;
    StructTable structs;
    ArrayTypeTable arrays;
    VSlotTable vslots;
    MetaList *metas;
    FuncList *func_list;
    InterfaceList *interfaces;
    StructList *structs_list;
    EnumList *enums;
    Scope *scope;
    Scope *global_scope;
    LLVMValueRef fn_global_init;
    LLVMTypeRef fn_global_init_ty;
    LLVMValueRef gv_global_init_done;
    LLVMValueRef gv_global_gc_frame;
    LLVMValueRef fn_main;
    LLVMTypeRef fn_main_ty;
    int fn_main_is_async;
    Type fn_main_ret_type;
    LLVMValueRef fn_print;
    LLVMTypeRef fn_print_ty;
    LLVMValueRef fn_println;
    LLVMTypeRef fn_println_ty;
    LLVMValueRef fn_print_i64;
    LLVMTypeRef fn_print_i64_ty;
    LLVMValueRef fn_print_bool;
    LLVMTypeRef fn_print_bool_ty;
    LLVMValueRef fn_print_char;
    LLVMTypeRef fn_print_char_ty;
    LLVMValueRef fn_readline;
    LLVMTypeRef fn_readline_ty;
    LLVMValueRef fn_time_tick;
    LLVMTypeRef fn_time_tick_ty;
    LLVMValueRef fn_time_sleep;
    LLVMTypeRef fn_time_sleep_ty;
    LLVMValueRef fn_time_parts;
    LLVMTypeRef fn_time_parts_ty;
    LLVMValueRef fn_time_format;
    LLVMTypeRef fn_time_format_ty;
    LLVMValueRef fn_time_nano;
    LLVMTypeRef fn_time_nano_ty;
    LLVMValueRef fn_strlen;
    LLVMTypeRef fn_strlen_ty;
    LLVMValueRef fn_str_concat;
    LLVMTypeRef fn_str_concat_ty;
    LLVMValueRef fn_str_eq;
    LLVMTypeRef fn_str_eq_ty;
    LLVMValueRef fn_mem_compare;
    LLVMTypeRef fn_mem_compare_ty;
    LLVMValueRef fn_string_to_chars;
    LLVMTypeRef fn_string_to_chars_ty;
    LLVMValueRef fn_dict_new;
    LLVMTypeRef fn_dict_new_ty;
    LLVMValueRef fn_dict_set;
    LLVMTypeRef fn_dict_set_ty;
    LLVMValueRef fn_dict_get;
    LLVMTypeRef fn_dict_get_ty;
    LLVMValueRef fn_dict_contains;
    LLVMTypeRef fn_dict_contains_ty;
    LLVMValueRef fn_dict_count;
    LLVMTypeRef fn_dict_count_ty;
    LLVMValueRef fn_dict_remove;
    LLVMTypeRef fn_dict_remove_ty;
    LLVMValueRef fn_dict_is_empty;
    LLVMTypeRef fn_dict_is_empty_ty;
    LLVMValueRef fn_dict_keys;
    LLVMTypeRef fn_dict_keys_ty;
    LLVMValueRef fn_dict_values;
    LLVMTypeRef fn_dict_values_ty;
    LLVMValueRef fn_list_new;
    LLVMTypeRef fn_list_new_ty;
    LLVMValueRef fn_list_add;
    LLVMTypeRef fn_list_add_ty;
    LLVMValueRef fn_list_get;
    LLVMTypeRef fn_list_get_ty;
    LLVMValueRef fn_list_set;
    LLVMTypeRef fn_list_set_ty;
    LLVMValueRef fn_list_insert;
    LLVMTypeRef fn_list_insert_ty;
    LLVMValueRef fn_list_count;
    LLVMTypeRef fn_list_count_ty;
    LLVMValueRef fn_list_clear;
    LLVMTypeRef fn_list_clear_ty;
    LLVMValueRef fn_list_remove_at;
    LLVMTypeRef fn_list_remove_at_ty;
    LLVMValueRef fn_list_contains;
    LLVMTypeRef fn_list_contains_ty;
    LLVMValueRef fn_list_pop;
    LLVMTypeRef fn_list_pop_ty;
    LLVMValueRef fn_list_index_of;
    LLVMTypeRef fn_list_index_of_ty;
    LLVMValueRef fn_list_is_empty;
    LLVMTypeRef fn_list_is_empty_ty;
    LLVMValueRef fn_set_new;
    LLVMTypeRef fn_set_new_ty;
    LLVMValueRef fn_set_add;
    LLVMTypeRef fn_set_add_ty;
    LLVMValueRef fn_set_contains;
    LLVMTypeRef fn_set_contains_ty;
    LLVMValueRef fn_set_count;
    LLVMTypeRef fn_set_count_ty;
    LLVMValueRef fn_set_remove;
    LLVMTypeRef fn_set_remove_ty;
    LLVMValueRef fn_set_clear;
    LLVMTypeRef fn_set_clear_ty;
    LLVMValueRef fn_set_is_empty;
    LLVMTypeRef fn_set_is_empty_ty;
    LLVMValueRef fn_set_values;
    LLVMTypeRef fn_set_values_ty;
    LLVMValueRef fn_any_to_string;
    LLVMTypeRef fn_any_to_string_ty;
    LLVMValueRef fn_any_to_i64;
    LLVMTypeRef fn_any_to_i64_ty;
    LLVMValueRef fn_any_to_f64;
    LLVMTypeRef fn_any_to_f64_ty;
    LLVMValueRef fn_any_to_bool;
    LLVMTypeRef fn_any_to_bool_ty;
    LLVMValueRef fn_any_to_char;
    LLVMTypeRef fn_any_to_char_ty;
    LLVMValueRef fn_any_to_ptr;
    LLVMTypeRef fn_any_to_ptr_ty;
    LLVMValueRef fn_i64_to_string;
    LLVMTypeRef fn_i64_to_string_ty;
    LLVMValueRef fn_f64_to_string;
    LLVMTypeRef fn_f64_to_string_ty;
    LLVMValueRef fn_char_to_string;
    LLVMTypeRef fn_char_to_string_ty;
    LLVMValueRef fn_ptr_to_string;
    LLVMTypeRef fn_ptr_to_string_ty;
    LLVMValueRef fn_string_to_i64;
    LLVMTypeRef fn_string_to_i64_ty;
    LLVMValueRef fn_string_to_bool;
    LLVMTypeRef fn_string_to_bool_ty;
    LLVMValueRef fn_string_to_char;
    LLVMTypeRef fn_string_to_char_ty;
    LLVMValueRef fn_string_to_f64;
    LLVMTypeRef fn_string_to_f64_ty;
    LLVMValueRef fn_panic;
    LLVMTypeRef fn_panic_ty;
    LLVMValueRef fn_gc_alloc;
    LLVMTypeRef fn_gc_alloc_ty;
    LLVMValueRef fn_gc_collect;
    LLVMTypeRef fn_gc_collect_ty;
    LLVMValueRef fn_gc_push;
    LLVMTypeRef fn_gc_push_ty;
    LLVMValueRef fn_gc_add_root;
    LLVMTypeRef fn_gc_add_root_ty;
    LLVMValueRef fn_gc_pop;
    LLVMTypeRef fn_gc_pop_ty;
    LLVMValueRef fn_gc_init;
    LLVMTypeRef fn_gc_init_ty;
    LLVMValueRef fn_sys_exit;
    LLVMTypeRef fn_sys_exit_ty;
    LLVMValueRef fn_sys_cmdline;
    LLVMTypeRef fn_sys_cmdline_ty;
    LLVMValueRef fn_sys_args;
    LLVMTypeRef fn_sys_args_ty;
    LLVMValueRef fn_sys_args_from_argv;
    LLVMTypeRef fn_sys_args_from_argv_ty;
    LLVMValueRef fn_sys_file_exists;
    LLVMTypeRef fn_sys_file_exists_ty;
    LLVMValueRef fn_sys_exec;
    LLVMTypeRef fn_sys_exec_ty;
    LLVMValueRef fn_sys_load_library;
    LLVMTypeRef fn_sys_load_library_ty;
    LLVMValueRef fn_sys_get_symbol;
    LLVMTypeRef fn_sys_get_symbol_ty;
    LLVMValueRef fn_sys_close_library;
    LLVMTypeRef fn_sys_close_library_ty;
    LLVMValueRef fn_sys_last_error;
    LLVMTypeRef fn_sys_last_error_ty;
    LLVMValueRef fn_async_spawn;
    LLVMTypeRef fn_async_spawn_ty;
    LLVMValueRef fn_async_is_completed;
    LLVMTypeRef fn_async_is_completed_ty;
    LLVMValueRef fn_async_wait;
    LLVMTypeRef fn_async_wait_ty;
    LLVMValueRef fn_async_exit_code;
    LLVMTypeRef fn_async_exit_code_ty;
    LLVMValueRef fn_async_close;
    LLVMTypeRef fn_async_close_ty;
    LLVMValueRef fn_async_task_spawn;
    LLVMTypeRef fn_async_task_spawn_ty;
    LLVMValueRef fn_async_task_run;
    LLVMTypeRef fn_async_task_run_ty;
    LLVMValueRef fn_async_task_complete;
    LLVMTypeRef fn_async_task_complete_ty;
    LLVMValueRef fn_async_task_fail;
    LLVMTypeRef fn_async_task_fail_ty;
    LLVMValueRef fn_async_task_sleep;
    LLVMTypeRef fn_async_task_sleep_ty;
    LLVMValueRef fn_async_task_yield;
    LLVMTypeRef fn_async_task_yield_ty;
    LLVMValueRef fn_meta_register_main;
    LLVMTypeRef fn_meta_register_main_ty;
    LLVMValueRef fn_meta_query;
    LLVMTypeRef fn_meta_query_ty;
    LLVMValueRef fn_meta_of;
    LLVMTypeRef fn_meta_of_ty;
    LLVMValueRef fn_meta_has;
    LLVMTypeRef fn_meta_has_ty;
    LLVMValueRef fn_meta_find;
    LLVMTypeRef fn_meta_find_ty;
    LLVMValueRef fn_meta_get_module;
    LLVMTypeRef fn_meta_get_module_ty;
    LLVMValueRef fn_file_create;
    LLVMTypeRef fn_file_create_ty;
    LLVMValueRef fn_file_touch;
    LLVMTypeRef fn_file_touch_ty;
    LLVMValueRef fn_file_read_all_text;
    LLVMTypeRef fn_file_read_all_text_ty;
    LLVMValueRef fn_file_read_first_line;
    LLVMTypeRef fn_file_read_first_line_ty;
    LLVMValueRef fn_file_write_all_text;
    LLVMTypeRef fn_file_write_all_text_ty;
    LLVMValueRef fn_file_append_all_text;
    LLVMTypeRef fn_file_append_all_text_ty;
    LLVMValueRef fn_file_append_line;
    LLVMTypeRef fn_file_append_line_ty;
    LLVMValueRef fn_file_delete;
    LLVMTypeRef fn_file_delete_ty;
    LLVMValueRef fn_file_delete_if_exists;
    LLVMTypeRef fn_file_delete_if_exists_ty;
    LLVMValueRef fn_file_size;
    LLVMTypeRef fn_file_size_ty;
    LLVMValueRef fn_file_is_empty;
    LLVMTypeRef fn_file_is_empty_ty;
    LLVMValueRef fn_file_copy;
    LLVMTypeRef fn_file_copy_ty;
    LLVMValueRef fn_file_move;
    LLVMTypeRef fn_file_move_ty;
    LLVMValueRef fn_file_read_lines;
    LLVMTypeRef fn_file_read_lines_ty;
    LLVMValueRef fn_file_write_lines;
    LLVMTypeRef fn_file_write_lines_ty;
    LLVMValueRef fn_file_replace_text;
    LLVMTypeRef fn_file_replace_text_ty;
    LLVMValueRef fn_dir_exists;
    LLVMTypeRef fn_dir_exists_ty;
    LLVMValueRef fn_dir_create;
    LLVMTypeRef fn_dir_create_ty;
    LLVMValueRef fn_dir_ensure;
    LLVMTypeRef fn_dir_ensure_ty;
    LLVMValueRef fn_dir_delete;
    LLVMTypeRef fn_dir_delete_ty;
    LLVMValueRef fn_dir_delete_if_exists;
    LLVMTypeRef fn_dir_delete_if_exists_ty;
    LLVMValueRef fn_path_combine;
    LLVMTypeRef fn_path_combine_ty;
    LLVMValueRef fn_path_join3;
    LLVMTypeRef fn_path_join3_ty;
    LLVMValueRef fn_path_join4;
    LLVMTypeRef fn_path_join4_ty;
    LLVMValueRef fn_path_file_name;
    LLVMTypeRef fn_path_file_name_ty;
    LLVMValueRef fn_path_extension;
    LLVMTypeRef fn_path_extension_ty;
    LLVMValueRef fn_path_directory;
    LLVMTypeRef fn_path_directory_ty;
    LLVMValueRef fn_path_normalize;
    LLVMTypeRef fn_path_normalize_ty;
    LLVMValueRef fn_path_cwd;
    LLVMTypeRef fn_path_cwd_ty;
    LLVMValueRef fn_path_change_extension;
    LLVMTypeRef fn_path_change_extension_ty;
    LLVMValueRef fn_path_is_absolute;
    LLVMTypeRef fn_path_is_absolute_ty;
    LLVMValueRef fn_path_base_name;
    LLVMTypeRef fn_path_base_name_ty;
    LLVMValueRef fn_path_ensure_trailing_separator;
    LLVMTypeRef fn_path_ensure_trailing_separator_ty;
    LLVMValueRef fn_text_contains;
    LLVMTypeRef fn_text_contains_ty;
    LLVMValueRef fn_text_starts_with;
    LLVMTypeRef fn_text_starts_with_ty;
    LLVMValueRef fn_text_ends_with;
    LLVMTypeRef fn_text_ends_with_ty;
    LLVMValueRef fn_text_index_of;
    LLVMTypeRef fn_text_index_of_ty;
    LLVMValueRef fn_text_last_index_of;
    LLVMTypeRef fn_text_last_index_of_ty;
    LLVMValueRef fn_text_trim;
    LLVMTypeRef fn_text_trim_ty;
    LLVMValueRef fn_text_replace;
    LLVMTypeRef fn_text_replace_ty;
    LLVMValueRef fn_text_to_upper;
    LLVMTypeRef fn_text_to_upper_ty;
    LLVMValueRef fn_text_to_lower;
    LLVMTypeRef fn_text_to_lower_ty;
    LLVMValueRef fn_text_slice;
    LLVMTypeRef fn_text_slice_ty;
    LLVMValueRef fn_text_count;
    LLVMTypeRef fn_text_count_ty;
    LLVMValueRef fn_text_split;
    LLVMTypeRef fn_text_split_ty;
    LLVMValueRef fn_text_join;
    LLVMTypeRef fn_text_join_ty;
    LLVMValueRef fn_text_lines;
    LLVMTypeRef fn_text_lines_ty;
    LLVMValueRef fn_exc_set;
    LLVMTypeRef fn_exc_set_ty;
    LLVMValueRef fn_exc_get;
    LLVMTypeRef fn_exc_get_ty;
    LLVMValueRef fn_exc_has;
    LLVMTypeRef fn_exc_has_ty;
    LLVMValueRef fn_exc_last;
    LLVMTypeRef fn_exc_last_ty;
    LLVMValueRef fn_exc_clear;
    LLVMTypeRef fn_exc_clear_ty;
    LLVMValueRef fn_exc_push;
    LLVMTypeRef fn_exc_push_ty;
    LLVMValueRef fn_exc_trace;
    LLVMTypeRef fn_exc_trace_ty;
    LLVMBasicBlockRef break_blocks[32];
    LLVMBasicBlockRef continue_blocks[32];
    int loop_depth;
    ClassDecl *current_class;
    LLVMValueRef current_this;
    Type current_ret_type;
    LLVMValueRef gc_frame;
    struct { LLVMBasicBlockRef catch_bb; LLVMBasicBlockRef after_bb; LLVMValueRef exc_slot; } try_stack[32];
    int try_depth;
    int in_runtime_block;
    BlockRecordList *runtime_block_records;
    LLVMBasicBlockRef *runtime_record_blocks;
    int runtime_record_count;
    LLVMBasicBlockRef runtime_block_start;
    LLVMBasicBlockRef runtime_block_done;
    int runtime_block_use_indirect;
    LLVMValueRef runtime_dispatch_table;
    LLVMTypeRef runtime_dispatch_table_ty;
    LLVMValueRef runtime_until_enabled;
    LLVMValueRef runtime_until_ip;
    ClassSymbol *error_class;
    int error_ctor_index;
    int error_title_index;
    int error_msg_index;
    int error_inner_index;
    int error_trace_index;
    ClassSymbol *datetime_class;
    int datetime_ticks_index;
    int datetime_year_index;
    int datetime_month_index;
    int datetime_day_index;
    int datetime_hour_index;
    int datetime_minute_index;
    int datetime_second_index;
    int datetime_millisecond_index;
    ClassSymbol *function_class;
    ClassSymbol *block_class;
    int function_invoke_index;
    int function_env_index;
    int block_invoke_index;
    int block_env_index;
    LLVMTypeRef pseudo_block_ty;
    LLVMTypeRef pseudo_block_ptr_ty;
    LLVMTypeRef callable_wrapper_ty;
    ClosureTable closures;
    const char *current_func_name;
    int target_is_windows;
    int target_is_x86;
    int emit_entry;
    int env_kind;
    int runtime_mode;
    int panic_mode;
    const char *entry_name;
} Codegen;

#include "codegen/kn_codegen_support.inc"
#include "codegen/kn_codegen_expr.inc"
#include "codegen/kn_codegen_runtime_stmt.inc"
#include "codegen/kn_codegen_decl.inc"
