#ifdef _WIN32
// Weak fallback stubs for toolchains that emit stack probes without CRT.
__attribute__((weak)) void __chkstk(void) {}
__attribute__((weak)) void ___chkstk_ms(void) {}
__attribute__((weak)) void __kn_chkstk_stub(void) {}
#endif
