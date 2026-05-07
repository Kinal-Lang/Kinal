# IO.Request

`IO.Request` is Kinal's outbound HTTP client package. It wraps the bundled CivetWeb client API through Kinal FFI and exposes a synchronous text-oriented request surface.

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

Important fields and helpers:

- `Url`
- `Method`
- `Body`
- `ContentType`
- `TimeoutMs`
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

options.SetHeader("X-App", "kinal");
options.SetBody("{\"ok\":true}", "application/json");
options.SetTimeout(3000);

IO.Request.Response response = IO.Request.Send(options);
```

## Response

`IO.Request.Response` exposes:

- `StatusCode`
- `StatusText`
- `BodyText`
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
}
Else
{
    response.EnsureSuccess();
}
```

## Convenience Functions

- `IO.Request.Fetch(url, timeoutMs = 10000)`
- `IO.Request.Head(url, timeoutMs = 10000)`
- `IO.Request.Post(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Put(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Patch(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.DeleteRequest(url, timeoutMs = 10000)`

## Notes

- Official bundled builds accept both `http://` and `https://`.
- `https://` uses CivetWeb's OpenSSL client path. The process must be able to load the OpenSSL 3 runtime:
  - Windows: `libssl-3-x64.dll` / `libcrypto-3-x64.dll`, or the ARM64 equivalents
  - Linux: `libssl.so.3` and `libcrypto.so.3`
- If the OpenSSL runtime is missing, `IO.Request` reports a direct runtime error instead of silently downgrading to plain HTTP.
- The current bridge does not expose CA or peer-verification options yet. HTTPS requests are encrypted, but server certificates are not validated by `IO.Request` today.
- The package is synchronous.
- Header names and values are validated before the request is sent.

## See Also

- [IO.Web](web.md) — Embedded HTTP server package
- [IO.System](system.md) — Lower-level system and FFI helpers
