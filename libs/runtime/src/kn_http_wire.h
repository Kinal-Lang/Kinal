#ifndef KN_HTTP_WIRE_H
#define KN_HTTP_WIRE_H
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define KN_HTTP_LOCAL _Thread_local
typedef SOCKET HttpSocket;
#define HTTP_INVALID INVALID_SOCKET
#define http_close closesocket
#define http_casecmp _stricmp
#ifdef __MINGW32__
/* These objects also link into MSVC-ABI Kinal programs using the system UCRT. */
#if defined(__x86_64__)
__attribute__((section(".drectve"), used)) static const char http_stack_probe_alias[] =
    " /alternatename:___chkstk_ms=__chkstk";
#endif
static int http_ucrt_vsnprintf(char *out, size_t size, const char *fmt, va_list ap)
{
    return __stdio_common_vsprintf(_CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR, out, size, fmt, NULL, ap);
}
static int http_ucrt_snprintf(char *out, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = http_ucrt_vsnprintf(out, size, fmt, ap);
    va_end(ap);
    return n;
}
#define vsnprintf http_ucrt_vsnprintf
#define snprintf http_ucrt_snprintf
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#define KN_HTTP_LOCAL _Thread_local
typedef int HttpSocket;
#define HTTP_INVALID (-1)
#define http_close close
#define http_casecmp strcasecmp
#endif

#define HTTP_HEAD_LIMIT 32768
#define HTTP_FIELD_LIMIT 128
#define HTTP_BODY_LIMIT (16u * 1024u * 1024u)

typedef struct
{
    const char *name, *value;
} HttpField;

typedef struct
{
    char text[HTTP_HEAD_LIMIT];
    char *line;
    HttpField fields[HTTP_FIELD_LIMIT];
    unsigned count;
    int64_t length;
    int chunked;
} HttpHead;

typedef struct HttpIO HttpIO;
struct HttpIO
{
    HttpSocket fd;
    void *tls;
    int limit_hit;
    int rejected;
    uint64_t read_deadline;
    int (*read)(HttpIO *, char *, int);
    int (*write)(HttpIO *, const char *, int);
    unsigned char buffer[8192];
    size_t pos, end;
};

static uint64_t http_millis(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
#endif
}

static int http_read_time(HttpIO *io)
{
    if (io->read_deadline == 0)
        return 1;
    uint64_t now = http_millis();
    if (now >= io->read_deadline)
        return 0;
    unsigned remaining = (unsigned)(io->read_deadline - now);
#ifdef _WIN32
    DWORD timeout = remaining;
#else
    struct timeval timeout = {remaining / 1000, (remaining % 1000) * 1000};
#endif
    return setsockopt(io->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == 0;
}
static int http_raw_read(HttpIO *io, char *dst, int n)
{
    return http_read_time(io) ? (int)recv(io->fd, dst, n, 0) : -1;
}
static int http_raw_write(HttpIO *io, const char *src, int n)
{
#ifdef _WIN32
    return send(io->fd, src, n, 0);
#else
    return (int)send(io->fd, src, n, MSG_NOSIGNAL);
#endif
}
static void http_init(HttpIO *io, HttpSocket fd)
{
    memset(io, 0, sizeof(*io));
    io->fd = fd;
    io->read = http_raw_read;
    io->write = http_raw_write;
}
static void http_timeout(HttpSocket fd, int ms)
{
#ifdef _WIN32
    DWORD t = (DWORD)ms;
#else
    struct timeval t = {ms / 1000, (ms % 1000) * 1000};
#endif
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof(t));
}
static int http_write(HttpIO *io, const void *src, size_t n)
{
    const char *s = (const char *)src;
    while (n)
    {
        int done = io->write(io, s, (int)(n > 65536 ? 65536 : n));
        if (done <= 0)
            return 0;
        s += done;
        n -= (size_t)done;
    }
    return 1;
}
static int http_fill(HttpIO *io)
{
    if (io->tls && !http_read_time(io))
        return -1;
    int n = io->read(io, (char *)io->buffer, sizeof(io->buffer));
    io->pos = 0;
    io->end = n > 0 ? (size_t)n : 0;
    return n;
}
static int http_line(HttpIO *io, char *dst, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap)
    {
        if (io->pos == io->end && http_fill(io) <= 0)
            return -1;
        unsigned char c = io->buffer[io->pos++];
        if (c == '\n')
        {
            if (!used || dst[used - 1] != '\r')
                return -1;
            dst[used - 1] = 0;
            return (int)(used - 1);
        }
        if (!c || (used && dst[used - 1] == '\r'))
            return -1;
        dst[used++] = (char)c;
    }
    return -1;
}
static int http_token(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           (c && strchr("!#$%&'*+-.^_\x60|~", c));
}
static int http_field(char *line, HttpField *out)
{
    char *sep = strchr(line, ':');
    if (!sep || sep == line)
        return 0;
    for (char *p = line; p < sep; p++)
        if (!http_token((unsigned char)*p))
            return 0;
    *sep++ = 0;
    while (*sep == ' ' || *sep == '\t')
        sep++;
    char *end = sep;
    for (; *end; end++)
        if (((unsigned char)*end < 32 && *end != '\t') || *end == 127)
            return 0;
    while (end > sep && (end[-1] == ' ' || end[-1] == '\t'))
        *--end = 0;
    out->name = line;
    out->value = sep;
    return 1;
}
static int http_number(const char *text, unsigned radix, int64_t *result)
{
    uint64_t n = 0;
    if (!*text)
        return 0;
    for (; *text; text++)
    {
        unsigned c = (unsigned char)*text, d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return 0;
        if (d >= radix || n > ((uint64_t)INT64_MAX - d) / radix)
            return 0;
        n = n * radix + d;
    }
    *result = (int64_t)n;
    return 1;
}
static const char *http_header(const HttpHead *h, const char *name)
{
    for (unsigned i = 0; i < h->count; i++)
        if (!http_casecmp(h->fields[i].name, name))
            return h->fields[i].value;
    return NULL;
}
static int http_head(HttpIO *io, HttpHead *h)
{
    size_t offset;
    h->count = 0;
    h->chunked = 0;
    h->length = -1;
    h->line = h->text;
    int n = http_line(io, h->text, sizeof(h->text));
    if (n <= 0)
        return 0;
    offset = (size_t)n + 1;
    for (;;)
    {
        char *line = h->text + offset;
        n = http_line(io, line, sizeof(h->text) - offset);
        if (n < 0)
            return 0;
        if (!n)
            return !(h->chunked && h->length >= 0);
        offset += (size_t)n + 1;
        if (h->count == HTTP_FIELD_LIMIT)
            return 0;
        HttpField *f = &h->fields[h->count++];
        if (!http_field(line, f))
            return 0;
        if (!http_casecmp(f->name, "Content-Length"))
        {
            if (h->length >= 0 || !http_number(f->value, 10, &h->length))
                return 0;
        }
        else if (!http_casecmp(f->name, "Transfer-Encoding"))
        {
            if (h->chunked || http_casecmp(f->value, "chunked"))
                return 0;
            h->chunked = 1;
        }
    }
}

typedef int (*HttpSink)(void *, const char *, size_t);

static int http_copy(HttpIO *io, int64_t want, size_t limit, size_t *used, HttpSink sink, void *arg)
{
    if (want >= 0 && (uint64_t)want > limit - *used)
    {
        io->limit_hit = 1;
        return 0;
    }
    while (want != 0)
    {
        if (io->pos == io->end)
        {
            int r = http_fill(io);
            if (r <= 0)
                return r == 0 && want < 0;
        }
        size_t n = io->end - io->pos;
        if (want >= 0 && n > (uint64_t)want)
            n = (size_t)want;
        if (n > limit - *used)
        {
            io->limit_hit = 1;
            return 0;
        }
        if (!sink(arg, (const char *)io->buffer + io->pos, n))
            return 0;
        *used += n;
        io->pos += n;
        if (want > 0)
            want -= (int64_t)n;
    }
    return 1;
}
static int http_body(HttpIO *io, const HttpHead *h, int eof_allowed, size_t limit, HttpSink sink, void *arg)
{
    size_t used = 0;
    if (!h->chunked)
        return http_copy(io, h->length < 0 && !eof_allowed ? 0 : h->length, limit, &used, sink, arg);
    for (;;)
    {
        char line[1024];
        if (http_line(io, line, sizeof(line)) <= 0)
            return 0;
        char *extension = strchr(line, ';');
        if (extension)
            *extension = 0;
        int64_t n;
        if (!http_number(line, 16, &n))
            return 0;
        if (!n)
        {
            size_t trailers = 0;
            for (;;)
            {
                int len = http_line(io, line, sizeof(line));
                if (len < 0 || (trailers += (size_t)len + 2) > HTTP_HEAD_LIMIT)
                    return 0;
                if (!len)
                    return 1;
                HttpField field;
                if (!http_field(line, &field))
                    return 0;
            }
        }
        if (!http_copy(io, n, limit, &used, sink, arg) || http_line(io, line, sizeof(line)) != 0)
            return 0;
    }
}
typedef struct
{
    char *data;
    size_t size, cap;
} HttpBuffer;
static int http_append(void *arg, const char *data, size_t n)
{
    HttpBuffer *b = (HttpBuffer *)arg;
    if (b->size + n + 1 > b->cap)
    {
        size_t cap = b->cap ? b->cap * 2 : 4096;
        if (cap < b->size + n + 1)
            cap = b->size + n + 1;
        char *p = (char *)realloc(b->data, cap);
        if (!p)
            return 0;
        b->data = p;
        b->cap = cap;
    }
    memcpy(b->data + b->size, data, n);
    b->size += n;
    b->data[b->size] = 0;
    return 1;
}
#endif
