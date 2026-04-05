#include "kn/util.h"
#include <stdio.h>

static KN_HANDLE g_heap;

typedef struct KnArenaBlock
{
    struct KnArenaBlock *next;
    size_t cap;
    size_t off;
} KnArenaBlock;

typedef enum
{
    KN_ALLOC_HEAP = 1,
    KN_ALLOC_ARENA = 2
} KnAllocKind;

typedef struct
{
    uint32_t magic;
    uint32_t kind; // KnAllocKind
    size_t size;   // requested payload size in bytes
} KnAllocHeader;

#define KN_ALLOC_MAGIC 0x4B4E414Cu /* 'KNAL' */

static int g_temp_arena_active;
static KnArenaBlock *g_temp_arena_first;
static KnArenaBlock *g_temp_arena_cur;
static KnArenaBlock *g_temp_arena_tail;

static size_t kn_align8(size_t n) { return (n + 7u) & ~(size_t)7u; }

static KnAllocHeader *kn_hdr(void *p)
{
    if (!p) return 0;
    KnAllocHeader *h = (KnAllocHeader *)((uint8_t *)p - sizeof(KnAllocHeader));
    if (h->magic != KN_ALLOC_MAGIC) return 0;
    return h;
}

static void *temp_arena_alloc(size_t n)
{
    if (!g_heap) g_heap = GetProcessHeap();
    if (!g_heap) return 0;

    // Always reserve room for an allocation header so kn_realloc can
    // safely copy only the old payload size (avoid OOB reads).
    if (n == 0) n = 1;
    size_t total = sizeof(KnAllocHeader) + n;
    total = kn_align8(total);

    KnArenaBlock *b = g_temp_arena_cur ? g_temp_arena_cur : g_temp_arena_first;
    while (b && b->off + total > b->cap)
        b = b->next;

    if (!b)
    {
        size_t cap = total < (64u * 1024u) ? (64u * 1024u) : total;
        size_t bytes = sizeof(KnArenaBlock) + cap;
        b = (KnArenaBlock *)HeapAlloc(g_heap, 0, bytes);
        if (!b) return 0;
        b->next = 0;
        b->cap = cap;
        b->off = 0;
        if (!g_temp_arena_first)
        {
            g_temp_arena_first = b;
            g_temp_arena_tail = b;
        }
        else
        {
            g_temp_arena_tail->next = b;
            g_temp_arena_tail = b;
        }
    }

    g_temp_arena_cur = b;
    uint8_t *base = (uint8_t *)(b + 1);
    KnAllocHeader *h = (KnAllocHeader *)(base + b->off);
    h->magic = KN_ALLOC_MAGIC;
    h->kind = KN_ALLOC_ARENA;
    h->size = n;
    void *p = (uint8_t *)h + sizeof(KnAllocHeader);
    b->off += total;
    return p;
}

size_t kn_strlen(const char *s)
{
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

int kn_strncmp(const char *a, const char *b, size_t n)
{
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    for (size_t i = 0; i < n; i++)
    {
        unsigned char ac = (unsigned char)a[i];
        unsigned char bc = (unsigned char)b[i];
        if (ac != bc) return ac < bc ? -1 : 1;
        if (ac == 0) return 0;
    }
    return 0;
}

int kn_strcmp(const char *a, const char *b)
{
    size_t na = kn_strlen(a);
    size_t nb = kn_strlen(b);
    size_t n = na < nb ? na : nb;
    int c = kn_strncmp(a, b, n);
    if (c != 0) return c;
    if (na == nb) return 0;
    return na < nb ? -1 : 1;
}

void kn_memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

void kn_memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
}

#ifndef KN_NO_MEM_OVERRIDE
void *memcpy(void *dst, const void *src, size_t n)
{
    kn_memcpy(dst, src, n);
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    kn_memset(dst, c, n);
    return dst;
}
#endif

void *kn_malloc(size_t n)
{
    if (g_temp_arena_active)
        return temp_arena_alloc(n);
    if (!g_heap) g_heap = GetProcessHeap();
    if (!g_heap) return 0;
    if (n == 0) n = 1;
    size_t total = sizeof(KnAllocHeader) + n;
    KnAllocHeader *h = (KnAllocHeader *)HeapAlloc(g_heap, 0, total);
    if (!h) return 0;
    h->magic = KN_ALLOC_MAGIC;
    h->kind = KN_ALLOC_HEAP;
    h->size = n;
    return (uint8_t *)h + sizeof(KnAllocHeader);
}

void kn_free(void *p)
{
    if (!p) return;
    KnAllocHeader *h = kn_hdr(p);
    if (!h) return;
    if (h->kind == KN_ALLOC_ARENA) return;
    if (!g_heap) g_heap = GetProcessHeap();
    if (g_heap) HeapFree(g_heap, 0, h);
}

void *kn_realloc(void *p, size_t n)
{
    if (!p) return kn_malloc(n);
    KnAllocHeader *h = kn_hdr(p);
    if (!h) return kn_malloc(n);
    if (n == 0) n = 1;

    if (h->kind == KN_ALLOC_HEAP)
    {
        if (!g_heap) g_heap = GetProcessHeap();
        if (!g_heap) return 0;
        size_t old_size = h->size;
        size_t total = sizeof(KnAllocHeader) + n;
        KnAllocHeader *nh = (KnAllocHeader *)HeapAlloc(g_heap, 0, total);
        if (!nh) return 0;
        nh->magic = KN_ALLOC_MAGIC;
        nh->kind = KN_ALLOC_HEAP;
        nh->size = n;
        void *np = (uint8_t *)nh + sizeof(KnAllocHeader);
        size_t to_copy = old_size < n ? old_size : n;
        kn_memcpy(np, p, to_copy);
        HeapFree(g_heap, 0, h);
        return np;
    }

    // Arena allocations can't be resized in place; copy the old payload.
    void *np = kn_malloc(n);
    if (!np) return 0;
    size_t to_copy = h->size < n ? h->size : n;
    kn_memcpy(np, p, to_copy);
    return np;
}

void kn_temp_arena_begin(void)
{
    g_temp_arena_active = 1;
    for (KnArenaBlock *b = g_temp_arena_first; b; b = b->next)
        b->off = 0;
    g_temp_arena_cur = g_temp_arena_first;
}

void kn_temp_arena_end(void)
{
    g_temp_arena_active = 0;
    g_temp_arena_cur = g_temp_arena_first;
}

static void kn_write_handle(KN_HANDLE h, const char *s)
{
    if (!s) return;
    KN_DWORD written = 0;
    WriteFile(h, s, (KN_DWORD)kn_strlen(s), &written, 0);
}

static void kn_write_handle_buf(KN_HANDLE h, const char *s, size_t len)
{
    if (!s || !len) return;
    KN_DWORD written = 0;
    WriteFile(h, s, (KN_DWORD)len, &written, 0);
}

void kn_write_str(const char *s)
{
    KN_HANDLE h = GetStdHandle(KN_STDERR_HANDLE);
    kn_write_handle(h, s ? s : "");
}

void kn_write_out_str(const char *s)
{
    KN_HANDLE h = GetStdHandle(KN_STDOUT_HANDLE);
    kn_write_handle(h, s ? s : "");
}

void kn_write_out_buf(const char *s, size_t len)
{
    KN_HANDLE h = GetStdHandle(KN_STDOUT_HANDLE);
    kn_write_handle_buf(h, s, len);
}

void kn_write_u32(uint32_t v)
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
    for (int j = i - 1; j >= 0; j--)
    {
        char tmp[2] = { buf[j], 0 };
        kn_write_str(tmp);
    }
}

void kn_die(const char *msg)
{
    kn_write_str(msg ? msg : "error");
    kn_write_str("\n");
    ExitProcess(1);
}

char *kn_read_file(const char *path, size_t *out_size)
{
    KN_HANDLE h = CreateFileA(path, KN_GENERIC_READ, KN_FILE_SHARE_READ, 0, KN_OPEN_EXISTING, KN_FILE_ATTRIBUTE_NORMAL, 0);
    if (!h || h == (KN_HANDLE)(intptr_t)-1)
        return 0;
    int64_t size64 = 0;
    if (!GetFileSizeEx(h, &size64))
    {
        CloseHandle(h);
        return 0;
    }
    if (size64 <= 0)
    {
        CloseHandle(h);
        return 0;
    }
    size_t size = (size_t)size64;
    char *buf = (char *)kn_malloc(size + 1);
    if (!buf)
    {
        CloseHandle(h);
        return 0;
    }
    KN_DWORD read = 0;
    if (!ReadFile(h, buf, (KN_DWORD)size, &read, 0) || read != (KN_DWORD)size)
    {
        CloseHandle(h);
        kn_free(buf);
        return 0;
    }
    CloseHandle(h);
    buf[size] = 0;
    if (out_size) *out_size = size;
    return buf;
}

char *kn_read_stdin(size_t *out_size)
{
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)kn_malloc(cap + 1);
    if (!buf) return 0;

    for (;;)
    {
        size_t avail = cap - len;
        if (avail == 0)
        {
            size_t new_cap = cap < 8192 ? cap * 2 : cap + 8192;
            char *nb = (char *)kn_realloc(buf, new_cap + 1);
            if (!nb)
            {
                kn_free(buf);
                return 0;
            }
            buf = nb;
            cap = new_cap;
            avail = cap - len;
        }

        size_t read = fread(buf + len, 1, avail, stdin);
        len += read;
        if (read < avail)
        {
            if (ferror(stdin))
            {
                kn_free(buf);
                return 0;
            }
            break;
        }
    }

    buf[len] = 0;
    if (out_size) *out_size = len;
    return buf;
}

// ----------------------
// Command line parsing
// ----------------------

static void args_push(KnArgs *a, char *s)
{
    if (a->argc % 8 == 0)
    {
        int new_cap = a->argc + 8;
        char **n = (char **)kn_malloc(sizeof(char *) * (size_t)new_cap);
        if (a->argv)
        {
            kn_memcpy(n, a->argv, sizeof(char *) * (size_t)a->argc);
            kn_free(a->argv);
        }
        a->argv = n;
    }
    a->argv[a->argc++] = s;
}

KnArgs kn_parse_cmdline(void)
{
    KnArgs args = {0, 0};
    const char *cmd = GetCommandLineA();
    size_t i = 0;
    while (cmd && cmd[i])
    {
        while (cmd[i] == ' ' || cmd[i] == '\t') i++;
        if (!cmd[i]) break;
        bool quoted = false;
        if (cmd[i] == '"') { quoted = true; i++; }
        size_t start = i;
        if (quoted)
        {
            while (cmd[i] && cmd[i] != '"') i++;
        }
        else
        {
            while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t') i++;
        }
        size_t len = i - start;
        char *s = (char *)kn_malloc(len + 1);
        if (!s) kn_die("out of memory");
        kn_memcpy(s, cmd + start, len);
        s[len] = 0;
        args_push(&args, s);
        if (quoted && cmd[i] == '"') i++;
    }
    return args;
}

void kn_append(char *buf, size_t cap, size_t *off, const char *s)
{
    if (!buf || !off || !s) return;
    while (*s && *off + 1 < cap)
        buf[(*off)++] = *s++;
    buf[*off] = 0;
}

// MSVC floating-point support symbol when linking without CRT.
#ifndef KN_NO_FLTUSED
int _fltused = 0;
#endif



