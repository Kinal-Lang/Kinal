// Internal codegen fragment: runtime emission, closures, statements, and globals

static void maybe_call_global_init(Codegen *g);
static void emit_meta_module_export(Codegen *g);

static LLVMValueRef entry_await_task_result(Codegen *g, LLVMValueRef taskv, Type logical_ret)
{
    LLVMValueRef anyv = run_async_task_any(g, taskv, "entry.async.tag");
    if (logical_ret.kind == TY_VOID)
        return 0;
    if (logical_ret.kind == TY_ANY)
        return anyv;
    return any_to_type(g, anyv, logical_ret);
}

static LLVMValueRef entry_result_to_i32(Codegen *g, Type ret_type, LLVMValueRef retv)
{
    if (ret_type.kind == TY_VOID)
        return LLVMConstInt(g->i32, 0, 0);
    {
        CGValue rv;
        rv.value = retv;
        rv.type = ret_type;
        return cast_value(g, rv, type_make(TY_I32));
    }
}

static void build_runtime_hosted(Codegen *g)
{
    LLVMTypeRef t_i8ptr = g->i8ptr;
    LLVMTypeRef t_i32 = g->i32;
    LLVMTypeRef t_i64 = g->i64;
    LLVMTypeRef t_void = g->voidt;

    // MSVC floating-point support symbol when linking without CRT.
    {
        LLVMValueRef flt = LLVMAddGlobal(g->mod, t_i32, "_fltused");
        LLVMSetInitializer(flt, LLVMConstInt(t_i32, 0, 0));
    }

    // Minimal __chkstk stub to satisfy COFF link without CRT.
    {
        LLVMTypeRef chk_ty = LLVMFunctionType(t_void, 0, 0, 0);
        LLVMValueRef chk = LLVMAddFunction(g->mod, "__chkstk", chk_ty);
        LLVMBasicBlockRef chk_bb = LLVMAppendBasicBlockInContext(g->ctx, chk, "entry");
        LLVMPositionBuilderAtEnd(g->builder, chk_bb);
        LLVMBuildRetVoid(g->builder);
    }

    LLVMTypeRef getstd_ty = LLVMFunctionType(t_i8ptr, &t_i32, 1, 0);
    LLVMValueRef getstd = LLVMAddFunction(g->mod, "GetStdHandle", getstd_ty);
    LLVMTypeRef setcp_ty = LLVMFunctionType(g->i1, &t_i32, 1, 0);
    LLVMValueRef set_console_output_cp = LLVMAddFunction(g->mod, "SetConsoleOutputCP", setcp_ty);
    LLVMValueRef set_console_input_cp = LLVMAddFunction(g->mod, "SetConsoleCP", setcp_ty);
    LLVMTypeRef get_console_mode_args[2] = { t_i8ptr, LLVMPointerType(t_i32, 0) };
    LLVMTypeRef get_console_mode_ty = LLVMFunctionType(g->i1, get_console_mode_args, 2, 0);
    LLVMValueRef get_console_mode = LLVMAddFunction(g->mod, "GetConsoleMode", get_console_mode_ty);
    LLVMTypeRef set_console_mode_args[2] = { t_i8ptr, t_i32 };
    LLVMTypeRef set_console_mode_ty = LLVMFunctionType(g->i1, set_console_mode_args, 2, 0);
    LLVMValueRef set_console_mode = LLVMAddFunction(g->mod, "SetConsoleMode", set_console_mode_ty);

    LLVMTypeRef wf_args[5] = { t_i8ptr, t_i8ptr, t_i32, LLVMPointerType(t_i32, 0), t_i8ptr };
    LLVMTypeRef writefile_ty = LLVMFunctionType(g->i1, wf_args, 5, 0);
    LLVMValueRef writefile = LLVMAddFunction(g->mod, "WriteFile", writefile_ty);
    LLVMValueRef readfile = LLVMAddFunction(g->mod, "ReadFile", writefile_ty);

    g->fn_gc_alloc_ty = LLVMFunctionType(t_i8ptr, &t_i64, 1, 0);
    g->fn_gc_alloc = LLVMAddFunction(g->mod, "__kn_gc_alloc", g->fn_gc_alloc_ty);
    g->fn_gc_collect_ty = LLVMFunctionType(t_void, 0, 0, 0);
    g->fn_gc_collect = LLVMAddFunction(g->mod, "__kn_gc_collect", g->fn_gc_collect_ty);
    g->fn_gc_push_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_gc_push = LLVMAddFunction(g->mod, "__kn_gc_push_frame", g->fn_gc_push_ty);
    LLVMTypeRef gc_root_args[3] = { t_i8ptr, t_i8ptr, t_i64 };
    g->fn_gc_add_root_ty = LLVMFunctionType(t_void, gc_root_args, 3, 0);
    g->fn_gc_add_root = LLVMAddFunction(g->mod, "__kn_gc_add_root", g->fn_gc_add_root_ty);
    g->fn_gc_pop_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_gc_pop = LLVMAddFunction(g->mod, "__kn_gc_pop_frame", g->fn_gc_pop_ty);
    g->fn_gc_init_ty = LLVMFunctionType(t_void, 0, 0, 0);
    g->fn_gc_init = LLVMAddFunction(g->mod, "__kn_gc_init", g->fn_gc_init_ty);

    g->fn_sys_exit_ty = LLVMFunctionType(t_void, &t_i32, 1, 0);
    g->fn_sys_exit = LLVMAddFunction(g->mod, "__kn_sys_exit", g->fn_sys_exit_ty);
    g->fn_sys_cmdline_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_sys_cmdline = LLVMAddFunction(g->mod, "__kn_sys_command_line", g->fn_sys_cmdline_ty);
    {
        LLVMTypeRef out_args[2] = { LLVMPointerType(LLVMPointerType(t_i8ptr, 0), 0), LLVMPointerType(t_i64, 0) };
        g->fn_sys_args_ty = LLVMFunctionType(t_void, out_args, 2, 0);
        g->fn_sys_args = LLVMAddFunction(g->mod, "__kn_sys_args", g->fn_sys_args_ty);

        LLVMTypeRef argv_args[4] = { t_i32, LLVMPointerType(t_i8ptr, 0), LLVMPointerType(LLVMPointerType(t_i8ptr, 0), 0), LLVMPointerType(t_i64, 0) };
        g->fn_sys_args_from_argv_ty = LLVMFunctionType(t_void, argv_args, 4, 0);
        g->fn_sys_args_from_argv = LLVMAddFunction(g->mod, "__kn_sys_args_from_argv", g->fn_sys_args_from_argv_ty);
    }
    g->fn_sys_file_exists_ty = LLVMFunctionType(g->i1, &t_i8ptr, 1, 0);
    g->fn_sys_file_exists = LLVMAddFunction(g->mod, "__kn_sys_file_exists", g->fn_sys_file_exists_ty);
    g->fn_sys_exec_ty = LLVMFunctionType(t_i64, &t_i8ptr, 1, 0);
    g->fn_sys_exec = LLVMAddFunction(g->mod, "__kn_sys_exec", g->fn_sys_exec_ty);
    g->fn_sys_load_library_ty = LLVMFunctionType(t_i8ptr, &t_i8ptr, 1, 0);
    g->fn_sys_load_library = LLVMAddFunction(g->mod, "__kn_sys_load_library", g->fn_sys_load_library_ty);
    {
        LLVMTypeRef args_dynsym[2] = { t_i8ptr, t_i8ptr };
        g->fn_sys_get_symbol_ty = LLVMFunctionType(t_i8ptr, args_dynsym, 2, 0);
        g->fn_sys_get_symbol = LLVMAddFunction(g->mod, "__kn_sys_get_symbol", g->fn_sys_get_symbol_ty);
    }
    g->fn_sys_close_library_ty = LLVMFunctionType(g->i1, &t_i8ptr, 1, 0);
    g->fn_sys_close_library = LLVMAddFunction(g->mod, "__kn_sys_close_library", g->fn_sys_close_library_ty);
    g->fn_sys_last_error_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_sys_last_error = LLVMAddFunction(g->mod, "__kn_sys_last_error", g->fn_sys_last_error_ty);
    g->fn_async_spawn_ty = LLVMFunctionType(t_i8ptr, &t_i8ptr, 1, 0);
    g->fn_async_spawn = LLVMAddFunction(g->mod, "__kn_async_process_spawn", g->fn_async_spawn_ty);
    g->fn_async_is_completed_ty = LLVMFunctionType(g->i1, &t_i8ptr, 1, 0);
    g->fn_async_is_completed = LLVMAddFunction(g->mod, "__kn_async_process_is_completed", g->fn_async_is_completed_ty);
    {
        LLVMTypeRef args_async_wait[2] = { t_i8ptr, t_i64 };
        g->fn_async_wait_ty = LLVMFunctionType(g->i1, args_async_wait, 2, 0);
        g->fn_async_wait = LLVMAddFunction(g->mod, "__kn_async_process_wait", g->fn_async_wait_ty);
    }
    g->fn_async_exit_code_ty = LLVMFunctionType(t_i64, &t_i8ptr, 1, 0);
    g->fn_async_exit_code = LLVMAddFunction(g->mod, "__kn_async_process_exit_code", g->fn_async_exit_code_ty);
    g->fn_async_close_ty = LLVMFunctionType(g->i1, &t_i8ptr, 1, 0);
    g->fn_async_close = LLVMAddFunction(g->mod, "__kn_async_process_close", g->fn_async_close_ty);
    {
        LLVMTypeRef args_spawn[2] = { t_i8ptr, t_i8ptr };
        g->fn_async_task_spawn_ty = LLVMFunctionType(t_i8ptr, args_spawn, 2, 0);
        g->fn_async_task_spawn = LLVMAddFunction(g->mod, "__kn_async_task_spawn", g->fn_async_task_spawn_ty);
        LLVMTypeRef args_run[3] = { t_i8ptr, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_async_task_run_ty = LLVMFunctionType(t_void, args_run, 3, 0);
        g->fn_async_task_run = LLVMAddFunction(g->mod, "__kn_async_task_run", g->fn_async_task_run_ty);
        LLVMTypeRef args_complete[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_async_task_complete_ty = LLVMFunctionType(t_void, args_complete, 3, 0);
        g->fn_async_task_complete = LLVMAddFunction(g->mod, "__kn_async_task_complete", g->fn_async_task_complete_ty);
        LLVMTypeRef args_fail[3] = { t_i8ptr, t_i8ptr, t_i8ptr };
        g->fn_async_task_fail_ty = LLVMFunctionType(t_void, args_fail, 3, 0);
        g->fn_async_task_fail = LLVMAddFunction(g->mod, "__kn_async_task_fail", g->fn_async_task_fail_ty);
        g->fn_async_task_sleep_ty = LLVMFunctionType(t_i8ptr, &t_i32, 1, 0);
        g->fn_async_task_sleep = LLVMAddFunction(g->mod, "__kn_async_task_sleep", g->fn_async_task_sleep_ty);
        g->fn_async_task_yield_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
        g->fn_async_task_yield = LLVMAddFunction(g->mod, "__kn_async_task_yield", g->fn_async_task_yield_ty);
    }
    g->fn_meta_register_main_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_meta_register_main = LLVMAddFunction(g->mod, "__kn_meta_register_main", g->fn_meta_register_main_ty);
    g->fn_meta_query_ty = LLVMFunctionType(t_i8ptr, &t_i8ptr, 1, 0);
    g->fn_meta_query = LLVMAddFunction(g->mod, "__kn_meta_query", g->fn_meta_query_ty);
    g->fn_meta_of_ty = LLVMFunctionType(t_i8ptr, &t_i8ptr, 1, 0);
    g->fn_meta_of = LLVMAddFunction(g->mod, "__kn_meta_of", g->fn_meta_of_ty);
    {
        LLVMTypeRef meta_has_args[2] = { t_i8ptr, t_i8ptr };
        g->fn_meta_has_ty = LLVMFunctionType(g->i1, meta_has_args, 2, 0);
        g->fn_meta_has = LLVMAddFunction(g->mod, "__kn_meta_has", g->fn_meta_has_ty);
        g->fn_meta_find_ty = LLVMFunctionType(t_i8ptr, meta_has_args, 2, 0);
        g->fn_meta_find = LLVMAddFunction(g->mod, "__kn_meta_find", g->fn_meta_find_ty);
    }
    g->fn_meta_get_module_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_meta_get_module = LLVMAddFunction(g->mod, "__kn_meta_get_module", g->fn_meta_get_module_ty);

    {
        LLVMTypeRef args_s2c[2] = { t_i8ptr, LLVMPointerType(t_i64, 0) };
        g->fn_string_to_chars_ty = LLVMFunctionType(t_i8ptr, args_s2c, 2, 0);
        g->fn_string_to_chars = LLVMAddFunction(g->mod, "__kn_string_to_chars", g->fn_string_to_chars_ty);
    }

    g->fn_dict_new_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_dict_new = LLVMAddFunction(g->mod, "__kn_dict_new", g->fn_dict_new_ty);
    {
        LLVMTypeRef args_set[4] = { t_i8ptr, t_i8ptr, t_i64, t_i64 };
        g->fn_dict_set_ty = LLVMFunctionType(t_void, args_set, 4, 0);
        g->fn_dict_set = LLVMAddFunction(g->mod, "__kn_dict_set", g->fn_dict_set_ty);
    }
    {
        LLVMTypeRef args_get[4] = { t_i8ptr, t_i8ptr, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_dict_get_ty = LLVMFunctionType(t_void, args_get, 4, 0);
        g->fn_dict_get = LLVMAddFunction(g->mod, "__kn_dict_get", g->fn_dict_get_ty);
    }
    {
        LLVMTypeRef args_has[2] = { t_i8ptr, t_i8ptr };
        g->fn_dict_contains_ty = LLVMFunctionType(g->i1, args_has, 2, 0);
        g->fn_dict_contains = LLVMAddFunction(g->mod, "__kn_dict_contains", g->fn_dict_contains_ty);
    }
    {
        LLVMTypeRef args_count[1] = { t_i8ptr };
        g->fn_dict_count_ty = LLVMFunctionType(t_i64, args_count, 1, 0);
        g->fn_dict_count = LLVMAddFunction(g->mod, "__kn_dict_count", g->fn_dict_count_ty);
    }
    {
        LLVMTypeRef args_remove[2] = { t_i8ptr, t_i8ptr };
        g->fn_dict_remove_ty = LLVMFunctionType(g->i1, args_remove, 2, 0);
        g->fn_dict_remove = LLVMAddFunction(g->mod, "__kn_dict_remove", g->fn_dict_remove_ty);
        LLVMTypeRef args_empty[1] = { t_i8ptr };
        g->fn_dict_is_empty_ty = LLVMFunctionType(g->i1, args_empty, 1, 0);
        g->fn_dict_is_empty = LLVMAddFunction(g->mod, "__kn_dict_is_empty", g->fn_dict_is_empty_ty);
        g->fn_dict_keys_ty = LLVMFunctionType(t_i8ptr, args_empty, 1, 0);
        g->fn_dict_keys = LLVMAddFunction(g->mod, "__kn_dict_keys", g->fn_dict_keys_ty);
        g->fn_dict_values_ty = LLVMFunctionType(t_i8ptr, args_empty, 1, 0);
        g->fn_dict_values = LLVMAddFunction(g->mod, "__kn_dict_values", g->fn_dict_values_ty);
    }
    g->fn_list_new_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_list_new = LLVMAddFunction(g->mod, "__kn_list_new", g->fn_list_new_ty);
    {
        LLVMTypeRef args_add[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_list_add_ty = LLVMFunctionType(t_void, args_add, 3, 0);
        g->fn_list_add = LLVMAddFunction(g->mod, "__kn_list_add", g->fn_list_add_ty);
    }
    {
        LLVMTypeRef args_get[4] = { t_i8ptr, t_i64, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_list_get_ty = LLVMFunctionType(t_void, args_get, 4, 0);
        g->fn_list_get = LLVMAddFunction(g->mod, "__kn_list_get", g->fn_list_get_ty);
    }
    {
        LLVMTypeRef args_set[4] = { t_i8ptr, t_i64, t_i64, t_i64 };
        g->fn_list_set_ty = LLVMFunctionType(t_void, args_set, 4, 0);
        g->fn_list_set = LLVMAddFunction(g->mod, "__kn_list_set", g->fn_list_set_ty);
        g->fn_list_insert_ty = LLVMFunctionType(t_void, args_set, 4, 0);
        g->fn_list_insert = LLVMAddFunction(g->mod, "__kn_list_insert", g->fn_list_insert_ty);
    }
    {
        LLVMTypeRef args_count[1] = { t_i8ptr };
        g->fn_list_count_ty = LLVMFunctionType(t_i64, args_count, 1, 0);
        g->fn_list_count = LLVMAddFunction(g->mod, "__kn_list_count", g->fn_list_count_ty);
        g->fn_list_clear_ty = LLVMFunctionType(t_void, args_count, 1, 0);
        g->fn_list_clear = LLVMAddFunction(g->mod, "__kn_list_clear", g->fn_list_clear_ty);
    }
    {
        LLVMTypeRef args_rm[4] = { t_i8ptr, t_i64, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_list_remove_at_ty = LLVMFunctionType(t_void, args_rm, 4, 0);
        g->fn_list_remove_at = LLVMAddFunction(g->mod, "__kn_list_remove_at", g->fn_list_remove_at_ty);
    }
    {
        LLVMTypeRef args_contains[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_list_contains_ty = LLVMFunctionType(g->i1, args_contains, 3, 0);
        g->fn_list_contains = LLVMAddFunction(g->mod, "__kn_list_contains", g->fn_list_contains_ty);
        LLVMTypeRef args_pop[3] = { t_i8ptr, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_list_pop_ty = LLVMFunctionType(t_void, args_pop, 3, 0);
        g->fn_list_pop = LLVMAddFunction(g->mod, "__kn_list_pop", g->fn_list_pop_ty);
        g->fn_list_index_of_ty = LLVMFunctionType(t_i64, args_contains, 3, 0);
        g->fn_list_index_of = LLVMAddFunction(g->mod, "__kn_list_index_of", g->fn_list_index_of_ty);
        LLVMTypeRef args_empty[1] = { t_i8ptr };
        g->fn_list_is_empty_ty = LLVMFunctionType(g->i1, args_empty, 1, 0);
        g->fn_list_is_empty = LLVMAddFunction(g->mod, "__kn_list_is_empty", g->fn_list_is_empty_ty);
    }

    g->fn_set_new_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_set_new = LLVMAddFunction(g->mod, "__kn_set_new", g->fn_set_new_ty);
    {
        LLVMTypeRef args_add[2] = { t_i8ptr, t_i8ptr };
        g->fn_set_add_ty = LLVMFunctionType(t_void, args_add, 2, 0);
        g->fn_set_add = LLVMAddFunction(g->mod, "__kn_set_add", g->fn_set_add_ty);
        g->fn_set_contains_ty = LLVMFunctionType(g->i1, args_add, 2, 0);
        g->fn_set_contains = LLVMAddFunction(g->mod, "__kn_set_contains", g->fn_set_contains_ty);
    }
    {
        LLVMTypeRef args_count[1] = { t_i8ptr };
        g->fn_set_count_ty = LLVMFunctionType(t_i64, args_count, 1, 0);
        g->fn_set_count = LLVMAddFunction(g->mod, "__kn_set_count", g->fn_set_count_ty);
        LLVMTypeRef args_clear[1] = { t_i8ptr };
        g->fn_set_clear_ty = LLVMFunctionType(t_void, args_clear, 1, 0);
        g->fn_set_clear = LLVMAddFunction(g->mod, "__kn_set_clear", g->fn_set_clear_ty);
        g->fn_set_is_empty_ty = LLVMFunctionType(g->i1, args_count, 1, 0);
        g->fn_set_is_empty = LLVMAddFunction(g->mod, "__kn_set_is_empty", g->fn_set_is_empty_ty);
        g->fn_set_values_ty = LLVMFunctionType(t_i8ptr, args_count, 1, 0);
        g->fn_set_values = LLVMAddFunction(g->mod, "__kn_set_values", g->fn_set_values_ty);
    }
    {
        LLVMTypeRef args_remove[2] = { t_i8ptr, t_i8ptr };
        g->fn_set_remove_ty = LLVMFunctionType(g->i1, args_remove, 2, 0);
        g->fn_set_remove = LLVMAddFunction(g->mod, "__kn_set_remove", g->fn_set_remove_ty);
    }

    {
        LLVMTypeRef args_path2[2] = { t_i8ptr, t_i8ptr };
        LLVMTypeRef args_path3[3] = { t_i8ptr, t_i8ptr, t_i8ptr };
        LLVMTypeRef args_path4[4] = { t_i8ptr, t_i8ptr, t_i8ptr, t_i8ptr };
        LLVMTypeRef args_path1[1] = { t_i8ptr };
        g->fn_path_combine_ty = LLVMFunctionType(t_i8ptr, args_path2, 2, 0);
        g->fn_path_combine = LLVMAddFunction(g->mod, "__kn_path_combine", g->fn_path_combine_ty);
        g->fn_path_join3_ty = LLVMFunctionType(t_i8ptr, args_path3, 3, 0);
        g->fn_path_join3 = LLVMAddFunction(g->mod, "__kn_path_join3", g->fn_path_join3_ty);
        g->fn_path_join4_ty = LLVMFunctionType(t_i8ptr, args_path4, 4, 0);
        g->fn_path_join4 = LLVMAddFunction(g->mod, "__kn_path_join4", g->fn_path_join4_ty);
        g->fn_path_file_name_ty = LLVMFunctionType(t_i8ptr, args_path1, 1, 0);
        g->fn_path_file_name = LLVMAddFunction(g->mod, "__kn_path_file_name", g->fn_path_file_name_ty);
        g->fn_path_extension_ty = LLVMFunctionType(t_i8ptr, args_path1, 1, 0);
        g->fn_path_extension = LLVMAddFunction(g->mod, "__kn_path_extension", g->fn_path_extension_ty);
        g->fn_path_directory_ty = LLVMFunctionType(t_i8ptr, args_path1, 1, 0);
        g->fn_path_directory = LLVMAddFunction(g->mod, "__kn_path_directory", g->fn_path_directory_ty);
        g->fn_path_normalize_ty = LLVMFunctionType(t_i8ptr, args_path1, 1, 0);
        g->fn_path_normalize = LLVMAddFunction(g->mod, "__kn_path_normalize", g->fn_path_normalize_ty);
        g->fn_path_cwd_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
        g->fn_path_cwd = LLVMAddFunction(g->mod, "__kn_path_cwd", g->fn_path_cwd_ty);
        g->fn_path_change_extension_ty = LLVMFunctionType(t_i8ptr, args_path2, 2, 0);
        g->fn_path_change_extension = LLVMAddFunction(g->mod, "__kn_path_change_extension", g->fn_path_change_extension_ty);
        g->fn_path_is_absolute_ty = LLVMFunctionType(g->i1, args_path1, 1, 0);
        g->fn_path_is_absolute = LLVMAddFunction(g->mod, "__kn_path_is_absolute", g->fn_path_is_absolute_ty);
        g->fn_path_base_name_ty = LLVMFunctionType(t_i8ptr, args_path1, 1, 0);
        g->fn_path_base_name = LLVMAddFunction(g->mod, "__kn_path_base_name", g->fn_path_base_name_ty);
        g->fn_path_ensure_trailing_separator_ty = LLVMFunctionType(t_i8ptr, args_path1, 1, 0);
        g->fn_path_ensure_trailing_separator = LLVMAddFunction(g->mod, "__kn_path_ensure_trailing_separator", g->fn_path_ensure_trailing_separator_ty);
    }

    {
        LLVMTypeRef args_text2[2] = { t_i8ptr, t_i8ptr };
        LLVMTypeRef args_text1[1] = { t_i8ptr };
        LLVMTypeRef args_text3[3] = { t_i8ptr, t_i8ptr, t_i8ptr };
        LLVMTypeRef args_text_slice[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_text_contains_ty = LLVMFunctionType(g->i1, args_text2, 2, 0);
        g->fn_text_contains = LLVMAddFunction(g->mod, "__kn_text_contains", g->fn_text_contains_ty);
        g->fn_text_starts_with_ty = LLVMFunctionType(g->i1, args_text2, 2, 0);
        g->fn_text_starts_with = LLVMAddFunction(g->mod, "__kn_text_starts_with", g->fn_text_starts_with_ty);
        g->fn_text_ends_with_ty = LLVMFunctionType(g->i1, args_text2, 2, 0);
        g->fn_text_ends_with = LLVMAddFunction(g->mod, "__kn_text_ends_with", g->fn_text_ends_with_ty);
        g->fn_text_index_of_ty = LLVMFunctionType(t_i64, args_text2, 2, 0);
        g->fn_text_index_of = LLVMAddFunction(g->mod, "__kn_text_index_of", g->fn_text_index_of_ty);
        g->fn_text_last_index_of_ty = LLVMFunctionType(t_i64, args_text2, 2, 0);
        g->fn_text_last_index_of = LLVMAddFunction(g->mod, "__kn_text_last_index_of", g->fn_text_last_index_of_ty);
        g->fn_text_trim_ty = LLVMFunctionType(t_i8ptr, args_text1, 1, 0);
        g->fn_text_trim = LLVMAddFunction(g->mod, "__kn_text_trim", g->fn_text_trim_ty);
        g->fn_text_replace_ty = LLVMFunctionType(t_i8ptr, args_text3, 3, 0);
        g->fn_text_replace = LLVMAddFunction(g->mod, "__kn_text_replace", g->fn_text_replace_ty);
        g->fn_text_to_upper_ty = LLVMFunctionType(t_i8ptr, args_text1, 1, 0);
        g->fn_text_to_upper = LLVMAddFunction(g->mod, "__kn_text_to_upper", g->fn_text_to_upper_ty);
        g->fn_text_to_lower_ty = LLVMFunctionType(t_i8ptr, args_text1, 1, 0);
        g->fn_text_to_lower = LLVMAddFunction(g->mod, "__kn_text_to_lower", g->fn_text_to_lower_ty);
        g->fn_text_slice_ty = LLVMFunctionType(t_i8ptr, args_text_slice, 3, 0);
        g->fn_text_slice = LLVMAddFunction(g->mod, "__kn_text_slice", g->fn_text_slice_ty);
        g->fn_text_count_ty = LLVMFunctionType(t_i64, args_text2, 2, 0);
        g->fn_text_count = LLVMAddFunction(g->mod, "__kn_text_count", g->fn_text_count_ty);
        g->fn_text_split_ty = LLVMFunctionType(t_i8ptr, args_text2, 2, 0);
        g->fn_text_split = LLVMAddFunction(g->mod, "__kn_text_split", g->fn_text_split_ty);
        g->fn_text_join_ty = LLVMFunctionType(t_i8ptr, args_text2, 2, 0);
        g->fn_text_join = LLVMAddFunction(g->mod, "__kn_text_join", g->fn_text_join_ty);
        g->fn_text_lines_ty = LLVMFunctionType(t_i8ptr, args_text1, 1, 0);
        g->fn_text_lines = LLVMAddFunction(g->mod, "__kn_text_lines", g->fn_text_lines_ty);
    }

    {
        LLVMTypeRef args_file1[1] = { t_i8ptr };
        LLVMTypeRef args_file2[2] = { t_i8ptr, t_i8ptr };
        LLVMTypeRef args_file3[3] = { t_i8ptr, t_i8ptr, t_i8ptr };
        g->fn_file_create_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_file_create = LLVMAddFunction(g->mod, "__kn_file_create", g->fn_file_create_ty);
        g->fn_file_touch_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_file_touch = LLVMAddFunction(g->mod, "__kn_file_touch", g->fn_file_touch_ty);
        g->fn_file_read_all_text_ty = LLVMFunctionType(t_i8ptr, args_file1, 1, 0);
        g->fn_file_read_all_text = LLVMAddFunction(g->mod, "__kn_file_read_all_text", g->fn_file_read_all_text_ty);
        g->fn_file_read_first_line_ty = LLVMFunctionType(t_i8ptr, args_file1, 1, 0);
        g->fn_file_read_first_line = LLVMAddFunction(g->mod, "__kn_file_read_first_line", g->fn_file_read_first_line_ty);
        g->fn_file_write_all_text_ty = LLVMFunctionType(g->i1, args_file2, 2, 0);
        g->fn_file_write_all_text = LLVMAddFunction(g->mod, "__kn_file_write_all_text", g->fn_file_write_all_text_ty);
        g->fn_file_append_all_text_ty = LLVMFunctionType(g->i1, args_file2, 2, 0);
        g->fn_file_append_all_text = LLVMAddFunction(g->mod, "__kn_file_append_all_text", g->fn_file_append_all_text_ty);
        g->fn_file_append_line_ty = LLVMFunctionType(g->i1, args_file2, 2, 0);
        g->fn_file_append_line = LLVMAddFunction(g->mod, "__kn_file_append_line", g->fn_file_append_line_ty);
        g->fn_file_delete_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_file_delete = LLVMAddFunction(g->mod, "__kn_file_delete", g->fn_file_delete_ty);
        g->fn_file_delete_if_exists_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_file_delete_if_exists = LLVMAddFunction(g->mod, "__kn_file_delete_if_exists", g->fn_file_delete_if_exists_ty);
        g->fn_file_size_ty = LLVMFunctionType(t_i64, args_file1, 1, 0);
        g->fn_file_size = LLVMAddFunction(g->mod, "__kn_file_size", g->fn_file_size_ty);
        g->fn_file_is_empty_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_file_is_empty = LLVMAddFunction(g->mod, "__kn_file_is_empty", g->fn_file_is_empty_ty);
        g->fn_file_copy_ty = LLVMFunctionType(g->i1, args_file2, 2, 0);
        g->fn_file_copy = LLVMAddFunction(g->mod, "__kn_file_copy", g->fn_file_copy_ty);
        g->fn_file_move_ty = LLVMFunctionType(g->i1, args_file2, 2, 0);
        g->fn_file_move = LLVMAddFunction(g->mod, "__kn_file_move", g->fn_file_move_ty);
        g->fn_file_read_lines_ty = LLVMFunctionType(t_i8ptr, args_file1, 1, 0);
        g->fn_file_read_lines = LLVMAddFunction(g->mod, "__kn_file_read_lines", g->fn_file_read_lines_ty);
        g->fn_file_write_lines_ty = LLVMFunctionType(g->i1, args_file2, 2, 0);
        g->fn_file_write_lines = LLVMAddFunction(g->mod, "__kn_file_write_lines", g->fn_file_write_lines_ty);
        g->fn_file_replace_text_ty = LLVMFunctionType(g->i1, args_file3, 3, 0);
        g->fn_file_replace_text = LLVMAddFunction(g->mod, "__kn_file_replace_text", g->fn_file_replace_text_ty);
        g->fn_dir_exists_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_dir_exists = LLVMAddFunction(g->mod, "__kn_dir_exists", g->fn_dir_exists_ty);
        g->fn_dir_create_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_dir_create = LLVMAddFunction(g->mod, "__kn_dir_create", g->fn_dir_create_ty);
        g->fn_dir_ensure_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_dir_ensure = LLVMAddFunction(g->mod, "__kn_dir_ensure", g->fn_dir_ensure_ty);
        g->fn_dir_delete_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_dir_delete = LLVMAddFunction(g->mod, "__kn_dir_delete", g->fn_dir_delete_ty);
        g->fn_dir_delete_if_exists_ty = LLVMFunctionType(g->i1, args_file1, 1, 0);
        g->fn_dir_delete_if_exists = LLVMAddFunction(g->mod, "__kn_dir_delete_if_exists", g->fn_dir_delete_if_exists_ty);
    }

    g->fn_exc_set_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_exc_set = LLVMAddFunction(g->mod, "__kn_exc_set", g->fn_exc_set_ty);
    g->fn_exc_get_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_exc_get = LLVMAddFunction(g->mod, "__kn_exc_get", g->fn_exc_get_ty);
    g->fn_exc_has_ty = LLVMFunctionType(g->i1, 0, 0, 0);
    g->fn_exc_has = LLVMAddFunction(g->mod, "__kn_exc_has", g->fn_exc_has_ty);
    g->fn_exc_last_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_exc_last = LLVMAddFunction(g->mod, "__kn_exc_last", g->fn_exc_last_ty);
    g->fn_exc_clear_ty = LLVMFunctionType(t_void, 0, 0, 0);
    g->fn_exc_clear = LLVMAddFunction(g->mod, "__kn_exc_clear", g->fn_exc_clear_ty);
    g->fn_exc_push_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_exc_push = LLVMAddFunction(g->mod, "__kn_exc_push", g->fn_exc_push_ty);
    g->fn_exc_trace_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_exc_trace = LLVMAddFunction(g->mod, "__kn_exc_trace", g->fn_exc_trace_ty);

    LLVMTypeRef exit_ty = LLVMFunctionType(t_void, &t_i32, 1, 0);
    LLVMValueRef exitp = ensure_function(g->mod, "ExitProcess", exit_ty);
    // __kn_print (no newline)
    LLVMTypeRef println_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_print_ty = println_ty;
    g->fn_print = LLVMAddFunction(g->mod, "__kn_print", println_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print, "entry");
        LLVMBasicBlockRef len_loop = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print, "len.loop");
        LLVMBasicBlockRef len_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print, "len.done");
        LLVMBasicBlockRef ret_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print, "ret");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef h = LLVMBuildCall2(g->builder, getstd_ty, getstd, (LLVMValueRef[]){ LLVMConstInt(t_i32, (uint32_t)-11, 1) }, 1, "h");
        LLVMValueRef s = LLVMGetParam(g->fn_print, 0);
        LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(t_i8ptr), "isnull");
        LLVMBuildCondBr(g->builder, isnull, ret_bb, len_loop);

        LLVMPositionBuilderAtEnd(g->builder, len_loop);
        LLVMValueRef len_alloca = LLVMBuildAlloca(g->builder, t_i32, "len");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 0, 0), len_alloca);
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print, "len.body");
        LLVMBuildBr(g->builder, loop_bb);
        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef lenv = LLVMBuildLoad2(g->builder, t_i32, len_alloca, "lenv");
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, (LLVMValueRef[]){ lenv }, 1, "p");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, ptr, "ch");
        LLVMValueRef iszero = LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, 0, 0), "isz");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print, "len.inc");
        LLVMBuildCondBr(g->builder, iszero, len_done, inc_bb);
        LLVMPositionBuilderAtEnd(g->builder, inc_bb);
        LLVMValueRef lenp1 = LLVMBuildAdd(g->builder, lenv, LLVMConstInt(t_i32, 1, 0), "lenp1");
        LLVMBuildStore(g->builder, lenp1, len_alloca);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, len_done);
        LLVMValueRef len_final = LLVMBuildLoad2(g->builder, t_i32, len_alloca, "lenf");
        LLVMValueRef written = LLVMBuildAlloca(g->builder, t_i32, "written");
        LLVMValueRef wf_args1[5] = { h, s, len_final, written, LLVMConstNull(t_i8ptr) };
        LLVMBuildCall2(g->builder, writefile_ty, writefile, wf_args1, 5, "");
        LLVMBuildBr(g->builder, ret_bb);

        LLVMPositionBuilderAtEnd(g->builder, ret_bb);
        LLVMBuildRetVoid(g->builder);
    }

    // __kn_println (with newline)
    g->fn_println_ty = println_ty;
    g->fn_println = LLVMAddFunction(g->mod, "__kn_println", println_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_println, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_println, 0);
        LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &s, 1, "");
        LLVMValueRef h = LLVMBuildCall2(g->builder, getstd_ty, getstd, (LLVMValueRef[]){ LLVMConstInt(t_i32, (uint32_t)-11, 1) }, 1, "h");
        LLVMValueRef nl = build_string_const(g, "\n", 1);
        LLVMValueRef written2 = LLVMBuildAlloca(g->builder, t_i32, "written2");
        LLVMValueRef wf_args2[5] = { h, nl, LLVMConstInt(t_i32, 1, 0), written2, LLVMConstNull(t_i8ptr) };
        LLVMBuildCall2(g->builder, writefile_ty, writefile, wf_args2, 5, "");
        LLVMBuildRetVoid(g->builder);
    }

    // __kn_panic (print message and exit)
    g->fn_panic_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_panic = LLVMAddFunction(g->mod, "__kn_panic", g->fn_panic_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_panic, "entry");
        LLVMBasicBlockRef done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_panic, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef msg = LLVMGetParam(g->fn_panic, 0);
        LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, msg, LLVMConstNull(t_i8ptr), "isnull");
        LLVMBasicBlockRef print_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_panic, "print");
        LLVMBuildCondBr(g->builder, isnull, done, print_bb);
        LLVMPositionBuilderAtEnd(g->builder, print_bb);
        LLVMBuildCall2(g->builder, g->fn_println_ty, g->fn_println, &msg, 1, "");
        LLVMBuildBr(g->builder, done);
        LLVMPositionBuilderAtEnd(g->builder, done);
        LLVMBuildCall2(g->builder, exit_ty, exitp, (LLVMValueRef[]){ LLVMConstInt(t_i32, 1, 0) }, 1, "");
        LLVMBuildUnreachable(g->builder);
    }

    // __kn_print_char
    {
        LLVMTypeRef pc_ty = LLVMFunctionType(t_void, &g->i8, 1, 0);
        g->fn_print_char_ty = pc_ty;
        g->fn_print_char = LLVMAddFunction(g->mod, "__kn_print_char", pc_ty);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_char, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef h = LLVMBuildCall2(g->builder, getstd_ty, getstd, (LLVMValueRef[]){ LLVMConstInt(t_i32, (uint32_t)-11, 1) }, 1, "h");
        LLVMValueRef ch = LLVMGetParam(g->fn_print_char, 0);
        LLVMValueRef buf = LLVMBuildAlloca(g->builder, g->i8, "buf");
        LLVMBuildStore(g->builder, ch, buf);
        LLVMValueRef written = LLVMBuildAlloca(g->builder, t_i32, "written");
        LLVMValueRef wf_args2[5] = { h, buf, LLVMConstInt(t_i32, 1, 0), written, LLVMConstNull(t_i8ptr) };
        LLVMBuildCall2(g->builder, writefile_ty, writefile, wf_args2, 5, "");
        LLVMBuildRetVoid(g->builder);
    }

    // __kn_print_bool
    {
        LLVMTypeRef pb_ty = LLVMFunctionType(t_void, &g->i1, 1, 0);
        g->fn_print_bool_ty = pb_ty;
        g->fn_print_bool = LLVMAddFunction(g->mod, "__kn_print_bool", pb_ty);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_bool, "entry");
        LLVMBasicBlockRef true_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_bool, "true");
        LLVMBasicBlockRef false_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_bool, "false");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_bool, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef b = LLVMGetParam(g->fn_print_bool, 0);
        LLVMBuildCondBr(g->builder, b, true_bb, false_bb);

        LLVMPositionBuilderAtEnd(g->builder, true_bb);
        LLVMValueRef tstr = build_string_const(g, "true", 4);
        LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &tstr, 1, "");
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, false_bb);
        LLVMValueRef fstr = build_string_const(g, "false", 5);
        LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &fstr, 1, "");
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMBuildRetVoid(g->builder);
    }

    // __kn_print_i64
    {
        LLVMTypeRef pi_ty = LLVMFunctionType(t_void, &t_i64, 1, 0);
        g->fn_print_i64_ty = pi_ty;
        g->fn_print_i64 = LLVMAddFunction(g->mod, "__kn_print_i64", pi_ty);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "entry");
        LLVMBasicBlockRef zero_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "zero");
        LLVMBasicBlockRef loop_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "loop.cond");
        LLVMBasicBlockRef loop_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "loop.body");
        LLVMBasicBlockRef loop_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "loop.done");
        LLVMBasicBlockRef sign_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "sign");
        LLVMBasicBlockRef write_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_print_i64, "write");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef val = LLVMGetParam(g->fn_print_i64, 0);
        LLVMValueRef neg = LLVMBuildICmp(g->builder, LLVMIntSLT, val, LLVMConstInt(t_i64, 0, 0), "neg");
        LLVMValueRef abs = LLVMBuildSelect(g->builder, neg, LLVMBuildNeg(g->builder, val, "nval"), val, "abs");
        LLVMValueRef abs_ptr = LLVMBuildAlloca(g->builder, t_i64, "abs");
        LLVMBuildStore(g->builder, abs, abs_ptr);
        LLVMTypeRef buf_ty = LLVMArrayType(g->i8, 32);
        LLVMValueRef buf = LLVMBuildAlloca(g->builder, buf_ty, "buf");
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i32, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 31, 0), idx);
        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, abs, LLVMConstInt(t_i64, 0, 0), "iszero");
        LLVMBuildCondBr(g->builder, is_zero, zero_bb, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, zero_bb);
        LLVMValueRef idxv0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idx0");
        LLVMValueRef ptr0 = LLVMBuildInBoundsGEP2(g->builder, buf_ty, buf, (LLVMValueRef[]){ LLVMConstInt(t_i32, 0, 0), idxv0 }, 2, "p0");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), ptr0);
        LLVMValueRef idxv0n = LLVMBuildSub(g->builder, idxv0, LLVMConstInt(t_i32, 1, 0), "idx0n");
        LLVMBuildStore(g->builder, idxv0n, idx);
        LLVMBuildBr(g->builder, loop_done);

        LLVMPositionBuilderAtEnd(g->builder, loop_cond);
        LLVMValueRef cur = LLVMBuildLoad2(g->builder, t_i64, abs_ptr, "cur");
        LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntUGT, cur, LLVMConstInt(t_i64, 0, 0), "cond");
        LLVMBuildCondBr(g->builder, cond, loop_body, loop_done);

        LLVMPositionBuilderAtEnd(g->builder, loop_body);
        LLVMValueRef curv = LLVMBuildLoad2(g->builder, t_i64, abs_ptr, "curv");
        LLVMValueRef digit = LLVMBuildURem(g->builder, curv, LLVMConstInt(t_i64, 10, 0), "digit");
        LLVMValueRef next = LLVMBuildUDiv(g->builder, curv, LLVMConstInt(t_i64, 10, 0), "next");
        LLVMBuildStore(g->builder, next, abs_ptr);
        LLVMValueRef idxv = LLVMBuildLoad2(g->builder, t_i32, idx, "idx");
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, buf_ty, buf, (LLVMValueRef[]){ LLVMConstInt(t_i32, 0, 0), idxv }, 2, "p");
        LLVMValueRef d8 = LLVMBuildTrunc(g->builder, digit, g->i8, "d8");
        LLVMValueRef ch = LLVMBuildAdd(g->builder, d8, LLVMConstInt(g->i8, '0', 0), "ch");
        LLVMBuildStore(g->builder, ch, ptr);
        LLVMValueRef idxvn = LLVMBuildSub(g->builder, idxv, LLVMConstInt(t_i32, 1, 0), "idxn");
        LLVMBuildStore(g->builder, idxvn, idx);
        LLVMBuildBr(g->builder, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, loop_done);
        LLVMBuildCondBr(g->builder, neg, sign_bb, write_bb);

        LLVMPositionBuilderAtEnd(g->builder, sign_bb);
        LLVMValueRef idxs = LLVMBuildLoad2(g->builder, t_i32, idx, "idxs");
        LLVMValueRef ptrs = LLVMBuildInBoundsGEP2(g->builder, buf_ty, buf, (LLVMValueRef[]){ LLVMConstInt(t_i32, 0, 0), idxs }, 2, "ps");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '-', 0), ptrs);
        LLVMValueRef idxsn = LLVMBuildSub(g->builder, idxs, LLVMConstInt(t_i32, 1, 0), "idxsn");
        LLVMBuildStore(g->builder, idxsn, idx);
        LLVMBuildBr(g->builder, write_bb);

        LLVMPositionBuilderAtEnd(g->builder, write_bb);
        LLVMValueRef idxf = LLVMBuildLoad2(g->builder, t_i32, idx, "idxf");
        LLVMValueRef start = LLVMBuildAdd(g->builder, idxf, LLVMConstInt(t_i32, 1, 0), "start");
        LLVMValueRef len = LLVMBuildSub(g->builder, LLVMConstInt(t_i32, 31, 0), idxf, "len");
        LLVMValueRef h = LLVMBuildCall2(g->builder, getstd_ty, getstd, (LLVMValueRef[]){ LLVMConstInt(t_i32, (uint32_t)-11, 1) }, 1, "h");
        LLVMValueRef p = LLVMBuildInBoundsGEP2(g->builder, buf_ty, buf, (LLVMValueRef[]){ LLVMConstInt(t_i32, 0, 0), start }, 2, "p");
        LLVMValueRef written = LLVMBuildAlloca(g->builder, t_i32, "written");
        LLVMValueRef wf_args3[5] = { h, p, len, written, LLVMConstNull(t_i8ptr) };
        LLVMBuildCall2(g->builder, writefile_ty, writefile, wf_args3, 5, "");
        LLVMBuildRetVoid(g->builder);
    }

    // __kn_readline
    {
        LLVMTypeRef readline_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
        g->fn_readline_ty = readline_ty;
        g->fn_readline = LLVMAddFunction(g->mod, "__kn_readline", readline_ty);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_readline, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_readline, "loop");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_readline, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef h = LLVMBuildCall2(g->builder, getstd_ty, getstd, (LLVMValueRef[]){ LLVMConstInt(t_i32, (uint32_t)-10, 1) }, 1, "h");
        LLVMValueRef buf = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
            (LLVMValueRef[]){ LLVMConstInt(t_i64, 1024, 0) }, 1, "buf");

        LLVMValueRef read = LLVMBuildAlloca(g->builder, t_i32, "read");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 0, 0), read);
        LLVMValueRef rf_args[5] = { h, buf, LLVMConstInt(t_i32, 1023, 0), read, LLVMConstNull(t_i8ptr) };
        LLVMBuildCall2(g->builder, writefile_ty, readfile, rf_args, 5, "");

        LLVMValueRef len = LLVMBuildLoad2(g->builder, t_i32, read, "len");
        LLVMValueRef len_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ len }, 1, "lenptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), len_ptr);

        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i32, "i");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 0, 0), idx);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef iv = LLVMBuildLoad2(g->builder, t_i32, idx, "iv");
        LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntSLT, iv, len, "cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_readline, "body");
        LLVMBuildCondBr(g->builder, cond, body_bb, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ iv }, 1, "p");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, ptr, "ch");
        LLVMValueRef is_nl = LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, '\n', 0), "isnl");
        LLVMValueRef is_cr = LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, '\r', 0), "iscr");
        LLVMValueRef is_break = LLVMBuildOr(g->builder, is_nl, is_cr, "isbreak");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_readline, "inc");
        LLVMBuildCondBr(g->builder, is_break, done_bb, inc_bb);

        LLVMPositionBuilderAtEnd(g->builder, inc_bb);
        LLVMValueRef iv_next = LLVMBuildAdd(g->builder, iv, LLVMConstInt(t_i32, 1, 0), "ivn");
        LLVMBuildStore(g->builder, iv_next, idx);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef iv_done = LLVMBuildLoad2(g->builder, t_i32, idx, "iv.done");
        LLVMValueRef p_done = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ iv_done }, 1, "p.done");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), p_done);
        LLVMBuildRet(g->builder, buf);
    }

    // __kn_strlen
    g->fn_strlen_ty = LLVMFunctionType(t_i64, &t_i8ptr, 1, 0);
    g->fn_strlen = LLVMAddFunction(g->mod, "__kn_strlen", g->fn_strlen_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "loop");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_strlen, 0);
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i64, "i");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), idx);
        LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(t_i8ptr), "isnull");
        LLVMBuildCondBr(g->builder, isnull, done_bb, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef iv = LLVMBuildLoad2(g->builder, t_i64, idx, "iv");
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, &iv, 1, "p");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, ptr, "ch");
        LLVMValueRef isz = LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, 0, 0), "isz");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "inc");
        LLVMBuildCondBr(g->builder, isz, done_bb, inc_bb);

        LLVMPositionBuilderAtEnd(g->builder, inc_bb);
        LLVMValueRef ivn = LLVMBuildAdd(g->builder, iv, LLVMConstInt(t_i64, 1, 0), "ivn");
        LLVMBuildStore(g->builder, ivn, idx);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef out = LLVMBuildLoad2(g->builder, t_i64, idx, "len");
        LLVMBuildRet(g->builder, out);
    }

    // __kn_str_concat
    {
        LLVMTypeRef concat_args[2] = { t_i8ptr, t_i8ptr };
        g->fn_str_concat_ty = LLVMFunctionType(t_i8ptr, concat_args, 2, 0);
        g->fn_str_concat = LLVMAddFunction(g->mod, "__kn_str_concat", g->fn_str_concat_ty);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "entry");
        LLVMBasicBlockRef copya_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copya.cond");
        LLVMBasicBlockRef copya_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copya.body");
        LLVMBasicBlockRef copya_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copya.done");
        LLVMBasicBlockRef copyb_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copyb.cond");
        LLVMBasicBlockRef copyb_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copyb.body");
        LLVMBasicBlockRef copyb_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copyb.done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef a = LLVMGetParam(g->fn_str_concat, 0);
        LLVMValueRef b = LLVMGetParam(g->fn_str_concat, 1);
        LLVMValueRef len_a = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &a, 1, "len_a");
        LLVMValueRef len_b = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &b, 1, "len_b");
        LLVMValueRef total = LLVMBuildAdd(g->builder, len_a, len_b, "total");
        total = LLVMBuildAdd(g->builder, total, LLVMConstInt(t_i64, 1, 0), "totalp1");
        LLVMValueRef buf = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                          (LLVMValueRef[]){ total }, 1, "buf");

        LLVMValueRef ia = LLVMBuildAlloca(g->builder, t_i64, "i");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), ia);
        LLVMValueRef ib = LLVMBuildAlloca(g->builder, t_i64, "j");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), ib);
        LLVMBuildBr(g->builder, copya_cond);

        LLVMPositionBuilderAtEnd(g->builder, copya_cond);
        LLVMValueRef iva = LLVMBuildLoad2(g->builder, t_i64, ia, "iva");
        LLVMValueRef cond_a = LLVMBuildICmp(g->builder, LLVMIntSLT, iva, len_a, "conda");
        LLVMBuildCondBr(g->builder, cond_a, copya_body, copya_done);

        LLVMPositionBuilderAtEnd(g->builder, copya_body);
        LLVMValueRef aptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, a, &iva, 1, "aptr");
        LLVMValueRef ach = LLVMBuildLoad2(g->builder, g->i8, aptr, "ach");
        LLVMValueRef bptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, &iva, 1, "bptr");
        LLVMBuildStore(g->builder, ach, bptr);
        LLVMValueRef iva_next = LLVMBuildAdd(g->builder, iva, LLVMConstInt(t_i64, 1, 0), "iva_next");
        LLVMBuildStore(g->builder, iva_next, ia);
        LLVMBuildBr(g->builder, copya_cond);

        LLVMPositionBuilderAtEnd(g->builder, copya_done);
        LLVMBuildBr(g->builder, copyb_cond);

        LLVMPositionBuilderAtEnd(g->builder, copyb_cond);
        LLVMValueRef ivb = LLVMBuildLoad2(g->builder, t_i64, ib, "ivb");
        LLVMValueRef cond_b = LLVMBuildICmp(g->builder, LLVMIntSLT, ivb, len_b, "condb");
        LLVMBuildCondBr(g->builder, cond_b, copyb_body, copyb_done);

        LLVMPositionBuilderAtEnd(g->builder, copyb_body);
        LLVMValueRef bptr2 = LLVMBuildInBoundsGEP2(g->builder, g->i8, b, &ivb, 1, "bptr2");
        LLVMValueRef bch = LLVMBuildLoad2(g->builder, g->i8, bptr2, "bch");
        LLVMValueRef off = LLVMBuildAdd(g->builder, len_a, ivb, "off");
        LLVMValueRef dst = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, &off, 1, "dst");
        LLVMBuildStore(g->builder, bch, dst);
        LLVMValueRef ivb_next = LLVMBuildAdd(g->builder, ivb, LLVMConstInt(t_i64, 1, 0), "ivb_next");
        LLVMBuildStore(g->builder, ivb_next, ib);
        LLVMBuildBr(g->builder, copyb_cond);

        LLVMPositionBuilderAtEnd(g->builder, copyb_done);
        LLVMValueRef end_off = LLVMBuildAdd(g->builder, len_a, len_b, "end_off");
        LLVMValueRef end_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, &end_off, 1, "end_ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), end_ptr);
        LLVMBuildRet(g->builder, buf);
    }

    // __kn_str_eq
    {
        LLVMTypeRef eq_args[2] = { t_i8ptr, t_i8ptr };
        g->fn_str_eq_ty = LLVMFunctionType(g->i1, eq_args, 2, 0);
        g->fn_str_eq = LLVMAddFunction(g->mod, "__kn_str_eq", g->fn_str_eq_ty);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "loop");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "done");
        LLVMBasicBlockRef mismatch_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "mismatch");
        LLVMBasicBlockRef false_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "false");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef a = LLVMGetParam(g->fn_str_eq, 0);
        LLVMValueRef b = LLVMGetParam(g->fn_str_eq, 1);
        LLVMValueRef isnull_a = LLVMBuildICmp(g->builder, LLVMIntEQ, a, LLVMConstNull(t_i8ptr), "anull");
        LLVMValueRef isnull_b = LLVMBuildICmp(g->builder, LLVMIntEQ, b, LLVMConstNull(t_i8ptr), "bnull");
        LLVMValueRef both_null = LLVMBuildAnd(g->builder, isnull_a, isnull_b, "bothnull");
        LLVMBasicBlockRef null_false_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "nullfalse");
        LLVMValueRef one_null = LLVMBuildXor(g->builder, isnull_a, isnull_b, "onenull");
        LLVMBuildCondBr(g->builder, both_null, done_bb, null_false_bb);

        LLVMPositionBuilderAtEnd(g->builder, null_false_bb);
        LLVMBuildCondBr(g->builder, one_null, false_bb, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i64, "i");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), idx);
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "body");
        LLVMBuildBr(g->builder, body_bb);

        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        LLVMValueRef iv = LLVMBuildLoad2(g->builder, t_i64, idx, "iv");
        LLVMValueRef ap = LLVMBuildInBoundsGEP2(g->builder, g->i8, a, &iv, 1, "ap");
        LLVMValueRef bp = LLVMBuildInBoundsGEP2(g->builder, g->i8, b, &iv, 1, "bp");
        LLVMValueRef ach = LLVMBuildLoad2(g->builder, g->i8, ap, "ach");
        LLVMValueRef bch = LLVMBuildLoad2(g->builder, g->i8, bp, "bch");
        LLVMValueRef eq = LLVMBuildICmp(g->builder, LLVMIntEQ, ach, bch, "eq");
        LLVMValueRef is_end = LLVMBuildICmp(g->builder, LLVMIntEQ, ach, LLVMConstInt(g->i8, 0, 0), "isend");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "inc");
        LLVMValueRef cont = LLVMBuildAnd(g->builder, eq, LLVMBuildNot(g->builder, is_end, "notend"), "cont");
        LLVMBuildCondBr(g->builder, cont, inc_bb, mismatch_bb);

        LLVMPositionBuilderAtEnd(g->builder, inc_bb);
        LLVMValueRef ivn = LLVMBuildAdd(g->builder, iv, LLVMConstInt(t_i64, 1, 0), "ivn");
        LLVMBuildStore(g->builder, ivn, idx);
        LLVMBuildBr(g->builder, body_bb);

        LLVMPositionBuilderAtEnd(g->builder, mismatch_bb);
        LLVMValueRef ok = LLVMBuildAnd(g->builder, eq, is_end, "ok");
        LLVMBuildRet(g->builder, ok);

        LLVMPositionBuilderAtEnd(g->builder, false_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i1, 0, 0));

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i1, 1, 0));
    }

    // __kn_mem_compare
    g->fn_mem_compare_ty = LLVMFunctionType(g->i32, (LLVMTypeRef[]){ t_i8ptr, t_i8ptr, t_i64 }, 3, 0);
    g->fn_mem_compare = LLVMAddFunction(g->mod, "__kn_mem_compare", g->fn_mem_compare_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "loop");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "body");
        LLVMBasicBlockRef less_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "less");
        LLVMBasicBlockRef greater_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "greater");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef a = LLVMGetParam(g->fn_mem_compare, 0);
        LLVMValueRef b = LLVMGetParam(g->fn_mem_compare, 1);
        LLVMValueRef n = LLVMGetParam(g->fn_mem_compare, 2);
        LLVMValueRef idx_ptr = LLVMBuildAlloca(g->builder, t_i64, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), idx_ptr);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef idx = LLVMBuildLoad2(g->builder, t_i64, idx_ptr, "idxv");
        LLVMValueRef done = LLVMBuildICmp(g->builder, LLVMIntUGE, idx, n, "done");
        LLVMBuildCondBr(g->builder, done, done_bb, body_bb);

        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        LLVMValueRef a_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, a, &idx, 1, "a.ptr");
        LLVMValueRef b_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, b, &idx, 1, "b.ptr");
        LLVMValueRef av = LLVMBuildLoad2(g->builder, g->i8, a_ptr, "av");
        LLVMValueRef bv = LLVMBuildLoad2(g->builder, g->i8, b_ptr, "bv");
        LLVMValueRef less = LLVMBuildICmp(g->builder, LLVMIntULT, av, bv, "lt");
        LLVMValueRef greater = LLVMBuildICmp(g->builder, LLVMIntUGT, av, bv, "gt");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "next");
        LLVMBuildCondBr(g->builder, less, less_bb, next_bb);

        LLVMPositionBuilderAtEnd(g->builder, next_bb);
        LLVMBuildCondBr(g->builder, greater, greater_bb, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, less_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i32, (uint64_t)-1, 1));

        LLVMPositionBuilderAtEnd(g->builder, greater_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i32, 1, 0));

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef idx_now = LLVMBuildLoad2(g->builder, t_i64, idx_ptr, "idx.now");
        LLVMValueRef still_loop = LLVMBuildICmp(g->builder, LLVMIntULT, idx_now, n, "still.loop");
        LLVMBasicBlockRef zero_ret = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "zero");
        LLVMBasicBlockRef inc_ret = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "incret");
        LLVMBuildCondBr(g->builder, still_loop, inc_ret, zero_ret);

        LLVMPositionBuilderAtEnd(g->builder, inc_ret);
        LLVMValueRef next = LLVMBuildAdd(g->builder, idx_now, LLVMConstInt(t_i64, 1, 0), "next");
        LLVMBuildStore(g->builder, next, idx_ptr);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, zero_ret);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i32, 0, 0));
    }

    // __kn_i64_to_string
    g->fn_i64_to_string_ty = LLVMFunctionType(t_i8ptr, &t_i64, 1, 0);
    g->fn_i64_to_string = LLVMAddFunction(g->mod, "__kn_i64_to_string", g->fn_i64_to_string_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "entry");
        LLVMBasicBlockRef zero_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "zero");
        LLVMBasicBlockRef loop_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "loop.cond");
        LLVMBasicBlockRef loop_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "loop.body");
        LLVMBasicBlockRef loop_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "loop.done");
        LLVMBasicBlockRef sign_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "sign");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef val = LLVMGetParam(g->fn_i64_to_string, 0);
        LLVMValueRef buf = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                          (LLVMValueRef[]){ LLVMConstInt(t_i64, 32, 0) }, 1, "buf");
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i32, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 31, 0), idx);
        LLVMValueRef idxv0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idx0");
        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv0 }, 1, "p0");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), p0);
        LLVMValueRef idxv0n = LLVMBuildSub(g->builder, idxv0, LLVMConstInt(t_i32, 1, 0), "idx0n");
        LLVMBuildStore(g->builder, idxv0n, idx);

        LLVMValueRef neg = LLVMBuildICmp(g->builder, LLVMIntSLT, val, LLVMConstInt(t_i64, 0, 0), "neg");
        LLVMValueRef abs = LLVMBuildSelect(g->builder, neg, LLVMBuildNeg(g->builder, val, "nval"), val, "abs");
        LLVMValueRef abs_ptr = LLVMBuildAlloca(g->builder, t_i64, "abs");
        LLVMBuildStore(g->builder, abs, abs_ptr);
        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, abs, LLVMConstInt(t_i64, 0, 0), "iszero");
        LLVMBuildCondBr(g->builder, is_zero, zero_bb, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, zero_bb);
        LLVMValueRef idxz = LLVMBuildLoad2(g->builder, t_i32, idx, "idxz");
        LLVMValueRef pz = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxz }, 1, "pz");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), pz);
        LLVMValueRef idxzn = LLVMBuildSub(g->builder, idxz, LLVMConstInt(t_i32, 1, 0), "idxzn");
        LLVMBuildStore(g->builder, idxzn, idx);
        LLVMBuildBr(g->builder, loop_done);

        LLVMPositionBuilderAtEnd(g->builder, loop_cond);
        LLVMValueRef cur = LLVMBuildLoad2(g->builder, t_i64, abs_ptr, "cur");
        LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntUGT, cur, LLVMConstInt(t_i64, 0, 0), "cond");
        LLVMBuildCondBr(g->builder, cond, loop_body, loop_done);

        LLVMPositionBuilderAtEnd(g->builder, loop_body);
        LLVMValueRef curv = LLVMBuildLoad2(g->builder, t_i64, abs_ptr, "curv");
        LLVMValueRef digit = LLVMBuildURem(g->builder, curv, LLVMConstInt(t_i64, 10, 0), "digit");
        LLVMValueRef next = LLVMBuildUDiv(g->builder, curv, LLVMConstInt(t_i64, 10, 0), "next");
        LLVMBuildStore(g->builder, next, abs_ptr);
        LLVMValueRef idxv = LLVMBuildLoad2(g->builder, t_i32, idx, "idx");
        LLVMValueRef p = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv }, 1, "p");
        LLVMValueRef d8 = LLVMBuildTrunc(g->builder, digit, g->i8, "d8");
        LLVMValueRef ch = LLVMBuildAdd(g->builder, d8, LLVMConstInt(g->i8, '0', 0), "ch");
        LLVMBuildStore(g->builder, ch, p);
        LLVMValueRef idxvn = LLVMBuildSub(g->builder, idxv, LLVMConstInt(t_i32, 1, 0), "idxn");
        LLVMBuildStore(g->builder, idxvn, idx);
        LLVMBuildBr(g->builder, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, loop_done);
        LLVMBuildCondBr(g->builder, neg, sign_bb, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, sign_bb);
        LLVMValueRef idxs = LLVMBuildLoad2(g->builder, t_i32, idx, "idxs");
        LLVMValueRef ps = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxs }, 1, "ps");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '-', 0), ps);
        LLVMValueRef idxsn = LLVMBuildSub(g->builder, idxs, LLVMConstInt(t_i32, 1, 0), "idxsn");
        LLVMBuildStore(g->builder, idxsn, idx);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef idxf = LLVMBuildLoad2(g->builder, t_i32, idx, "idxf");
        LLVMValueRef start = LLVMBuildAdd(g->builder, idxf, LLVMConstInt(t_i32, 1, 0), "start");
        LLVMValueRef retp = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ start }, 1, "retp");
        LLVMBuildRet(g->builder, retp);
    }

    // __kn_char_to_string
    g->fn_char_to_string_ty = LLVMFunctionType(t_i8ptr, &g->i8, 1, 0);
    g->fn_char_to_string = LLVMAddFunction(g->mod, "__kn_char_to_string", g->fn_char_to_string_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_char_to_string, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef buf = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                          (LLVMValueRef[]){ LLVMConstInt(t_i64, 2, 0) }, 1, "buf");
        LLVMValueRef ch = LLVMGetParam(g->fn_char_to_string, 0);
        LLVMBuildStore(g->builder, ch, buf);
        LLVMValueRef pz = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ LLVMConstInt(t_i32, 1, 0) }, 1, "pz");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), pz);
        LLVMBuildRet(g->builder, buf);
    }

    // __kn_ptr_to_string
    g->fn_ptr_to_string_ty = LLVMFunctionType(t_i8ptr, &t_i8ptr, 1, 0);
    g->fn_ptr_to_string = LLVMAddFunction(g->mod, "__kn_ptr_to_string", g->fn_ptr_to_string_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "entry");
        LLVMBasicBlockRef zero_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "zero");
        LLVMBasicBlockRef loop_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "loop.cond");
        LLVMBasicBlockRef loop_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "loop.body");
        LLVMBasicBlockRef prefix_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "prefix");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef p = LLVMGetParam(g->fn_ptr_to_string, 0);
        LLVMValueRef iv = LLVMBuildPtrToInt(g->builder, p, t_i64, "p2i");

        LLVMValueRef buf = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                          (LLVMValueRef[]){ LLVMConstInt(t_i64, 32, 0) }, 1, "buf");
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i32, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 31, 0), idx);
        LLVMValueRef idxv0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idx0");
        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv0 }, 1, "p0");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), p0);
        LLVMValueRef idxv0n = LLVMBuildSub(g->builder, idxv0, LLVMConstInt(t_i32, 1, 0), "idx0n");
        LLVMBuildStore(g->builder, idxv0n, idx);

        LLVMValueRef val = LLVMBuildAlloca(g->builder, t_i64, "val");
        LLVMBuildStore(g->builder, iv, val);

        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, iv, LLVMConstInt(t_i64, 0, 0), "iszero");
        LLVMBuildCondBr(g->builder, is_zero, zero_bb, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, zero_bb);
        LLVMValueRef idxz = LLVMBuildLoad2(g->builder, t_i32, idx, "idxz");
        LLVMValueRef pz = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxz }, 1, "pz");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), pz);
        LLVMValueRef idxzn = LLVMBuildSub(g->builder, idxz, LLVMConstInt(t_i32, 1, 0), "idxzn");
        LLVMBuildStore(g->builder, idxzn, idx);
        LLVMBuildBr(g->builder, prefix_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_cond);
        LLVMValueRef cur = LLVMBuildLoad2(g->builder, t_i64, val, "cur");
        LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntUGT, cur, LLVMConstInt(t_i64, 0, 0), "cond");
        LLVMBuildCondBr(g->builder, cond, loop_body, prefix_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_body);
        LLVMValueRef curv = LLVMBuildLoad2(g->builder, t_i64, val, "curv");
        LLVMValueRef digit = LLVMBuildAnd(g->builder, curv, LLVMConstInt(t_i64, 15, 0), "digit");
        LLVMValueRef next = LLVMBuildLShr(g->builder, curv, LLVMConstInt(t_i64, 4, 0), "next");
        LLVMBuildStore(g->builder, next, val);
        LLVMValueRef idxv = LLVMBuildLoad2(g->builder, t_i32, idx, "idxv");
        LLVMValueRef pd = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv }, 1, "pd");
        LLVMValueRef is_lt10 = LLVMBuildICmp(g->builder, LLVMIntULT, digit, LLVMConstInt(t_i64, 10, 0), "lt10");
        LLVMValueRef d0 = LLVMBuildTrunc(g->builder, digit, g->i8, "d0");
        LLVMValueRef ch0 = LLVMBuildAdd(g->builder, d0, LLVMConstInt(g->i8, '0', 0), "ch0");
        LLVMValueRef d10 = LLVMBuildSub(g->builder, digit, LLVMConstInt(t_i64, 10, 0), "d10");
        LLVMValueRef d10_8 = LLVMBuildTrunc(g->builder, d10, g->i8, "d10_8");
        LLVMValueRef ch1 = LLVMBuildAdd(g->builder, d10_8, LLVMConstInt(g->i8, 'a', 0), "ch1");
        LLVMValueRef ch = LLVMBuildSelect(g->builder, is_lt10, ch0, ch1, "ch");
        LLVMBuildStore(g->builder, ch, pd);
        LLVMValueRef idxvn = LLVMBuildSub(g->builder, idxv, LLVMConstInt(t_i32, 1, 0), "idxvn");
        LLVMBuildStore(g->builder, idxvn, idx);
        LLVMBuildBr(g->builder, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, prefix_bb);
        LLVMValueRef idxp = LLVMBuildLoad2(g->builder, t_i32, idx, "idxp");
        LLVMValueRef px = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxp }, 1, "px");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 'x', 0), px);
        LLVMValueRef idxpn = LLVMBuildSub(g->builder, idxp, LLVMConstInt(t_i32, 1, 0), "idxpn");
        LLVMBuildStore(g->builder, idxpn, idx);
        LLVMValueRef idxp0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idxp0");
        LLVMValueRef p0x = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxp0 }, 1, "p0x");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), p0x);
        LLVMValueRef idxp0n = LLVMBuildSub(g->builder, idxp0, LLVMConstInt(t_i32, 1, 0), "idxp0n");
        LLVMBuildStore(g->builder, idxp0n, idx);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef idxf = LLVMBuildLoad2(g->builder, t_i32, idx, "idxf");
        LLVMValueRef start = LLVMBuildAdd(g->builder, idxf, LLVMConstInt(t_i32, 1, 0), "start");
        LLVMValueRef retp = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ start }, 1, "retp");
        LLVMBuildRet(g->builder, retp);
    }

    // __kn_f64_to_string (truncate to int)
    g->fn_f64_to_string_ty = LLVMFunctionType(t_i8ptr, &g->f64, 1, 0);
    g->fn_f64_to_string = LLVMAddFunction(g->mod, "__kn_f64_to_string", g->fn_f64_to_string_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_f64_to_string, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef fv = LLVMGetParam(g->fn_f64_to_string, 0);
        LLVMValueRef iv = LLVMBuildFPToSI(g->builder, fv, t_i64, "f2i");
        LLVMValueRef retp = LLVMBuildCall2(g->builder, g->fn_i64_to_string_ty, g->fn_i64_to_string, &iv, 1, "f2s");
        LLVMBuildRet(g->builder, retp);
    }

    // __kn_string_to_i64
    g->fn_string_to_i64_ty = LLVMFunctionType(t_i64, &t_i8ptr, 1, 0);
    g->fn_string_to_i64 = LLVMAddFunction(g->mod, "__kn_string_to_i64", g->fn_string_to_i64_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "loop");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "done");
        LLVMBasicBlockRef neg_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "neg");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_string_to_i64, 0);
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i64, "i");
        LLVMValueRef acc = LLVMBuildAlloca(g->builder, t_i64, "acc");
        LLVMValueRef negflag = LLVMBuildAlloca(g->builder, g->i1, "neg");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), idx);
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), acc);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i1, 0, 0), negflag);
        LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(t_i8ptr), "isnull");
        LLVMBasicBlockRef nonnull_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "nonnull");
        LLVMBuildCondBr(g->builder, isnull, done_bb, nonnull_bb);

        LLVMPositionBuilderAtEnd(g->builder, nonnull_bb);

        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, (LLVMValueRef[]){ LLVMConstInt(t_i64, 0, 0) }, 1, "p0");
        LLVMValueRef c0 = LLVMBuildLoad2(g->builder, g->i8, p0, "c0");
        LLVMValueRef isneg = LLVMBuildICmp(g->builder, LLVMIntEQ, c0, LLVMConstInt(g->i8, '-', 0), "isneg");
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "cont");
        LLVMBuildCondBr(g->builder, isneg, neg_bb, cont_bb);

        LLVMPositionBuilderAtEnd(g->builder, neg_bb);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i1, 1, 0), negflag);
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 1, 0), idx);
        LLVMBuildBr(g->builder, cont_bb);

        LLVMPositionBuilderAtEnd(g->builder, cont_bb);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef iv = LLVMBuildLoad2(g->builder, t_i64, idx, "iv");
        LLVMValueRef p = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, &iv, 1, "p");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, p, "ch");
        LLVMValueRef is_lt = LLVMBuildICmp(g->builder, LLVMIntSLT, ch, LLVMConstInt(g->i8, '0', 0), "lt0");
        LLVMValueRef is_gt = LLVMBuildICmp(g->builder, LLVMIntSGT, ch, LLVMConstInt(g->i8, '9', 0), "gt9");
        LLVMValueRef stop = LLVMBuildOr(g->builder, is_lt, is_gt, "stop");
        LLVMBasicBlockRef step_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_i64, "step");
        LLVMBuildCondBr(g->builder, stop, done_bb, step_bb);

        LLVMPositionBuilderAtEnd(g->builder, step_bb);
        LLVMValueRef accv = LLVMBuildLoad2(g->builder, t_i64, acc, "accv");
        LLVMValueRef digit = LLVMBuildSub(g->builder, LLVMBuildSExt(g->builder, ch, t_i64, "ch64"), LLVMConstInt(t_i64, '0', 0), "digit");
        LLVMValueRef acc10 = LLVMBuildMul(g->builder, accv, LLVMConstInt(t_i64, 10, 0), "acc10");
        LLVMValueRef accn = LLVMBuildAdd(g->builder, acc10, digit, "accn");
        LLVMBuildStore(g->builder, accn, acc);
        LLVMValueRef ivn = LLVMBuildAdd(g->builder, iv, LLVMConstInt(t_i64, 1, 0), "ivn");
        LLVMBuildStore(g->builder, ivn, idx);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef accf = LLVMBuildLoad2(g->builder, t_i64, acc, "accf");
        LLVMValueRef negv = LLVMBuildLoad2(g->builder, g->i1, negflag, "negv");
        LLVMValueRef res = LLVMBuildSelect(g->builder, negv, LLVMBuildNeg(g->builder, accf, "negacc"), accf, "res");
        LLVMBuildRet(g->builder, res);
    }

    // __kn_string_to_bool
    g->fn_string_to_bool_ty = LLVMFunctionType(g->i1, &t_i8ptr, 1, 0);
    g->fn_string_to_bool = LLVMAddFunction(g->mod, "__kn_string_to_bool", g->fn_string_to_bool_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_bool, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_string_to_bool, 0);
        LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(t_i8ptr), "isnull");
        LLVMBasicBlockRef nonnull_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_bool, "nonnull");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_bool, "done");
        LLVMBuildCondBr(g->builder, isnull, done_bb, nonnull_bb);

        LLVMPositionBuilderAtEnd(g->builder, nonnull_bb);
        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, (LLVMValueRef[]){ LLVMConstInt(t_i64, 0, 0) }, 1, "p0");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, p0, "ch");
        LLVMValueRef istrue = LLVMBuildOr(g->builder,
            LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, 't', 0), "t"),
            LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, '1', 0), "1"),
            "istrue");
        LLVMBuildRet(g->builder, istrue);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i1, 0, 0));
    }

    // __kn_string_to_char
    g->fn_string_to_char_ty = LLVMFunctionType(g->i8, &t_i8ptr, 1, 0);
    g->fn_string_to_char = LLVMAddFunction(g->mod, "__kn_string_to_char", g->fn_string_to_char_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_char, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_string_to_char, 0);
        LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(t_i8ptr), "isnull");
        LLVMBasicBlockRef nonnull_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_char, "nonnull");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_char, "done");
        LLVMBuildCondBr(g->builder, isnull, done_bb, nonnull_bb);

        LLVMPositionBuilderAtEnd(g->builder, nonnull_bb);
        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, (LLVMValueRef[]){ LLVMConstInt(t_i64, 0, 0) }, 1, "p0");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, p0, "ch");
        LLVMBuildRet(g->builder, ch);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i8, 0, 0));
    }

    // __kn_string_to_f64 (via int)
    g->fn_string_to_f64_ty = LLVMFunctionType(g->f64, &t_i8ptr, 1, 0);
    g->fn_string_to_f64 = LLVMAddFunction(g->mod, "__kn_string_to_f64", g->fn_string_to_f64_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_string_to_f64, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_string_to_f64, 0);
        LLVMValueRef iv = LLVMBuildCall2(g->builder, g->fn_string_to_i64_ty, g->fn_string_to_i64, &s, 1, "s2i");
        LLVMValueRef fv = LLVMBuildSIToFP(g->builder, iv, g->f64, "i2f");
        LLVMBuildRet(g->builder, fv);
    }

    // __kn_any_to_i64
    g->fn_any_to_i64_ty = LLVMFunctionType(t_i64, &g->any_ty, 1, 0);
    g->fn_any_to_i64 = LLVMAddFunction(g->mod, "__kn_any_to_i64", g->fn_any_to_i64_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_i64, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, t_i64, "res");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), res);

        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "i64");
        LLVMBasicBlockRef case_f64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "f64");
        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "str");
        LLVMBasicBlockRef case_ptr = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "ptr");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_i64, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 5);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_BOOL, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_CHAR, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_F64, 0), case_f64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_PTR, 0), case_ptr);

        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        LLVMBuildStore(g->builder, payload, res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, case_f64);
        {
            LLVMValueRef dv = LLVMBuildBitCast(g->builder, payload, g->f64, "d");
            LLVMValueRef iv = LLVMBuildFPToSI(g->builder, dv, t_i64, "d2i");
            LLVMBuildStore(g->builder, iv, res);
            LLVMBuildBr(g->builder, done_bb);
        }

        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef sp = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "sp");
            LLVMValueRef iv = LLVMBuildCall2(g->builder, g->fn_string_to_i64_ty, g->fn_string_to_i64, &sp, 1, "s2i");
            LLVMBuildStore(g->builder, iv, res);
            LLVMBuildBr(g->builder, done_bb);
        }

        LLVMPositionBuilderAtEnd(g->builder, case_ptr);
        LLVMBuildStore(g->builder, payload, res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, t_i64, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }

    // __kn_any_to_f64
    g->fn_any_to_f64_ty = LLVMFunctionType(g->f64, &g->any_ty, 1, 0);
    g->fn_any_to_f64 = LLVMAddFunction(g->mod, "__kn_any_to_f64", g->fn_any_to_f64_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_f64, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_f64, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_f64, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, g->f64, "res");
        LLVMBuildStore(g->builder, LLVMConstReal(g->f64, 0.0), res);

        LLVMBasicBlockRef case_f64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_f64, "f64");
        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_f64, "i64");
        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_f64, "str");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_f64, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 4);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_F64, 0), case_f64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_BOOL, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_CHAR, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);

        LLVMPositionBuilderAtEnd(g->builder, case_f64);
        {
            LLVMValueRef dv = LLVMBuildBitCast(g->builder, payload, g->f64, "d");
            LLVMBuildStore(g->builder, dv, res);
            LLVMBuildBr(g->builder, done_bb);
        }

        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        {
            LLVMValueRef dv = LLVMBuildSIToFP(g->builder, payload, g->f64, "i2f");
            LLVMBuildStore(g->builder, dv, res);
            LLVMBuildBr(g->builder, done_bb);
        }

        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef sp = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "sp");
            LLVMValueRef dv = LLVMBuildCall2(g->builder, g->fn_string_to_f64_ty, g->fn_string_to_f64, &sp, 1, "s2f");
            LLVMBuildStore(g->builder, dv, res);
            LLVMBuildBr(g->builder, done_bb);
        }

        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, LLVMConstReal(g->f64, 0.0), res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, g->f64, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }

    // __kn_any_to_bool
    g->fn_any_to_bool_ty = LLVMFunctionType(g->i1, &g->any_ty, 1, 0);
    g->fn_any_to_bool = LLVMAddFunction(g->mod, "__kn_any_to_bool", g->fn_any_to_bool_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_bool, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, g->i1, "res");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i1, 0, 0), res);

        LLVMBasicBlockRef case_bool = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "bool");
        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "i64");
        LLVMBasicBlockRef case_f64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "f64");
        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "str");
        LLVMBasicBlockRef case_ptr = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "ptr");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_bool, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 5);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_BOOL, 0), case_bool);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_CHAR, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_F64, 0), case_f64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_PTR, 0), case_ptr);

        LLVMPositionBuilderAtEnd(g->builder, case_bool);
        {
            LLVMValueRef bv = LLVMBuildTrunc(g->builder, payload, g->i1, "b");
            LLVMBuildStore(g->builder, bv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        {
            LLVMValueRef zero = LLVMConstInt(t_i64, 0, 0);
            LLVMValueRef bv = LLVMBuildICmp(g->builder, LLVMIntNE, payload, zero, "b");
            LLVMBuildStore(g->builder, bv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_f64);
        {
            LLVMValueRef dv = LLVMBuildBitCast(g->builder, payload, g->f64, "d");
            LLVMValueRef bv = LLVMBuildFCmp(g->builder, LLVMRealONE, dv, LLVMConstReal(g->f64, 0.0), "b");
            LLVMBuildStore(g->builder, bv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef sp = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "sp");
            LLVMValueRef bv = LLVMBuildCall2(g->builder, g->fn_string_to_bool_ty, g->fn_string_to_bool, &sp, 1, "s2b");
            LLVMBuildStore(g->builder, bv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_ptr);
        {
            LLVMValueRef zero = LLVMConstInt(t_i64, 0, 0);
            LLVMValueRef bv = LLVMBuildICmp(g->builder, LLVMIntNE, payload, zero, "b");
            LLVMBuildStore(g->builder, bv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i1, 0, 0), res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, g->i1, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }

    // __kn_any_to_char
    g->fn_any_to_char_ty = LLVMFunctionType(g->i8, &g->any_ty, 1, 0);
    g->fn_any_to_char = LLVMAddFunction(g->mod, "__kn_any_to_char", g->fn_any_to_char_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_char, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_char, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_char, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, g->i8, "res");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), res);

        LLVMBasicBlockRef case_char = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_char, "char");
        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_char, "i64");
        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_char, "str");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_char, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 4);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_CHAR, 0), case_char);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_BOOL, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);

        LLVMPositionBuilderAtEnd(g->builder, case_char);
        {
            LLVMValueRef cv = LLVMBuildTrunc(g->builder, payload, g->i8, "c");
            LLVMBuildStore(g->builder, cv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        {
            LLVMValueRef cv = LLVMBuildTrunc(g->builder, payload, g->i8, "c");
            LLVMBuildStore(g->builder, cv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef sp = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "sp");
            LLVMValueRef cv = LLVMBuildCall2(g->builder, g->fn_string_to_char_ty, g->fn_string_to_char, &sp, 1, "s2c");
            LLVMBuildStore(g->builder, cv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, g->i8, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }

    // __kn_any_to_ptr
    g->fn_any_to_ptr_ty = LLVMFunctionType(t_i8ptr, &g->any_ty, 1, 0);
    g->fn_any_to_ptr = LLVMAddFunction(g->mod, "__kn_any_to_ptr", g->fn_any_to_ptr_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_ptr, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_ptr, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_ptr, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, t_i8ptr, "res");
        LLVMBuildStore(g->builder, LLVMConstNull(t_i8ptr), res);

        LLVMBasicBlockRef case_ptr = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_ptr, "ptr");
        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_ptr, "str");
        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_ptr, "i64");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_ptr, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 6);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_PTR, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_DICT, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_LIST, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_SET, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);

        LLVMPositionBuilderAtEnd(g->builder, case_ptr);
        {
            LLVMValueRef pv = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "p");
            LLVMBuildStore(g->builder, pv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef pv = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "p");
            LLVMBuildStore(g->builder, pv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        {
            LLVMValueRef pv = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "p");
            LLVMBuildStore(g->builder, pv, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, LLVMConstNull(t_i8ptr), res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, t_i8ptr, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }

    // __kn_any_to_string
    g->fn_any_to_string_ty = LLVMFunctionType(t_i8ptr, &g->any_ty, 1, 0);
    g->fn_any_to_string = LLVMAddFunction(g->mod, "__kn_any_to_string", g->fn_any_to_string_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_string, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, t_i8ptr, "res");
        LLVMValueRef unknown = build_string_const(g, "<any>", 5);
        LLVMBuildStore(g->builder, unknown, res);

        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "str");
        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "i64");
        LLVMBasicBlockRef case_f64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "f64");
        LLVMBasicBlockRef case_bool = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "bool");
        LLVMBasicBlockRef case_char = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "char");
        LLVMBasicBlockRef case_ptr = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "ptr");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 10);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_BOOL, 0), case_bool);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_F64, 0), case_f64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_CHAR, 0), case_char);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_PTR, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_DICT, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_LIST, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_SET, 0), case_ptr);

        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef sp = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "sp");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        {
            LLVMValueRef iv = payload;
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_i64_to_string_ty, g->fn_i64_to_string, &iv, 1, "i2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_f64);
        {
            LLVMValueRef dv = LLVMBuildBitCast(g->builder, payload, g->f64, "d");
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_f64_to_string_ty, g->fn_f64_to_string, &dv, 1, "f2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_bool);
        {
            LLVMValueRef is_true = LLVMBuildICmp(g->builder, LLVMIntNE, payload, LLVMConstInt(t_i64, 0, 0), "b");
            LLVMValueRef tstr = build_string_const(g, "true", 4);
            LLVMValueRef fstr = build_string_const(g, "false", 5);
            LLVMValueRef sp = LLVMBuildSelect(g->builder, is_true, tstr, fstr, "bstr");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_char);
        {
            LLVMValueRef cv = LLVMBuildTrunc(g->builder, payload, g->i8, "c");
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_char_to_string_ty, g->fn_char_to_string, &cv, 1, "c2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_ptr);
        {
            LLVMValueRef pv = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "p");
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_ptr_to_string_ty, g->fn_ptr_to_string, &pv, 1, "p2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, unknown, res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, t_i8ptr, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }

    // Time functions
    g->fn_time_tick_ty = LLVMFunctionType(t_i64, 0, 0, 0);
    g->fn_time_tick = LLVMAddFunction(g->mod, "__kn_time_tick", g->fn_time_tick_ty);
    g->fn_time_sleep_ty = LLVMFunctionType(t_void, &t_i32, 1, 0);
    g->fn_time_sleep = LLVMAddFunction(g->mod, "__kn_time_sleep", g->fn_time_sleep_ty);
    {
        LLVMTypeRef args_parts[8] = {
            LLVMPointerType(t_i32, 0), LLVMPointerType(t_i32, 0),
            LLVMPointerType(t_i32, 0), LLVMPointerType(t_i32, 0),
            LLVMPointerType(t_i32, 0), LLVMPointerType(t_i32, 0),
            LLVMPointerType(t_i32, 0), LLVMPointerType(t_i64, 0)
        };
        g->fn_time_parts_ty = LLVMFunctionType(t_void, args_parts, 8, 0);
        g->fn_time_parts = LLVMAddFunction(g->mod, "__kn_time_now_parts", g->fn_time_parts_ty);
    }
    {
        LLVMTypeRef args_fmt[2] = { t_i64, t_i8ptr };
        g->fn_time_format_ty = LLVMFunctionType(t_i8ptr, args_fmt, 2, 0);
        g->fn_time_format = LLVMAddFunction(g->mod, "__kn_time_format", g->fn_time_format_ty);
    }
    g->fn_time_nano_ty = LLVMFunctionType(t_i64, 0, 0, 0);
    g->fn_time_nano = LLVMAddFunction(g->mod, "__kn_time_nano", g->fn_time_nano_ty);

    if (g->emit_entry)
    {
        if (g->target_is_windows)
        {
            LLVMTypeRef main_args[2] = { t_i32, LLVMPointerType(t_i8ptr, 0) };
            LLVMTypeRef main_ty = LLVMFunctionType(t_i32, main_args, 2, 0);
            LLVMValueRef main_fn = LLVMAddFunction(g->mod, "main", main_ty);
            LLVMBasicBlockRef m_entry = LLVMAppendBasicBlockInContext(g->ctx, main_fn, "entry");
            LLVMPositionBuilderAtEnd(g->builder, m_entry);

            LLVMValueRef cp65001 = LLVMConstInt(t_i32, 65001u, 0);
            LLVMBuildCall2(g->builder, setcp_ty, set_console_output_cp, &cp65001, 1, "");
            LLVMBuildCall2(g->builder, setcp_ty, set_console_input_cp, &cp65001, 1, "");
            {
                LLVMValueRef stdout_handle = LLVMBuildCall2(g->builder, getstd_ty, getstd, (LLVMValueRef[]){ LLVMConstInt(t_i32, (uint32_t)-11, 1) }, 1, "stdout.handle");
                LLVMValueRef mode_ptr = LLVMBuildAlloca(g->builder, t_i32, "stdout.mode");
                LLVMValueRef ok_mode = LLVMBuildCall2(g->builder, get_console_mode_ty, get_console_mode,
                    (LLVMValueRef[]){ stdout_handle, mode_ptr }, 2, "stdout.mode.ok");
                LLVMValueRef has_mode = LLVMBuildICmp(g->builder, LLVMIntNE, ok_mode, LLVMConstInt(g->i1, 0, 0), "stdout.mode.cond");
                LLVMBasicBlockRef vt_yes = LLVMAppendBasicBlockInContext(g->ctx, main_fn, "stdout.vt.yes");
                LLVMBasicBlockRef vt_no = LLVMAppendBasicBlockInContext(g->ctx, main_fn, "stdout.vt.no");
                LLVMBuildCondBr(g->builder, has_mode, vt_yes, vt_no);

                LLVMPositionBuilderAtEnd(g->builder, vt_yes);
                {
                    LLVMValueRef mode = LLVMBuildLoad2(g->builder, t_i32, mode_ptr, "stdout.mode.value");
                    LLVMValueRef vt_mode = LLVMBuildOr(g->builder, mode, LLVMConstInt(t_i32, 0x0004u, 0), "stdout.mode.vt");
                    LLVMBuildCall2(g->builder, set_console_mode_ty, set_console_mode,
                        (LLVMValueRef[]){ stdout_handle, vt_mode }, 2, "");
                    LLVMBuildBr(g->builder, vt_no);
                }

                LLVMPositionBuilderAtEnd(g->builder, vt_no);
            }

            LLVMValueRef rc_final = LLVMConstInt(t_i32, 1, 0);
                if (g->fn_main)
                {
                    if (g->fn_gc_init)
                        LLVMBuildCall2(g->builder, g->fn_gc_init_ty, g->fn_gc_init, 0, 0, "");
                    if (g->fn_meta_get_module && g->fn_meta_register_main)
                    {
                        LLVMValueRef meta_mod = LLVMBuildCall2(g->builder, g->fn_meta_get_module_ty, g->fn_meta_get_module, 0, 0, "meta.mod");
                        LLVMBuildCall2(g->builder, g->fn_meta_register_main_ty, g->fn_meta_register_main, &meta_mod, 1, "");
                    }
                    LLVMTypeRef fn_ty = g->fn_main_ty ? g->fn_main_ty : fn_type(g->fn_main);
                unsigned param_count = LLVMCountParams(g->fn_main);
                LLVMValueRef retv;
                if (param_count == 0)
                {
                    retv = LLVMBuildCall2(g->builder, fn_ty, g->fn_main, 0, 0, "");
                }
                else
                {
                    Type arr = type_array(TY_STRING, -1);
                    LLVMValueRef argc = LLVMGetParam(main_fn, 0);
                    LLVMValueRef argv = LLVMGetParam(main_fn, 1);
                    LLVMTypeRef data_ptr_ty = LLVMPointerType(t_i8ptr, 0);
                    LLVMValueRef data_tmp = build_entry_alloca(g, main_fn, data_ptr_ty, "args.data");
                    LLVMValueRef len_tmp = build_entry_alloca(g, main_fn, g->i64, "args.len");
                    LLVMBuildCall2(
                        g->builder,
                        g->fn_sys_args_from_argv_ty,
                        g->fn_sys_args_from_argv,
                        (LLVMValueRef[]){ argc, argv, data_tmp, len_tmp },
                        4,
                        "");
                    LLVMValueRef args_data = LLVMBuildLoad2(g->builder, data_ptr_ty, data_tmp, "args.data.v");
                    LLVMValueRef args_len = LLVMBuildLoad2(g->builder, g->i64, len_tmp, "args.len.v");
                    LLVMValueRef args_val = LLVMGetUndef(llvm_type(g, arr));
                    args_val = LLVMBuildInsertValue(g->builder, args_val, args_data, 0, "");
                    args_val = LLVMBuildInsertValue(g->builder, args_val, args_len, 1, "");
                    retv = LLVMBuildCall2(g->builder, fn_ty, g->fn_main, &args_val, 1, "");
                }
                if (g->fn_main_is_async)
                    retv = entry_await_task_result(g, retv, g->fn_main_ret_type);
                rc_final = entry_result_to_i32(g, g->fn_main_ret_type, retv);
                rc_final = finalize_entry_return_code(g, t_i32, rc_final);
                if (g->fn_gc_collect)
                    LLVMBuildCall2(g->builder, g->fn_gc_collect_ty, g->fn_gc_collect, 0, 0, "");
            }
            LLVMBuildRet(g->builder, rc_final);
        }
        else
        {
            LLVMTypeRef main_args[2] = { t_i32, LLVMPointerType(t_i8ptr, 0) };
            LLVMTypeRef main_ty = LLVMFunctionType(t_i32, main_args, 2, 0);
            LLVMValueRef main_fn = LLVMAddFunction(g->mod, "main", main_ty);
            LLVMBasicBlockRef m_entry = LLVMAppendBasicBlockInContext(g->ctx, main_fn, "entry");
            LLVMPositionBuilderAtEnd(g->builder, m_entry);

            LLVMValueRef rc_final = LLVMConstInt(t_i32, 1, 0);
                if (g->fn_main)
                {
                    if (g->fn_gc_init)
                        LLVMBuildCall2(g->builder, g->fn_gc_init_ty, g->fn_gc_init, 0, 0, "");
                    if (g->fn_meta_get_module && g->fn_meta_register_main)
                    {
                        LLVMValueRef meta_mod = LLVMBuildCall2(g->builder, g->fn_meta_get_module_ty, g->fn_meta_get_module, 0, 0, "meta.mod");
                        LLVMBuildCall2(g->builder, g->fn_meta_register_main_ty, g->fn_meta_register_main, &meta_mod, 1, "");
                    }
                    LLVMTypeRef fn_ty = g->fn_main_ty ? g->fn_main_ty : fn_type(g->fn_main);
                unsigned param_count = LLVMCountParams(g->fn_main);
                LLVMValueRef retv;
                if (param_count == 0)
                {
                    retv = LLVMBuildCall2(g->builder, fn_ty, g->fn_main, 0, 0, "");
                }
                else
                {
                    Type arr = type_array(TY_STRING, -1);
                    LLVMValueRef argc = LLVMGetParam(main_fn, 0);
                    LLVMValueRef argv = LLVMGetParam(main_fn, 1);
                    LLVMTypeRef data_ptr_ty = LLVMPointerType(t_i8ptr, 0);
                    LLVMValueRef data_tmp = build_entry_alloca(g, main_fn, data_ptr_ty, "args.data");
                    LLVMValueRef len_tmp = build_entry_alloca(g, main_fn, g->i64, "args.len");
                    LLVMBuildCall2(
                        g->builder,
                        g->fn_sys_args_from_argv_ty,
                        g->fn_sys_args_from_argv,
                        (LLVMValueRef[]){ argc, argv, data_tmp, len_tmp },
                        4,
                        "");
                    LLVMValueRef args_data = LLVMBuildLoad2(g->builder, data_ptr_ty, data_tmp, "args.data.v");
                    LLVMValueRef args_len = LLVMBuildLoad2(g->builder, g->i64, len_tmp, "args.len.v");
                    LLVMValueRef args_val = LLVMGetUndef(llvm_type(g, arr));
                    args_val = LLVMBuildInsertValue(g->builder, args_val, args_data, 0, "");
                    args_val = LLVMBuildInsertValue(g->builder, args_val, args_len, 1, "");
                    retv = LLVMBuildCall2(g->builder, fn_ty, g->fn_main, &args_val, 1, "");
                }
                if (g->fn_main_is_async)
                    retv = entry_await_task_result(g, retv, g->fn_main_ret_type);
                rc_final = entry_result_to_i32(g, g->fn_main_ret_type, retv);
                rc_final = finalize_entry_return_code(g, t_i32, rc_final);
                if (g->fn_gc_collect)
                    LLVMBuildCall2(g->builder, g->fn_gc_collect_ty, g->fn_gc_collect, 0, 0, "");
            }
            LLVMBuildRet(g->builder, rc_final);
        }
    }
}

static void declare_freestanding_optional_runtime(Codegen *g)
{
    LLVMTypeRef t_i8ptr = g->i8ptr;
    LLVMTypeRef t_i32 = g->i32;
    LLVMTypeRef t_i64 = g->i64;
    LLVMTypeRef t_void = g->voidt;

    if (!g || g->runtime_mode == KN_RUNTIME_NONE)
        return;

    g->fn_gc_alloc_ty = LLVMFunctionType(t_i8ptr, &t_i64, 1, 0);
    g->fn_gc_alloc = LLVMAddFunction(g->mod, "__kn_gc_alloc", g->fn_gc_alloc_ty);
    g->fn_gc_collect_ty = LLVMFunctionType(t_void, 0, 0, 0);
    g->fn_gc_collect = LLVMAddFunction(g->mod, "__kn_gc_collect", g->fn_gc_collect_ty);
    g->fn_gc_push_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_gc_push = LLVMAddFunction(g->mod, "__kn_gc_push_frame", g->fn_gc_push_ty);
    LLVMTypeRef gc_root_args[3] = { t_i8ptr, t_i8ptr, t_i64 };
    g->fn_gc_add_root_ty = LLVMFunctionType(t_void, gc_root_args, 3, 0);
    g->fn_gc_add_root = LLVMAddFunction(g->mod, "__kn_gc_add_root", g->fn_gc_add_root_ty);
    g->fn_gc_pop_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_gc_pop = LLVMAddFunction(g->mod, "__kn_gc_pop_frame", g->fn_gc_pop_ty);
    g->fn_gc_init_ty = LLVMFunctionType(t_void, 0, 0, 0);
    g->fn_gc_init = LLVMAddFunction(g->mod, "__kn_gc_init", g->fn_gc_init_ty);

    LLVMTypeRef str2_ty = LLVMFunctionType(t_i8ptr, (LLVMTypeRef[]){ t_i8ptr, t_i8ptr }, 2, 0);
    g->fn_str_concat_ty = str2_ty;
    g->fn_str_concat = LLVMAddFunction(g->mod, "__kn_str_concat", str2_ty);
    g->fn_any_to_string_ty = LLVMFunctionType(t_i8ptr, &g->any_ty, 1, 0);
    g->fn_any_to_string = LLVMAddFunction(g->mod, "__kn_any_to_string", g->fn_any_to_string_ty);
    {
        LLVMTypeRef args_s2c[2] = { t_i8ptr, LLVMPointerType(t_i64, 0) };
        g->fn_string_to_chars_ty = LLVMFunctionType(t_i8ptr, args_s2c, 2, 0);
        g->fn_string_to_chars = LLVMAddFunction(g->mod, "__kn_string_to_chars", g->fn_string_to_chars_ty);
    }

    g->fn_dict_new_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_dict_new = LLVMAddFunction(g->mod, "__kn_dict_new", g->fn_dict_new_ty);
    {
        LLVMTypeRef args_set[4] = { t_i8ptr, t_i8ptr, t_i64, t_i64 };
        g->fn_dict_set_ty = LLVMFunctionType(t_void, args_set, 4, 0);
        g->fn_dict_set = LLVMAddFunction(g->mod, "__kn_dict_set", g->fn_dict_set_ty);
        LLVMTypeRef args_get[4] = { t_i8ptr, t_i8ptr, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_dict_get_ty = LLVMFunctionType(t_void, args_get, 4, 0);
        g->fn_dict_get = LLVMAddFunction(g->mod, "__kn_dict_get", g->fn_dict_get_ty);
        LLVMTypeRef args_has[2] = { t_i8ptr, t_i8ptr };
        g->fn_dict_contains_ty = LLVMFunctionType(g->i1, args_has, 2, 0);
        g->fn_dict_contains = LLVMAddFunction(g->mod, "__kn_dict_contains", g->fn_dict_contains_ty);
        LLVMTypeRef args_count[1] = { t_i8ptr };
        g->fn_dict_count_ty = LLVMFunctionType(t_i64, args_count, 1, 0);
        g->fn_dict_count = LLVMAddFunction(g->mod, "__kn_dict_count", g->fn_dict_count_ty);
        g->fn_dict_remove_ty = LLVMFunctionType(g->i1, args_has, 2, 0);
        g->fn_dict_remove = LLVMAddFunction(g->mod, "__kn_dict_remove", g->fn_dict_remove_ty);
        g->fn_dict_is_empty_ty = LLVMFunctionType(g->i1, args_count, 1, 0);
        g->fn_dict_is_empty = LLVMAddFunction(g->mod, "__kn_dict_is_empty", g->fn_dict_is_empty_ty);
        g->fn_dict_keys_ty = LLVMFunctionType(t_i8ptr, args_count, 1, 0);
        g->fn_dict_keys = LLVMAddFunction(g->mod, "__kn_dict_keys", g->fn_dict_keys_ty);
        g->fn_dict_values_ty = LLVMFunctionType(t_i8ptr, args_count, 1, 0);
        g->fn_dict_values = LLVMAddFunction(g->mod, "__kn_dict_values", g->fn_dict_values_ty);
    }

    g->fn_list_new_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_list_new = LLVMAddFunction(g->mod, "__kn_list_new", g->fn_list_new_ty);
    {
        LLVMTypeRef args_add[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_list_add_ty = LLVMFunctionType(t_void, args_add, 3, 0);
        g->fn_list_add = LLVMAddFunction(g->mod, "__kn_list_add", g->fn_list_add_ty);
        LLVMTypeRef args_get[4] = { t_i8ptr, t_i64, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_list_get_ty = LLVMFunctionType(t_void, args_get, 4, 0);
        g->fn_list_get = LLVMAddFunction(g->mod, "__kn_list_get", g->fn_list_get_ty);
        LLVMTypeRef args_set[4] = { t_i8ptr, t_i64, t_i64, t_i64 };
        g->fn_list_set_ty = LLVMFunctionType(t_void, args_set, 4, 0);
        g->fn_list_set = LLVMAddFunction(g->mod, "__kn_list_set", g->fn_list_set_ty);
        g->fn_list_insert_ty = LLVMFunctionType(t_void, args_set, 4, 0);
        g->fn_list_insert = LLVMAddFunction(g->mod, "__kn_list_insert", g->fn_list_insert_ty);
        LLVMTypeRef args_count[1] = { t_i8ptr };
        g->fn_list_count_ty = LLVMFunctionType(t_i64, args_count, 1, 0);
        g->fn_list_count = LLVMAddFunction(g->mod, "__kn_list_count", g->fn_list_count_ty);
        g->fn_list_clear_ty = LLVMFunctionType(t_void, args_count, 1, 0);
        g->fn_list_clear = LLVMAddFunction(g->mod, "__kn_list_clear", g->fn_list_clear_ty);
        LLVMTypeRef args_rm[4] = { t_i8ptr, t_i64, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_list_remove_at_ty = LLVMFunctionType(t_void, args_rm, 4, 0);
        g->fn_list_remove_at = LLVMAddFunction(g->mod, "__kn_list_remove_at", g->fn_list_remove_at_ty);
        LLVMTypeRef args_contains[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_list_contains_ty = LLVMFunctionType(g->i1, args_contains, 3, 0);
        g->fn_list_contains = LLVMAddFunction(g->mod, "__kn_list_contains", g->fn_list_contains_ty);
        LLVMTypeRef args_pop[3] = { t_i8ptr, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_list_pop_ty = LLVMFunctionType(t_void, args_pop, 3, 0);
        g->fn_list_pop = LLVMAddFunction(g->mod, "__kn_list_pop", g->fn_list_pop_ty);
        g->fn_list_index_of_ty = LLVMFunctionType(t_i64, args_contains, 3, 0);
        g->fn_list_index_of = LLVMAddFunction(g->mod, "__kn_list_index_of", g->fn_list_index_of_ty);
        g->fn_list_is_empty_ty = LLVMFunctionType(g->i1, args_count, 1, 0);
        g->fn_list_is_empty = LLVMAddFunction(g->mod, "__kn_list_is_empty", g->fn_list_is_empty_ty);
    }

    g->fn_set_new_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_set_new = LLVMAddFunction(g->mod, "__kn_set_new", g->fn_set_new_ty);
    {
        LLVMTypeRef args_add[2] = { t_i8ptr, t_i8ptr };
        g->fn_set_add_ty = LLVMFunctionType(t_void, args_add, 2, 0);
        g->fn_set_add = LLVMAddFunction(g->mod, "__kn_set_add", g->fn_set_add_ty);
        g->fn_set_contains_ty = LLVMFunctionType(g->i1, args_add, 2, 0);
        g->fn_set_contains = LLVMAddFunction(g->mod, "__kn_set_contains", g->fn_set_contains_ty);
        LLVMTypeRef args_count[1] = { t_i8ptr };
        g->fn_set_count_ty = LLVMFunctionType(t_i64, args_count, 1, 0);
        g->fn_set_count = LLVMAddFunction(g->mod, "__kn_set_count", g->fn_set_count_ty);
        g->fn_set_clear_ty = LLVMFunctionType(t_void, args_count, 1, 0);
        g->fn_set_clear = LLVMAddFunction(g->mod, "__kn_set_clear", g->fn_set_clear_ty);
        g->fn_set_is_empty_ty = LLVMFunctionType(g->i1, args_count, 1, 0);
        g->fn_set_is_empty = LLVMAddFunction(g->mod, "__kn_set_is_empty", g->fn_set_is_empty_ty);
        g->fn_set_values_ty = LLVMFunctionType(t_i8ptr, args_count, 1, 0);
        g->fn_set_values = LLVMAddFunction(g->mod, "__kn_set_values", g->fn_set_values_ty);
        g->fn_set_remove_ty = LLVMFunctionType(g->i1, args_add, 2, 0);
        g->fn_set_remove = LLVMAddFunction(g->mod, "__kn_set_remove", g->fn_set_remove_ty);
    }

    g->fn_exc_set_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_exc_set = LLVMAddFunction(g->mod, "__kn_exc_set", g->fn_exc_set_ty);
    g->fn_exc_get_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_exc_get = LLVMAddFunction(g->mod, "__kn_exc_get", g->fn_exc_get_ty);
    g->fn_exc_has_ty = LLVMFunctionType(g->i1, 0, 0, 0);
    g->fn_exc_has = LLVMAddFunction(g->mod, "__kn_exc_has", g->fn_exc_has_ty);
    g->fn_exc_last_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_exc_last = LLVMAddFunction(g->mod, "__kn_exc_last", g->fn_exc_last_ty);
    g->fn_exc_clear_ty = LLVMFunctionType(t_void, 0, 0, 0);
    g->fn_exc_clear = LLVMAddFunction(g->mod, "__kn_exc_clear", g->fn_exc_clear_ty);
    g->fn_exc_push_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_exc_push = LLVMAddFunction(g->mod, "__kn_exc_push", g->fn_exc_push_ty);
    g->fn_exc_trace_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
    g->fn_exc_trace = LLVMAddFunction(g->mod, "__kn_exc_trace", g->fn_exc_trace_ty);

    {
        LLVMTypeRef args_spawn[2] = { t_i8ptr, t_i8ptr };
        g->fn_async_task_spawn_ty = LLVMFunctionType(t_i8ptr, args_spawn, 2, 0);
        g->fn_async_task_spawn = LLVMAddFunction(g->mod, "__kn_async_task_spawn", g->fn_async_task_spawn_ty);
        LLVMTypeRef args_run[3] = { t_i8ptr, LLVMPointerType(t_i64, 0), LLVMPointerType(t_i64, 0) };
        g->fn_async_task_run_ty = LLVMFunctionType(t_void, args_run, 3, 0);
        g->fn_async_task_run = LLVMAddFunction(g->mod, "__kn_async_task_run", g->fn_async_task_run_ty);
        LLVMTypeRef args_complete[3] = { t_i8ptr, t_i64, t_i64 };
        g->fn_async_task_complete_ty = LLVMFunctionType(t_void, args_complete, 3, 0);
        g->fn_async_task_complete = LLVMAddFunction(g->mod, "__kn_async_task_complete", g->fn_async_task_complete_ty);
        LLVMTypeRef args_fail[3] = { t_i8ptr, t_i8ptr, t_i8ptr };
        g->fn_async_task_fail_ty = LLVMFunctionType(t_void, args_fail, 3, 0);
        g->fn_async_task_fail = LLVMAddFunction(g->mod, "__kn_async_task_fail", g->fn_async_task_fail_ty);
        g->fn_async_task_sleep_ty = LLVMFunctionType(t_i8ptr, &t_i32, 1, 0);
        g->fn_async_task_sleep = LLVMAddFunction(g->mod, "__kn_async_task_sleep", g->fn_async_task_sleep_ty);
        g->fn_async_task_yield_ty = LLVMFunctionType(t_i8ptr, 0, 0, 0);
        g->fn_async_task_yield = LLVMAddFunction(g->mod, "__kn_async_task_yield", g->fn_async_task_yield_ty);
    }
}

static void ensure_freestanding_scratch_storage(Codegen *g,
                                                LLVMValueRef *ring_out,
                                                LLVMTypeRef *ring_ty_out,
                                                LLVMTypeRef *slot_ty_out,
                                                LLVMValueRef *index_out)
{
    const unsigned scratch_count = 8;
    const unsigned scratch_size = 512;
    LLVMTypeRef slot_ty = LLVMArrayType(g->i8, scratch_size);
    LLVMTypeRef ring_ty = LLVMArrayType(slot_ty, scratch_count);

    LLVMValueRef ring = LLVMGetNamedGlobal(g->mod, "__kn_fs_scratch_ring");
    if (!ring)
    {
        ring = LLVMAddGlobal(g->mod, ring_ty, "__kn_fs_scratch_ring");
        LLVMSetLinkage(ring, LLVMInternalLinkage);
        LLVMSetInitializer(ring, LLVMConstNull(ring_ty));
    }

    LLVMValueRef index = LLVMGetNamedGlobal(g->mod, "__kn_fs_scratch_index");
    if (!index)
    {
        index = LLVMAddGlobal(g->mod, g->i64, "__kn_fs_scratch_index");
        LLVMSetLinkage(index, LLVMInternalLinkage);
        LLVMSetInitializer(index, LLVMConstInt(g->i64, 0, 0));
    }
    if (ring_out) *ring_out = ring;
    if (ring_ty_out) *ring_ty_out = ring_ty;
    if (slot_ty_out) *slot_ty_out = slot_ty;
    if (index_out) *index_out = index;
}

static LLVMValueRef build_freestanding_scratch_acquire(Codegen *g,
                                                       LLVMValueRef ring,
                                                       LLVMTypeRef ring_ty,
                                                       LLVMTypeRef slot_ty,
                                                       LLVMValueRef index)
{
    const uint64_t scratch_count = 8;
    LLVMValueRef idx = LLVMBuildLoad2(g->builder, g->i64, index, "scratch.idx");
    LLVMValueRef next = LLVMBuildAdd(g->builder, idx, LLVMConstInt(g->i64, 1, 0), "scratch.next");
    LLVMValueRef wrap = LLVMBuildICmp(g->builder, LLVMIntUGE, next, LLVMConstInt(g->i64, scratch_count, 0), "scratch.wrap");
    LLVMValueRef next_wrapped = LLVMBuildSelect(g->builder, wrap, LLVMConstInt(g->i64, 0, 0), next, "scratch.next.wrapped");
    LLVMBuildStore(g->builder, next_wrapped, index);

    LLVMValueRef zero = LLVMConstInt(g->i64, 0, 0);
    LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(g->builder, ring_ty, ring, (LLVMValueRef[]){ zero, idx }, 2, "scratch.slot.ptr");
    return LLVMBuildInBoundsGEP2(g->builder, slot_ty, slot_ptr, (LLVMValueRef[]){ zero, zero }, 2, "scratch.data.ptr");
}

static void build_freestanding_core_string_helpers(Codegen *g)
{
    if (!g || g->runtime_mode != KN_RUNTIME_NONE)
        return;

    const uint64_t scratch_size = 512;
    LLVMTypeRef t_i8ptr = g->i8ptr;
    LLVMTypeRef t_i64 = g->i64;
    LLVMTypeRef t_i32 = g->i32;
    LLVMValueRef scratch_ring = 0;
    LLVMValueRef scratch_index = 0;
    LLVMTypeRef scratch_ring_ty = 0;
    LLVMTypeRef scratch_slot_ty = 0;
    ensure_freestanding_scratch_storage(g, &scratch_ring, &scratch_ring_ty, &scratch_slot_ty, &scratch_index);

    // __kn_str_concat
    {
        LLVMTypeRef concat_args[2] = { t_i8ptr, t_i8ptr };
        g->fn_str_concat_ty = LLVMFunctionType(t_i8ptr, concat_args, 2, 0);
        g->fn_str_concat = LLVMAddFunction(g->mod, "__kn_str_concat", g->fn_str_concat_ty);
        LLVMSetLinkage(g->fn_str_concat, LLVMInternalLinkage);

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "entry");
        LLVMBasicBlockRef copya_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copya.cond");
        LLVMBasicBlockRef copya_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copya.body");
        LLVMBasicBlockRef copya_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copya.done");
        LLVMBasicBlockRef copyb_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copyb.cond");
        LLVMBasicBlockRef copyb_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copyb.body");
        LLVMBasicBlockRef copyb_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_concat, "copyb.done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef a = LLVMGetParam(g->fn_str_concat, 0);
        LLVMValueRef b = LLVMGetParam(g->fn_str_concat, 1);
        LLVMValueRef len_a = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &a, 1, "len_a");
        LLVMValueRef len_b = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &b, 1, "len_b");
        LLVMValueRef max_copy = LLVMConstInt(t_i64, scratch_size - 1, 0);
        LLVMValueRef a_fits = LLVMBuildICmp(g->builder, LLVMIntULE, len_a, max_copy, "a.fits");
        LLVMValueRef copy_a = LLVMBuildSelect(g->builder, a_fits, len_a, max_copy, "copy_a");
        LLVMValueRef remain = LLVMBuildSub(g->builder, max_copy, copy_a, "remain");
        LLVMValueRef b_fits = LLVMBuildICmp(g->builder, LLVMIntULE, len_b, remain, "b.fits");
        LLVMValueRef copy_b = LLVMBuildSelect(g->builder, b_fits, len_b, remain, "copy_b");
        LLVMValueRef buf = build_freestanding_scratch_acquire(g, scratch_ring, scratch_ring_ty, scratch_slot_ty, scratch_index);

        LLVMValueRef ia = LLVMBuildAlloca(g->builder, t_i64, "i");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), ia);
        LLVMValueRef ib = LLVMBuildAlloca(g->builder, t_i64, "j");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), ib);
        LLVMBuildBr(g->builder, copya_cond);

        LLVMPositionBuilderAtEnd(g->builder, copya_cond);
        LLVMValueRef iva = LLVMBuildLoad2(g->builder, t_i64, ia, "iva");
        LLVMValueRef cond_a = LLVMBuildICmp(g->builder, LLVMIntULT, iva, copy_a, "conda");
        LLVMBuildCondBr(g->builder, cond_a, copya_body, copya_done);

        LLVMPositionBuilderAtEnd(g->builder, copya_body);
        LLVMValueRef aptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, a, &iva, 1, "aptr");
        LLVMValueRef ach = LLVMBuildLoad2(g->builder, g->i8, aptr, "ach");
        LLVMValueRef dsta = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, &iva, 1, "dsta");
        LLVMBuildStore(g->builder, ach, dsta);
        LLVMValueRef iva_next = LLVMBuildAdd(g->builder, iva, LLVMConstInt(t_i64, 1, 0), "iva_next");
        LLVMBuildStore(g->builder, iva_next, ia);
        LLVMBuildBr(g->builder, copya_cond);

        LLVMPositionBuilderAtEnd(g->builder, copya_done);
        LLVMBuildBr(g->builder, copyb_cond);

        LLVMPositionBuilderAtEnd(g->builder, copyb_cond);
        LLVMValueRef ivb = LLVMBuildLoad2(g->builder, t_i64, ib, "ivb");
        LLVMValueRef cond_b = LLVMBuildICmp(g->builder, LLVMIntULT, ivb, copy_b, "condb");
        LLVMBuildCondBr(g->builder, cond_b, copyb_body, copyb_done);

        LLVMPositionBuilderAtEnd(g->builder, copyb_body);
        LLVMValueRef srcb = LLVMBuildInBoundsGEP2(g->builder, g->i8, b, &ivb, 1, "srcb");
        LLVMValueRef bch = LLVMBuildLoad2(g->builder, g->i8, srcb, "bch");
        LLVMValueRef off = LLVMBuildAdd(g->builder, copy_a, ivb, "off");
        LLVMValueRef dstb = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, &off, 1, "dstb");
        LLVMBuildStore(g->builder, bch, dstb);
        LLVMValueRef ivb_next = LLVMBuildAdd(g->builder, ivb, LLVMConstInt(t_i64, 1, 0), "ivb_next");
        LLVMBuildStore(g->builder, ivb_next, ib);
        LLVMBuildBr(g->builder, copyb_cond);

        LLVMPositionBuilderAtEnd(g->builder, copyb_done);
        LLVMValueRef end_off = LLVMBuildAdd(g->builder, copy_a, copy_b, "end_off");
        LLVMValueRef end_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, &end_off, 1, "end_ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), end_ptr);
        LLVMBuildRet(g->builder, buf);
    }

    // __kn_i64_to_string
    g->fn_i64_to_string_ty = LLVMFunctionType(t_i8ptr, &t_i64, 1, 0);
    g->fn_i64_to_string = LLVMAddFunction(g->mod, "__kn_i64_to_string", g->fn_i64_to_string_ty);
    LLVMSetLinkage(g->fn_i64_to_string, LLVMInternalLinkage);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "entry");
        LLVMBasicBlockRef zero_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "zero");
        LLVMBasicBlockRef loop_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "loop.cond");
        LLVMBasicBlockRef loop_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "loop.body");
        LLVMBasicBlockRef loop_done = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "loop.done");
        LLVMBasicBlockRef sign_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "sign");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_i64_to_string, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef val = LLVMGetParam(g->fn_i64_to_string, 0);
        LLVMValueRef buf = build_freestanding_scratch_acquire(g, scratch_ring, scratch_ring_ty, scratch_slot_ty, scratch_index);
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i32, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 31, 0), idx);
        LLVMValueRef idxv0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idx0");
        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv0 }, 1, "p0");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), p0);
        LLVMValueRef idxv0n = LLVMBuildSub(g->builder, idxv0, LLVMConstInt(t_i32, 1, 0), "idx0n");
        LLVMBuildStore(g->builder, idxv0n, idx);

        LLVMValueRef neg = LLVMBuildICmp(g->builder, LLVMIntSLT, val, LLVMConstInt(t_i64, 0, 0), "neg");
        LLVMValueRef abs = LLVMBuildSelect(g->builder, neg, LLVMBuildNeg(g->builder, val, "nval"), val, "abs");
        LLVMValueRef abs_ptr = LLVMBuildAlloca(g->builder, t_i64, "abs");
        LLVMBuildStore(g->builder, abs, abs_ptr);
        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, abs, LLVMConstInt(t_i64, 0, 0), "iszero");
        LLVMBuildCondBr(g->builder, is_zero, zero_bb, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, zero_bb);
        LLVMValueRef idxz = LLVMBuildLoad2(g->builder, t_i32, idx, "idxz");
        LLVMValueRef pz = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxz }, 1, "pz");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), pz);
        LLVMValueRef idxzn = LLVMBuildSub(g->builder, idxz, LLVMConstInt(t_i32, 1, 0), "idxzn");
        LLVMBuildStore(g->builder, idxzn, idx);
        LLVMBuildBr(g->builder, loop_done);

        LLVMPositionBuilderAtEnd(g->builder, loop_cond);
        LLVMValueRef cur = LLVMBuildLoad2(g->builder, t_i64, abs_ptr, "cur");
        LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntUGT, cur, LLVMConstInt(t_i64, 0, 0), "cond");
        LLVMBuildCondBr(g->builder, cond, loop_body, loop_done);

        LLVMPositionBuilderAtEnd(g->builder, loop_body);
        LLVMValueRef curv = LLVMBuildLoad2(g->builder, t_i64, abs_ptr, "curv");
        LLVMValueRef digit = LLVMBuildURem(g->builder, curv, LLVMConstInt(t_i64, 10, 0), "digit");
        LLVMValueRef next = LLVMBuildUDiv(g->builder, curv, LLVMConstInt(t_i64, 10, 0), "next");
        LLVMBuildStore(g->builder, next, abs_ptr);
        LLVMValueRef idxv = LLVMBuildLoad2(g->builder, t_i32, idx, "idx");
        LLVMValueRef p = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv }, 1, "p");
        LLVMValueRef d8 = LLVMBuildTrunc(g->builder, digit, g->i8, "d8");
        LLVMValueRef ch = LLVMBuildAdd(g->builder, d8, LLVMConstInt(g->i8, '0', 0), "ch");
        LLVMBuildStore(g->builder, ch, p);
        LLVMValueRef idxvn = LLVMBuildSub(g->builder, idxv, LLVMConstInt(t_i32, 1, 0), "idxn");
        LLVMBuildStore(g->builder, idxvn, idx);
        LLVMBuildBr(g->builder, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, loop_done);
        LLVMBuildCondBr(g->builder, neg, sign_bb, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, sign_bb);
        LLVMValueRef idxs = LLVMBuildLoad2(g->builder, t_i32, idx, "idxs");
        LLVMValueRef ps = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxs }, 1, "ps");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '-', 0), ps);
        LLVMValueRef idxsn = LLVMBuildSub(g->builder, idxs, LLVMConstInt(t_i32, 1, 0), "idxsn");
        LLVMBuildStore(g->builder, idxsn, idx);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef idxf = LLVMBuildLoad2(g->builder, t_i32, idx, "idxf");
        LLVMValueRef start = LLVMBuildAdd(g->builder, idxf, LLVMConstInt(t_i32, 1, 0), "start");
        LLVMValueRef retp = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ start }, 1, "retp");
        LLVMBuildRet(g->builder, retp);
    }

    // __kn_char_to_string
    g->fn_char_to_string_ty = LLVMFunctionType(t_i8ptr, &g->i8, 1, 0);
    g->fn_char_to_string = LLVMAddFunction(g->mod, "__kn_char_to_string", g->fn_char_to_string_ty);
    LLVMSetLinkage(g->fn_char_to_string, LLVMInternalLinkage);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_char_to_string, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef buf = build_freestanding_scratch_acquire(g, scratch_ring, scratch_ring_ty, scratch_slot_ty, scratch_index);
        LLVMValueRef ch = LLVMGetParam(g->fn_char_to_string, 0);
        LLVMBuildStore(g->builder, ch, buf);
        LLVMValueRef pz = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ LLVMConstInt(t_i32, 1, 0) }, 1, "pz");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), pz);
        LLVMBuildRet(g->builder, buf);
    }

    // __kn_ptr_to_string
    g->fn_ptr_to_string_ty = LLVMFunctionType(t_i8ptr, &t_i8ptr, 1, 0);
    g->fn_ptr_to_string = LLVMAddFunction(g->mod, "__kn_ptr_to_string", g->fn_ptr_to_string_ty);
    LLVMSetLinkage(g->fn_ptr_to_string, LLVMInternalLinkage);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "entry");
        LLVMBasicBlockRef zero_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "zero");
        LLVMBasicBlockRef loop_cond = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "loop.cond");
        LLVMBasicBlockRef loop_body = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "loop.body");
        LLVMBasicBlockRef prefix_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "prefix");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_ptr_to_string, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef p = LLVMGetParam(g->fn_ptr_to_string, 0);
        LLVMValueRef iv = LLVMBuildPtrToInt(g->builder, p, t_i64, "p2i");
        LLVMValueRef buf = build_freestanding_scratch_acquire(g, scratch_ring, scratch_ring_ty, scratch_slot_ty, scratch_index);
        LLVMValueRef idx = LLVMBuildAlloca(g->builder, t_i32, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i32, 31, 0), idx);
        LLVMValueRef idxv0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idx0");
        LLVMValueRef p0 = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv0 }, 1, "p0");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 0, 0), p0);
        LLVMValueRef idxv0n = LLVMBuildSub(g->builder, idxv0, LLVMConstInt(t_i32, 1, 0), "idx0n");
        LLVMBuildStore(g->builder, idxv0n, idx);

        LLVMValueRef val = LLVMBuildAlloca(g->builder, t_i64, "val");
        LLVMBuildStore(g->builder, iv, val);
        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, iv, LLVMConstInt(t_i64, 0, 0), "iszero");
        LLVMBuildCondBr(g->builder, is_zero, zero_bb, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, zero_bb);
        LLVMValueRef idxz = LLVMBuildLoad2(g->builder, t_i32, idx, "idxz");
        LLVMValueRef pz = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxz }, 1, "pz");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), pz);
        LLVMValueRef idxzn = LLVMBuildSub(g->builder, idxz, LLVMConstInt(t_i32, 1, 0), "idxzn");
        LLVMBuildStore(g->builder, idxzn, idx);
        LLVMBuildBr(g->builder, prefix_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_cond);
        LLVMValueRef cur = LLVMBuildLoad2(g->builder, t_i64, val, "cur");
        LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntUGT, cur, LLVMConstInt(t_i64, 0, 0), "cond");
        LLVMBuildCondBr(g->builder, cond, loop_body, prefix_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_body);
        LLVMValueRef curv = LLVMBuildLoad2(g->builder, t_i64, val, "curv");
        LLVMValueRef digit = LLVMBuildAnd(g->builder, curv, LLVMConstInt(t_i64, 15, 0), "digit");
        LLVMValueRef next = LLVMBuildLShr(g->builder, curv, LLVMConstInt(t_i64, 4, 0), "next");
        LLVMBuildStore(g->builder, next, val);
        LLVMValueRef idxv = LLVMBuildLoad2(g->builder, t_i32, idx, "idxv");
        LLVMValueRef pd = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxv }, 1, "pd");
        LLVMValueRef is_lt10 = LLVMBuildICmp(g->builder, LLVMIntULT, digit, LLVMConstInt(t_i64, 10, 0), "lt10");
        LLVMValueRef d0 = LLVMBuildTrunc(g->builder, digit, g->i8, "d0");
        LLVMValueRef ch0 = LLVMBuildAdd(g->builder, d0, LLVMConstInt(g->i8, '0', 0), "ch0");
        LLVMValueRef d10 = LLVMBuildSub(g->builder, digit, LLVMConstInt(t_i64, 10, 0), "d10");
        LLVMValueRef d10_8 = LLVMBuildTrunc(g->builder, d10, g->i8, "d10_8");
        LLVMValueRef ch1 = LLVMBuildAdd(g->builder, d10_8, LLVMConstInt(g->i8, 'a', 0), "ch1");
        LLVMValueRef ch = LLVMBuildSelect(g->builder, is_lt10, ch0, ch1, "ch");
        LLVMBuildStore(g->builder, ch, pd);
        LLVMValueRef idxvn = LLVMBuildSub(g->builder, idxv, LLVMConstInt(t_i32, 1, 0), "idxvn");
        LLVMBuildStore(g->builder, idxvn, idx);
        LLVMBuildBr(g->builder, loop_cond);

        LLVMPositionBuilderAtEnd(g->builder, prefix_bb);
        LLVMValueRef idxp = LLVMBuildLoad2(g->builder, t_i32, idx, "idxp");
        LLVMValueRef px = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxp }, 1, "px");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, 'x', 0), px);
        LLVMValueRef idxpn = LLVMBuildSub(g->builder, idxp, LLVMConstInt(t_i32, 1, 0), "idxpn");
        LLVMBuildStore(g->builder, idxpn, idx);
        LLVMValueRef idxp0 = LLVMBuildLoad2(g->builder, t_i32, idx, "idxp0");
        LLVMValueRef p0x = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ idxp0 }, 1, "p0x");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i8, '0', 0), p0x);
        LLVMValueRef idxp0n = LLVMBuildSub(g->builder, idxp0, LLVMConstInt(t_i32, 1, 0), "idxp0n");
        LLVMBuildStore(g->builder, idxp0n, idx);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef idxf = LLVMBuildLoad2(g->builder, t_i32, idx, "idxf");
        LLVMValueRef start = LLVMBuildAdd(g->builder, idxf, LLVMConstInt(t_i32, 1, 0), "start");
        LLVMValueRef retp = LLVMBuildInBoundsGEP2(g->builder, g->i8, buf, (LLVMValueRef[]){ start }, 1, "retp");
        LLVMBuildRet(g->builder, retp);
    }

    // __kn_f64_to_string (truncate to int)
    g->fn_f64_to_string_ty = LLVMFunctionType(t_i8ptr, &g->f64, 1, 0);
    g->fn_f64_to_string = LLVMAddFunction(g->mod, "__kn_f64_to_string", g->fn_f64_to_string_ty);
    LLVMSetLinkage(g->fn_f64_to_string, LLVMInternalLinkage);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_f64_to_string, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef fv = LLVMGetParam(g->fn_f64_to_string, 0);
        LLVMValueRef iv = LLVMBuildFPToSI(g->builder, fv, t_i64, "f2i");
        LLVMValueRef retp = LLVMBuildCall2(g->builder, g->fn_i64_to_string_ty, g->fn_i64_to_string, &iv, 1, "f2s");
        LLVMBuildRet(g->builder, retp);
    }

    // __kn_any_to_string
    g->fn_any_to_string_ty = LLVMFunctionType(t_i8ptr, &g->any_ty, 1, 0);
    g->fn_any_to_string = LLVMAddFunction(g->mod, "__kn_any_to_string", g->fn_any_to_string_ty);
    LLVMSetLinkage(g->fn_any_to_string, LLVMInternalLinkage);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "entry");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef anyv = LLVMGetParam(g->fn_any_to_string, 0);
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "payload");
        LLVMValueRef res = LLVMBuildAlloca(g->builder, t_i8ptr, "res");
        LLVMValueRef null_str = build_string_const(g, "null", 4);
        LLVMValueRef unknown = build_string_const(g, "<any>", 5);
        LLVMBuildStore(g->builder, null_str, res);

        LLVMBasicBlockRef case_str = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "str");
        LLVMBasicBlockRef case_i64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "i64");
        LLVMBasicBlockRef case_f64 = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "f64");
        LLVMBasicBlockRef case_bool = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "bool");
        LLVMBasicBlockRef case_char = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "char");
        LLVMBasicBlockRef case_ptr = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "ptr");
        LLVMBasicBlockRef def_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_any_to_string, "def");

        LLVMValueRef sw = LLVMBuildSwitch(g->builder, tag, def_bb, 10);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_STRING, 0), case_str);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_I64, 0), case_i64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_BOOL, 0), case_bool);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_F64, 0), case_f64);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_CHAR, 0), case_char);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_PTR, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_DICT, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_LIST, 0), case_ptr);
        LLVMAddCase(sw, LLVMConstInt(t_i64, KN_ANY_TAG_SET, 0), case_ptr);

        LLVMPositionBuilderAtEnd(g->builder, case_str);
        {
            LLVMValueRef sp = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "sp");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_i64);
        {
            LLVMValueRef iv = payload;
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_i64_to_string_ty, g->fn_i64_to_string, &iv, 1, "i2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_f64);
        {
            LLVMValueRef dv = LLVMBuildBitCast(g->builder, payload, g->f64, "d");
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_f64_to_string_ty, g->fn_f64_to_string, &dv, 1, "f2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_bool);
        {
            LLVMValueRef is_true = LLVMBuildICmp(g->builder, LLVMIntNE, payload, LLVMConstInt(t_i64, 0, 0), "b");
            LLVMValueRef tstr = build_string_const(g, "true", 4);
            LLVMValueRef fstr = build_string_const(g, "false", 5);
            LLVMValueRef sp = LLVMBuildSelect(g->builder, is_true, tstr, fstr, "bstr");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_char);
        {
            LLVMValueRef cv = LLVMBuildTrunc(g->builder, payload, g->i8, "c");
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_char_to_string_ty, g->fn_char_to_string, &cv, 1, "c2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, case_ptr);
        {
            LLVMValueRef pv = LLVMBuildIntToPtr(g->builder, payload, t_i8ptr, "p");
            LLVMValueRef sp = LLVMBuildCall2(g->builder, g->fn_ptr_to_string_ty, g->fn_ptr_to_string, &pv, 1, "p2s");
            LLVMBuildStore(g->builder, sp, res);
            LLVMBuildBr(g->builder, done_bb);
        }
        LLVMPositionBuilderAtEnd(g->builder, def_bb);
        LLVMBuildStore(g->builder, unknown, res);
        LLVMBuildBr(g->builder, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef outv = LLVMBuildLoad2(g->builder, t_i8ptr, res, "outv");
        LLVMBuildRet(g->builder, outv);
    }
}

static void build_runtime_freestanding(Codegen *g)
{
    LLVMTypeRef t_i8ptr = g->i8ptr;
    LLVMTypeRef t_i64 = g->i64;
    LLVMTypeRef t_void = g->voidt;

    declare_freestanding_optional_runtime(g);

    g->fn_panic_ty = LLVMFunctionType(t_void, &t_i8ptr, 1, 0);
    g->fn_panic = LLVMAddFunction(g->mod, "__kn_panic", g->fn_panic_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_panic, "entry");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        if (g->panic_mode == KN_PANIC_LOOP)
        {
            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_panic, "loop");
            LLVMBuildBr(g->builder, loop_bb);
            LLVMPositionBuilderAtEnd(g->builder, loop_bb);
            LLVMBuildBr(g->builder, loop_bb);
        }
        else
        {
            LLVMBuildUnreachable(g->builder);
        }
    }

    g->fn_strlen_ty = LLVMFunctionType(t_i64, &t_i8ptr, 1, 0);
    g->fn_strlen = LLVMAddFunction(g->mod, "__kn_strlen", g->fn_strlen_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "loop");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "body");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_strlen, "done");
        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef s = LLVMGetParam(g->fn_strlen, 0);
        LLVMValueRef nullp = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(t_i8ptr), "null");
        LLVMValueRef len_ptr = LLVMBuildAlloca(g->builder, t_i64, "len");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), len_ptr);
        LLVMBuildCondBr(g->builder, nullp, done_bb, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef lenv = LLVMBuildLoad2(g->builder, t_i64, len_ptr, "lenv");
        LLVMValueRef ch_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, s, &lenv, 1, "ch.ptr");
        LLVMValueRef ch = LLVMBuildLoad2(g->builder, g->i8, ch_ptr, "ch");
        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, ch, LLVMConstInt(g->i8, 0, 0), "is.zero");
        LLVMBuildCondBr(g->builder, is_zero, done_bb, body_bb);

        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        LLVMValueRef next = LLVMBuildAdd(g->builder, lenv, LLVMConstInt(t_i64, 1, 0), "next");
        LLVMBuildStore(g->builder, next, len_ptr);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMBuildRet(g->builder, LLVMBuildLoad2(g->builder, t_i64, len_ptr, "out"));
    }

    g->fn_str_eq_ty = LLVMFunctionType(g->i1, (LLVMTypeRef[]){ t_i8ptr, t_i8ptr }, 2, 0);
    g->fn_str_eq = LLVMAddFunction(g->mod, "__kn_str_eq", g->fn_str_eq_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "loop");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "body");
        LLVMBasicBlockRef true_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "true");
        LLVMBasicBlockRef false_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "false");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef a = LLVMGetParam(g->fn_str_eq, 0);
        LLVMValueRef b = LLVMGetParam(g->fn_str_eq, 1);
        LLVMValueRef same = LLVMBuildICmp(g->builder, LLVMIntEQ, a, b, "same");
        LLVMBasicBlockRef null_check = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "nullcheck");
        LLVMBuildCondBr(g->builder, same, true_bb, null_check);

        LLVMPositionBuilderAtEnd(g->builder, null_check);
        LLVMValueRef a_null = LLVMBuildICmp(g->builder, LLVMIntEQ, a, LLVMConstNull(t_i8ptr), "a.null");
        LLVMValueRef b_null = LLVMBuildICmp(g->builder, LLVMIntEQ, b, LLVMConstNull(t_i8ptr), "b.null");
        LLVMValueRef either_null = LLVMBuildOr(g->builder, a_null, b_null, "either.null");
        LLVMValueRef idx_ptr = LLVMBuildAlloca(g->builder, t_i64, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), idx_ptr);
        LLVMBuildCondBr(g->builder, either_null, false_bb, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef idx = LLVMBuildLoad2(g->builder, t_i64, idx_ptr, "idxv");
        LLVMValueRef a_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, a, &idx, 1, "a.ptr");
        LLVMValueRef b_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, b, &idx, 1, "b.ptr");
        LLVMValueRef ac = LLVMBuildLoad2(g->builder, g->i8, a_ptr, "ac");
        LLVMValueRef bc = LLVMBuildLoad2(g->builder, g->i8, b_ptr, "bc");
        LLVMValueRef diff = LLVMBuildICmp(g->builder, LLVMIntNE, ac, bc, "diff");
        LLVMBuildCondBr(g->builder, diff, false_bb, body_bb);

        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        LLVMValueRef is_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, ac, LLVMConstInt(g->i8, 0, 0), "is.zero");
        LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_str_eq, "inc");
        LLVMBuildCondBr(g->builder, is_zero, true_bb, inc_bb);
        LLVMPositionBuilderAtEnd(g->builder, inc_bb);
        LLVMValueRef next = LLVMBuildAdd(g->builder, idx, LLVMConstInt(t_i64, 1, 0), "next");
        LLVMBuildStore(g->builder, next, idx_ptr);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, true_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i1, 1, 0));

        LLVMPositionBuilderAtEnd(g->builder, false_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i1, 0, 0));
    }

    g->fn_mem_compare_ty = LLVMFunctionType(g->i32, (LLVMTypeRef[]){ t_i8ptr, t_i8ptr, t_i64 }, 3, 0);
    g->fn_mem_compare = LLVMAddFunction(g->mod, "__kn_mem_compare", g->fn_mem_compare_ty);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "entry");
        LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "loop");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "body");
        LLVMBasicBlockRef less_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "less");
        LLVMBasicBlockRef greater_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "greater");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "done");

        LLVMPositionBuilderAtEnd(g->builder, entry);
        LLVMValueRef a = LLVMGetParam(g->fn_mem_compare, 0);
        LLVMValueRef b = LLVMGetParam(g->fn_mem_compare, 1);
        LLVMValueRef n = LLVMGetParam(g->fn_mem_compare, 2);
        LLVMValueRef idx_ptr = LLVMBuildAlloca(g->builder, t_i64, "idx");
        LLVMBuildStore(g->builder, LLVMConstInt(t_i64, 0, 0), idx_ptr);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, loop_bb);
        LLVMValueRef idx = LLVMBuildLoad2(g->builder, t_i64, idx_ptr, "idxv");
        LLVMValueRef done = LLVMBuildICmp(g->builder, LLVMIntUGE, idx, n, "done");
        LLVMBuildCondBr(g->builder, done, done_bb, body_bb);

        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        LLVMValueRef a_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, a, &idx, 1, "a.ptr");
        LLVMValueRef b_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8, b, &idx, 1, "b.ptr");
        LLVMValueRef av = LLVMBuildLoad2(g->builder, g->i8, a_ptr, "av");
        LLVMValueRef bv = LLVMBuildLoad2(g->builder, g->i8, b_ptr, "bv");
        LLVMValueRef less = LLVMBuildICmp(g->builder, LLVMIntULT, av, bv, "lt");
        LLVMValueRef greater = LLVMBuildICmp(g->builder, LLVMIntUGT, av, bv, "gt");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "next");
        LLVMBuildCondBr(g->builder, less, less_bb, next_bb);

        LLVMPositionBuilderAtEnd(g->builder, next_bb);
        LLVMBuildCondBr(g->builder, greater, greater_bb, done_bb);

        LLVMPositionBuilderAtEnd(g->builder, less_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i32, (uint64_t)-1, 1));

        LLVMPositionBuilderAtEnd(g->builder, greater_bb);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i32, 1, 0));

        LLVMPositionBuilderAtEnd(g->builder, done_bb);
        LLVMValueRef idx_now = LLVMBuildLoad2(g->builder, t_i64, idx_ptr, "idx.now");
        LLVMValueRef still_loop = LLVMBuildICmp(g->builder, LLVMIntULT, idx_now, n, "still.loop");
        LLVMBasicBlockRef zero_ret = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "zero");
        LLVMBasicBlockRef inc_ret = LLVMAppendBasicBlockInContext(g->ctx, g->fn_mem_compare, "incret");
        LLVMBuildCondBr(g->builder, still_loop, inc_ret, zero_ret);

        LLVMPositionBuilderAtEnd(g->builder, inc_ret);
        LLVMValueRef next = LLVMBuildAdd(g->builder, idx_now, LLVMConstInt(t_i64, 1, 0), "next");
        LLVMBuildStore(g->builder, next, idx_ptr);
        LLVMBuildBr(g->builder, loop_bb);

        LLVMPositionBuilderAtEnd(g->builder, zero_ret);
        LLVMBuildRet(g->builder, LLVMConstInt(g->i32, 0, 0));
    }

    build_freestanding_core_string_helpers(g);

}

static void build_runtime(Codegen *g)
{
    if (g && g->env_kind == KN_ENV_FREESTANDING)
        build_runtime_freestanding(g);
    else
        build_runtime_hosted(g);
}

static void emit_freestanding_entry_wrapper(Codegen *g)
{
    if (!g || g->env_kind != KN_ENV_FREESTANDING || !g->emit_entry || !g->entry_name || !g->entry_name[0])
        return;

    LLVMValueRef target_fn = g->fn_main;
    LLVMTypeRef target_ty = g->fn_main_ty;
    if (!target_fn)
    {
        for (int i = 0; i < g->funcs.count; i++)
        {
            FuncSymbol *fs = &g->funcs.items[i];
            if (!fs || !fs->func || !fs->fn) continue;
            if ((fs->func->name && kn_strcmp(fs->func->name, g->entry_name) == 0) ||
                (fs->qname && kn_strcmp(fs->qname, g->entry_name) == 0))
            {
                target_fn = fs->fn;
                target_ty = fs->fn_ty;
                break;
            }
        }
    }
    if (!target_fn || !target_ty)
        return;

    const char *wrapper_name = "__kn_entry";
    unsigned param_count = LLVMCountParams(target_fn);
    LLVMTypeRef entry_args[1];
    if (param_count > 0)
        entry_args[0] = LLVMTypeOf(LLVMGetParam(target_fn, 0));
    LLVMTypeRef entry_ty = LLVMFunctionType(g->voidt, entry_args, param_count > 0 ? 1 : 0, 0);
    LLVMValueRef entry_fn = LLVMGetNamedFunction(g->mod, wrapper_name);
    if (!entry_fn)
        entry_fn = LLVMAddFunction(g->mod, wrapper_name, entry_ty);
    LLVMSetSection(entry_fn, ".text.kn.entry");
    if (LLVMCountBasicBlocks(entry_fn) != 0)
        return;

    LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(g->ctx, entry_fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry_bb);
    maybe_call_global_init(g);
    if (param_count == 0)
    {
        LLVMValueRef retv = LLVMBuildCall2(g->builder, target_ty, target_fn, 0, 0, "");
        if (g->fn_main_is_async)
            (void)entry_await_task_result(g, retv, g->fn_main_ret_type);
    }
    else
    {
        LLVMValueRef arg0 = LLVMGetParam(entry_fn, 0);
        LLVMValueRef retv = LLVMBuildCall2(g->builder, target_ty, target_fn, &arg0, 1, "");
        if (g->fn_main_is_async)
            (void)entry_await_task_result(g, retv, g->fn_main_ret_type);
    }
    LLVMBuildRetVoid(g->builder);
}

static const char *mangle_global_var_name(const char *name)
{
    const char *prefix = "__kn_gv$";
    size_t pn = kn_strlen(prefix);
    size_t nn = name ? kn_strlen(name) : 0;
    char *out = (char *)kn_malloc(pn + nn + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, prefix, pn);
    if (nn > 0)
        kn_memcpy(out + pn, name, nn);
    out[pn + nn] = 0;
    return out;
}

typedef struct
{
    const char *owner_name;
    const char *owner_callable_qname;
    uint64_t owner_kind;
    uint64_t owner_param_count;
    Attribute *attr;
    MetaDecl *meta;
} MetaRuntimeAttr;

typedef struct
{
    MetaRuntimeAttr *items;
    int count;
    int cap;
} MetaRuntimeAttrList;

static const char *mangle_method_name(ClassSymbol *cs, Method *m, int idx);

static void meta_runtime_attr_push(MetaRuntimeAttrList *list, MetaRuntimeAttr item)
{
    if (!list) return;
    if (list->count + 1 > list->cap)
    {
        int new_cap = list->cap ? list->cap * 2 : 8;
        MetaRuntimeAttr *next = (MetaRuntimeAttr *)kn_malloc(sizeof(MetaRuntimeAttr) * (size_t)new_cap);
        if (!next) kn_die("out of memory");
        if (list->items)
        {
            kn_memcpy(next, list->items, sizeof(MetaRuntimeAttr) * (size_t)list->count);
            kn_free(list->items);
        }
        list->items = next;
        list->cap = new_cap;
    }
    list->items[list->count++] = item;
}

static MetaDecl *codegen_find_meta_decl(Codegen *g, const char *resolved_name)
{
    if (!g || !g->metas || !resolved_name) return 0;
    for (int i = 0; i < g->metas->count; i++)
    {
        MetaDecl *m = &g->metas->items[i];
        if (m->qname && kn_strcmp(m->qname, resolved_name) == 0)
            return m;
    }
    return 0;
}

static const char *codegen_join_meta_owner_name(const char *owner, const char *name)
{
    if (!owner || !name) return owner ? owner : name;
    size_t olen = kn_strlen(owner);
    size_t nlen = kn_strlen(name);
    char *out = (char *)kn_malloc(olen + 1 + nlen + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, owner, olen);
    out[olen] = '.';
    kn_memcpy(out + olen + 1, name, nlen);
    out[olen + 1 + nlen] = 0;
    return out;
}

static void collect_runtime_attrs_from_attr_list(Codegen *g, AttrList *attrs, const char *owner_name,
                                                 uint64_t owner_kind, uint64_t owner_param_count,
                                                 const char *owner_callable_qname, MetaRuntimeAttrList *out)
{
    if (!g || !attrs || !owner_name || !out) return;
    for (int i = 0; i < attrs->count; i++)
    {
        Attribute *a = &attrs->items[i];
        MetaDecl *meta = codegen_find_meta_decl(g, a->resolved_name ? a->resolved_name : a->name);
        if (!meta || meta->keep_kind != KN_META_KEEP_RUNTIME)
            continue;
        {
            MetaRuntimeAttr item;
            kn_memset(&item, 0, sizeof(item));
            item.owner_name = owner_name;
            item.owner_callable_qname = owner_callable_qname;
            item.owner_kind = owner_kind;
            item.owner_param_count = owner_param_count;
            item.attr = a;
            item.meta = meta;
            meta_runtime_attr_push(out, item);
        }
    }
}

static void collect_runtime_meta_attrs(Codegen *g, MetaRuntimeAttrList *out)
{
    if (!g || !out) return;

    if (g->func_list)
    {
        for (int i = 0; i < g->func_list->count; i++)
        {
            Func *f = &g->func_list->items[i];
            collect_runtime_attrs_from_attr_list(g, &f->attrs, f->qname ? f->qname : f->name,
                                                 KN_META_OWNER_FUNCTION,
                                                 (uint64_t)f->params.count,
                                                 f->qname ? f->qname : f->name,
                                                 out);
        }
    }

    for (int i = 0; i < g->classes.count; i++)
    {
        ClassDecl *c = g->classes.items[i].decl;
        if (!c) continue;
        collect_runtime_attrs_from_attr_list(g, &c->attrs, c->qname ? c->qname : c->name,
                                             KN_META_OWNER_CLASS, 0, 0, out);
        for (int fi = 0; fi < c->fields.count; fi++)
        {
            Field *f = &c->fields.items[fi];
            collect_runtime_attrs_from_attr_list(g, &f->attrs,
                                                 codegen_join_meta_owner_name(c->qname ? c->qname : c->name, f->name),
                                                 KN_META_OWNER_FIELD, 0, 0, out);
        }
        for (int mi = 0; mi < c->methods.count; mi++)
        {
            ClassSymbol *cs = &g->classes.items[i];
            Method *m = &c->methods.items[mi];
            const char *callable_qname = 0;
            uint64_t param_count = 0;
            if (m->is_static && !m->is_constructor)
            {
                callable_qname = mangle_method_name(cs, m, mi);
                param_count = (uint64_t)m->params.count;
            }
            collect_runtime_attrs_from_attr_list(g, &m->attrs,
                                                 codegen_join_meta_owner_name(c->qname ? c->qname : c->name, m->name),
                                                 KN_META_OWNER_METHOD, param_count, callable_qname, out);
        }
    }

    if (g->structs_list)
    {
        for (int i = 0; i < g->structs_list->count; i++)
        {
            StructDecl *st = &g->structs_list->items[i];
            collect_runtime_attrs_from_attr_list(g, &st->attrs, st->qname ? st->qname : st->name,
                                                 KN_META_OWNER_STRUCT, 0, 0, out);
            for (int fi = 0; fi < st->fields.count; fi++)
            {
                Field *f = &st->fields.items[fi];
                collect_runtime_attrs_from_attr_list(g, &f->attrs,
                                                     codegen_join_meta_owner_name(st->qname ? st->qname : st->name, f->name),
                                                     KN_META_OWNER_FIELD, 0, 0, out);
            }
        }
    }

    if (g->interfaces)
    {
        for (int i = 0; i < g->interfaces->count; i++)
        {
            InterfaceDecl *it = &g->interfaces->items[i];
            collect_runtime_attrs_from_attr_list(g, &it->attrs, it->qname ? it->qname : it->name,
                                                 KN_META_OWNER_INTERFACE, 0, 0, out);
            for (int mi = 0; mi < it->methods.count; mi++)
            {
                Method *m = &it->methods.items[mi];
                collect_runtime_attrs_from_attr_list(g, &m->attrs,
                                                     codegen_join_meta_owner_name(it->qname ? it->qname : it->name, m->name),
                                                     KN_META_OWNER_METHOD, 0, 0, out);
            }
        }
    }

    if (g->enums)
    {
        for (int i = 0; i < g->enums->count; i++)
        {
            EnumDecl *en = &g->enums->items[i];
            collect_runtime_attrs_from_attr_list(g, &en->attrs, en->qname ? en->qname : en->name,
                                                 KN_META_OWNER_ENUM, 0, 0, out);
        }
    }
}

static LLVMValueRef meta_emit_const_array_ptr(Codegen *g, LLVMTypeRef elem_ty, LLVMValueRef *values, int count)
{
    if (!g || count <= 0)
        return LLVMConstNull(LLVMPointerType(elem_ty, 0));
    LLVMTypeRef arr_ty = LLVMArrayType(elem_ty, (unsigned)count);
    LLVMValueRef arr = LLVMConstArray(elem_ty, values, (unsigned)count);
    LLVMValueRef gv = LLVMAddGlobal(g->mod, arr_ty, "");
    LLVMSetLinkage(gv, LLVMPrivateLinkage);
    LLVMSetGlobalConstant(gv, 1);
    LLVMSetInitializer(gv, arr);
    {
        LLVMValueRef idxs[2] = { LLVMConstInt(g->i32, 0, 0), LLVMConstInt(g->i32, 0, 0) };
        return LLVMConstInBoundsGEP2(arr_ty, gv, idxs, 2);
    }
}

static void meta_expr_to_const_parts(Codegen *g, Expr *expr, LLVMValueRef *out_tag, LLVMValueRef *out_payload)
{
    LLVMValueRef tag = LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0);
    LLVMValueRef payload = LLVMConstInt(g->i64, 0, 0);
    if (expr)
    {
        switch (expr->kind)
        {
        case EXPR_STRING:
            tag = LLVMConstInt(g->i64, KN_ANY_TAG_STRING, 0);
            payload = LLVMConstPtrToInt(build_string_const(g, expr->v.str_val.ptr, expr->v.str_val.len), g->i64);
            break;
        case EXPR_INT:
            tag = LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0);
            payload = LLVMConstInt(g->i64, (uint64_t)expr->v.int_val, 1);
            break;
        case EXPR_BOOL:
            tag = LLVMConstInt(g->i64, KN_ANY_TAG_BOOL, 0);
            payload = LLVMConstInt(g->i64, expr->v.int_val ? 1 : 0, 0);
            break;
        case EXPR_CHAR:
            tag = LLVMConstInt(g->i64, KN_ANY_TAG_CHAR, 0);
            payload = LLVMConstInt(g->i64, (uint64_t)(unsigned char)expr->v.int_val, 0);
            break;
        case EXPR_FLOAT:
            tag = LLVMConstInt(g->i64, KN_ANY_TAG_F64, 0);
            payload = LLVMConstBitCast(LLVMConstReal(g->f64, expr->v.float_val), g->i64);
            break;
        case EXPR_MEMBER:
            if (expr->v.member.is_enum_const)
            {
                tag = LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0);
                payload = LLVMConstInt(g->i64, (uint64_t)expr->v.member.enum_value, 1);
            }
            break;
        default:
            break;
        }
    }
    if (out_tag) *out_tag = tag;
    if (out_payload) *out_payload = payload;
}

static const char *codegen_primary_module_name(Codegen *g)
{
    if (!g) return "Main";
    if (g->metas && g->metas->count > 0 && g->metas->items[0].unit && g->metas->items[0].unit[0])
        return g->metas->items[0].unit;
    if (g->func_list)
    {
        for (int i = 0; i < g->func_list->count; i++)
        {
            Func *f = &g->func_list->items[i];
            if (f->unit && f->unit[0]) return f->unit;
        }
    }
    if (g->classes.count > 0 && g->classes.items[0].decl && g->classes.items[0].decl->unit && g->classes.items[0].decl->unit[0])
        return g->classes.items[0].decl->unit;
    if (g->structs_list && g->structs_list->count > 0 && g->structs_list->items[0].unit && g->structs_list->items[0].unit[0])
        return g->structs_list->items[0].unit;
    if (g->interfaces && g->interfaces->count > 0 && g->interfaces->items[0].unit && g->interfaces->items[0].unit[0])
        return g->interfaces->items[0].unit;
    if (g->enums && g->enums->count > 0 && g->enums->items[0].unit && g->enums->items[0].unit[0])
        return g->enums->items[0].unit;
    return "Main";
}

static void emit_meta_module_export(Codegen *g)
{
    if (!g || !g->fn_meta_get_module || !g->fn_meta_get_module_ty)
        return;

    LLVMTypeRef t_i8ptr = g->i8ptr;
    LLVMTypeRef t_i64 = g->i64;
    LLVMTypeRef meta_arg_ty = LLVMStructCreateNamed(g->ctx, "KnMetaExportArg");
    LLVMTypeRef meta_inst_ty = LLVMStructCreateNamed(g->ctx, "KnMetaExportInstance");
    LLVMTypeRef meta_type_ty = LLVMStructCreateNamed(g->ctx, "KnMetaExportType");
    LLVMTypeRef meta_mod_ty = LLVMStructCreateNamed(g->ctx, "KnMetaExportModule");

    {
        LLVMTypeRef fields[3] = { t_i8ptr, t_i64, t_i64 };
        LLVMStructSetBody(meta_arg_ty, fields, 3, 0);
    }
    {
        LLVMTypeRef fields[8] = {
            t_i8ptr, t_i64, t_i64, t_i8ptr,
            t_i8ptr, t_i8ptr, LLVMPointerType(meta_arg_ty, 0), t_i64
        };
        LLVMStructSetBody(meta_inst_ty, fields, 8, 0);
    }
    {
        LLVMTypeRef fields[7] = {
            t_i8ptr, t_i8ptr, t_i64, t_i64, t_i64,
            LLVMPointerType(t_i8ptr, 0), t_i64
        };
        LLVMStructSetBody(meta_type_ty, fields, 7, 0);
    }
    {
        LLVMTypeRef fields[5] = {
            t_i8ptr,
            LLVMPointerType(meta_type_ty, 0), t_i64,
            LLVMPointerType(meta_inst_ty, 0), t_i64
        };
        LLVMStructSetBody(meta_mod_ty, fields, 5, 0);
    }

    MetaRuntimeAttrList insts = {0};
    collect_runtime_meta_attrs(g, &insts);

    int runtime_meta_count = 0;
    if (g->metas)
    {
        for (int i = 0; i < g->metas->count; i++)
            if (g->metas->items[i].keep_kind == KN_META_KEEP_RUNTIME)
                runtime_meta_count++;
    }

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_meta_get_module, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    if (runtime_meta_count <= 0 && insts.count <= 0)
    {
        LLVMBuildRet(g->builder, LLVMConstNull(t_i8ptr));
        return;
    }

    LLVMValueRef *type_vals = 0;
    if (runtime_meta_count > 0)
        type_vals = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)runtime_meta_count);
    int type_index = 0;
    if (g->metas)
    {
        for (int i = 0; i < g->metas->count; i++)
        {
            MetaDecl *meta = &g->metas->items[i];
            if (meta->keep_kind != KN_META_KEEP_RUNTIME)
                continue;
            LLVMValueRef param_names_ptr = LLVMConstNull(LLVMPointerType(t_i8ptr, 0));
            if (meta->params.count > 0)
            {
                LLVMValueRef *param_names = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)meta->params.count);
                for (int pi = 0; pi < meta->params.count; pi++)
                    param_names[pi] = build_string_const(g, meta->params.items[pi].name, (int)kn_strlen(meta->params.items[pi].name));
                param_names_ptr = meta_emit_const_array_ptr(g, t_i8ptr, param_names, meta->params.count);
            }
            {
                LLVMValueRef fields[7];
                fields[0] = build_string_const(g, meta->qname ? meta->qname : meta->name, (int)kn_strlen(meta->qname ? meta->qname : meta->name));
                fields[1] = build_string_const(g, meta->name, (int)kn_strlen(meta->name));
                fields[2] = LLVMConstInt(t_i64, meta->target_mask, 0);
                fields[3] = LLVMConstInt(t_i64, meta->keep_kind, 0);
                fields[4] = LLVMConstInt(t_i64, meta->repeatable ? 1 : 0, 0);
                fields[5] = param_names_ptr;
                fields[6] = LLVMConstInt(t_i64, meta->params.count, 0);
                type_vals[type_index++] = LLVMConstNamedStruct(meta_type_ty, fields, 7);
            }
        }
    }
    LLVMValueRef types_ptr = meta_emit_const_array_ptr(g, meta_type_ty, type_vals, type_index);

    LLVMValueRef *inst_vals = 0;
    if (insts.count > 0)
        inst_vals = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)insts.count);
    for (int i = 0; i < insts.count; i++)
    {
        MetaRuntimeAttr *item = &insts.items[i];
        int arg_count = item->meta ? item->meta->params.count : 0;
        LLVMValueRef args_ptr = LLVMConstNull(LLVMPointerType(meta_arg_ty, 0));
        if (arg_count > 0)
        {
            LLVMValueRef *arg_vals = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)arg_count);
            for (int ai = 0; ai < arg_count; ai++)
            {
                Expr *arg_expr = ai < item->attr->args.count ? item->attr->args.items[ai] : item->meta->params.items[ai].default_value;
                LLVMValueRef tag = LLVMConstInt(t_i64, KN_ANY_TAG_NONE, 0);
                LLVMValueRef payload = LLVMConstInt(t_i64, 0, 0);
                meta_expr_to_const_parts(g, arg_expr, &tag, &payload);
                {
                    LLVMValueRef fields[3];
                    fields[0] = build_string_const(g, item->meta->params.items[ai].name, (int)kn_strlen(item->meta->params.items[ai].name));
                    fields[1] = tag;
                    fields[2] = payload;
                    arg_vals[ai] = LLVMConstNamedStruct(meta_arg_ty, fields, 3);
                }
            }
            args_ptr = meta_emit_const_array_ptr(g, meta_arg_ty, arg_vals, arg_count);
        }
        {
            const char *attr_q = item->meta->qname ? item->meta->qname : item->meta->name;
            LLVMValueRef owner_ptr = LLVMConstNull(t_i8ptr);
            if (item->owner_callable_qname)
            {
                FuncSymbol *fs = functable_find(&g->funcs, item->owner_callable_qname);
                if (fs)
                {
                    LLVMValueRef wrap = ensure_func_wrapper(g, fs);
                    if (wrap)
                        owner_ptr = LLVMConstBitCast(wrap, t_i8ptr);
                }
            }

            LLVMValueRef fields[8];
            fields[0] = build_string_const(g, item->owner_name, (int)kn_strlen(item->owner_name));
            fields[1] = LLVMConstInt(t_i64, item->owner_kind, 0);
            fields[2] = LLVMConstInt(t_i64, item->owner_param_count, 0);
            fields[3] = owner_ptr;
            fields[4] = build_string_const(g, attr_q, (int)kn_strlen(attr_q));
            fields[5] = build_string_const(g, item->meta->name, (int)kn_strlen(item->meta->name));
            fields[6] = args_ptr;
            fields[7] = LLVMConstInt(t_i64, arg_count, 0);
            inst_vals[i] = LLVMConstNamedStruct(meta_inst_ty, fields, 8);
        }
    }
    LLVMValueRef inst_ptr = meta_emit_const_array_ptr(g, meta_inst_ty, inst_vals, insts.count);

    {
        LLVMValueRef fields[5];
        const char *module_name = codegen_primary_module_name(g);
        fields[0] = build_string_const(g, module_name, (int)kn_strlen(module_name));
        fields[1] = types_ptr;
        fields[2] = LLVMConstInt(t_i64, type_index, 0);
        fields[3] = inst_ptr;
        fields[4] = LLVMConstInt(t_i64, insts.count, 0);
        LLVMValueRef gv = LLVMAddGlobal(g->mod, meta_mod_ty, "__kn_meta_module_data");
        LLVMSetLinkage(gv, LLVMPrivateLinkage);
        LLVMSetGlobalConstant(gv, 1);
        LLVMSetInitializer(gv, LLVMConstNamedStruct(meta_mod_ty, fields, 5));
        LLVMBuildRet(g->builder, LLVMConstBitCast(gv, t_i8ptr));
    }
}

static void declare_global_vars(Codegen *g, StmtList *globals)
{
    if (!g || !globals || !g->global_scope) return;
    for (int i = 0; i < globals->count; i++)
    {
        Stmt *st = globals->items[i];
        if (!st || st->kind != ST_VAR) continue;
        if (scope_find(g->global_scope, st->v.var.name))
            continue;

        LLVMTypeRef ty = llvm_type(g, st->v.var.type);
        const char *gname = mangle_global_var_name(st->v.var.name);
        LLVMValueRef gv = LLVMAddGlobal(g->mod, ty, gname);
        LLVMSetLinkage(gv, LLVMInternalLinkage);
        LLVMSetInitializer(gv, build_zeroed_global_initializer(g, st->v.var.type));
        scope_add(g->global_scope, st->v.var.name, st->v.var.type, gv);
    }
}

static void maybe_call_global_init(Codegen *g)
{
    if (!g || !g->fn_global_init || !g->fn_global_init_ty)
        return;
    LLVMBuildCall2(g->builder, g->fn_global_init_ty, g->fn_global_init, 0, 0, "");
}

static bool global_init_needed(Codegen *g, StmtList *globals)
{
    if (!g || !globals)
        return false;

    for (int i = 0; i < globals->count; i++)
    {
        Stmt *st = globals->items[i];
        if (st && st->kind == ST_VAR)
            return true;
    }

    for (int ci = 0; ci < g->classes.count; ci++)
    {
        ClassSymbol *cs = &g->classes.items[ci];
        if (!cs || !cs->decl) continue;

        for (int fi = 0; fi < cs->decl->fields.count; fi++)
        {
            Field *f = &cs->decl->fields.items[fi];
            if (!f->is_static) continue;
            if (f->init || is_gc_root_type(f->type))
                return true;
        }
    }

    return false;
}

static void codegen_global_init(Codegen *g, StmtList *globals)
{
    if (!global_init_needed(g, globals))
        return;

    g->fn_global_init_ty = LLVMFunctionType(g->voidt, 0, 0, 0);
    g->fn_global_init = LLVMAddFunction(g->mod, "__kn_global_init", g->fn_global_init_ty);
    LLVMSetLinkage(g->fn_global_init, LLVMInternalLinkage);

    g->gv_global_init_done = LLVMAddGlobal(g->mod, g->i1, "__kn_global_init_done");
    LLVMSetLinkage(g->gv_global_init_done, LLVMInternalLinkage);
    LLVMSetInitializer(g->gv_global_init_done, LLVMConstInt(g->i1, 0, 0));

    g->gv_global_gc_frame = LLVMAddGlobal(g->mod, g->i8ptr, "__kn_global_gc_frame");
    LLVMSetLinkage(g->gv_global_gc_frame, LLVMInternalLinkage);
    LLVMSetInitializer(g->gv_global_gc_frame, LLVMConstNull(g->i8ptr));

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, g->fn_global_init, "entry");
    LLVMBasicBlockRef init_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_global_init, "init");
    LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, g->fn_global_init, "done");
    LLVMPositionBuilderAtEnd(g->builder, entry);
    LLVMValueRef inited = LLVMBuildLoad2(g->builder, g->i1, g->gv_global_init_done, "g.init.done");
    LLVMBuildCondBr(g->builder, inited, done_bb, init_bb);

    LLVMPositionBuilderAtEnd(g->builder, init_bb);
    // Mark as initialized before executing initializers to break recursive re-entry.
    LLVMBuildStore(g->builder, LLVMConstInt(g->i1, 1, 0), g->gv_global_init_done);

    LLVMValueRef frame = LLVMConstNull(g->i8ptr);
    if (g->fn_gc_push)
    {
        LLVMValueRef cur = LLVMBuildLoad2(g->builder, g->i8ptr, g->gv_global_gc_frame, "g.gc.cur");
        LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, cur, LLVMConstNull(g->i8ptr), "g.gc.isnull");
        LLVMBasicBlockRef frame_new = LLVMAppendBasicBlockInContext(g->ctx, g->fn_global_init, "g.gc.new");
        LLVMBasicBlockRef frame_have = LLVMAppendBasicBlockInContext(g->ctx, g->fn_global_init, "g.gc.have");
        LLVMBasicBlockRef frame_join = LLVMAppendBasicBlockInContext(g->ctx, g->fn_global_init, "g.gc.join");
        LLVMBuildCondBr(g->builder, is_null, frame_new, frame_have);

        LLVMPositionBuilderAtEnd(g->builder, frame_new);
        LLVMValueRef pushed = LLVMBuildCall2(g->builder, g->fn_gc_push_ty, g->fn_gc_push, 0, 0, "g.gc.push");
        LLVMBuildStore(g->builder, pushed, g->gv_global_gc_frame);
        LLVMBuildBr(g->builder, frame_join);

        LLVMPositionBuilderAtEnd(g->builder, frame_have);
        LLVMBuildBr(g->builder, frame_join);

        LLVMPositionBuilderAtEnd(g->builder, frame_join);
        LLVMValueRef phi = LLVMBuildPhi(g->builder, g->i8ptr, "g.gc.frame");
        LLVMValueRef vals[2] = { pushed, cur };
        LLVMBasicBlockRef bbs[2] = { frame_new, frame_have };
        LLVMAddIncoming(phi, vals, bbs, 2);
        frame = phi;
    }

    Scope *saved_scope = g->scope;
    Type saved_ret = g->current_ret_type;
    ClassDecl *saved_class = g->current_class;
    LLVMValueRef saved_this = g->current_this;
    const char *saved_name = g->current_func_name;
    LLVMValueRef saved_gc = g->gc_frame;
    int saved_loop = g->loop_depth;
    int saved_try = g->try_depth;

    Scope sc;
    sc.vars = 0;
    sc.parent = g->global_scope;
    g->scope = &sc;
    g->current_ret_type = type_make(TY_VOID);
    g->current_class = 0;
    g->current_this = 0;
    g->current_func_name = build_trace_label("<global init>", 0, 0, 0);
    g->gc_frame = 0;
    g->loop_depth = 0;
    g->try_depth = 0;

    for (int i = 0; i < globals->count; i++)
    {
        Stmt *st = globals->items[i];
        if (!st || st->kind != ST_VAR) continue;
        VarSymbol *gv = scope_find(g->global_scope, st->v.var.name);
        if (!gv) continue;

        if (g->fn_gc_add_root && g->fn_gc_push && is_gc_root_type(gv->type))
        {
            LLVMValueRef sizev = LLVMSizeOf(llvm_type(g, gv->type));
            if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
                sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "g.root.sz");
            LLVMValueRef addr = LLVMBuildBitCast(g->builder, gv->value, g->i8ptr, "g.root.addr");
            LLVMBuildCall2(g->builder, g->fn_gc_add_root_ty, g->fn_gc_add_root,
                           (LLVMValueRef[]){ frame, addr, sizev }, 3, "");
        }

        if (st->v.var.init)
        {
            CGValue iv = gen_expr(g, st->v.var.init);
            LLVMValueRef cv = cast_value(g, iv, gv->type);
            LLVMBuildStore(g->builder, cv, gv->value);
        }
    }

    for (int ci = 0; ci < g->classes.count; ci++)
    {
        ClassSymbol *cs = &g->classes.items[ci];
        if (!cs || !cs->decl) continue;

        ClassDecl *saved_eval_class = g->current_class;
        LLVMValueRef saved_eval_this = g->current_this;
        g->current_class = cs->decl;
        g->current_this = 0;

        for (int fi = 0; fi < cs->decl->fields.count; fi++)
        {
            Field *f = &cs->decl->fields.items[fi];
            if (!f->is_static) continue;

            LLVMValueRef gv = ensure_static_field_global(g, cg_class_qname(cs->decl), f);

            if (g->fn_gc_add_root && g->fn_gc_push && is_gc_root_type(f->type))
            {
                LLVMValueRef sizev = LLVMSizeOf(llvm_type(g, f->type));
                if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
                    sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "sf.root.sz");
                LLVMValueRef addr = LLVMBuildBitCast(g->builder, gv, g->i8ptr, "sf.root.addr");
                LLVMBuildCall2(g->builder, g->fn_gc_add_root_ty, g->fn_gc_add_root,
                               (LLVMValueRef[]){ frame, addr, sizev }, 3, "");
            }

            if (f->init)
            {
                CGValue iv = gen_expr(g, f->init);
                LLVMValueRef cv = cast_value(g, iv, f->type);
                LLVMBuildStore(g->builder, cv, gv);
            }
        }

        g->current_class = saved_eval_class;
        g->current_this = saved_eval_this;
    }

    g->scope = saved_scope;
    g->current_ret_type = saved_ret;
    g->current_class = saved_class;
    g->current_this = saved_this;
    g->current_func_name = saved_name;
    g->gc_frame = saved_gc;
    g->loop_depth = saved_loop;
    g->try_depth = saved_try;

    LLVMBuildBr(g->builder, done_bb);

    LLVMPositionBuilderAtEnd(g->builder, done_bb);
    LLVMBuildRetVoid(g->builder);
}
