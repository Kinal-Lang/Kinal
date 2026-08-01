#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void *__kn_gc_alloc(uint64_t size);
extern const char *__kn_sys_executable_path(void);
extern int __kn_sys_exec(const char *command_line);

const char *kn_sh_rt_executable_path(void)
{
    return __kn_sys_executable_path();
}

int kn_sh_rt_execute(const char *command_line)
{
    return __kn_sys_exec(command_line);
}

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

int kn_sh_rt_string_equal(const char *left, const char *right)
{
    uint64_t i = 0;
    if (left == right) return 1;
    if (!left || !right) return 0;
    while (left[i] && right[i])
    {
        if (left[i] != right[i]) return 0;
        i++;
    }
    return left[i] == right[i] ? 1 : 0;
}

const char *kn_sh_rt_string_concat(const char *left, const char *right)
{
    uint64_t left_length = text_length(left);
    uint64_t right_length = text_length(right);
    char *result = (char *)__kn_gc_alloc(left_length + right_length + 1);
    if (!result) return "";
    for (uint64_t i = 0; i < left_length; i++) result[i] = left[i];
    for (uint64_t i = 0; i < right_length; i++) result[left_length + i] = right[i];
    result[left_length + right_length] = 0;
    return result;
}

const char *kn_sh_rt_i64_to_string(int64_t value)
{
    char reversed[32];
    uint64_t magnitude;
    uint64_t count = 0;
    uint64_t prefix = value < 0 ? 1 : 0;
    char *result;
    if (value < 0)
        magnitude = (uint64_t)(-(value + 1)) + 1;
    else
        magnitude = (uint64_t)value;
    do
    {
        reversed[count++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    } while (magnitude != 0);
    result = (char *)__kn_gc_alloc(prefix + count + 1);
    if (!result) return "";
    if (prefix) result[0] = '-';
    for (uint64_t i = 0; i < count; i++)
        result[prefix + i] = reversed[count - i - 1];
    result[prefix + count] = 0;
    return result;
}

const char *kn_sh_rt_f64_to_string(double value)
{
    return kn_sh_rt_i64_to_string((int64_t)value);
}

const char *kn_sh_rt_char_to_string(uint8_t value)
{
    char *result = (char *)__kn_gc_alloc(2);
    if (!result) return "";
    result[0] = (char)value;
    result[1] = 0;
    return result;
}

const char *kn_sh_rt_any_to_string(int64_t tag, int64_t payload)
{
    if (tag == 1) return kn_sh_rt_i64_to_string(payload);
    if (tag == 3) return payload ? "true" : "false";
    if (tag == 4) return kn_sh_rt_char_to_string((uint8_t)payload);
    if (tag == 5) return (const char *)(uintptr_t)payload;
    return "null";
}

int64_t kn_sh_rt_string_to_i64(const char *text)
{
    int negative = 0;
    uint64_t index = 0;
    uint64_t value = 0;
    uint64_t base = 10;
    if (!text) return 0;
    while (text[index] == ' ' || text[index] == '\t') index++;
    if (text[index] == '+' || text[index] == '-')
    {
        negative = text[index] == '-';
        index++;
    }
    if (text[index] == '0' && (text[index + 1] == 'x' || text[index + 1] == 'X'))
    {
        base = 16;
        index += 2;
    }
    while (text[index])
    {
        uint64_t digit;
        char current = text[index];
        if (current >= '0' && current <= '9') digit = (uint64_t)(current - '0');
        else if (current >= 'a' && current <= 'f') digit = (uint64_t)(current - 'a' + 10);
        else if (current >= 'A' && current <= 'F') digit = (uint64_t)(current - 'A' + 10);
        else break;
        if (digit >= base) break;
        value = value * base + digit;
        index++;
    }
    return negative ? -(int64_t)value : (int64_t)value;
}

double kn_sh_rt_string_to_f64(const char *text)
{
    return text ? strtod(text, 0) : 0.0;
}

float kn_sh_rt_string_to_f32(const char *text)
{
    return text ? (float)strtod(text, 0) : 0.0f;
}

int kn_sh_rt_string_to_bool(const char *text)
{
    return text && (text[0] == 't' || text[0] == '1') ? 1 : 0;
}

uint8_t kn_sh_rt_string_to_char(const char *text)
{
    return text ? (uint8_t)text[0] : 0;
}

typedef struct KnShError
{
    void *vtable;
    int64_t runtime_type_id;
    const char *title;
    const char *message;
    const char *trace;
    struct KnShError *inner;
} KnShError;

void *kn_sh_rt_error_new(int64_t runtime_type_id,
                         const char *title,
                         const char *message)
{
    KnShError *error = (KnShError *)__kn_gc_alloc((uint64_t)sizeof(KnShError));
    if (!error) return 0;
    error->vtable = 0;
    error->runtime_type_id = runtime_type_id;
    error->title = title ? title : "Error";
    error->message = message ? message : "";
    error->trace = "";
    error->inner = 0;
    return error;
}

const char *kn_sh_rt_error_title(const void *value)
{
    const KnShError *error = (const KnShError *)value;
    return error ? error->title : 0;
}

const char *kn_sh_rt_error_message(const void *value)
{
    const KnShError *error = (const KnShError *)value;
    return error ? error->message : 0;
}

const char *kn_sh_rt_error_trace(const void *value)
{
    const KnShError *error = (const KnShError *)value;
    return error ? error->trace : 0;
}

void *kn_sh_rt_error_inner(const void *value)
{
    const KnShError *error = (const KnShError *)value;
    return error ? error->inner : 0;
}

void kn_sh_rt_error_link_inner(void *value, void *inner)
{
    KnShError *error = (KnShError *)value;
    if (error && !error->inner && inner && inner != value)
        error->inner = (KnShError *)inner;
}

void kn_sh_rt_error_set_trace(void *value, const char *trace)
{
    KnShError *error = (KnShError *)value;
    if (error) error->trace = trace ? trace : "";
}

const char *kn_sh_rt_trace_format(const char *trace)
{
    uint64_t length = text_length(trace);
    char *result = (char *)__kn_gc_alloc(length + 1);
    uint64_t source = 0;
    uint64_t destination = 0;
    int first = 1;
    if (!result) return "";
    while (source < length)
    {
        uint64_t line_start = source;
        uint64_t line_end;
        while (source < length && trace[source] != '\n') source++;
        line_end = source;
        if (line_end >= line_start + 3 &&
            trace[line_start] == 'a' &&
            trace[line_start + 1] == 't' &&
            trace[line_start + 2] == ' ')
            line_start += 3;
        if (line_end > line_start)
        {
            if (!first)
            {
                result[destination++] = ' ';
                result[destination++] = '-';
                result[destination++] = '>';
                result[destination++] = ' ';
            }
            while (line_start < line_end)
                result[destination++] = trace[line_start++];
            first = 0;
        }
        if (source < length && trace[source] == '\n') source++;
    }
    result[destination] = 0;
    return result;
}

void kn_sh_rt_print(const char *text)
{
    fputs(text ? text : "", stdout);
    fflush(stdout);
}

void kn_sh_rt_print_line(const char *text)
{
    fputs(text ? text : "", stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

const char *kn_sh_rt_read_line(void)
{
    uint64_t length = 0;
    uint64_t capacity = 128;
    char *temporary = (char *)malloc((size_t)capacity);
    char *result;
    int current;
    if (!temporary) return "";
    while ((current = fgetc(stdin)) != EOF && current != '\n')
    {
        if (current == '\r') continue;
        if (length + 1 >= capacity)
        {
            uint64_t next_capacity = capacity * 2;
            char *next = (char *)realloc(temporary, (size_t)next_capacity);
            if (!next)
            {
                free(temporary);
                return "";
            }
            temporary = next;
            capacity = next_capacity;
        }
        temporary[length++] = (char)current;
    }
    result = (char *)__kn_gc_alloc(length + 1);
    if (!result)
    {
        free(temporary);
        return "";
    }
    for (uint64_t i = 0; i < length; i++) result[i] = temporary[i];
    result[length] = 0;
    free(temporary);
    return result;
}

void kn_sh_rt_memory_copy(void *destination, const void *source, uint64_t count)
{
    if (destination && source && count) memcpy(destination, source, (size_t)count);
}

void kn_sh_rt_memory_zero(void *destination, uint64_t count)
{
    if (destination && count) memset(destination, 0, (size_t)count);
}

uint8_t kn_sh_rt_volatile_read8(const volatile uint8_t *address) { return address ? *address : 0; }
uint16_t kn_sh_rt_volatile_read16(const volatile uint16_t *address) { return address ? *address : 0; }
uint32_t kn_sh_rt_volatile_read32(const volatile uint32_t *address) { return address ? *address : 0; }
uint64_t kn_sh_rt_volatile_read64(const volatile uint64_t *address) { return address ? *address : 0; }
void kn_sh_rt_volatile_write8(volatile uint8_t *address, uint8_t value) { if (address) *address = value; }
void kn_sh_rt_volatile_write16(volatile uint16_t *address, uint16_t value) { if (address) *address = value; }
void kn_sh_rt_volatile_write32(volatile uint32_t *address, uint32_t value) { if (address) *address = value; }
void kn_sh_rt_volatile_write64(volatile uint64_t *address, uint64_t value) { if (address) *address = value; }
