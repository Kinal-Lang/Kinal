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
