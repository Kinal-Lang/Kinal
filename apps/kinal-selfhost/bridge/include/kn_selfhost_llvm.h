#ifndef KN_SELFHOST_LLVM_H
#define KN_SELFHOST_LLVM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stable, ownership-explicit ABI used by the Kinal compiler. LLVM objects
 * never cross this boundary directly; every handle is owned by the bridge.
 */
int kn_sh_llvm_version_major(void);
void *kn_sh_llvm_module_create(const char *name);
void kn_sh_llvm_module_dispose(void *module_handle);
int kn_sh_llvm_build_probe(void *module_handle, const char *function_name,
                           int return_value);
const char *kn_sh_llvm_module_ir(void *module_handle);
int kn_sh_llvm_emit_object(void *module_handle, const char *target_triple,
                           const char *output_path);
const char *kn_sh_llvm_last_error(void);
const char *kn_sh_rt_executable_path(void);
int kn_sh_rt_execute(const char *command_line);

/* Small bridge-owned vectors keep Kinal arrays out of the C ABI. */
void *kn_sh_llvm_handles_create(void);
int kn_sh_llvm_handles_add(void *handles, void *handle);
int kn_sh_llvm_handles_count(void *handles);
void *kn_sh_llvm_handles_get(void *handles, int index);

/* Types. */
void *kn_sh_llvm_type_void(void *module_handle);
void *kn_sh_llvm_type_int(void *module_handle, int bits);
void *kn_sh_llvm_type_float(void *module_handle, int bits);
void *kn_sh_llvm_type_pointer(void *element_type);
void *kn_sh_llvm_type_named_struct(void *module_handle, const char *name);
void *kn_sh_llvm_type_literal_struct(void *module_handle, void *fields,
                                    int packed);
int kn_sh_llvm_type_set_struct_body(void *struct_type, void *fields,
                                    int packed);
void *kn_sh_llvm_type_function(void *return_type, void *parameters,
                               int variadic);
void *kn_sh_llvm_type_array(void *element_type, int count);
void *kn_sh_llvm_type_of(void *value);

/* Functions, blocks, and constants. */
void *kn_sh_llvm_add_function(void *module_handle, const char *name,
                              void *function_type);
void *kn_sh_llvm_get_function(void *module_handle, const char *name);
void *kn_sh_llvm_get_parameter(void *function, int index);
void *kn_sh_llvm_append_block(void *module_handle, void *function,
                              const char *name);
void kn_sh_llvm_position_at_end(void *module_handle, void *block);
void *kn_sh_llvm_current_block(void *module_handle);
int kn_sh_llvm_block_is_terminated(void *block);
void *kn_sh_llvm_const_int(void *integer_type, int64_t value,
                           int sign_extend);
void *kn_sh_llvm_const_null(void *type);
void *kn_sh_llvm_const_undef(void *type);
void *kn_sh_llvm_build_global_string(void *module_handle, const char *text,
                                     const char *name);

/* Memory and aggregate instructions. */
void *kn_sh_llvm_build_alloca(void *module_handle, void *type,
                              const char *name);
void *kn_sh_llvm_build_load(void *module_handle, void *type, void *pointer,
                            const char *name);
void *kn_sh_llvm_build_store(void *module_handle, void *value, void *pointer);
void *kn_sh_llvm_build_struct_gep(void *module_handle, void *struct_type,
                                  void *pointer, int index, const char *name);
void *kn_sh_llvm_build_gep(void *module_handle, void *element_type,
                           void *pointer, void *indices, int in_bounds,
                           const char *name);
void *kn_sh_llvm_build_extract_value(void *module_handle, void *aggregate,
                                     int index, const char *name);
void *kn_sh_llvm_build_insert_value(void *module_handle, void *aggregate,
                                    void *value, int index, const char *name);
void *kn_sh_llvm_build_size_of(void *type);

/* Scalar instructions and casts. */
void *kn_sh_llvm_build_add(void *module_handle, void *left, void *right,
                           const char *name);
void *kn_sh_llvm_build_sub(void *module_handle, void *left, void *right,
                           const char *name);
void *kn_sh_llvm_build_mul(void *module_handle, void *left, void *right,
                           const char *name);
void *kn_sh_llvm_build_sdiv(void *module_handle, void *left, void *right,
                            const char *name);
void *kn_sh_llvm_build_srem(void *module_handle, void *left, void *right,
                            const char *name);
void *kn_sh_llvm_build_and(void *module_handle, void *left, void *right,
                           const char *name);
void *kn_sh_llvm_build_or(void *module_handle, void *left, void *right,
                          const char *name);
void *kn_sh_llvm_build_xor(void *module_handle, void *left, void *right,
                           const char *name);
void *kn_sh_llvm_build_shl(void *module_handle, void *left, void *right,
                           const char *name);
void *kn_sh_llvm_build_ashr(void *module_handle, void *left, void *right,
                            const char *name);
void *kn_sh_llvm_build_neg(void *module_handle, void *value,
                           const char *name);
void *kn_sh_llvm_build_not(void *module_handle, void *value,
                           const char *name);
void *kn_sh_llvm_build_icmp(void *module_handle, int predicate, void *left,
                            void *right, const char *name);
void *kn_sh_llvm_build_select(void *module_handle, void *condition,
                              void *when_true, void *when_false,
                              const char *name);
void *kn_sh_llvm_build_bitcast(void *module_handle, void *value, void *type,
                               const char *name);
void *kn_sh_llvm_build_zext(void *module_handle, void *value, void *type,
                            const char *name);
void *kn_sh_llvm_build_sext(void *module_handle, void *value, void *type,
                            const char *name);
void *kn_sh_llvm_build_trunc(void *module_handle, void *value, void *type,
                             const char *name);
void *kn_sh_llvm_build_ptr_to_int(void *module_handle, void *value, void *type,
                                  const char *name);
void *kn_sh_llvm_build_int_to_ptr(void *module_handle, void *value, void *type,
                                  const char *name);

/* Calls and control flow. */
void *kn_sh_llvm_build_call(void *module_handle, void *function_type,
                            void *function, void *arguments,
                            const char *name);
void *kn_sh_llvm_build_return(void *module_handle, void *value);
void *kn_sh_llvm_build_return_void(void *module_handle);
void *kn_sh_llvm_build_branch(void *module_handle, void *destination);
void *kn_sh_llvm_build_cond_branch(void *module_handle, void *condition,
                                   void *when_true, void *when_false);
void *kn_sh_llvm_build_phi(void *module_handle, void *type,
                           const char *name);
int kn_sh_llvm_phi_add_incoming(void *phi, void *value, void *block);
int kn_sh_llvm_verify_module(void *module_handle);

#ifdef __cplusplus
}
#endif

#endif
