/* The certificate and private key stay in the Windows user or machine MY store. */
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>

typedef struct
{
    CredHandle handle;
    int acquired;
} WebTlsIdentity;

typedef struct
{
    CtxtHandle context;
    int acquired;
    SecPkgContext_StreamSizes sizes;
    unsigned char encoded[65536], plain[16384];
    size_t used, pos, end;
} WebTlsSession;

static void identity_free(WebTlsIdentity *id)
{
    if (id->acquired)
        FreeCredentialsHandle(&id->handle);
    id->acquired = 0;
}
static const char *identity_load(WebTlsIdentity *id, const char *thumbprint, const char *store)
{
    DWORD location;
    if (!strcmp(store, "CurrentUser"))
        location = CERT_SYSTEM_STORE_CURRENT_USER;
    else if (!strcmp(store, "LocalMachine"))
        location = CERT_SYSTEM_STORE_LOCAL_MACHINE;
    else
        return "Certificate store must be CurrentUser or LocalMachine";
    BYTE hash[20];
    if (strlen(thumbprint) != sizeof(hash) * 2)
        return "Expected a Windows certificate SHA-1 thumbprint";
    for (size_t i = 0; i < sizeof(hash); i++)
    {
        char hex[] = {thumbprint[2 * i], thumbprint[2 * i + 1], 0};
        int64_t value;
        if (!http_number(hex, 16, &value))
            return "Invalid certificate thumbprint";
        hash[i] = (BYTE)value;
    }
    HCERTSTORE my = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                  location | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG, L"MY");
    if (!my)
        return "Cannot open Windows MY certificate store";
    CRYPT_HASH_BLOB blob = {sizeof(hash), hash};
    PCCERT_CONTEXT cert = CertFindCertificateInStore(my, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                                                     CERT_FIND_HASH, &blob, NULL);
    SECURITY_STATUS result = SEC_E_NO_CREDENTIALS;
    if (cert)
    {
        SCHANNEL_CRED creds = {0};
        creds.dwVersion = SCHANNEL_CRED_VERSION;
        creds.cCreds = 1;
        creds.paCred = &cert;
        creds.dwFlags = SCH_USE_STRONG_CRYPTO;
        TimeStamp expiry;
        result = AcquireCredentialsHandleW(NULL, UNISP_NAME_W, SECPKG_CRED_INBOUND, NULL, &creds, NULL, NULL,
                                           &id->handle, &expiry);
        CertFreeCertificateContext(cert);
    }
    CertCloseStore(my, 0);
    id->acquired = result == SEC_E_OK;
    return id->acquired
               ? NULL
               : "Could not acquire Schannel credentials for the specified certificate and private key";
}
static int read_record(HttpIO *io, WebTlsSession *s)
{
    if (s->used >= sizeof(s->encoded))
        return 0;
    int n = http_raw_read(io, (char *)s->encoded + s->used, (int)(sizeof(s->encoded) - s->used));
    if (n <= 0)
        return 0;
    s->used += (size_t)n;
    return 1;
}
static void keep_extra(WebTlsSession *s, SecBuffer *buffers, int count)
{
    size_t extra = 0;
    for (int i = 0; i < count; i++)
        if (buffers[i].BufferType == SECBUFFER_EXTRA)
            extra = buffers[i].cbBuffer;
    if (extra && extra <= s->used)
        memmove(s->encoded, s->encoded + s->used - extra, extra);
    s->used = extra;
}
static int write_raw(HttpIO *io, const void *src, size_t len)
{
    const char *p = src;
    while (len)
    {
        int n = http_raw_write(io, p, (int)len);
        if (n <= 0)
            return 0;
        p += n;
        len -= (size_t)n;
    }
    return 1;
}
static int schannel_read(HttpIO *io, char *dst, int size)
{
    WebTlsSession *s = io->tls;
    while (s->pos == s->end)
    {
        if (!s->used && !read_record(io, s))
            return -1;
        SecBuffer buffers[4] = {{(ULONG)s->used, SECBUFFER_DATA, s->encoded}, {0}, {0}, {0}};
        SecBufferDesc input = {SECBUFFER_VERSION, 4, buffers};
        SECURITY_STATUS rc = DecryptMessage(&s->context, &input, 0, NULL);
        if (rc == SEC_E_INCOMPLETE_MESSAGE)
        {
            if (!read_record(io, s))
                return -1;
            continue;
        }
        if (rc == SEC_I_CONTEXT_EXPIRED)
            return 0;
        if (rc != SEC_E_OK)
            return -1;
        s->pos = 0;
        s->end = 0;
        for (int i = 0; i < 4; i++)
            if (buffers[i].BufferType == SECBUFFER_DATA && buffers[i].cbBuffer)
            {
                if (buffers[i].cbBuffer > sizeof(s->plain))
                    return -1;
                memcpy(s->plain, buffers[i].pvBuffer, buffers[i].cbBuffer);
                s->end = buffers[i].cbBuffer;
            }
        keep_extra(s, buffers, 4);
    }
    size_t n = s->end - s->pos;
    if (n > (size_t)size)
        n = (size_t)size;
    memcpy(dst, s->plain + s->pos, n);
    s->pos += n;
    return (int)n;
}
static int schannel_write(HttpIO *io, const char *src, int size)
{
    WebTlsSession *s = io->tls;
    unsigned char record[32768];
    size_t n = (size_t)size > s->sizes.cbMaximumMessage ? s->sizes.cbMaximumMessage : (size_t)size;
    size_t h = s->sizes.cbHeader, t = s->sizes.cbTrailer;
    if (h + n + t > sizeof(record))
        return -1;
    memcpy(record + h, src, n);
    SecBuffer buffers[4] = {{(ULONG)h, SECBUFFER_STREAM_HEADER, record},
                            {(ULONG)n, SECBUFFER_DATA, record + h},
                            {(ULONG)t, SECBUFFER_STREAM_TRAILER, record + h + n},
                            {0}};
    SecBufferDesc desc = {SECBUFFER_VERSION, 4, buffers};
    if (EncryptMessage(&s->context, 0, &desc, 0) != SEC_E_OK)
        return -1;
    for (int i = 0; i < 3; i++)
        if (!write_raw(io, buffers[i].pvBuffer, buffers[i].cbBuffer))
            return -1;
    return (int)n;
}
static int web_tls_accept(HttpIO *io, WebTlsSession *s, WebTlsIdentity *id)
{
    for (;;)
    {
        if (!s->used && !read_record(io, s))
            return 0;
        SecBuffer in[2] = {{(ULONG)s->used, SECBUFFER_TOKEN, s->encoded}, {0}};
        SecBuffer out = {0, SECBUFFER_TOKEN, NULL};
        SecBufferDesc input = {SECBUFFER_VERSION, 2, in}, output = {SECBUFFER_VERSION, 1, &out};
        ULONG attrs;
        TimeStamp expiry;
        SECURITY_STATUS rc =
            AcceptSecurityContext(&id->handle, s->acquired ? &s->context : NULL, &input,
                                  ASC_REQ_STREAM | ASC_REQ_CONFIDENTIALITY | ASC_REQ_ALLOCATE_MEMORY |
                                      ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT,
                                  SECURITY_NATIVE_DREP, &s->context, &output, &attrs, &expiry);
        if (rc == SEC_I_CONTINUE_NEEDED || rc == SEC_E_OK)
            s->acquired = 1;
        if (out.pvBuffer)
        {
            int ok = write_raw(io, out.pvBuffer, out.cbBuffer);
            FreeContextBuffer(out.pvBuffer);
            if (!ok)
                return 0;
        }
        if (rc == SEC_E_INCOMPLETE_MESSAGE)
        {
            if (!read_record(io, s))
                return 0;
            continue;
        }
        if (rc != SEC_I_CONTINUE_NEEDED && rc != SEC_E_OK)
            return 0;
        keep_extra(s, in, 2);
        if (rc == SEC_E_OK)
            break;
    }
    if (QueryContextAttributesW(&s->context, SECPKG_ATTR_STREAM_SIZES, &s->sizes) != SEC_E_OK)
        return 0;
    io->read = schannel_read;
    io->write = schannel_write;
    io->tls = s;
    return 1;
}
static void web_tls_finish(HttpIO *io, WebTlsSession *s, WebTlsIdentity *id, int connected)
{
    if (connected)
    {
        DWORD shutdown_token = SCHANNEL_SHUTDOWN;
        SecBuffer token = {sizeof(shutdown_token), SECBUFFER_TOKEN, &shutdown_token};
        SecBufferDesc control = {SECBUFFER_VERSION, 1, &token};
        if (ApplyControlToken(&s->context, &control) == SEC_E_OK)
        {
            SecBuffer out = {0, SECBUFFER_TOKEN, NULL};
            SecBufferDesc output = {SECBUFFER_VERSION, 1, &out};
            ULONG flags;
            TimeStamp expiry;
            AcceptSecurityContext(&id->handle, &s->context, NULL, ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM,
                                  SECURITY_NATIVE_DREP, &s->context, &output, &flags, &expiry);
            if (out.pvBuffer)
            {
                write_raw(io, out.pvBuffer, out.cbBuffer);
                FreeContextBuffer(out.pvBuffer);
            }
        }
    }
    if (s->acquired)
        DeleteSecurityContext(&s->context);
}
