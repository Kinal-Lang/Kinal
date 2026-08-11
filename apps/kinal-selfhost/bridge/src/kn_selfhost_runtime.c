#include <stdint.h>

#include "kn_selfhost_runtime.h"

static uint64_t text_length(const char *text)
{
    uint64_t length = 0;
    if (!text) return 0;
    while (text[length]) length++;
    return length;
}

int64_t kn_sh_rt_string_length(const char *text)
{
    return (int64_t)text_length(text);
}

/* Distinct symbols keep C stage0 declarations with pointer-equivalent source
 * types from colliding in LLVM while exercising the same C pointer ABI. */
int64_t kn_sh_rt_char_array_length(const char *text)
{
    return (int64_t)text_length(text);
}

int64_t kn_sh_rt_pointer_length(const char *text)
{
    return (int64_t)text_length(text);
}

int32_t *kn_sh_rt_borrowed_i32_values(void)
{
    static int32_t values[] = { 11, 22, 33 };
    return values;
}

/* Test-only FFI probe: array parameters must decay to their C data pointer. */
void kn_sh_rt_memory_copy(int32_t *destination, const int32_t *source,
                          uint64_t byte_count)
{
    uint8_t *destination_bytes = (uint8_t *)destination;
    const uint8_t *source_bytes = (const uint8_t *)source;
    if (!destination_bytes || !source_bytes) return;
    for (uint64_t index = 0; index < byte_count; index++)
        destination_bytes[index] = source_bytes[index];
}

uint8_t kn_sh_rt_volatile_read8(const volatile uint8_t *address) { return address ? *address : 0; }
uint16_t kn_sh_rt_volatile_read16(const volatile uint16_t *address) { return address ? *address : 0; }
uint32_t kn_sh_rt_volatile_read32(const volatile uint32_t *address) { return address ? *address : 0; }
uint64_t kn_sh_rt_volatile_read64(const volatile uint64_t *address) { return address ? *address : 0; }
void kn_sh_rt_volatile_write8(volatile uint8_t *address, uint8_t value) { if (address) *address = value; }
void kn_sh_rt_volatile_write16(volatile uint16_t *address, uint16_t value) { if (address) *address = value; }
void kn_sh_rt_volatile_write32(volatile uint32_t *address, uint32_t value) { if (address) *address = value; }
void kn_sh_rt_volatile_write64(volatile uint64_t *address, uint64_t value) { if (address) *address = value; }
