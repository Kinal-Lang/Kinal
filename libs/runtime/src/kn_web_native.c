#include "kn_http_wire.h"
#ifdef _WIN32
#include <io.h>
#include "kn_web_schannel.h"
typedef SRWLOCK WebLock;
#define WEB_LOCK(l) AcquireSRWLockExclusive(l)
#define WEB_UNLOCK(l) ReleaseSRWLockExclusive(l)
#else
#define __cdecl
#include "kn_http_tls_linux.h"
typedef pthread_mutex_t WebLock;
#define WEB_LOCK(l) pthread_mutex_lock(l)
#define WEB_UNLOCK(l) pthread_mutex_unlock(l)
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dirent.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

typedef struct
{
    uint64_t tag;
    uint64_t payload;
} KnAny;

#if defined(_WIN32) || defined(_WIN64)
typedef volatile LONG KnWebAtomicInt;
static int kn_web_atomic_inc(KnWebAtomicInt *value)
{
    return (int)InterlockedIncrement(value);
}
static int kn_web_atomic_dec(KnWebAtomicInt *value)
{
    return (int)InterlockedDecrement(value);
}
static int kn_web_atomic_load(KnWebAtomicInt *value)
{
    return (int)InterlockedCompareExchange(value, 0, 0);
}
static void kn_web_atomic_store(KnWebAtomicInt *value, int v)
{
    InterlockedExchange(value, (LONG)v);
}
static void kn_web_sleep_ms(unsigned int ms)
{
    Sleep(ms);
}
#else
typedef _Atomic int KnWebAtomicInt;
static int kn_web_atomic_inc(KnWebAtomicInt *value)
{
    return atomic_fetch_add_explicit(value, 1, memory_order_seq_cst) + 1;
}
static int kn_web_atomic_dec(KnWebAtomicInt *value)
{
    return atomic_fetch_sub_explicit(value, 1, memory_order_seq_cst) - 1;
}
static int kn_web_atomic_load(KnWebAtomicInt *value)
{
    return atomic_load_explicit(value, memory_order_seq_cst);
}
static void kn_web_atomic_store(KnWebAtomicInt *value, int v)
{
    atomic_store_explicit(value, v, memory_order_seq_cst);
}
static void kn_web_sleep_ms(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
    nanosleep(&ts, 0);
}
#endif

typedef KnAny(__cdecl *KnCallableWrapper)(void *env, KnAny *args, uint64_t argc);

typedef struct
{
    char *prefix;
    char *directory;
    char *default_file;
} KnWebStaticMount;

typedef struct
{
    HttpSocket listener;
    WebLock lock;
    WebTlsIdentity identity;
    int use_tls;
    unsigned generation;
    KnWebAtomicInt workers;
    size_t body_limit;
    char *host;
    int port;
    char listen_text[128];
    KnWebStaticMount *mounts;
    size_t mount_count;
    size_t mount_cap;
    void *dispatcher;
    KnWebAtomicInt running;
    char last_error[512];

} KnWebServer;

typedef struct
{
    HttpIO *io;
    HttpHead *head;
    KnWebServer *server;
    char *method, *path, *query;
    HttpBuffer body;
    int replied;
} KnWebRequestScope;

static KN_HTTP_LOCAL KnWebRequestScope *g_kn_web_current;
static KN_HTTP_LOCAL char g_kn_web_query_value[4096];
static const char *kn_web_safe(const char *text)
{
    return text ? text : "";
}

static char *kn_web_strdup(const char *text)
{
    size_t len;
    char *copy;
    if (!text)
        text = "";
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (!copy)
        return 0;
    memcpy(copy, text, len + 1);
    return copy;
}

static void kn_web_set_error(KnWebServer *server, const char *fmt, ...)
{
    va_list ap;
    if (!server)
        return;
    va_start(ap, fmt);
    vsnprintf(server->last_error, sizeof(server->last_error), fmt, ap);
    server->last_error[sizeof(server->last_error) - 1] = 0;
    va_end(ap);
}

static void kn_web_format_listen_text(KnWebServer *server)
{
    if (!server)
        return;
    if (!server->host || !server->host[0])
        server->host = kn_web_strdup("0.0.0.0");
    if (!server->host)
        return;
    if (strchr(server->host, ':') && server->host[0] != '[')
        snprintf(server->listen_text, sizeof(server->listen_text), "[%s]:%d", server->host, server->port);
    else
        snprintf(server->listen_text, sizeof(server->listen_text), "%s:%d", server->host, server->port);
    server->listen_text[sizeof(server->listen_text) - 1] = 0;
}

static int kn_web_path_prefix_match(const char *path, const char *prefix)
{
    size_t plen;
    if (!path || !prefix || !prefix[0])
        return 0;
    if (strcmp(prefix, "/") == 0)
        return path[0] == '/';

    plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0)
        return 0;
    if (path[plen] == 0)
        return 1;
    if (prefix[plen - 1] == '/')
        return 1;
    return path[plen] == '/';
}

static char *kn_web_normalize_prefix_copy(const char *prefix)
{
    size_t len;
    char *copy;
    if (!prefix || !prefix[0])
        prefix = "/";
    if (prefix[0] != '/')
        return 0;

    len = strlen(prefix);
    while (len > 1 && prefix[len - 1] == '/')
        len--;

    copy = (char *)malloc(len + 1);
    if (!copy)
        return 0;
    memcpy(copy, prefix, len);
    copy[len] = 0;
    return copy;
}

static void kn_web_free_mount(KnWebStaticMount *mount)
{
    if (!mount)
        return;
    free(mount->prefix);
    free(mount->directory);
    free(mount->default_file);
    memset(mount, 0, sizeof(*mount));
}

static int kn_web_relative_path_is_safe(const char *relative_path)
{
    const char *cur;
    if (!relative_path)
        return 0;
    for (cur = relative_path; *cur; cur++)
        if (*cur == '\\' || *cur == ':')
            return 0;

    cur = relative_path;
    while (*cur)
    {
        const char *start = cur;
        size_t len;
        while (*cur && *cur != '/')
            cur++;
        len = (size_t)(cur - start);
        if (len == 1 && start[0] == '.')
            return 0;
        if (len == 2 && start[0] == '.' && start[1] == '.')
            return 0;
        if (*cur == '/')
            cur++;
    }
    return 1;
}

static char *kn_web_join_path(const char *base, const char *relative)
{
    size_t base_len;
    size_t rel_len;
    size_t off = 0;
    int need_sep;
    const char *src;
    char *out;
    if (!base || !base[0] || !relative || !relative[0])
        return 0;

    base_len = strlen(base);
    rel_len = strlen(relative);
    need_sep = !(base[base_len - 1] == '\\' || base[base_len - 1] == '/');
    out = (char *)malloc(base_len + rel_len + (need_sep ? 2 : 1));
    if (!out)
        return 0;

    memcpy(out + off, base, base_len);
    off += base_len;
    if (need_sep)
        out[off++] =
#if defined(_WIN32) || defined(_WIN64)
            '\\';
#else
            '/';
#endif

    src = relative;
    while (*src)
    {
        char ch = *src++;
        if (ch == '/' || ch == '\\')
        {
#if defined(_WIN32) || defined(_WIN64)
            out[off++] = '\\';
#else
            out[off++] = '/';
#endif
        }
        else
        {
            out[off++] = ch;
        }
    }
    out[off] = 0;
    return out;
}

static int kn_web_path_is_directory(const char *path)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD attrs;
    if (!path || !path[0])
        return 0;
    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (!path || !path[0])
        return 0;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

static int kn_web_path_is_regular_file(const char *path)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD attrs;
    if (!path || !path[0])
        return 0;
    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat st;
    if (!path || !path[0])
        return 0;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
#endif
}

static char *kn_web_build_static_path(const KnWebStaticMount *mount, const char *request_path)
{
    const char *relative;
    size_t prefix_len;
    int use_default = 0;
    char *full = 0;
    if (!mount || !request_path)
        return 0;

    prefix_len = strlen(kn_web_safe(mount->prefix));
    if (strcmp(kn_web_safe(mount->prefix), "/") == 0)
    {
        relative = request_path;
        while (*relative == '/')
            relative++;
    }
    else
    {
        relative = request_path + prefix_len;
        while (*relative == '/')
            relative++;
    }

    if (!relative[0] || request_path[strlen(request_path) - 1] == '/')
        use_default = 1;

    if (relative[0] && !kn_web_relative_path_is_safe(relative))
        return 0;

    if (relative[0])
    {
        full = kn_web_join_path(kn_web_safe(mount->directory), relative);
        if (!full)
            return 0;
    }
    else
    {
        full = kn_web_strdup(kn_web_safe(mount->directory));
        if (!full)
            return 0;
    }

    if (use_default || kn_web_path_is_directory(full))
    {
        char *with_default;
        if (!mount->default_file || !mount->default_file[0])
        {
            free(full);
            return 0;
        }
        with_default = kn_web_join_path(full, mount->default_file);
        free(full);
        full = with_default;
        if (!full)
            return 0;
    }

    if (!kn_web_path_is_regular_file(full))
    {
        free(full);
        return 0;
    }
    return full;
}

static const KnWebStaticMount *kn_web_find_best_mount(KnWebServer *server, const char *request_path)
{
    const KnWebStaticMount *best = 0;
    size_t best_len = 0;
    size_t i;
    if (!server || !request_path)
        return 0;

    for (i = 0; i < server->mount_count; i++)
    {
        const KnWebStaticMount *mount = &server->mounts[i];
        size_t len = strlen(kn_web_safe(mount->prefix));
        if (!kn_web_path_prefix_match(request_path, kn_web_safe(mount->prefix)))
            continue;
        if (!best || len > best_len)
        {
            best = mount;
            best_len = len;
        }
    }
    return best;
}

#if defined(_WIN32) || defined(_WIN64)
static uint64_t kn_web_filetime_u64(FILETIME ft)
{
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return value.QuadPart;
}

static uint64_t kn_web_scan_directory_reload_version(const char *directory)
{
    WIN32_FILE_ATTRIBUTE_DATA info;
    WIN32_FIND_DATAA data;
    HANDLE handle;
    char *pattern;
    uint64_t best = 0;

    if (!directory || !directory[0])
        return 0;
    if (!GetFileAttributesExA(directory, GetFileExInfoStandard, &info))
        return 0;

    best = kn_web_filetime_u64(info.ftLastWriteTime);
    pattern = kn_web_join_path(directory, "*");
    if (!pattern)
        return best;

    handle = FindFirstFileA(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE)
        return best;

    do
    {
        char *child;
        uint64_t child_value;
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
            continue;

        child_value = kn_web_filetime_u64(data.ftLastWriteTime);
        if (child_value > best)
            best = child_value;

        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            continue;

        child = kn_web_join_path(directory, data.cFileName);
        if (!child)
            continue;
        child_value = kn_web_scan_directory_reload_version(child);
        if (child_value > best)
            best = child_value;
        free(child);
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
    return best;
}
#endif

#if !defined(_WIN32) && !defined(_WIN64)
static uint64_t kn_web_stat_time_u64(const struct stat *st)
{
#if defined(__APPLE__)
    return (uint64_t)st->st_mtimespec.tv_sec * 1000000000ull + (uint64_t)st->st_mtimespec.tv_nsec;
#elif defined(__linux__)
    return (uint64_t)st->st_mtim.tv_sec * 1000000000ull + (uint64_t)st->st_mtim.tv_nsec;
#else
    return (uint64_t)st->st_mtime * 1000000000ull;
#endif
}

static uint64_t kn_web_scan_directory_reload_version(const char *directory)
{
    struct stat st;
    DIR *dir;
    struct dirent *ent;
    uint64_t best = 0;

    if (!directory || !directory[0])
        return 0;
    if (stat(directory, &st) != 0)
        return 0;
    best = kn_web_stat_time_u64(&st);

    dir = opendir(directory);
    if (!dir)
        return best;

    while ((ent = readdir(dir)) != 0)
    {
        char *child;
        uint64_t child_value;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        child = kn_web_join_path(directory, ent->d_name);
        if (!child)
            continue;
        if (lstat(child, &st) == 0)
        {
            child_value = kn_web_stat_time_u64(&st);
            if (child_value > best)
                best = child_value;
            if (S_ISDIR(st.st_mode))
            {
                child_value = kn_web_scan_directory_reload_version(child);
                if (child_value > best)
                    best = child_value;
            }
        }
        free(child);
    }

    closedir(dir);
    return best;
}
#endif

void *__kn_web_server_create(const char *host, int port)
{
    KnWebServer *server;
    if (port <= 0 || port > 65535)
        return 0;

    server = (KnWebServer *)calloc(1, sizeof(KnWebServer));
    if (!server)
        return 0;

    server->host = kn_web_strdup((host && host[0]) ? host : "0.0.0.0");
    if (!server->host)
    {
        free(server);
        return 0;
    }

#ifndef _WIN32
    pthread_mutex_init(&server->lock, NULL);
#endif
    server->body_limit = HTTP_BODY_LIMIT;
    server->port = port;
    server->listener = HTTP_INVALID;
    kn_web_format_listen_text(server);
    return server;
}

const char *__kn_web_server_last_error(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    if (!server || !server->last_error[0])
        return "";
    return server->last_error;
}

int __kn_web_server_set_dispatcher(void *handle, void *dispatcher)
{
    KnWebServer *server = (KnWebServer *)handle;
    if (!server || !dispatcher)
        return 0;
    server->dispatcher = dispatcher;
    server->last_error[0] = 0;
    return 1;
}

static int mount_locked(KnWebServer *server, const char *prefix, const char *directory,
                        const char *default_file)
{
    KnWebStaticMount *slot;
    char *norm_prefix;
    if (!server)
        return 0;
    if (!directory || !directory[0])
    {
        kn_web_set_error(server, "Static directory is empty");
        return 0;
    }

    norm_prefix = kn_web_normalize_prefix_copy(prefix);
    if (!norm_prefix)
    {
        kn_web_set_error(server, "Static prefix must start with '/'");
        return 0;
    }

    if (server->mount_count + 1 > server->mount_cap)
    {
        size_t new_cap = server->mount_cap ? server->mount_cap * 2 : 4;
        KnWebStaticMount *next =
            (KnWebStaticMount *)realloc(server->mounts, sizeof(KnWebStaticMount) * new_cap);
        if (!next)
        {
            free(norm_prefix);
            kn_web_set_error(server, "Out of memory while growing static mount table");
            return 0;
        }
        server->mounts = next;
        server->mount_cap = new_cap;
    }

    slot = &server->mounts[server->mount_count];
    memset(slot, 0, sizeof(*slot));
    slot->prefix = norm_prefix;
    slot->directory = kn_web_strdup(directory);
    slot->default_file = kn_web_strdup(default_file && default_file[0] ? default_file : "index.html");
    if (!slot->directory || !slot->default_file)
    {
        kn_web_free_mount(slot);
        kn_web_set_error(server, "Out of memory while storing static mount");
        return 0;
    }

    server->mount_count++;
    server->last_error[0] = 0;
    return 1;
}

int __kn_web_server_mount(void *handle, const char *prefix, const char *directory, const char *default_file)
{
    KnWebServer *s = handle;
    if (!s)
        return 0;
    WEB_LOCK(&s->lock);
    int result = mount_locked(s, prefix, directory, default_file);
    WEB_UNLOCK(&s->lock);
    return result;
}

void *__kn_gc_alloc(size_t size);

static const char *managed_string(const char *value)
{
    if (!value || !*value)
        return "";
    size_t size = strlen(value) + 1;
    char *result = __kn_gc_alloc(size);
    if (result)
        memcpy(result, value, size);
    return result ? result : "";
}

static int decode(char *dst, size_t cap, const char *src, size_t len, int plus)
{
    size_t out = 0;
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)src[i];
        if (c == '%')
        {
            if (i + 2 >= len)
                return 0;
            char hex[3] = {src[i + 1], src[i + 2], 0};
            int64_t value;
            if (!http_number(hex, 16, &value))
                return 0;
            c = (unsigned char)value;
            i += 2;
        }
        else if (plus && c == '+')
            c = ' ';
        if (c < 32 || c == 127 || out + 1 >= cap)
            return 0;
        dst[out++] = (char)c;
    }
    dst[out] = 0;
    return 1;
}

void *__kn_web_current_server(void)
{
    return g_kn_web_current ? g_kn_web_current->server : NULL;
}
const char *__kn_web_current_method(void)
{
    return managed_string(g_kn_web_current ? g_kn_web_current->method : NULL);
}
const char *__kn_web_current_uri(void)
{
    return managed_string(g_kn_web_current ? g_kn_web_current->path : NULL);
}
const char *__kn_web_current_query(void)
{
    return managed_string(g_kn_web_current ? g_kn_web_current->query : NULL);
}

const char *__kn_web_current_query_value(const char *name)
{
    if (!name)
        return "";
    const char *p = g_kn_web_current ? g_kn_web_current->query : "";
    while (*p)
    {
        const char *end = p + strcspn(p, "&");
        const char *eq = memchr(p, '=', (size_t)(end - p));
        if (eq && decode(g_kn_web_query_value, sizeof(g_kn_web_query_value), p, (size_t)(eq - p), 1) &&
            !strcmp(g_kn_web_query_value, name) &&
            decode(g_kn_web_query_value, sizeof(g_kn_web_query_value), eq + 1, (size_t)(end - eq - 1), 1))
            return managed_string(g_kn_web_query_value);
        p = *end ? end + 1 : end;
    }
    return "";
}

const char *__kn_web_current_header(const char *name)
{
    return managed_string(g_kn_web_current && name ? http_header(g_kn_web_current->head, name) : NULL);
}
int64_t __kn_web_current_content_length(void)
{
    return g_kn_web_current ? (int64_t)g_kn_web_current->body.size : 0;
}
const char *__kn_web_current_body_text(void)
{
    return managed_string(g_kn_web_current ? g_kn_web_current->body.data : NULL);
}
int __kn_web_current_replied(void)
{
    return g_kn_web_current && g_kn_web_current->replied;
}

static const char *reason_phrase(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 304:
        return "Not Modified";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 413:
        return "Content Too Large";
    case 500:
        return "Internal Server Error";
    case 503:
        return "Service Unavailable";
    default:
        return "";
    }
}
static int send_head(HttpIO *io, int status, const char *type, int64_t length)
{
    char head[1024];
    if (strpbrk(type, "\r\n"))
        return 0;
    int n =
        snprintf(head, sizeof(head),
                 "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n",
                 status, reason_phrase(status), type, (long long)length);
    return n > 0 && (size_t)n < sizeof(head) && http_write(io, head, (size_t)n);
}

int __kn_web_reply_text(int status, const char *type, const char *body)
{
    KnWebRequestScope *s = g_kn_web_current;
    if (!s || s->replied || status < 200 || status > 599)
        return 0;
    const char *text = kn_web_safe(body);
    size_t n = status == 204 || status == 304 ? 0 : strlen(text);
    s->replied = 1;
    return send_head(s->io, status, type && *type ? type : "text/plain; charset=utf-8", (int64_t)n) &&
           (!strcmp(s->method, "HEAD") || http_write(s->io, text, n));
}

static const char *mime_type(const char *path)
{
    static const char *const types[][2] = {{"html", "text/html; charset=utf-8"},
                                           {"css", "text/css; charset=utf-8"},
                                           {"js", "text/javascript; charset=utf-8"},
                                           {"mjs", "text/javascript; charset=utf-8"},
                                           {"json", "application/json"},
                                           {"svg", "image/svg+xml"},
                                           {"png", "image/png"},
                                           {"jpg", "image/jpeg"},
                                           {"jpeg", "image/jpeg"},
                                           {"gif", "image/gif"},
                                           {"webp", "image/webp"},
                                           {"ico", "image/x-icon"},
                                           {"woff", "font/woff"},
                                           {"woff2", "font/woff2"},
                                           {"wasm", "application/wasm"},
                                           {"xml", "application/xml"},
                                           {"pdf", "application/pdf"},
                                           {"txt", "text/plain; charset=utf-8"},
                                           {"mp4", "video/mp4"}};
    const char *ext = strrchr(path, '.');
    if (ext)
        for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
            if (!http_casecmp(ext + 1, types[i][0]))
                return types[i][1];
    return "application/octet-stream";
}

/* Compare the open file's final path to the mounted directory before sending any bytes. */
static int within_mount(FILE *f, const char *root)
{
#ifdef _WIN32
    HANDLE dir =
        CreateFileA(root, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (dir == INVALID_HANDLE_VALUE)
        return 0;
    char base[4096], file[4096];
    DWORD a = GetFinalPathNameByHandleA(dir, base, sizeof(base), 0),
          b = GetFinalPathNameByHandleA((HANDLE)_get_osfhandle(_fileno(f)), file, sizeof(file), 0);
    CloseHandle(dir);
    if (!a || a >= sizeof(base) || !b || b >= sizeof(file))
        return 0;
    while (a && base[a - 1] == '\\')
        a--;
    return b > a && !_strnicmp(base, file, a) && file[a] == '\\';
#else
    char *base = realpath(root, NULL);
    char link[64], file[PATH_MAX + 1];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fileno(f));
    ssize_t len = readlink(link, file, sizeof(file) - 1);
    struct stat st;
    int ok = 0;
    if (base && len > 0 && (size_t)len < sizeof(file) - 1 && !fstat(fileno(f), &st) && S_ISREG(st.st_mode))
    {
        size_t n = strlen(base);
        if (n == 1 && *base == '/')
            n = 0;
        ok = (size_t)len > n && !memcmp(base, file, n) && file[n] == '/';
    }
    free(base);
    return ok;
#endif
}

int __kn_web_server_try_serve_static(void *handle)
{
    KnWebRequestScope *s = g_kn_web_current;
    if (!s || s->server != handle || s->replied || (strcmp(s->method, "GET") && strcmp(s->method, "HEAD")))
        return 0;
    /* Mount strings have server lifetime, while the mount table can grow. */
    KnWebStaticMount mount = {0};
    WEB_LOCK(&s->server->lock);
    const KnWebStaticMount *found = kn_web_find_best_mount(s->server, s->path);
    if (found)
        mount = *found;
    WEB_UNLOCK(&s->server->lock);
    char *path = mount.directory ? kn_web_build_static_path(&mount, s->path) : NULL;
    if (!path)
        return 0;
    FILE *f = fopen(path, "rb");
    if (f && within_mount(f, mount.directory))
    {
#ifdef _WIN32
        int64_t size = _filelengthi64(_fileno(f));
#else
        struct stat st;
        int64_t size = fstat(fileno(f), &st) == 0 ? (int64_t)st.st_size : -1;
#endif
        if (size >= 0)
        {
            s->replied = 1;
            if (send_head(s->io, 200, mime_type(path), size) && strcmp(s->method, "HEAD"))
            {
                char data[16384];
                while (size > 0)
                {
                    size_t n = fread(data, 1, size > (int64_t)sizeof(data) ? sizeof(data) : (size_t)size, f);
                    if (!n || !http_write(s->io, data, n))
                        break;
                    size -= (int64_t)n;
                }
            }
        }
    }
    if (f)
        fclose(f);
    free(path);
    return s->replied;
}

static void serve_connection(KnWebServer *server, HttpIO *io)
{
    HttpHead head;
    KnWebRequestScope s = {0};
    s.io = io;
    s.head = &head;
    s.server = server;
    int error = 400;
    if (!http_head(io, &head))
        goto rejected;
    s.method = head.line;
    char *space = strchr(s.method, ' ');
    if (!space || space == s.method)
        goto rejected;
    *space = 0;
    s.path = space + 1;
    for (char *p = s.method; *p; p++)
        if (!http_token((unsigned char)*p))
            goto rejected;
    space = strchr(s.path, ' ');
    if (!space || *s.path != '/')
        goto rejected;
    *space = 0;
    if (strcmp(space + 1, "HTTP/1.1") && strcmp(space + 1, "HTTP/1.0"))
        goto rejected;
    s.query = strchr(s.path, '?');
    if (s.query)
        *s.query++ = 0;
    else
        s.query = "";
    if (!decode(s.path, strlen(s.path) + 1, s.path, strlen(s.path), 0) ||
        !kn_web_relative_path_is_safe(s.path))
        goto rejected;
    if (head.length > (int64_t)server->body_limit)
    {
        error = 413;
        goto rejected;
    }
    const char *expect = http_header(&head, "Expect");
    if (expect && !http_casecmp(expect, "100-continue") &&
        !http_write(io, "HTTP/1.1 100 Continue\r\n\r\n", 25))
        return;
    if (!http_body(io, &head, 0, server->body_limit, http_append, &s.body))
        goto rejected;
    io->read_deadline = 0;
    g_kn_web_current = &s;
    ((KnCallableWrapper)server->dispatcher)(NULL, NULL, 0);
    if (!s.replied)
        __kn_web_reply_text(404, "text/plain", "Not Found");
    g_kn_web_current = NULL;
    free(s.body.data);
    return;
rejected:
    io->rejected = 1;
    free(s.body.data);
    if (io->limit_hit)
        error = 413;
    char message[96];
    int len = error == 413
                  ? snprintf(message, sizeof(message), "Request body exceeds %zu bytes.", server->body_limit)
                  : snprintf(message, sizeof(message), "Invalid HTTP request.");
    if (len > 0 && send_head(io, error, "text/plain; charset=utf-8", len))
        http_write(io, message, (size_t)len);
}

#ifdef _WIN32
static INIT_ONCE winsock_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK start_winsock(PINIT_ONCE once, void *param, void **ctx)
{
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}
static DWORD WINAPI web_worker(void *arg)
#else
static void *web_worker(void *arg)
#endif
{
    KnWebServer *s = arg;
#ifndef _WIN32
    sigset_t pipes;
    sigemptyset(&pipes);
    sigaddset(&pipes, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipes, NULL);
#endif
    HttpSocket listener = s->listener;
    while (kn_web_atomic_load(&s->running))
    {
#ifdef _WIN32
        WSAPOLLFD wait = {listener, POLLRDNORM, 0};
        if (WSAPoll(&wait, 1, 100) <= 0)
            continue;
#else
        struct pollfd wait = {listener, POLLIN, 0};
        if (poll(&wait, 1, 100) <= 0)
            continue;
#endif
        HttpSocket fd = accept(listener, NULL, NULL);
        if (fd == HTTP_INVALID)
            continue;
#ifdef _WIN32
        u_long blocking = 0;
        ioctlsocket(fd, FIONBIO, &blocking);
#else
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        fcntl(fd, F_SETFL, 0);
#endif
        http_timeout(fd, 10000);
        HttpIO io;
        http_init(&io, fd);
        io.read_deadline = http_millis() + 10000;
        if (s->use_tls)
        {
            WebTlsSession session = {0};
            int ok = web_tls_accept(&io, &session, &s->identity);
            if (ok)
                serve_connection(s, &io);
            web_tls_finish(&io, &session, &s->identity, ok);
        }
        else
            serve_connection(s, &io);
        /* A peer may still be sending its body when it receives a 413. Flush
           the response before closing with unread socket data (TCP reset). */
        if (io.rejected)
        {
#ifdef _WIN32
            shutdown(fd, SD_SEND);
#else
            shutdown(fd, SHUT_WR);
#endif
            io.read_deadline = http_millis() + 250;
            char discard[4096];
            for (size_t bytes = 0; bytes < 262144;)
            {
                int n = http_raw_read(&io, discard, sizeof(discard));
                if (n <= 0)
                    break;
                bytes += (size_t)n;
            }
        }
        http_close(fd);
    }
    WEB_LOCK(&s->lock);
    if (kn_web_atomic_dec(&s->workers) == 0)
    {
        http_close(s->listener);
        s->listener = HTTP_INVALID;
        identity_free(&s->identity);
        s->use_tls = 0;
    }
    WEB_UNLOCK(&s->lock);
    return 0;
}

static HttpSocket open_listener(const char *host, int port)
{
    char service[8];
    snprintf(service, sizeof(service), "%d", port);
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(host, service, &hints, &addresses) != 0)
        return HTTP_INVALID;
    HttpSocket fd = HTTP_INVALID;
    for (struct addrinfo *a = addresses; a; a = a->ai_next)
    {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd == HTTP_INVALID)
            continue;
        int one = 1;
#ifdef _WIN32
        setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&one, sizeof(one));
        u_long nonblock = 1;
        int rc = ioctlsocket(fd, FIONBIO, &nonblock);
#else
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        int rc = fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
        if (!rc && !bind(fd, a->ai_addr, (int)a->ai_addrlen) && !listen(fd, 128))
            break;
        http_close(fd);
        fd = HTTP_INVALID;
    }
    freeaddrinfo(addresses);
    return fd;
}
void __kn_web_server_stop(void *handle)
{
    KnWebServer *s = handle;
    if (!s)
        return;
    WEB_LOCK(&s->lock);
    unsigned generation = s->generation;
    kn_web_atomic_store(&s->running, 0);
    while ((!g_kn_web_current || g_kn_web_current->server != s) && s->generation == generation &&
           kn_web_atomic_load(&s->workers))
    {
        WEB_UNLOCK(&s->lock);
        kn_web_sleep_ms(10);
        WEB_LOCK(&s->lock);
    }
    WEB_UNLOCK(&s->lock);
}
static int start_locked(KnWebServer *s, const char *cert, const char *key)
{
    if (kn_web_atomic_load(&s->running))
        return 1;
    if (kn_web_atomic_load(&s->workers))
    {
        kn_web_set_error(s, "Server is stopping");
        return 0;
    }
#ifdef _WIN32
    if (!InitOnceExecuteOnce(&winsock_once, start_winsock, NULL, NULL))
    {
        kn_web_set_error(s, "Winsock startup failed");
        return 0;
    }
#endif
    if (cert && cert[0])
    {
        const char *error = identity_load(&s->identity, cert, kn_web_safe(key));
        if (error)
        {
            kn_web_set_error(s, "%s", error);
            return 0;
        }
        s->use_tls = 1;
    }
    s->listener = open_listener(s->host, s->port);
    if (s->listener == HTTP_INVALID)
    {
        identity_free(&s->identity);
        s->use_tls = 0;
        kn_web_set_error(s, "Cannot listen on %s", s->listen_text);
        return 0;
    }
    s->generation++;
    kn_web_atomic_store(&s->running, 1);
    for (int i = 0; i < 8; i++)
    {
        kn_web_atomic_inc(&s->workers);
#ifdef _WIN32
        HANDLE thread = CreateThread(NULL, 0, web_worker, s, 0, NULL);
        if (thread)
        {
            CloseHandle(thread);
            continue;
        }
#else
        pthread_t thread;
        if (!pthread_create(&thread, NULL, web_worker, s))
        {
            pthread_detach(thread);
            continue;
        }
#endif
        kn_web_atomic_dec(&s->workers);
        kn_web_atomic_store(&s->running, 0);
        if (!kn_web_atomic_load(&s->workers))
        {
            http_close(s->listener);
            s->listener = HTTP_INVALID;
            identity_free(&s->identity);
            s->use_tls = 0;
        }
        kn_web_set_error(s, "Cannot create HTTP worker");
        return 0;
    }
    return 1;
}
int __kn_web_server_start(void *handle, const char *cert, const char *key)
{
    KnWebServer *s = handle;
    if (!s || !s->dispatcher)
        return 0;
    WEB_LOCK(&s->lock);
    int ok = start_locked(s, cert, key);
    WEB_UNLOCK(&s->lock);
    return ok;
}
void __kn_web_server_wait(void *handle)
{
    KnWebServer *s = handle;
    if (!s)
        return;
    while (kn_web_atomic_load(&s->running))
        kn_web_sleep_ms(50);
}
int64_t __kn_web_server_reload_version(void *handle)
{
    KnWebServer *s = handle;
    uint64_t version = 0;
    if (!s)
        return 0;
    WEB_LOCK(&s->lock);
    for (size_t i = 0; i < s->mount_count; i++)
    {
        uint64_t v = kn_web_scan_directory_reload_version(s->mounts[i].directory);
        if (v > version)
            version = v;
    }
    WEB_UNLOCK(&s->lock);
    return (int64_t)version;
}
const char *__kn_web_server_context_info(void *handle)
{
    KnWebServer *s = handle;
    return s && kn_web_atomic_load(&s->running)
               ? "{\"backend\":\"native\",\"protocol\":\"HTTP/1.1\",\"workers\":8}"
               : "{}";
}

int __kn_web_server_set_body_limit(void *handle, int64_t bytes)
{
    KnWebServer *s = handle;
    if (!s || bytes < 1 || bytes > INT_MAX || kn_web_atomic_load(&s->running))
        return 0;
    s->body_limit = (size_t)bytes;
    return 1;
}
