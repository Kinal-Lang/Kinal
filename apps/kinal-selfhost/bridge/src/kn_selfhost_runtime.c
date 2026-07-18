#include <stdint.h>
#include <stdio.h>

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

const char *kn_sh_rt_char_to_string(uint8_t value)
{
    char *result = (char *)__kn_gc_alloc(2);
    if (!result) return "";
    result[0] = (char)value;
    result[1] = 0;
    return result;
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
