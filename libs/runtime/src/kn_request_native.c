#include "kn_http_wire.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winhttp.h>
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

static KN_HTTP_LOCAL char g_kn_request_error[512];

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

static int save_header(KnRequestResponse *r, const HttpField *field)
{
    if (r->header_count >= HTTP_FIELD_LIMIT)
        return 0;
    KnRequestHeader *h = &r->headers[r->header_count++];
    h->name = kn_request_strdup(field->name);
    h->value = kn_request_strdup(field->value);
    return h->name && h->value;
}

/* Framing, authority and connection headers belong to the transport. */
static int valid_headers(const char *block)
{
    size_t size = strlen(block);
    if (size >= HTTP_HEAD_LIMIT)
        return 0;
    char *copy = kn_request_strdup(block);
    if (!copy)
        return 0;
    int ok = 1;
    for (char *line = copy; *line;)
    {
        char *end = strstr(line, "\r\n");
        HttpField f;
        if (!end)
        {
            ok = 0;
            break;
        }
        *end = 0;
        if (!http_field(line, &f) || !http_casecmp(f.name, "Host") ||
            !http_casecmp(f.name, "Content-Length") || !http_casecmp(f.name, "Transfer-Encoding") ||
            !http_casecmp(f.name, "Connection"))
        {
            ok = 0;
            break;
        }
        line = end + 2;
    }
    free(copy);
    return ok;
}

#ifdef _WIN32
static wchar_t *wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    wchar_t *w = n ? calloc((size_t)n, sizeof(*w)) : NULL;
    if (w && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n))
    {
        free(w);
        return NULL;
    }
    return w;
}

static int win_response_headers(HINTERNET request, KnRequestResponse *r)
{
    DWORD bytes = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, NULL, &bytes, NULL);
    if (!bytes || bytes > HTTP_HEAD_LIMIT * sizeof(wchar_t))
        return 0;
    wchar_t *w = calloc(1, bytes + sizeof(*w));
    if (!w)
        return 0;
    char *text = NULL;
    int ok = 0;
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, w, &bytes, NULL))
        goto done;
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (!len || !(text = malloc(len)) || !WideCharToMultiByte(CP_UTF8, 0, w, -1, text, len, NULL, NULL))
        goto done;
    char *line = strstr(text, "\r\n");
    if (!line)
        goto done;
    *line = 0;
    char *reason = strchr(text, ' ');
    reason = reason ? strchr(reason + 1, ' ') : NULL;
    r->status_text = kn_request_strdup(reason ? reason + 1 : "");
    if (!r->status_text)
        goto done;
    for (line += 2; *line && *line != '\r';)
    {
        char *end = strstr(line, "\r\n");
        HttpField field;
        if (!end)
            goto done;
        *end = 0;
        if (!http_field(line, &field) || !save_header(r, &field))
            goto done;
        line = end + 2;
    }
    ok = 1;
done:
    free(w);
    free(text);
    return ok;
}

static INIT_ONCE winhttp_once = INIT_ONCE_STATIC_INIT;
static HINTERNET winhttp_session;
static BOOL CALLBACK open_winhttp(PINIT_ONCE once, void *arg, void **context)
{
    winhttp_session = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    return winhttp_session != NULL;
}

static int exchange(KnRequestUrl *url, const char *method, const char *headers, const char *body, int ms,
                    size_t limit, KnRequestResponse *r, HttpBuffer *buffer)
{
    HINTERNET connection = NULL, request = NULL;
    wchar_t *host = wide(url->host), *path = wide(url->path), *verb = wide(method), *fields = wide(headers);
    int ok = 0;
    if (!host || !path || !verb || !fields)
        goto done;
    if (!InitOnceExecuteOnce(&winhttp_once, open_winhttp, NULL, NULL))
        goto done;
    connection = WinHttpConnect(winhttp_session, host, (INTERNET_PORT)url->port, 0);
    if (!connection)
        goto done;
    request =
        WinHttpOpenRequest(connection, verb, path, NULL, NULL, NULL, url->use_ssl ? WINHTTP_FLAG_SECURE : 0);
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
    if (!request || !WinHttpSetTimeouts(request, ms, ms, ms, ms) ||
        !WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))
        goto done;
    size_t length = body ? strlen(body) : 0;
    if (length > UINT32_MAX ||
        !WinHttpSendRequest(request, fields, (DWORD)wcslen(fields), (void *)body, (DWORD)length,
                            (DWORD)length, 0) ||
        !WinHttpReceiveResponse(request, NULL))
        goto done;
    DWORD status = 0, n = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status,
                             &n, NULL) ||
        !win_response_headers(request, r))
        goto done;
    r->status_code = (int)status;
    char chunk[8192];
    for (;;)
    {
        if (!WinHttpReadData(request, chunk, sizeof(chunk), &n))
            goto done;
        if (!n)
            break;
        if (n > limit - buffer->size)
        {
            kn_request_set_error("Response exceeds MaxResponseBytes");
            goto done;
        }
        if (!http_append(buffer, chunk, n))
            goto done;
    }
    ok = 1;
done:
    if (!ok && !g_kn_request_error[0])
        kn_request_set_error("WinHTTP request failed (%lu)", (unsigned long)GetLastError());
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    free(host);
    free(path);
    free(verb);
    free(fields);
    return ok;
}
#else
#include "kn_http_tls_linux.h"
static int connect_http(const KnRequestUrl *url, int ms)
{
    struct addrinfo hints = {0}, *addresses = NULL;
    char port[8];
    snprintf(port, sizeof(port), "%d", url->port);
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(url->host, port, &hints, &addresses) != 0)
        return -1;
    int fd = -1;
    for (struct addrinfo *a = addresses; a; a = a->ai_next)
    {
        fd = socket(a->ai_family, SOCK_STREAM | SOCK_CLOEXEC, a->ai_protocol);
        if (fd < 0)
            continue;
        if (fcntl(fd, F_SETFL, O_NONBLOCK) == 0)
        {
            int rc = connect(fd, a->ai_addr, a->ai_addrlen);
            if (rc < 0 && errno == EINPROGRESS)
            {
                struct pollfd wait = {fd, POLLOUT, 0};
                int error = 1;
                socklen_t len = sizeof(error);
                if (poll(&wait, 1, ms) > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 &&
                    !error)
                    rc = 0;
            }
            if (rc == 0 && fcntl(fd, F_SETFL, 0) == 0)
            {
                http_timeout(fd, ms);
                break;
            }
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    return fd;
}

static int exchange(KnRequestUrl *url, const char *method, const char *headers, const char *body, int ms,
                    size_t limit, KnRequestResponse *r, HttpBuffer *buffer)
{
    if (url->use_ssl)
    {
        pthread_once(&tls_once, load_tls);
        if (!provider)
        {
            kn_request_set_error("TlsBackendUnavailable: install system OpenSSL 3 or GnuTLS");
            return 0;
        }
    }
    int fd = connect_http(url, ms);
    if (fd < 0)
    {
        kn_request_set_error("Could not connect to HTTP server");
        return 0;
    }
    int ok = 0, masked = 0;
    HttpIO io;
    http_init(&io, fd);
    io.read_deadline = http_millis() + (unsigned)ms;
    RequestTls tls = {0};
    sigset_t pipes, previous, pending;
    if (url->use_ssl)
    {
        sigemptyset(&pipes);
        sigaddset(&pipes, SIGPIPE);
        if (pthread_sigmask(SIG_BLOCK, &pipes, &previous) != 0)
            goto done;
        masked = 1;
        sigpending(&pending);
        if (!tls_start(&io, &tls, url->host))
        {
            kn_request_set_error("TLS handshake or peer verification failed");
            goto done;
        }
    }
    char address[1024];
    int n = strchr(url->host, ':') ? snprintf(address, sizeof(address), "[%s]:%d", url->host, url->port)
                                   : snprintf(address, sizeof(address), "%s:%d", url->host, url->port);
    if (n < 0 || (size_t)n >= sizeof(address))
        goto done;
    char frame[128];
    snprintf(frame, sizeof(frame), "\r\nConnection: close\r\nContent-Length: %zu\r\n",
             body ? strlen(body) : 0);
    const char *parts[] = {method, " ",     url->path, " HTTP/1.1\r\nHost: ", address,
                           frame,  headers, "\r\n",    body ? body : ""};
    for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
        if (!http_write(&io, parts[i], strlen(parts[i])))
            goto done;
    HttpHead head;
    for (int i = 0;; i++)
    {
        if (i > 8 || !http_head(&io, &head) || strncmp(head.line, "HTTP/1.", 7) || strlen(head.line) < 12 ||
            head.line[8] != ' ' || head.line[9] < '1' || head.line[9] > '5' ||
            !isdigit((unsigned char)head.line[10]) || !isdigit((unsigned char)head.line[11]) ||
            (head.line[12] && head.line[12] != ' '))
            goto done;
        r->status_code = atoi(head.line + 9);
        if (r->status_code >= 200)
            break;
        if (r->status_code == 101)
            goto done;
    }
    r->status_text = kn_request_strdup(head.line[12] ? head.line + 13 : "");
    if (!r->status_text)
        goto done;
    for (unsigned i = 0; i < head.count; i++)
        if (!save_header(r, &head.fields[i]))
            goto done;
    if (strcmp(method, "HEAD") && r->status_code != 204 && r->status_code != 304 &&
        !http_body(&io, &head, 1, limit, http_append, buffer))
    {
        kn_request_set_error("Incomplete HTTP body or response exceeds MaxResponseBytes");
        goto done;
    }
    ok = 1;
done:
    tls_free(&tls);
    close(fd);
    if (masked)
    {
        if (!sigismember(&previous, SIGPIPE) && !sigismember(&pending, SIGPIPE))
        {
            struct timespec zero = {0, 0};
            sigtimedwait(&pipes, NULL, &zero);
        }
        pthread_sigmask(SIG_SETMASK, &previous, NULL);
    }
    return ok;
}
#endif

void *__kn_request_send(const char *method, const char *url_text, const char *headers, const char *body,
                        int64_t ms, int64_t limit)
{
    KnRequestUrl url;
    kn_request_clear_error();
    for (const unsigned char *p = (const unsigned char *)kn_request_safe(url_text); *p; p++)
        if (*p <= 32 || *p == 127 || *p == '\\')
        {
            kn_request_set_error("Invalid URL character");
            return NULL;
        }
    if (!method || !*method || ms <= 0 || ms > INT_MAX || limit <= 0 || limit > INT_MAX ||
        !valid_headers(kn_request_safe(headers)))
    {
        kn_request_set_error("Invalid HTTP options or reserved header");
        return NULL;
    }
    for (const char *p = method; *p; p++)
        if (!http_token((unsigned char)*p))
        {
            kn_request_set_error("Invalid method");
            return NULL;
        }
    if (!kn_request_parse_url(url_text, &url))
        return NULL;
    HttpBuffer buffer = {0};
    KnRequestResponse *r = calloc(1, sizeof(*r));
    if (!r || !(r->headers = calloc(HTTP_FIELD_LIMIT, sizeof(*r->headers))) || strchr(url.host, '@') ||
        !exchange(&url, method, kn_request_safe(headers), body, (int)ms, (size_t)limit, r, &buffer))
    {
        kn_request_response_destroy(r);
        r = NULL;
        free(buffer.data);
        if (!g_kn_request_error[0])
            kn_request_set_error("HTTP exchange failed");
    }
    else
    {
        r->body_text = buffer.data;
        r->body_length = (int64_t)buffer.size;
    }
    kn_request_url_clear(&url);
    return r;
}
