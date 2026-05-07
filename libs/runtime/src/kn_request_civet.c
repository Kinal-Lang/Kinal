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
#include <stdatomic.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define KN_REQUEST_TLS __declspec(thread)
typedef volatile LONG KnRequestAtomicInt;
static int kn_request_atomic_inc(KnRequestAtomicInt *value) { return (int)InterlockedIncrement(value); }
static int kn_request_atomic_dec(KnRequestAtomicInt *value) { return (int)InterlockedDecrement(value); }
static int kn_request_atomic_load(const KnRequestAtomicInt *value) { return (int)(*value); }
static void kn_request_atomic_store(KnRequestAtomicInt *value, int next) { *value = (LONG)next; }
#else
#define KN_REQUEST_TLS __thread
typedef _Atomic int KnRequestAtomicInt;
static int kn_request_atomic_inc(KnRequestAtomicInt *value) { return atomic_fetch_add_explicit(value, 1, memory_order_relaxed) + 1; }
static int kn_request_atomic_dec(KnRequestAtomicInt *value) { return atomic_fetch_sub_explicit(value, 1, memory_order_relaxed) - 1; }
static int kn_request_atomic_load(const KnRequestAtomicInt *value) { return atomic_load_explicit(value, memory_order_relaxed); }
static void kn_request_atomic_store(KnRequestAtomicInt *value, int next) { atomic_store_explicit(value, next, memory_order_relaxed); }
#endif

typedef struct
{
    char *name;
    char *value;
} KnRequestHeader;

typedef struct
{
    int status_code;
    char *status_text;
    char *body_text;
    int64_t body_length;
    KnRequestHeader *headers;
    int header_count;
} KnRequestResponse;

typedef struct
{
    int use_ssl;
    char *host;
    int port;
    char *path;
} KnRequestUrl;

static KN_REQUEST_TLS char g_kn_request_error[512];
static KnRequestAtomicInt g_kn_request_lib_refs = 0;
static KnRequestAtomicInt g_kn_request_ssl_ready = 0;

void *__kn_gc_alloc(size_t size);

static void kn_request_clear_error(void)
{
    g_kn_request_error[0] = 0;
}

static void kn_request_set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_kn_request_error, sizeof(g_kn_request_error), fmt, ap);
    g_kn_request_error[sizeof(g_kn_request_error) - 1] = 0;
    va_end(ap);
}

static const char *kn_request_safe(const char *text)
{
    return text ? text : "";
}

static char *kn_request_strdup_n(const char *text, size_t len)
{
    char *copy = (char *)malloc(len + 1);
    if (!copy)
        return 0;
    if (len > 0 && text)
        memcpy(copy, text, len);
    copy[len] = 0;
    return copy;
}

static char *kn_request_strdup(const char *text)
{
    return kn_request_strdup_n(kn_request_safe(text), strlen(kn_request_safe(text)));
}

static char *kn_request_strdup_gc(const char *text)
{
    const char *value = kn_request_safe(text);
    size_t len = strlen(value);
    char *copy = (char *)__kn_gc_alloc(len + 1);
    if (!copy)
        return 0;
    memcpy(copy, value, len + 1);
    return copy;
}

static void kn_request_header_free(KnRequestHeader *header)
{
    if (!header)
        return;
    free(header->name);
    free(header->value);
    header->name = 0;
    header->value = 0;
}

static void kn_request_response_destroy(KnRequestResponse *response)
{
    int index;
    if (!response)
        return;
    for (index = 0; index < response->header_count; index++)
        kn_request_header_free(&response->headers[index]);
    free(response->headers);
    free(response->status_text);
    free(response->body_text);
    free(response);
}

static void kn_request_url_clear(KnRequestUrl *url)
{
    if (!url)
        return;
    free(url->host);
    free(url->path);
    url->host = 0;
    url->path = 0;
    url->port = 0;
    url->use_ssl = 0;
}

static int kn_request_library_retain(void)
{
    if (kn_request_atomic_inc(&g_kn_request_lib_refs) == 1)
    {
        unsigned features = mg_init_library(MG_FEATURES_SSL);
        kn_request_atomic_store(&g_kn_request_ssl_ready, (features & MG_FEATURES_SSL) ? 1 : 0);
    }
    return 1;
}

static void kn_request_library_release(void)
{
    if (kn_request_atomic_dec(&g_kn_request_lib_refs) == 0)
    {
        kn_request_atomic_store(&g_kn_request_ssl_ready, 0);
        mg_exit_library();
    }
}

static int kn_request_library_has_ssl(void)
{
    return kn_request_atomic_load(&g_kn_request_ssl_ready) != 0;
}

static int kn_request_parse_port(const char *text, size_t len, int *out_port)
{
    size_t index;
    int value = 0;
    if (!text || len == 0 || !out_port)
        return 0;
    for (index = 0; index < len; index++)
    {
        char ch = text[index];
        if (ch < '0' || ch > '9')
            return 0;
        value = value * 10 + (int)(ch - '0');
        if (value > 65535)
            return 0;
    }
    if (value <= 0)
        return 0;
    *out_port = value;
    return 1;
}

static int kn_request_parse_url(const char *url_text, KnRequestUrl *out)
{
    const char *cursor;
    const char *authority_end;
    const char *fragment;
    const char *path_start;
    const char *port_start = 0;
    const char *host_start;
    const char *host_end;
    size_t path_len;

    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));

    if (!url_text || !url_text[0])
    {
        kn_request_set_error("Request URL cannot be empty");
        return 0;
    }

    if (strncmp(url_text, "http://", 7) == 0)
    {
        out->port = 80;
        cursor = url_text + 7;
    }
    else if (strncmp(url_text, "https://", 8) == 0)
    {
        out->use_ssl = 1;
        out->port = 443;
        cursor = url_text + 8;
    }
    else
    {
        kn_request_set_error("Only absolute http:// and https:// URLs are supported");
        return 0;
    }

    host_start = cursor;
    authority_end = cursor;
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#')
        authority_end++;

    if (host_start == authority_end)
    {
        kn_request_set_error("Request URL is missing a host name");
        return 0;
    }

    if (*host_start == '[')
    {
        const char *bracket = host_start + 1;
        while (bracket < authority_end && *bracket != ']')
            bracket++;
        if (bracket >= authority_end || *bracket != ']')
        {
            kn_request_set_error("IPv6 request URLs must use a closing ']'");
            return 0;
        }
        host_start++;
        host_end = bracket;
        if (bracket + 1 < authority_end)
        {
            if (bracket[1] != ':')
            {
                kn_request_set_error("Unexpected characters after IPv6 host");
                return 0;
            }
            port_start = bracket + 2;
        }
    }
    else
    {
        const char *scan = host_start;
        const char *colon = 0;
        while (scan < authority_end)
        {
            if (*scan == ':')
            {
                if (colon)
                {
                    kn_request_set_error("IPv6 request URLs must use brackets");
                    return 0;
                }
                colon = scan;
            }
            scan++;
        }
        host_end = colon ? colon : authority_end;
        if (colon)
            port_start = colon + 1;
    }

    if (host_end <= host_start)
    {
        kn_request_set_error("Request URL host cannot be empty");
        return 0;
    }

    out->host = kn_request_strdup_n(host_start, (size_t)(host_end - host_start));
    if (!out->host)
    {
        kn_request_set_error("Out of memory while copying request host");
        kn_request_url_clear(out);
        return 0;
    }

    if (port_start)
    {
        if (!kn_request_parse_port(port_start, (size_t)(authority_end - port_start), &out->port))
        {
            kn_request_set_error("Request URL port is invalid");
            kn_request_url_clear(out);
            return 0;
        }
    }

    path_start = authority_end;
    if (*path_start == 0 || *path_start == '#')
    {
        out->path = kn_request_strdup("/");
    }
    else
    {
        fragment = strchr(path_start, '#');
        path_len = fragment ? (size_t)(fragment - path_start) : strlen(path_start);
        if (path_len == 0)
        {
            out->path = kn_request_strdup("/");
        }
        else if (*path_start == '/')
        {
            out->path = kn_request_strdup_n(path_start, path_len);
        }
        else
        {
            out->path = (char *)malloc(path_len + 2);
            if (!out->path)
            {
                kn_request_set_error("Out of memory while copying request path");
                kn_request_url_clear(out);
                return 0;
            }
            out->path[0] = '/';
            memcpy(out->path + 1, path_start, path_len);
            out->path[path_len + 1] = 0;
        }
    }

    if (!out->path)
    {
        kn_request_set_error("Out of memory while building request path");
        kn_request_url_clear(out);
        return 0;
    }

    return 1;
}

static int kn_request_write_all(struct mg_connection *conn, const char *text, size_t len)
{
    size_t written = 0;
    if (!conn || !text)
        return len == 0;
    while (written < len)
    {
        int chunk = mg_write(conn, text + written, len - written);
        if (chunk <= 0)
            return 0;
        written += (size_t)chunk;
    }
    return 1;
}

static int kn_request_response_copy_headers(KnRequestResponse *response, const struct mg_response_info *info)
{
    int index;
    if (!response || !info || info->num_headers <= 0)
        return 1;

    response->headers = (KnRequestHeader *)calloc((size_t)info->num_headers, sizeof(KnRequestHeader));
    if (!response->headers)
        return 0;
    response->header_count = info->num_headers;

    for (index = 0; index < info->num_headers; index++)
    {
        response->headers[index].name = kn_request_strdup(info->http_headers[index].name);
        response->headers[index].value = kn_request_strdup(info->http_headers[index].value);
        if (!response->headers[index].name || !response->headers[index].value)
            return 0;
    }
    return 1;
}

static int kn_request_response_read_body(KnRequestResponse *response, struct mg_connection *conn)
{
    char scratch[4096];
    char *body = 0;
    size_t cap = 0;
    size_t len = 0;

    if (!response || !conn)
        return 0;

    for (;;)
    {
        int got = mg_read(conn, scratch, sizeof(scratch));
        if (got <= 0)
            break;
        if (len + (size_t)got + 1 > cap)
        {
            size_t next_cap = cap ? cap * 2 : 4096;
            char *next_body;
            while (next_cap < len + (size_t)got + 1)
                next_cap *= 2;
            next_body = (char *)realloc(body, next_cap);
            if (!next_body)
            {
                free(body);
                return 0;
            }
            body = next_body;
            cap = next_cap;
        }
        memcpy(body + len, scratch, (size_t)got);
        len += (size_t)got;
    }

    if (!body)
    {
        body = (char *)malloc(1);
        if (!body)
            return 0;
        body[0] = 0;
    }
    else
    {
        body[len] = 0;
    }

    response->body_text = body;
    response->body_length = (int64_t)len;
    return 1;
}

void *__kn_request_send(const char *method,
                        const char *url_text,
                        const char *header_lines,
                        const char *body,
                        int timeout_ms)
{
    KnRequestUrl url;
    KnRequestResponse *response = 0;
    struct mg_connection *conn = 0;
    const struct mg_response_info *info = 0;
    char error_text[512];
    int has_body = body != 0;
    size_t body_len = body ? strlen(body) : 0;

    kn_request_clear_error();
    memset(&url, 0, sizeof(url));
    error_text[0] = 0;

    if (!method || !method[0])
    {
        kn_request_set_error("HTTP method cannot be empty");
        return 0;
    }
    if (timeout_ms <= 0)
        timeout_ms = 10000;
    if (!kn_request_parse_url(url_text, &url))
        return 0;
    if (!kn_request_library_retain())
    {
        kn_request_url_clear(&url);
        return 0;
    }
    if (url.use_ssl && !kn_request_library_has_ssl())
    {
        kn_request_set_error("HTTPS requires the OpenSSL 3 runtime; ensure libssl/libcrypto are visible to the process");
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    conn = mg_connect_client(url.host, url.port, url.use_ssl, error_text, sizeof(error_text));
    if (!conn)
    {
        kn_request_set_error("Failed to connect to %s:%d: %s", kn_request_safe(url.host), url.port, kn_request_safe(error_text));
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (mg_printf(conn,
                  "%s %s HTTP/1.1\r\n"
                  "Host: %s\r\n"
                  "Connection: close\r\n",
                  method,
                  kn_request_safe(url.path),
                  kn_request_safe(url.host)) <= 0)
    {
        kn_request_set_error("Failed to write the HTTP request line");
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (header_lines && header_lines[0] && !kn_request_write_all(conn, header_lines, strlen(header_lines)))
    {
        kn_request_set_error("Failed to write custom request headers");
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (has_body)
    {
        if (mg_printf(conn, "Content-Length: %llu\r\n", (unsigned long long)body_len) <= 0)
        {
            kn_request_set_error("Failed to write the request Content-Length");
            mg_close_connection(conn);
            kn_request_library_release();
            kn_request_url_clear(&url);
            return 0;
        }
    }

    if (!kn_request_write_all(conn, "\r\n", 2))
    {
        kn_request_set_error("Failed to terminate the HTTP request headers");
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (has_body && body_len > 0 && !kn_request_write_all(conn, body, body_len))
    {
        kn_request_set_error("Failed to write the HTTP request body");
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (mg_get_response(conn, error_text, sizeof(error_text), timeout_ms) < 0)
    {
        kn_request_set_error("The HTTP server did not return a valid response: %s", kn_request_safe(error_text));
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    info = mg_get_response_info(conn);
    if (!info)
    {
        kn_request_set_error("The HTTP response metadata is unavailable");
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    response = (KnRequestResponse *)calloc(1, sizeof(KnRequestResponse));
    if (!response)
    {
        kn_request_set_error("Out of memory while creating the HTTP response");
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    response->status_code = info->status_code;
    response->status_text = kn_request_strdup(info->status_text);
    if (!response->status_text)
    {
        kn_request_set_error("Out of memory while copying the HTTP status text");
        kn_request_response_destroy(response);
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (!kn_request_response_copy_headers(response, info))
    {
        kn_request_set_error("Out of memory while copying HTTP response headers");
        kn_request_response_destroy(response);
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    if (!kn_request_response_read_body(response, conn))
    {
        kn_request_set_error("Out of memory while reading the HTTP response body");
        kn_request_response_destroy(response);
        mg_close_connection(conn);
        kn_request_library_release();
        kn_request_url_clear(&url);
        return 0;
    }

    mg_close_connection(conn);
    kn_request_library_release();
    kn_request_url_clear(&url);
    return response;
}

void __kn_request_response_free(void *handle)
{
    kn_request_response_destroy((KnRequestResponse *)handle);
}

const char *__kn_request_last_error(void)
{
    return g_kn_request_error;
}

int __kn_request_response_status_code(void *handle)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    return response ? response->status_code : 0;
}

const char *__kn_request_response_status_text(void *handle)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    return response ? kn_request_strdup_gc(response->status_text) : "";
}

const char *__kn_request_response_body_text(void *handle)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    return response ? kn_request_strdup_gc(response->body_text) : "";
}

int64_t __kn_request_response_body_length(void *handle)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    return response ? response->body_length : 0;
}

int __kn_request_response_header_count(void *handle)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    return response ? response->header_count : 0;
}

const char *__kn_request_response_header_name_at(void *handle, int index)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    if (!response || index < 0 || index >= response->header_count)
        return "";
    return kn_request_strdup_gc(response->headers[index].name);
}

const char *__kn_request_response_header_value_at(void *handle, int index)
{
    KnRequestResponse *response = (KnRequestResponse *)handle;
    if (!response || index < 0 || index >= response->header_count)
        return "";
    return kn_request_strdup_gc(response->headers[index].value);
}
