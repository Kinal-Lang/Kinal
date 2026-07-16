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
