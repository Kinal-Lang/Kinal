# IO.Request

`IO.Request` 是 Kinal 的对外 HTTP 客户端包。它通过 Kinal FFI 包装内置的 CivetWeb client API，提供同步、面向文本响应的请求接口。

## 导入

```kinal
Get IO.Request;
```

## 快速开始

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

`https://` 的用法和 `http://` 一样：

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

## Method 枚举

`IO.Request.Method` 包含：

- `GET`
- `POST`
- `PUT`
- `PATCH`
- `DELETE`
- `HEAD`
- `OPTIONS`

## 请求选项

`IO.Request.Options` 是主要的配置对象。

常用字段和方法：

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

## 响应对象

`IO.Request.Response` 提供：

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

## 便捷函数

- `IO.Request.Fetch(url, timeoutMs = 10000)`
- `IO.Request.Head(url, timeoutMs = 10000)`
- `IO.Request.Post(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Put(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Patch(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.DeleteRequest(url, timeoutMs = 10000)`

## 说明

- 官方随包构建现在同时接受 `http://` 和 `https://`。
- `https://` 走 CivetWeb 的 OpenSSL client 路径。运行进程需要能加载 OpenSSL 3 运行时：
  - Windows：`libssl-3-x64.dll` / `libcrypto-3-x64.dll`，或对应的 ARM64 版本
  - Linux：`libssl.so.3` 和 `libcrypto.so.3`
- 如果 OpenSSL 运行时缺失，`IO.Request` 会直接给出运行时错误，不会悄悄降级成明文 HTTP。
- 当前桥接层还没有暴露 CA 或对端证书校验选项。也就是说 `IO.Request` 现在的 HTTPS 已经加密传输，但还不会校验服务端证书。
- 这个包是同步接口。
- 发送前会校验请求头名称和值。

## 相关

- [IO.Web](web.md) — 内嵌 HTTP 服务包
- [IO.System](system.md) — 更底层的系统与 FFI 接口
