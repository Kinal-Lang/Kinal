/* System TLS providers are resolved only on the first HTTPS request. */
#include <dlfcn.h>
#include <signal.h>
static struct
{
    const void *(*TLS_client_method)(void);
    void *(*SSL_CTX_new)(const void *);
    void (*SSL_CTX_free)(void *);
    int (*SSL_CTX_set_default_verify_paths)(void *);
    void (*SSL_CTX_set_verify)(void *, int, int (*)(int, void *));
    void *(*SSL_new)(void *);
    void (*SSL_free)(void *);
    int (*SSL_set_fd)(void *, int);
    long (*SSL_ctrl)(void *, int, long, void *);
    int (*SSL_set1_host)(void *, const char *);
    void *(*SSL_get0_param)(void *);
    int (*X509_VERIFY_PARAM_set1_ip_asc)(void *, const char *);
    int (*SSL_connect)(void *);
    int (*SSL_get_error)(void *, int);
    int (*SSL_read)(void *, void *, int);
    int (*SSL_write)(void *, const void *, int);
    const void *(*TLS_server_method)(void);
    int (*SSL_CTX_use_certificate_chain_file)(void *, const char *);
    int (*SSL_CTX_use_PrivateKey_file)(void *, const char *, int);
    int (*SSL_CTX_check_private_key)(const void *);
    int (*SSL_accept)(void *);
    int (*SSL_shutdown)(void *);
} ossl;
static struct
{
    int (*gnutls_global_init)(void);
    int (*gnutls_init)(void **, unsigned);
    int (*gnutls_set_default_priority)(void *);
    int (*gnutls_certificate_allocate_credentials)(void **);
    int (*gnutls_certificate_set_x509_system_trust)(void *);
    int (*gnutls_credentials_set)(void *, unsigned, void *);
    void (*gnutls_session_set_verify_cert)(void *, const char *, unsigned);
    int (*gnutls_server_name_set)(void *, unsigned, const void *, size_t);
    void (*gnutls_transport_set_int2)(void *, int, int);
    int (*gnutls_handshake)(void *);
    ssize_t (*gnutls_record_recv)(void *, void *, size_t);
    ssize_t (*gnutls_record_send)(void *, const void *, size_t);
    void (*gnutls_deinit)(void *);
    void (*gnutls_certificate_free_credentials)(void *);
    int (*gnutls_certificate_set_x509_key_file)(void *, const char *, const char *, int);
    int (*gnutls_bye)(void *, int);
} gnu;

#define LOAD(api, symbol)                                                                                    \
    if (!(api.symbol = dlsym(library, #symbol)))                                                             \
    goto missing
static int load_ossl(void)
{
    void *library = dlopen("libssl.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!library)
        return 0;
    LOAD(ossl, TLS_server_method);
    LOAD(ossl, SSL_CTX_use_certificate_chain_file);
    LOAD(ossl, SSL_CTX_use_PrivateKey_file);
    LOAD(ossl, SSL_CTX_check_private_key);
    LOAD(ossl, SSL_accept);
    LOAD(ossl, SSL_shutdown);
    LOAD(ossl, TLS_client_method);
    LOAD(ossl, SSL_CTX_new);
    LOAD(ossl, SSL_CTX_free);
    LOAD(ossl, SSL_CTX_set_default_verify_paths);
    LOAD(ossl, SSL_CTX_set_verify);
    LOAD(ossl, SSL_new);
    LOAD(ossl, SSL_free);
    LOAD(ossl, SSL_set_fd);
    LOAD(ossl, SSL_ctrl);
    LOAD(ossl, SSL_set1_host);
    LOAD(ossl, SSL_get0_param);
    LOAD(ossl, X509_VERIFY_PARAM_set1_ip_asc);
    LOAD(ossl, SSL_connect);
    LOAD(ossl, SSL_get_error);
    LOAD(ossl, SSL_read);
    LOAD(ossl, SSL_write);
    return 1;
missing:
    dlclose(library);
    return 0;
}
static int load_gnu(void)
{
    void *library = dlopen("libgnutls.so.30", RTLD_NOW | RTLD_LOCAL);
    if (!library)
        return 0;
    LOAD(gnu, gnutls_global_init);
    LOAD(gnu, gnutls_certificate_set_x509_key_file);
    LOAD(gnu, gnutls_bye);
    LOAD(gnu, gnutls_init);
    LOAD(gnu, gnutls_set_default_priority);
    LOAD(gnu, gnutls_certificate_allocate_credentials);
    LOAD(gnu, gnutls_certificate_set_x509_system_trust);
    LOAD(gnu, gnutls_credentials_set);
    LOAD(gnu, gnutls_session_set_verify_cert);
    LOAD(gnu, gnutls_server_name_set);
    LOAD(gnu, gnutls_transport_set_int2);
    LOAD(gnu, gnutls_handshake);
    LOAD(gnu, gnutls_record_recv);
    LOAD(gnu, gnutls_record_send);
    LOAD(gnu, gnutls_deinit);
    LOAD(gnu, gnutls_certificate_free_credentials);
    return 1;
missing:
    dlclose(library);
    return 0;
}

#undef LOAD
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;
static int provider;
static void load_tls(void)
{
    if (load_ossl())
        provider = 1;
    else if (load_gnu() && gnu.gnutls_global_init() == 0)
        provider = 2;
}
typedef struct
{
    void *session, *credentials;
} RequestTls;

static int tls_read(HttpIO *io, char *dst, int size)
{
    RequestTls *tls = io->tls;
    if (provider == 2)
        return (int)gnu.gnutls_record_recv(tls->session, dst, size);
    int n = ossl.SSL_read(tls->session, dst, size);
    if (n <= 0)
        return ossl.SSL_get_error(tls->session, n) == 6 ? 0 : -1;
    return n;
}
static int tls_write(HttpIO *io, const char *src, int size)
{
    RequestTls *tls = io->tls;
    if (provider == 2)
        return (int)gnu.gnutls_record_send(tls->session, src, size);
    return ossl.SSL_write(tls->session, src, size);
}
static int tls_start(HttpIO *io, RequestTls *tls, const char *host)
{
    unsigned char addr[16];
    int ip = inet_pton(AF_INET, host, addr) == 1 || inet_pton(AF_INET6, host, addr) == 1;
    if (provider == 1)
    {
        tls->credentials = ossl.SSL_CTX_new(ossl.TLS_client_method());
        if (!tls->credentials || !ossl.SSL_CTX_set_default_verify_paths(tls->credentials))
            return 0;
        ossl.SSL_CTX_set_verify(tls->credentials, 1, NULL);
        tls->session = ossl.SSL_new(tls->credentials);
        if (!tls->session || !ossl.SSL_set_fd(tls->session, io->fd))
            return 0;
        if (ip)
        {
            if (!ossl.X509_VERIFY_PARAM_set1_ip_asc(ossl.SSL_get0_param(tls->session), host))
                return 0;
        }
        else if (!ossl.SSL_set1_host(tls->session, host) || !ossl.SSL_ctrl(tls->session, 55, 0, (void *)host))
            return 0;
        if (ossl.SSL_connect(tls->session) != 1)
            return 0;
    }
    else
    {
        if (gnu.gnutls_certificate_allocate_credentials(&tls->credentials) < 0 ||
            gnu.gnutls_certificate_set_x509_system_trust(tls->credentials) <= 0 ||
            gnu.gnutls_init(&tls->session, 2) < 0 || gnu.gnutls_set_default_priority(tls->session) < 0 ||
            gnu.gnutls_credentials_set(tls->session, 1, tls->credentials) < 0)
            return 0;
        gnu.gnutls_session_set_verify_cert(tls->session, host, 0);
        if (!ip && gnu.gnutls_server_name_set(tls->session, 1, host, strlen(host)) < 0)
            return 0;
        gnu.gnutls_transport_set_int2(tls->session, io->fd, io->fd);
        if (gnu.gnutls_handshake(tls->session) < 0)
            return 0;
    }
    io->tls = tls;
    io->read = tls_read;
    io->write = tls_write;
    return 1;
}
static void tls_free(RequestTls *tls)
{
    if (provider == 1)
    {
        if (tls->session)
            ossl.SSL_free(tls->session);
        if (tls->credentials)
            ossl.SSL_CTX_free(tls->credentials);
    }
    else if (provider == 2)
    {
        if (tls->session)
            gnu.gnutls_deinit(tls->session);
        if (tls->credentials)
            gnu.gnutls_certificate_free_credentials(tls->credentials);
    }
}

typedef struct
{
    void *credentials;
} WebTlsIdentity;
typedef RequestTls WebTlsSession;

static void identity_free(WebTlsIdentity *id)
{
    if (!id->credentials)
        return;
    if (provider == 1)
        ossl.SSL_CTX_free(id->credentials);
    else
        gnu.gnutls_certificate_free_credentials(id->credentials);
    id->credentials = NULL;
}
static const char *identity_load(WebTlsIdentity *id, const char *cert, const char *key)
{
    pthread_once(&tls_once, load_tls);
    if (!provider)
        return "TlsBackendUnavailable: install system OpenSSL 3 or GnuTLS";
    if (provider == 1)
    {
        id->credentials = ossl.SSL_CTX_new(ossl.TLS_server_method());
        if (id->credentials && ossl.SSL_CTX_use_certificate_chain_file(id->credentials, cert) == 1 &&
            ossl.SSL_CTX_use_PrivateKey_file(id->credentials, key, 1) == 1 &&
            ossl.SSL_CTX_check_private_key(id->credentials) == 1)
            return NULL;
    }
    else if (gnu.gnutls_certificate_allocate_credentials(&id->credentials) >= 0 &&
             gnu.gnutls_certificate_set_x509_key_file(id->credentials, cert, key, 1) >= 0)
        return NULL;
    identity_free(id);
    return "Cannot load PEM certificate chain and private key";
}
static int web_tls_accept(HttpIO *io, WebTlsSession *tls, WebTlsIdentity *id)
{
    if (provider == 1)
    {
        tls->session = ossl.SSL_new(id->credentials);
        if (!tls->session || !ossl.SSL_set_fd(tls->session, io->fd) || ossl.SSL_accept(tls->session) != 1)
            return 0;
    }
    else
    {
        if (gnu.gnutls_init(&tls->session, 1) < 0 || gnu.gnutls_set_default_priority(tls->session) < 0 ||
            gnu.gnutls_credentials_set(tls->session, 1, id->credentials) < 0)
            return 0;
        gnu.gnutls_transport_set_int2(tls->session, io->fd, io->fd);
        if (gnu.gnutls_handshake(tls->session) < 0)
            return 0;
    }
    io->tls = tls;
    io->read = tls_read;
    io->write = tls_write;
    return 1;
}
static void web_tls_finish(WebTlsSession *tls, int connected)
{
    if (connected)
    {
        if (provider == 1)
            ossl.SSL_shutdown(tls->session);
        else
            gnu.gnutls_bye(tls->session, 1);
    }
    tls_free(tls);
}
