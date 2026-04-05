#include "kn/platform.h"

static size_t fx_strlen(const char *s)
{
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

void fx_write_str(const char *s)
{
    if (!s) return;
    KN_HANDLE h = GetStdHandle(KN_STDOUT_HANDLE);
    KN_DWORD written = 0;
    WriteFile(h, s, (KN_DWORD)fx_strlen(s), &written, 0);
}

void fx_write_u32(uint32_t v)
{
    char buf[16];
    int i = 0;
    if (v == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (v && i < (int)(sizeof(buf) - 1))
        {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    while (i > 0)
    {
        char ch[2];
        KN_DWORD written = 0;
        ch[0] = buf[--i];
        ch[1] = 0;
        WriteFile(GetStdHandle(KN_STDOUT_HANDLE), ch, 1, &written, 0);
    }
}

int fx_strcmp(const char *a, const char *b)
{
    size_t i = 0;
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (a[i] && b[i])
    {
        unsigned char ac = (unsigned char)a[i];
        unsigned char bc = (unsigned char)b[i];
        if (ac != bc) return ac < bc ? -1 : 1;
        i++;
    }
    if (a[i] == b[i]) return 0;
    return ((unsigned char)a[i] < (unsigned char)b[i]) ? -1 : 1;
}
