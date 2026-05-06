# IO.Request

`IO.Request` 是 Kinal 的对外 HTTP 客户端包。

它基于仓库内置的 CivetWeb client API，通过 Kinal FFI 暴露为一个小型同步请求接口。

## 导入

```kinal
Get IO.Request;
kinal

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
kinal

## 请求选项

```kinal
IO.Request.Options options = New IO.Request.Options(
    "http://127.0.0.1:8000/api/echo",
    IO.Request.Method.POST
);

options.SetHeader("X-App", "kinal");
options.SetBody("{\"ok\":true}", "application/json");
options.SetTimeout(3000);

IO.Request.Response response = IO.Request.Send(options);
kinal

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

## 便捷函数

- `IO.Request.Fetch(url, timeoutMs = 10000)`
- `IO.Request.Head(url, timeoutMs = 10000)`
- `IO.Request.Post(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Put(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Patch(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.DeleteRequest(url, timeoutMs = 10000)`

## 说明

- 当前官方随包构建仅支持 `http://`。
- 现阶段运行时会主动拒绝 `https://`，因为内置 CivetWeb 资产仍以无 TLS 方式编译。
- 这个包是同步、面向文本响应的请求接口。
