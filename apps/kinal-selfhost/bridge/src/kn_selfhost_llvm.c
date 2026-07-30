#include "kn_selfhost_llvm.h"

#include <stdint.h>

#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"

typedef struct KnShLlvmModule
{
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
} KnShLlvmModule;

typedef struct KnShHandleList
{
    uint64_t count;
    uint64_t capacity;
    void **items;
} KnShHandleList;

/* The hosted Kinal runtime owns bridge allocations and scans them as roots. */
extern void *__kn_gc_alloc(uint64_t size);

static char g_last_error[1024];
static int g_target_initialized;

static void clear_error(void)
{
    g_last_error[0] = 0;
}

static void set_error(const char *text)
{
    uint64_t i = 0;
    if (!text)
        text = "unknown LLVM bridge error";
    while (text[i] && i + 1 < (uint64_t)sizeof(g_last_error))
    {
        g_last_error[i] = text[i];
        i++;
    }
    g_last_error[i] = 0;
}

static char *copy_runtime_string(const char *text)
{
    uint64_t length = 0;
    char *copy;
    if (!text)
        text = "";
    while (text[length])
        length++;
    copy = (char *)__kn_gc_alloc(length + 1);
    if (!copy)
    {
        set_error("out of memory while copying an LLVM string");
        return 0;
    }
    for (uint64_t i = 0; i < length; i++)
        copy[i] = text[i];
    copy[length] = 0;
    return copy;
}

static int initialize_native_target(void)
{
    if (g_target_initialized)
        return 1;
    if (LLVMInitializeNativeTarget() != 0 ||
        LLVMInitializeNativeAsmPrinter() != 0 ||
        LLVMInitializeNativeAsmParser() != 0)
    {
        set_error("LLVM native target initialization failed");
        return 0;
    }
    g_target_initialized = 1;
    return 1;
}

int kn_sh_llvm_version_major(void)
{
    unsigned major = 0;
    LLVMGetVersion(&major, 0, 0);
    return (int)major;
}

void *kn_sh_llvm_module_create(const char *name)
{
    KnShLlvmModule *state;
    clear_error();
    state = (KnShLlvmModule *)__kn_gc_alloc((uint64_t)sizeof(KnShLlvmModule));
    if (!state)
    {
        set_error("out of memory while creating an LLVM module");
        return 0;
    }
    state->context = LLVMContextCreate();
    if (!state->context)
    {
        set_error("LLVMContextCreate failed");
        return 0;
    }
    state->module = LLVMModuleCreateWithNameInContext(
        name && name[0] ? name : "kinal.selfhost", state->context);
    if (!state->module)
    {
        LLVMContextDispose(state->context);
        state->context = 0;
        set_error("LLVMModuleCreateWithNameInContext failed");
        return 0;
    }
    state->builder = LLVMCreateBuilderInContext(state->context);
    if (!state->builder)
    {
        LLVMDisposeModule(state->module);
        LLVMContextDispose(state->context);
        state->module = 0;
        state->context = 0;
        set_error("LLVMCreateBuilderInContext failed");
        return 0;
    }
    return state;
}

void kn_sh_llvm_module_dispose(void *module_handle)
{
    KnShLlvmModule *state = (KnShLlvmModule *)module_handle;
    if (!state)
        return;
    if (state->builder)
        LLVMDisposeBuilder(state->builder);
    if (state->module)
        LLVMDisposeModule(state->module);
    if (state->context)
        LLVMContextDispose(state->context);
    state->builder = 0;
    state->module = 0;
    state->context = 0;
}

int kn_sh_llvm_build_probe(void *module_handle, const char *function_name,
                           int return_value)
{
    KnShLlvmModule *state = (KnShLlvmModule *)module_handle;
    LLVMTypeRef i32_type;
    LLVMTypeRef function_type;
    LLVMValueRef function;
    LLVMBasicBlockRef entry;
    clear_error();
    if (!state || !state->context || !state->module || !state->builder)
    {
        set_error("invalid LLVM module handle");
        return 0;
    }
    if (!function_name || !function_name[0])
        function_name = "kn_selfhost_probe";
    if (LLVMGetNamedFunction(state->module, function_name))
    {
        set_error("probe function already exists");
        return 0;
    }
    i32_type = LLVMInt32TypeInContext(state->context);
    function_type = LLVMFunctionType(i32_type, 0, 0, 0);
    function = LLVMAddFunction(state->module, function_name, function_type);
    entry = LLVMAppendBasicBlockInContext(state->context, function, "entry");
    LLVMPositionBuilderAtEnd(state->builder, entry);
    LLVMBuildRet(state->builder,
                 LLVMConstInt(i32_type, (unsigned long long)(unsigned int)return_value, 0));
    return 1;
}

const char *kn_sh_llvm_module_ir(void *module_handle)
{
    KnShLlvmModule *state = (KnShLlvmModule *)module_handle;
    char *message;
    char *copy;
    clear_error();
    if (!state || !state->module)
    {
        set_error("invalid LLVM module handle");
        return "";
    }
    message = LLVMPrintModuleToString(state->module);
    if (!message)
    {
        set_error("LLVMPrintModuleToString failed");
        return "";
    }
    copy = copy_runtime_string(message);
    LLVMDisposeMessage(message);
    return copy ? copy : "";
}

int kn_sh_llvm_emit_object(void *module_handle, const char *target_triple,
                           const char *output_path)
{
    KnShLlvmModule *state = (KnShLlvmModule *)module_handle;
    char *default_triple = 0;
    const char *triple = target_triple;
    LLVMTargetRef target = 0;
    LLVMTargetMachineRef target_machine = 0;
    LLVMTargetDataRef target_data = 0;
    char *message = 0;
    int ok = 0;
    clear_error();
    if (!state || !state->module)
    {
        set_error("invalid LLVM module handle");
        return 0;
    }
    if (!output_path || !output_path[0])
    {
        set_error("object output path is empty");
        return 0;
    }
    if (!initialize_native_target())
        return 0;
    if (!triple || !triple[0])
    {
        default_triple = LLVMGetDefaultTargetTriple();
        triple = default_triple;
    }
    if (!triple || !triple[0])
    {
        set_error("LLVM did not provide a target triple");
        goto cleanup;
    }
    if (LLVMGetTargetFromTriple(triple, &target, &message) != 0)
    {
        set_error(message ? message : "LLVMGetTargetFromTriple failed");
        goto cleanup;
    }
    LLVMSetTarget(state->module, triple);
    target_machine = LLVMCreateTargetMachine(
        target, triple, "generic", "", LLVMCodeGenLevelDefault,
        LLVMRelocDefault, LLVMCodeModelDefault);
    if (!target_machine)
    {
        set_error("LLVMCreateTargetMachine failed");
        goto cleanup;
    }
    target_data = LLVMCreateTargetDataLayout(target_machine);
    if (!target_data)
    {
        set_error("LLVMCreateTargetDataLayout failed");
        goto cleanup;
    }
    LLVMSetModuleDataLayout(state->module, target_data);
    if (LLVMTargetMachineEmitToFile(target_machine, state->module,
                                    (char *)output_path, LLVMObjectFile,
                                    &message) != 0)
    {
        set_error(message ? message : "LLVMTargetMachineEmitToFile failed");
        goto cleanup;
    }
    ok = 1;

cleanup:
    if (message)
        LLVMDisposeMessage(message);
    if (target_data)
        LLVMDisposeTargetData(target_data);
    if (target_machine)
        LLVMDisposeTargetMachine(target_machine);
    if (default_triple)
        LLVMDisposeMessage(default_triple);
    return ok;
}

const char *kn_sh_llvm_last_error(void)
{
    return g_last_error;
}

static KnShLlvmModule *module_state(void *module_handle)
{
    KnShLlvmModule *state = (KnShLlvmModule *)module_handle;
    return state && state->context && state->module && state->builder ? state : 0;
}

static const char *safe_name(const char *name)
{
    return name ? name : "";
}

void *kn_sh_llvm_handles_create(void)
{
    KnShHandleList *list = (KnShHandleList *)__kn_gc_alloc(sizeof(KnShHandleList));
    if (!list)
        return 0;
    list->count = 0;
    list->capacity = 8;
    list->items = (void **)__kn_gc_alloc(sizeof(void *) * list->capacity);
    if (!list->items)
        return 0;
    return list;
}

int kn_sh_llvm_handles_add(void *handles, void *handle)
{
    KnShHandleList *list = (KnShHandleList *)handles;
    if (!list)
        return 0;
    if (list->count == list->capacity)
    {
        uint64_t next_capacity = list->capacity * 2;
        void **next = (void **)__kn_gc_alloc(sizeof(void *) * next_capacity);
        if (!next)
            return 0;
        for (uint64_t i = 0; i < list->count; i++)
            next[i] = list->items[i];
        list->items = next;
        list->capacity = next_capacity;
    }
    list->items[list->count++] = handle;
    return 1;
}

int kn_sh_llvm_handles_count(void *handles)
{
    KnShHandleList *list = (KnShHandleList *)handles;
    return list ? (int)list->count : 0;
}

void *kn_sh_llvm_handles_get(void *handles, int index)
{
    KnShHandleList *list = (KnShHandleList *)handles;
    if (!list || index < 0 || (uint64_t)index >= list->count)
        return 0;
    return list->items[index];
}

void *kn_sh_llvm_type_void(void *module_handle)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state ? LLVMVoidTypeInContext(state->context) : 0;
}

void *kn_sh_llvm_type_int(void *module_handle, int bits)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && bits > 0 ? LLVMIntTypeInContext(state->context, (unsigned)bits) : 0;
}

void *kn_sh_llvm_type_float(void *module_handle, int bits)
{
    KnShLlvmModule *state = module_state(module_handle);
    if (!state)
        return 0;
    if (bits == 16) return LLVMHalfTypeInContext(state->context);
    if (bits == 32) return LLVMFloatTypeInContext(state->context);
    if (bits == 64) return LLVMDoubleTypeInContext(state->context);
    if (bits == 128) return LLVMFP128TypeInContext(state->context);
    return 0;
}

void *kn_sh_llvm_type_pointer(void *element_type)
{
    return element_type ? LLVMPointerType((LLVMTypeRef)element_type, 0) : 0;
}

void *kn_sh_llvm_type_named_struct(void *module_handle, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state ? LLVMStructCreateNamed(state->context, safe_name(name)) : 0;
}

void *kn_sh_llvm_type_literal_struct(void *module_handle, void *fields, int packed)
{
    KnShLlvmModule *state = module_state(module_handle);
    KnShHandleList *list = (KnShHandleList *)fields;
    return state && list ? LLVMStructTypeInContext(
        state->context, (LLVMTypeRef *)list->items, (unsigned)list->count, packed) : 0;
}

int kn_sh_llvm_type_set_struct_body(void *struct_type, void *fields, int packed)
{
    KnShHandleList *list = (KnShHandleList *)fields;
    if (!struct_type || !list)
        return 0;
    LLVMStructSetBody((LLVMTypeRef)struct_type, (LLVMTypeRef *)list->items,
                      (unsigned)list->count, packed);
    return 1;
}

void *kn_sh_llvm_type_function(void *return_type, void *parameters, int variadic)
{
    KnShHandleList *list = (KnShHandleList *)parameters;
    if (!return_type || !list)
        return 0;
    return LLVMFunctionType((LLVMTypeRef)return_type, (LLVMTypeRef *)list->items,
                            (unsigned)list->count, variadic);
}

void *kn_sh_llvm_type_array(void *element_type, int count)
{
    return element_type && count >= 0 ?
        LLVMArrayType((LLVMTypeRef)element_type, (unsigned)count) : 0;
}

void *kn_sh_llvm_type_of(void *value)
{
    return value ? LLVMTypeOf((LLVMValueRef)value) : 0;
}

void *kn_sh_llvm_add_function(void *module_handle, const char *name, void *function_type)
{
    KnShLlvmModule *state = module_state(module_handle);
    LLVMValueRef existing;
    if (!state || !function_type || !name || !name[0])
        return 0;
    existing = LLVMGetNamedFunction(state->module, name);
    return existing ? existing : LLVMAddFunction(state->module, name, (LLVMTypeRef)function_type);
}

void *kn_sh_llvm_get_function(void *module_handle, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && name ? LLVMGetNamedFunction(state->module, name) : 0;
}

void *kn_sh_llvm_get_parameter(void *function, int index)
{
    if (!function || index < 0 || (unsigned)index >= LLVMCountParams((LLVMValueRef)function))
        return 0;
    return LLVMGetParam((LLVMValueRef)function, (unsigned)index);
}

void *kn_sh_llvm_append_block(void *module_handle, void *function, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && function ? LLVMAppendBasicBlockInContext(
        state->context, (LLVMValueRef)function, safe_name(name)) : 0;
}

void kn_sh_llvm_position_at_end(void *module_handle, void *block)
{
    KnShLlvmModule *state = module_state(module_handle);
    if (state && block)
        LLVMPositionBuilderAtEnd(state->builder, (LLVMBasicBlockRef)block);
}

void *kn_sh_llvm_current_block(void *module_handle)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state ? LLVMGetInsertBlock(state->builder) : 0;
}

int kn_sh_llvm_block_is_terminated(void *block)
{
    return block && LLVMGetBasicBlockTerminator((LLVMBasicBlockRef)block) ? 1 : 0;
}

void *kn_sh_llvm_const_int(void *integer_type, int64_t value, int sign_extend)
{
    return integer_type ? LLVMConstInt((LLVMTypeRef)integer_type,
        (unsigned long long)(uint64_t)value, sign_extend) : 0;
}

void *kn_sh_llvm_const_float(void *float_type, double value)
{
    return float_type ? LLVMConstReal((LLVMTypeRef)float_type, value) : 0;
}

void *kn_sh_llvm_const_null(void *type)
{
    return type ? LLVMConstNull((LLVMTypeRef)type) : 0;
}

void *kn_sh_llvm_const_undef(void *type)
{
    return type ? LLVMGetUndef((LLVMTypeRef)type) : 0;
}

void *kn_sh_llvm_add_global(void *module_handle, void *type, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && type ? LLVMAddGlobal(state->module, (LLVMTypeRef)type,
        safe_name(name)) : 0;
}

int kn_sh_llvm_set_initializer(void *global, void *value)
{
    if (!global || !value) return 0;
    LLVMSetInitializer((LLVMValueRef)global, (LLVMValueRef)value);
    return 1;
}

int kn_sh_llvm_set_alignment(void *value, int alignment)
{
    if (!value || alignment <= 0) return 0;
    LLVMSetAlignment((LLVMValueRef)value, (unsigned)alignment);
    return 1;
}

void *kn_sh_llvm_build_global_string(void *module_handle, const char *text, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state ? LLVMBuildGlobalStringPtr(state->builder, text ? text : "", safe_name(name)) : 0;
}

#define KN_SH_BUILD_BINARY(name, llvm_name) \
void *name(void *module_handle, void *left, void *right, const char *value_name) \
{ \
    KnShLlvmModule *state = module_state(module_handle); \
    return state && left && right ? llvm_name(state->builder, (LLVMValueRef)left, \
        (LLVMValueRef)right, safe_name(value_name)) : 0; \
}

void *kn_sh_llvm_build_alloca(void *module_handle, void *type, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    LLVMBasicBlockRef current;
    LLVMValueRef function;
    LLVMBasicBlockRef entry;
    LLVMValueRef first;
    LLVMValueRef value;
    if (!state || !type) return 0;
    current = LLVMGetInsertBlock(state->builder);
    if (!current) return 0;
    function = LLVMGetBasicBlockParent(current);
    entry = function ? LLVMGetEntryBasicBlock(function) : 0;
    if (!entry) return 0;
    first = LLVMGetFirstInstruction(entry);
    if (first) LLVMPositionBuilderBefore(state->builder, first);
    else LLVMPositionBuilderAtEnd(state->builder, entry);
    value = LLVMBuildAlloca(state->builder, (LLVMTypeRef)type, safe_name(name));
    LLVMPositionBuilderAtEnd(state->builder, current);
    return value;
}

void *kn_sh_llvm_build_load(void *module_handle, void *type, void *pointer, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && type && pointer ? LLVMBuildLoad2(state->builder, (LLVMTypeRef)type,
        (LLVMValueRef)pointer, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_store(void *module_handle, void *value, void *pointer)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && value && pointer ? LLVMBuildStore(state->builder,
        (LLVMValueRef)value, (LLVMValueRef)pointer) : 0;
}

void *kn_sh_llvm_build_struct_gep(void *module_handle, void *struct_type,
                                  void *pointer, int index, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && struct_type && pointer && index >= 0 ? LLVMBuildStructGEP2(
        state->builder, (LLVMTypeRef)struct_type, (LLVMValueRef)pointer,
        (unsigned)index, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_gep(void *module_handle, void *element_type, void *pointer,
                           void *indices, int in_bounds, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    KnShHandleList *list = (KnShHandleList *)indices;
    if (!state || !element_type || !pointer || !list)
        return 0;
    if (in_bounds)
        return LLVMBuildInBoundsGEP2(state->builder, (LLVMTypeRef)element_type,
            (LLVMValueRef)pointer, (LLVMValueRef *)list->items,
            (unsigned)list->count, safe_name(name));
    return LLVMBuildGEP2(state->builder, (LLVMTypeRef)element_type,
        (LLVMValueRef)pointer, (LLVMValueRef *)list->items,
        (unsigned)list->count, safe_name(name));
}

void *kn_sh_llvm_build_extract_value(void *module_handle, void *aggregate,
                                     int index, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && aggregate && index >= 0 ? LLVMBuildExtractValue(state->builder,
        (LLVMValueRef)aggregate, (unsigned)index, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_insert_value(void *module_handle, void *aggregate,
                                    void *value, int index, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && aggregate && value && index >= 0 ? LLVMBuildInsertValue(
        state->builder, (LLVMValueRef)aggregate, (LLVMValueRef)value,
        (unsigned)index, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_size_of(void *type)
{
    return type ? LLVMSizeOf((LLVMTypeRef)type) : 0;
}

KN_SH_BUILD_BINARY(kn_sh_llvm_build_add, LLVMBuildAdd)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_sub, LLVMBuildSub)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_mul, LLVMBuildMul)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_sdiv, LLVMBuildSDiv)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_srem, LLVMBuildSRem)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_udiv, LLVMBuildUDiv)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_urem, LLVMBuildURem)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_fadd, LLVMBuildFAdd)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_fsub, LLVMBuildFSub)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_fmul, LLVMBuildFMul)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_fdiv, LLVMBuildFDiv)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_and, LLVMBuildAnd)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_or, LLVMBuildOr)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_xor, LLVMBuildXor)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_shl, LLVMBuildShl)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_ashr, LLVMBuildAShr)
KN_SH_BUILD_BINARY(kn_sh_llvm_build_lshr, LLVMBuildLShr)

void *kn_sh_llvm_build_neg(void *module_handle, void *value, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && value ? LLVMBuildNeg(state->builder, (LLVMValueRef)value, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_fneg(void *module_handle, void *value, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && value ? LLVMBuildFNeg(state->builder, (LLVMValueRef)value, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_not(void *module_handle, void *value, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && value ? LLVMBuildNot(state->builder, (LLVMValueRef)value, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_icmp(void *module_handle, int predicate, void *left,
                            void *right, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && left && right ? LLVMBuildICmp(state->builder,
        (LLVMIntPredicate)predicate, (LLVMValueRef)left, (LLVMValueRef)right,
        safe_name(name)) : 0;
}

void *kn_sh_llvm_build_fcmp(void *module_handle, int predicate, void *left,
                            void *right, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && left && right ? LLVMBuildFCmp(state->builder,
        (LLVMRealPredicate)predicate, (LLVMValueRef)left, (LLVMValueRef)right,
        safe_name(name)) : 0;
}

void *kn_sh_llvm_build_select(void *module_handle, void *condition,
                              void *when_true, void *when_false, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && condition && when_true && when_false ? LLVMBuildSelect(
        state->builder, (LLVMValueRef)condition, (LLVMValueRef)when_true,
        (LLVMValueRef)when_false, safe_name(name)) : 0;
}

#define KN_SH_BUILD_CAST(name, llvm_name) \
void *name(void *module_handle, void *value, void *type, const char *value_name) \
{ \
    KnShLlvmModule *state = module_state(module_handle); \
    return state && value && type ? llvm_name(state->builder, (LLVMValueRef)value, \
        (LLVMTypeRef)type, safe_name(value_name)) : 0; \
}

KN_SH_BUILD_CAST(kn_sh_llvm_build_bitcast, LLVMBuildBitCast)
KN_SH_BUILD_CAST(kn_sh_llvm_build_zext, LLVMBuildZExt)
KN_SH_BUILD_CAST(kn_sh_llvm_build_sext, LLVMBuildSExt)
KN_SH_BUILD_CAST(kn_sh_llvm_build_trunc, LLVMBuildTrunc)
KN_SH_BUILD_CAST(kn_sh_llvm_build_fp_ext, LLVMBuildFPExt)
KN_SH_BUILD_CAST(kn_sh_llvm_build_fp_trunc, LLVMBuildFPTrunc)
KN_SH_BUILD_CAST(kn_sh_llvm_build_ptr_to_int, LLVMBuildPtrToInt)
KN_SH_BUILD_CAST(kn_sh_llvm_build_int_to_ptr, LLVMBuildIntToPtr)
KN_SH_BUILD_CAST(kn_sh_llvm_build_si_to_fp, LLVMBuildSIToFP)
KN_SH_BUILD_CAST(kn_sh_llvm_build_ui_to_fp, LLVMBuildUIToFP)
KN_SH_BUILD_CAST(kn_sh_llvm_build_fp_to_si, LLVMBuildFPToSI)
KN_SH_BUILD_CAST(kn_sh_llvm_build_fp_to_ui, LLVMBuildFPToUI)

void *kn_sh_llvm_build_call(void *module_handle, void *function_type,
                            void *function, void *arguments, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    KnShHandleList *list = (KnShHandleList *)arguments;
    return state && function_type && function && list ? LLVMBuildCall2(
        state->builder, (LLVMTypeRef)function_type, (LLVMValueRef)function,
        (LLVMValueRef *)list->items, (unsigned)list->count, safe_name(name)) : 0;
}

void *kn_sh_llvm_build_return(void *module_handle, void *value)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && value ? LLVMBuildRet(state->builder, (LLVMValueRef)value) : 0;
}

void *kn_sh_llvm_build_return_void(void *module_handle)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state ? LLVMBuildRetVoid(state->builder) : 0;
}

void *kn_sh_llvm_build_branch(void *module_handle, void *destination)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && destination ? LLVMBuildBr(state->builder,
        (LLVMBasicBlockRef)destination) : 0;
}

void *kn_sh_llvm_build_cond_branch(void *module_handle, void *condition,
                                   void *when_true, void *when_false)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && condition && when_true && when_false ? LLVMBuildCondBr(
        state->builder, (LLVMValueRef)condition, (LLVMBasicBlockRef)when_true,
        (LLVMBasicBlockRef)when_false) : 0;
}

void *kn_sh_llvm_build_phi(void *module_handle, void *type, const char *name)
{
    KnShLlvmModule *state = module_state(module_handle);
    return state && type ? LLVMBuildPhi(state->builder, (LLVMTypeRef)type,
        safe_name(name)) : 0;
}

int kn_sh_llvm_phi_add_incoming(void *phi, void *value, void *block)
{
    LLVMValueRef incoming_value;
    LLVMBasicBlockRef incoming_block;
    if (!phi || !value || !block)
        return 0;
    incoming_value = (LLVMValueRef)value;
    incoming_block = (LLVMBasicBlockRef)block;
    LLVMAddIncoming((LLVMValueRef)phi, &incoming_value, &incoming_block, 1);
    return 1;
}

int kn_sh_llvm_verify_module(void *module_handle)
{
    KnShLlvmModule *state = module_state(module_handle);
    char *message = 0;
    clear_error();
    if (!state)
    {
        set_error("invalid LLVM module handle");
        return 0;
    }
    if (LLVMVerifyModule(state->module, LLVMReturnStatusAction, &message) != 0)
    {
        set_error(message ? message : "LLVM module verification failed");
        if (message) LLVMDisposeMessage(message);
        return 0;
    }
    if (message) LLVMDisposeMessage(message);
    return 1;
}
