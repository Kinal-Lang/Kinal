#ifndef KN_SELFHOST_RUNTIME_H
#define KN_SELFHOST_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Raw console-mode leaves. Diagnostic/ANSI policy remains in Kinal. */
int32_t kn_sh_rt_console_mode(int32_t descriptor);
int32_t kn_sh_rt_set_console_mode(int32_t descriptor, uint32_t mode);

/* Test-only FFI probes used by the selfhost differential suite. */
int64_t kn_sh_rt_string_length(const char *text);
int64_t kn_sh_rt_char_array_length(const char *text);
int64_t kn_sh_rt_pointer_length(const char *text);
int32_t *kn_sh_rt_borrowed_i32_values(void);
void kn_sh_rt_memory_copy(int32_t *destination, const int32_t *source,
                          uint64_t byte_count);

/* Volatile access is an ABI leaf and intentionally has no Kinal policy. */
uint8_t kn_sh_rt_volatile_read8(const volatile uint8_t *address);
uint16_t kn_sh_rt_volatile_read16(const volatile uint16_t *address);
uint32_t kn_sh_rt_volatile_read32(const volatile uint32_t *address);
uint64_t kn_sh_rt_volatile_read64(const volatile uint64_t *address);
void kn_sh_rt_volatile_write8(volatile uint8_t *address, uint8_t value);
void kn_sh_rt_volatile_write16(volatile uint16_t *address, uint16_t value);
void kn_sh_rt_volatile_write32(volatile uint32_t *address, uint32_t value);
void kn_sh_rt_volatile_write64(volatile uint64_t *address, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif
