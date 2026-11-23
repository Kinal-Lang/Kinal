# IO.Request

`IO.Request` 2.0 provides a synchronous HTTP client for hosted Native applications. Windows uses WinHTTP. Linux uses system sockets.

## Import

```kinal
Get IO.Request;
```

## Quick Start

```kinal
Get Console By IO.Console;
Get IO.Request;

Static Function int Main()
{
    IO.Request.Response response = IO.Request.Fetch("http://127.0.0.1:8000/");
    Console.PrintLine(response.StatusCode);
    Console.PrintLine(response.BodyText);
    Return 0;
}
```

HTTPS works the same way:

```kinal
Get Console By IO.Console;
Get IO.Request;

Static Function int Main()
{
    IO.Request.Response response = IO.Request.Fetch("https://127.0.0.1:8443/hello");
    Console.PrintLine(response.StatusCode);
    Console.PrintLine(response.BodyText);
    Return 0;
}
```

## Method Enum

`IO.Request.Method` defines:

- `GET`
- `POST`
- `PUT`
- `PATCH`
- `DELETE`
- `HEAD`
- `OPTIONS`

## Request Options

`IO.Request.Options` is the main configuration object.

Important properties and helpers:

- `Url`
- `Method`
- `Body`
- `BodyJson`
- `BodyObject`
- `BodyDict`
- `ContentType`
- `TimeoutMs`
- `MaxResponseBytes`
- `Headers`
- `SetMethod(method)`
- `SetBody(body, contentType = "text/plain; charset=utf-8")`
- `SetTimeout(timeoutMs)`
- `SetHeader(name, value)`
- `RemoveHeader(name)`

```kinal
IO.Request.Options options = New IO.Request.Options(
    "http://127.0.0.1:8000/api/echo",
    IO.Request.Method.POST
);

options.Headers.Set("X-App", "kinal");
dict payload = dict.Create();
payload.Set("ok", true);
options.BodyDict = payload;
options.TimeoutMs = 3000;

IO.Request.Response response = IO.Request.Send(options);
```

`Headers` is a `dict`. Header names are stored by their normalized text such as `Content-Type`.

For JSON payloads, `Options` now exposes property-style JSON helpers:

- `BodyJson` works with `IO.Json.Value`
- `BodyObject` works with `IO.Json.Object`
- `BodyDict` works with raw `dict`

`BodyDict` is the preferred JSON path. `BodyObject` is only a convenience wrapper layered on top of the same `dict` data.

Assigning any of these JSON properties updates `Body` and automatically switches `ContentType` to `application/json; charset=utf-8`.

## Response

`IO.Request.Response` exposes:

- `StatusCode`
- `StatusText`
- `BodyText`
- `BodyJson`
- `BodyObject`
- `BodyDict`
- `BodyLength`
- `Headers`
- `ContentType`
- `Header(name)`
- `IsSuccess()`
- `EnsureSuccess()`

```kinal
If (response.IsSuccess())
{
    Console.PrintLine(response.ContentType);
    Console.PrintLine(response.Headers.TryFetch("Content-Type", ""));
    Console.PrintLine(response.BodyDict.TryFetch("ok", false));
}
Else
{
    response.EnsureSuccess();
}
```

`BodyText` remains available for plain-text or non-JSON responses. The JSON properties are convenience views when the body content is JSON.
When the response body is a JSON object, prefer `BodyDict`; `BodyObject` is the property-style wrapper for the same parsed data.

## Convenience Functions

- `IO.Request.Fetch(url, timeoutMs = 10000)`
- `IO.Request.Head(url, timeoutMs = 10000)`
- `IO.Request.Post(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Put(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Patch(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.DeleteRequest(url, timeoutMs = 10000)`

## Notes

- Official bundled builds accept both `http://` and `https://`.
- HTTPS validates the certificate chain and hostname. Windows uses system trust. Linux loads system OpenSSL 3 or GnuTLS on demand.
- With neither provider installed, Linux supports plain HTTP and reports `TlsBackendUnavailable` in an `IO.Error` for HTTPS.
- The package is synchronous.
- Header names and values are validated before the request is sent.
- `Options.Headers` and `Response.Headers` both use `dict`.

## See Also

- [IO.Web](web.md) — Embedded HTTP server package
- [IO.System](system.md) — Lower-level system and FFI helpers

## Defaults, Errors and Memory

The default method is GET and timeout is 10000 milliseconds. Send is synchronous. Inspect HTTP status using IsSuccess or EnsureSuccess; the latter throws IO.Error for non-2xx status. Configuration, connection and parsing failures may also throw IO.Error. BodyDict reparses the text on access and requires an object root.

Headers preserve their spelling in the dictionary. `Response.Header(name)` performs a case-insensitive lookup.

`Options.MaxResponseBytes` bounds the buffered body in bytes and defaults to 16 MiB. Exceeding the limit throws `IO.Error`.

## Public Source Signatures

The tables list public fields, constructors, properties and methods from the current package source. Inherited members are listed under their declaring base type. Get/Set identify property accessors; signatures include declared defaults. Native handles and Host/Win32 helpers require their ABI/lifetime contracts; ordinary applications should start with the package high-level objects.

### Enums

- `IO.Request.Method`: `GET = 1`, `POST = 2`, `PUT = 3`, `PATCH = 4`, `DELETE = 5`, `HEAD = 6`, `OPTIONS = 7`

### IO.Request.Response

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(int statusCode, string statusText, string bodyText, int bodyLength, dict headers)` | — |
| `StatusCode` | `Public Property int StatusCode` | Get/Set |
| `StatusText` | `Public Property string StatusText` | Get/Set |
| `BodyText` | `Public Property string BodyText` | Get/Set |
| `BodyJson` | `Public Trusted Property IO.Json.Value BodyJson` | Get |
| `BodyObject` | `Public Trusted Property IO.Json.Object BodyObject` | Get |
| `BodyDict` | `Public Trusted Property dict BodyDict` | Get |
| `BodyLength` | `Public Property int BodyLength` | Get/Set |
| `Headers` | `Public Property dict Headers` | Get/Set |
| `IsSuccess` | `Public Safe Function bool IsSuccess()` | — |
| `Header` | `Public Trusted Function string Header(string name)` | — |
| `ContentType` | `Public Trusted Property string ContentType` | Get |
| `EnsureSuccess` | `Public Trusted Function void EnsureSuccess()` | — |

### IO.Request.Options

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string url, IO.Request.Method method = IO.Request.Method.GET)` | — |
| `Url` | `Public Property string Url` | Get/Set |
| `Method` | `Public Property IO.Request.Method Method` | Get/Set |
| `Body` | `Public Property string Body` | Get/Set |
| `BodyJson` | `Public Trusted Property IO.Json.Value BodyJson` | Get/Set |
| `BodyObject` | `Public Trusted Property IO.Json.Object BodyObject` | Get/Set |
| `BodyDict` | `Public Trusted Property dict BodyDict` | Get/Set |
| `ContentType` | `Public Property string ContentType` | Get/Set |
| `TimeoutMs` | `Public Property int TimeoutMs` | Get/Set |
| `MaxResponseBytes` | `Public int MaxResponseBytes` | 16 MiB |
| `Headers` | `Public Property dict Headers` | Get/Set |
| `SetMethod` | `Public Trusted Function IO.Request.Options SetMethod(IO.Request.Method method)` | — |
| `SetBody` | `Public Trusted Function IO.Request.Options SetBody(string body, string contentType = "text/plain; charset=utf-8")` | — |
| `SetTimeout` | `Public Trusted Function IO.Request.Options SetTimeout(int timeoutMs)` | — |
| `SetHeader` | `Public Trusted Function IO.Request.Options SetHeader(string name, string value)` | — |
| `RemoveHeader` | `Public Trusted Function IO.Request.Options RemoveHeader(string name)` | — |
| `HeaderBlock` | `Public Trusted Function string HeaderBlock()` | — |
| `Validate` | `Public Trusted Function void Validate()` | — |

### IO.Request

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Send` | `Trusted Function IO.Request.Response Send(IO.Request.Options options)` | — |
| `Fetch` | `Trusted Function IO.Request.Response Fetch(string url, int timeoutMs = 10000)` | — |
| `Head` | `Trusted Function IO.Request.Response Head(string url, int timeoutMs = 10000)` | — |
| `Post` | `Trusted Function IO.Request.Response Post(string url, string body, string contentType = "text/plain; charset=utf-8", int timeoutMs = 10000)` | — |
| `Put` | `Trusted Function IO.Request.Response Put(string url, string body, string contentType = "text/plain; charset=utf-8", int timeoutMs = 10000)` | — |
| `Patch` | `Trusted Function IO.Request.Response Patch(string url, string body, string contentType = "text/plain; charset=utf-8", int timeoutMs = 10000)` | — |
| `DeleteRequest` | `Trusted Function IO.Request.Response DeleteRequest(string url, int timeoutMs = 10000)` | — |
