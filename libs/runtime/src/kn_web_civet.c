#include "civetweb.h"

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
static int kn_web_atomic_inc(KnWebAtomicInt *value) { return (int)InterlockedIncrement(value); }
static int kn_web_atomic_dec(KnWebAtomicInt *value) { return (int)InterlockedDecrement(value); }
static int kn_web_atomic_load(KnWebAtomicInt *value) { return (int)InterlockedCompareExchange(value, *value, *value); }
static void kn_web_atomic_store(KnWebAtomicInt *value, int v) { InterlockedExchange(value, (LONG)v); }
static void kn_web_sleep_ms(unsigned int ms) { Sleep(ms); }
static uint64_t kn_web_tick_ms(void) { return (uint64_t)GetTickCount64(); }
static unsigned int kn_web_cpu_count(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? (unsigned int)info.dwNumberOfProcessors : 1u;
}
#else
typedef _Atomic int KnWebAtomicInt;
static int kn_web_atomic_inc(KnWebAtomicInt *value) { return atomic_fetch_add_explicit(value, 1, memory_order_relaxed) + 1; }
static int kn_web_atomic_dec(KnWebAtomicInt *value) { return atomic_fetch_sub_explicit(value, 1, memory_order_relaxed) - 1; }
static int kn_web_atomic_load(KnWebAtomicInt *value) { return atomic_load_explicit(value, memory_order_relaxed); }
static void kn_web_atomic_store(KnWebAtomicInt *value, int v) { atomic_store_explicit(value, v, memory_order_relaxed); }
static void kn_web_sleep_ms(unsigned int ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
    nanosleep(&ts, 0);
}
static uint64_t kn_web_tick_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}
static unsigned int kn_web_cpu_count(void)
{
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    return cpus > 0 ? (unsigned int)cpus : 1u;
}
#endif

static unsigned int kn_web_thread_count(void)
{
    unsigned int threads = kn_web_cpu_count() * 8u;
    if (threads < 50u)
        threads = 50u;
    if (threads > 128u)
        threads = 128u;
    return threads;
}

static unsigned int kn_web_prespawn_thread_count(unsigned int threads)
{
    unsigned int prespawn = threads / 2u;
    if (prespawn < 4u)
        prespawn = threads < 4u ? threads : 4u;
    if (prespawn > 16u)
        prespawn = 16u;
    return prespawn > threads ? threads : prespawn;
}

typedef KnAny(__cdecl *KnCallableWrapper)(void *env, KnAny *args, uint64_t argc);

typedef struct
{
    char *prefix;
    char *directory;
    char *default_file;
} KnWebStaticMount;

typedef struct
{
    struct mg_context *ctx;
    char *host;
    int port;
    char listen_text[128];
    KnWebStaticMount *mounts;
    size_t mount_count;
    size_t mount_cap;
    void *dispatcher;
    KnWebAtomicInt running;
    char last_error[512];
    char *context_info;
    size_t context_info_cap;
    uint64_t reload_scan_tick;
    uint64_t reload_version;
} KnWebServer;

typedef struct
{
    struct mg_connection *conn;
    KnWebServer *server;
    int replied;
    int status;
    char *body_text;
    int64_t body_length;
    int body_loaded;
} KnWebRequestScope;

#if defined(_WIN32) || defined(_WIN64)
#define KN_WEB_TLS __declspec(thread)
#else
#define KN_WEB_TLS __thread
#endif

static KN_WEB_TLS KnWebRequestScope *g_kn_web_current = 0;
static KN_WEB_TLS char g_kn_web_query_value[4096];
static KnWebAtomicInt g_kn_web_lib_refs = 0;

static const char *kn_web_safe(const char *text)
{
    return text ? text : "";
}

static int kn_web_case_eq(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (!a || !b)
        return 0;
    while (*a && *b)
    {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z')
            ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
    }
    return *a == 0 && *b == 0;
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

static void kn_web_request_ensure_body(KnWebRequestScope *scope)
{
    const struct mg_request_info *info;
    int64_t want;
    int64_t got = 0;
    char *body;
    if (!scope || scope->body_loaded)
        return;

    scope->body_loaded = 1;
    scope->body_length = 0;
    info = scope->conn ? mg_get_request_info(scope->conn) : 0;
    want = info ? info->content_length : 0;
    if (want <= 0)
    {
        scope->body_text = kn_web_strdup("");
        return;
    }
    if ((uint64_t)want >= (uint64_t)SIZE_MAX)
        return;

    body = (char *)malloc((size_t)want + 1);
    if (!body)
        return;

    while (got < want)
    {
        int chunk = mg_read(scope->conn, body + got, (size_t)(want - got));
        if (chunk <= 0)
            break;
        got += (int64_t)chunk;
    }

    body[got] = 0;
    scope->body_text = body;
    scope->body_length = got;
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

static void kn_web_library_retain(void)
{
    if (kn_web_atomic_inc(&g_kn_web_lib_refs) == 1)
        mg_init_library(0);
}

static void kn_web_library_release(void)
{
    if (kn_web_atomic_dec(&g_kn_web_lib_refs) == 0)
        mg_exit_library();
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

static int kn_web_send_text_response(struct mg_connection *conn,
                                     int status,
                                     const char *content_type,
                                     const char *body,
                                     const char *extra_name,
                                     const char *extra_value)
{
    char len_text[32];
    size_t body_len;
    if (!conn)
        return 0;
    if (status <= 0)
        status = 200;
    if (!content_type || !content_type[0])
        content_type = "text/plain; charset=utf-8";
    if (!body)
        body = "";

    body_len = strlen(body);
    snprintf(len_text, sizeof(len_text), "%llu", (unsigned long long)body_len);
    len_text[sizeof(len_text) - 1] = 0;

    if (mg_response_header_start(conn, status) < 0)
        return 0;
    if (mg_response_header_add(conn, "Content-Type", content_type, -1) < 0)
        return 0;
    if (mg_response_header_add(conn, "Content-Length", len_text, -1) < 0)
        return 0;
    if (extra_name && extra_name[0] && extra_value && extra_value[0])
    {
        if (mg_response_header_add(conn, extra_name, extra_value, -1) < 0)
            return 0;
    }
    if (mg_response_header_send(conn) < 0)
        return 0;
    if (body_len > 0 && mg_write(conn, body, body_len) < 0)
        return 0;
    return 1;
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

        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
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
#elif defined(st_mtim)
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
        if (stat(child, &st) == 0)
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

static int kn_web_dispatch(struct mg_connection *conn, void *cbdata)
{
    KnWebServer *server = (KnWebServer *)cbdata;
    KnWebRequestScope scope;
    KnWebRequestScope *prev;
    if (!server || !conn)
        return 500;

    memset(&scope, 0, sizeof(scope));
    scope.conn = conn;
    scope.server = server;
    scope.status = 200;

    prev = g_kn_web_current;
    g_kn_web_current = &scope;

    if (server->dispatcher)
    {
        KnCallableWrapper handler = (KnCallableWrapper)server->dispatcher;
        (void)handler(0, 0, 0);
    }
    else
    {
        kn_web_send_text_response(conn,
                                  500,
                                  "text/plain; charset=utf-8",
                                  "IO.Web dispatcher is not configured",
                                  0,
                                  0);
        scope.replied = 1;
        scope.status = 500;
    }

    if (!scope.replied)
    {
        kn_web_send_text_response(conn, 404, "text/plain; charset=utf-8", "Not Found", 0, 0);
        scope.replied = 1;
        scope.status = 404;
    }

    free(scope.body_text);
    g_kn_web_current = prev;
    return scope.status > 0 ? scope.status : 200;
}

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

    server->port = port;
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

int __kn_web_server_mount(void *handle, const char *prefix, const char *directory, const char *default_file)
{
    KnWebServer *server = (KnWebServer *)handle;
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
        KnWebStaticMount *next = (KnWebStaticMount *)realloc(server->mounts, sizeof(KnWebStaticMount) * new_cap);
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

int __kn_web_server_start(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    const char *options[11];
    char num_threads_text[16];
    char prespawn_threads_text[16];
    unsigned int thread_count;
    unsigned int prespawn_thread_count;
    if (!server)
        return 0;
    if (server->ctx)
        return 1;
    if (!server->dispatcher)
    {
        kn_web_set_error(server, "IO.Web dispatcher is not configured");
        return 0;
    }

    server->last_error[0] = 0;
    kn_web_format_listen_text(server);
    thread_count = kn_web_thread_count();
    prespawn_thread_count = kn_web_prespawn_thread_count(thread_count);
    snprintf(num_threads_text, sizeof(num_threads_text), "%u", thread_count);
    num_threads_text[sizeof(num_threads_text) - 1] = 0;
    snprintf(prespawn_threads_text, sizeof(prespawn_threads_text), "%u", prespawn_thread_count);
    prespawn_threads_text[sizeof(prespawn_threads_text) - 1] = 0;

    options[0] = "listening_ports";
    options[1] = server->listen_text;
    options[2] = "num_threads";
    options[3] = num_threads_text;
    options[4] = "prespawn_threads";
    options[5] = prespawn_threads_text;
    options[6] = "enable_keep_alive";
    options[7] = "yes";
    options[8] = "tcp_nodelay";
    options[9] = "1";
    options[10] = 0;

    kn_web_library_retain();
    server->ctx = mg_start(0, server, options);
    if (!server->ctx)
    {
        kn_web_set_error(server, "CivetWeb failed to start on %s", server->listen_text);
        kn_web_library_release();
        return 0;
    }

    mg_set_request_handler(server->ctx, "/", kn_web_dispatch, server);
    kn_web_atomic_store(&server->running, 1);
    return 1;
}

void __kn_web_server_wait(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    if (!server)
        return;
    while (server->ctx && kn_web_atomic_load(&server->running) != 0)
        kn_web_sleep_ms(50);
}

void __kn_web_server_stop(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    if (!server || !server->ctx)
        return;
    kn_web_atomic_store(&server->running, 0);
    mg_stop(server->ctx);
    server->ctx = 0;
    kn_web_library_release();
}

void *__kn_web_current_server(void)
{
    if (!g_kn_web_current)
        return 0;
    return g_kn_web_current->server;
}

const char *__kn_web_current_method(void)
{
    const struct mg_request_info *info;
    if (!g_kn_web_current || !g_kn_web_current->conn)
        return "";
    info = mg_get_request_info(g_kn_web_current->conn);
    return info ? kn_web_safe(info->request_method) : "";
}

const char *__kn_web_current_uri(void)
{
    const struct mg_request_info *info;
    if (!g_kn_web_current || !g_kn_web_current->conn)
        return "";
    info = mg_get_request_info(g_kn_web_current->conn);
    if (!info)
        return "";
    return info->local_uri ? info->local_uri : kn_web_safe(info->request_uri);
}

const char *__kn_web_current_query(void)
{
    const struct mg_request_info *info;
    if (!g_kn_web_current || !g_kn_web_current->conn)
        return "";
    info = mg_get_request_info(g_kn_web_current->conn);
    return info ? kn_web_safe(info->query_string) : "";
}

const char *__kn_web_current_query_value(const char *name)
{
    const char *query;
    int rc;
    if (!name || !name[0])
        return "";
    query = __kn_web_current_query();
    if (!query || !query[0])
        return "";

    g_kn_web_query_value[0] = 0;
    rc = mg_get_var(query, (int)strlen(query), name, g_kn_web_query_value, sizeof(g_kn_web_query_value));
    if (rc < 0)
        return "";
    return g_kn_web_query_value;
}

const char *__kn_web_current_header(const char *name)
{
    const char *value;
    if (!g_kn_web_current || !g_kn_web_current->conn || !name || !name[0])
        return "";
    value = mg_get_header(g_kn_web_current->conn, name);
    return kn_web_safe(value);
}

int64_t __kn_web_current_content_length(void)
{
    const struct mg_request_info *info;
    if (!g_kn_web_current || !g_kn_web_current->conn)
        return 0;
    info = mg_get_request_info(g_kn_web_current->conn);
    if (!info || info->content_length <= 0)
        return 0;
    return (int64_t)info->content_length;
}

const char *__kn_web_current_body_text(void)
{
    if (!g_kn_web_current || !g_kn_web_current->conn)
        return "";
    kn_web_request_ensure_body(g_kn_web_current);
    return g_kn_web_current->body_text ? g_kn_web_current->body_text : "";
}

int __kn_web_current_replied(void)
{
    if (!g_kn_web_current)
        return 0;
    return g_kn_web_current->replied;
}

int __kn_web_server_try_serve_static(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    const struct mg_request_info *info;
    const char *method;
    const char *path;
    const KnWebStaticMount *mount;
    char *file_path;
    if (!server || !g_kn_web_current || !g_kn_web_current->conn)
        return 0;
    if (g_kn_web_current->server != server)
        return 0;

    info = mg_get_request_info(g_kn_web_current->conn);
    if (!info)
        return 0;

    method = kn_web_safe(info->request_method);
    if (!kn_web_case_eq(method, "GET") && !kn_web_case_eq(method, "HEAD"))
        return 0;

    path = info->local_uri ? info->local_uri : kn_web_safe(info->request_uri);
    mount = kn_web_find_best_mount(server, path);
    if (!mount)
        return 0;

    file_path = kn_web_build_static_path(mount, path);
    if (!file_path)
        return 0;

    mg_send_file(g_kn_web_current->conn, file_path);
    g_kn_web_current->replied = 1;
    g_kn_web_current->status = 200;
    free(file_path);
    return 1;
}

int __kn_web_reply_text(int status, const char *content_type, const char *body)
{
    if (!g_kn_web_current || !g_kn_web_current->conn)
        return 0;
    if (!kn_web_send_text_response(g_kn_web_current->conn, status, content_type, body, 0, 0))
        return 0;
    g_kn_web_current->replied = 1;
    g_kn_web_current->status = status > 0 ? status : 200;
    return 1;
}

int64_t __kn_web_server_reload_version(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    uint64_t now;
    uint64_t best = 0;
    size_t i;
    if (!server)
        return 0;

#if defined(_WIN32) || defined(_WIN64)
    now = kn_web_tick_ms();
    if (server->reload_scan_tick != 0 && now - server->reload_scan_tick < 750)
        return (int64_t)server->reload_version;

    for (i = 0; i < server->mount_count; i++)
    {
        uint64_t value = kn_web_scan_directory_reload_version(server->mounts[i].directory);
        if (value > best)
            best = value;
    }

    server->reload_scan_tick = now;
    server->reload_version = best;
    return (int64_t)best;
#else
    now = kn_web_tick_ms();
    if (server->reload_scan_tick != 0 && now - server->reload_scan_tick < 750)
        return (int64_t)server->reload_version;

    for (i = 0; i < server->mount_count; i++)
    {
        uint64_t value = kn_web_scan_directory_reload_version(server->mounts[i].directory);
        if (value > best)
            best = value;
    }

    server->reload_scan_tick = now;
    server->reload_version = best;
    return (int64_t)best;
#endif
}

const char *__kn_web_server_context_info(void *handle)
{
    KnWebServer *server = (KnWebServer *)handle;
    int needed;
    if (!server || !server->ctx)
        return "{}";

    needed = mg_get_context_info(server->ctx, 0, 0);
    if (needed <= 0)
        return "{}";

    if (server->context_info_cap < (size_t)needed + 1)
    {
        char *next = (char *)realloc(server->context_info, (size_t)needed + 1);
        if (!next)
        {
            kn_web_set_error(server, "Out of memory while building server context info");
            return "{}";
        }
        server->context_info = next;
        server->context_info_cap = (size_t)needed + 1;
    }

    server->context_info[0] = 0;
    mg_get_context_info(server->ctx, server->context_info, (int)server->context_info_cap);
    server->context_info[server->context_info_cap - 1] = 0;
    return server->context_info;
}
