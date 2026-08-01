#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#define KN_API __declspec(dllexport)
#else
#define KN_API
#endif

KN_API int kn_native_add(int a, int b)
{
    return a + b;
}

KN_API int kn_native_mul(int a, int b)
{
    return a * b;
}

KN_API const char *kn_native_hello(void)
{
    return "ffi-native";
}

KN_API int kn_native_negative(void)
{
    return -7;
}

KN_API int kn_native_int_echo(int value)
{
    return value;
}

KN_API int kn_native_truthy_two(void)
{
    return 2;
}

KN_API int kn_native_false(void)
{
    return 0;
}

KN_API int kn_native_bool_echo(int value)
{
    return value ? 2 : 0;
}

KN_API int32_t kn_native_sum_i32(const int32_t *values, int32_t count)
{
    int32_t sum = 0;
    for (int32_t i = 0; values && i < count; i++)
        sum += values[i];
    return sum;
}

KN_API void kn_native_increment_i32(int32_t *values, int32_t count)
{
    for (int32_t i = 0; values && i < count; i++)
        values[i]++;
}

KN_API int32_t kn_native_ascii_sum(const char *values, int32_t count)
{
    int32_t sum = 0;
    for (int32_t i = 0; values && i < count; i++)
        sum += (unsigned char)values[i];
    return sum;
}

KN_API int32_t kn_native_string_length(const char *value)
{
    int32_t length = 0;
    while (value && value[length]) length++;
    return length;
}

KN_API int32_t *kn_native_numbers(void)
{
    static int32_t values[] = { 11, 22, 33 };
    return values;
}
