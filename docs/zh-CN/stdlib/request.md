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

- 当前官方随包构建仅支持 `http://`。
- 现阶段运行时会拒绝 `https://`，因为内置 CivetWeb 资产没有启用 TLS。
- 这个包是同步接口。
- 发送前会校验请求头名称和值。

## 相关

- [IO.Web](web.md) — 内嵌 HTTP 服务包
- [IO.System](system.md) — 更底层的系统与 FFI 接口
