void *malloc(unsigned long long size);
void free(void *ptr);
void *memcpy(void *dst, const void *src, unsigned long long size);
void *memset(void *dst, int value, unsigned long long size);
char *strncpy(char *dst, const char *src, unsigned long long size);
int strcmp(const char *a, const char *b);

__declspec(dllimport) void __stdcall Sleep(unsigned int);

int kn_hosted_probe(void)
{
    char buf[8];
    char *heap = (char *)malloc(8);
    int ok = 0;
    if (!heap)
        return -1;

    memset(buf, 0, sizeof(buf));
    memcpy(buf, "ok", 3);
    strncpy(heap, buf, 8);
    ok = strcmp(heap, "ok") == 0;
    free(heap);
    Sleep(0);
    return ok ? 77 : 13;
}
