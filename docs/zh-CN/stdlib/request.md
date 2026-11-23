# IO.Request

`IO.Request` 2.0 是同步 HTTP 客户端，支持 Windows 和 Linux 的 Native 程序。Windows 使用 WinHTTP，Linux 使用系统套接字。

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

常用属性和方法：

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

`Headers` 是一个 `dict`。请求头会按规范化后的名字存储，比如 `Content-Type`。

如果请求体是 JSON，`Options` 现在额外提供属性式 JSON 入口：

- `BodyJson` 对应 `IO.Json.Value`
- `BodyObject` 对应 `IO.Json.Object`
- `BodyDict` 对应原始 `dict`

其中 `BodyDict` 是首选 JSON 路径；`BodyObject` 只是建立在同一份 `dict` 数据之上的便捷包装。

给这些 JSON 属性赋值时，会同步更新 `Body`，并自动把 `ContentType` 切换成 `application/json; charset=utf-8`。

## 响应对象

`IO.Request.Response` 提供：

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

`BodyText` 仍然保留给纯文本或非 JSON 响应使用；JSON 属性只是方便在响应内容确实是 JSON 时直接走属性和 `dict`。如果响应体本身是 JSON 对象，优先使用 `BodyDict`，`BodyObject` 只是同一份解析结果的属性式包装。

## 便捷函数

- `IO.Request.Fetch(url, timeoutMs = 10000)`
- `IO.Request.Head(url, timeoutMs = 10000)`
- `IO.Request.Post(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Put(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.Patch(url, body, contentType = "text/plain; charset=utf-8", timeoutMs = 10000)`
- `IO.Request.DeleteRequest(url, timeoutMs = 10000)`

## 说明

- 官方随包构建现在同时接受 `http://` 和 `https://`。
- HTTPS 会验证证书链和目标主机名。Windows 使用系统信任库，Linux 按需加载系统 OpenSSL 3 或 GnuTLS。
- Linux 缺少两个 TLS 提供程序时，HTTP 仍可用，HTTPS 抛出带有 `TlsBackendUnavailable` 的 `IO.Error`。
- 这个包是同步接口。
- 发送前会校验请求头名称和值。
- `Options.Headers` 和 `Response.Headers` 都使用 `dict`。

## 相关

- [IO.Web](web.md) — 内嵌 HTTP 服务包
- [IO.System](system.md) — 更底层的系统与 FFI 接口

## 默认值、错误与内存

默认方法 GET，超时 10000 毫秒。Send 是同步调用；检查响应状态需要 IsSuccess 或 EnsureSuccess，后者对非 2xx 抛出 IO.Error。配置、连接与解析失败同样可能抛出 IO.Error。BodyDict 每次访问都解析文本并要求对象根。

请求头保留原有拼写，`Response.Header(name)` 忽略大小写。

`Options.MaxResponseBytes` 以字节为单位，默认 16 MiB。超限响应会抛出 `IO.Error`。

## 公开源码签名

下表逐项列出当前包源码的公开字段、构造函数、属性和方法；继承成员在基类下列出。Get/Set 表示属性访问器，函数行的参数包含声明默认值。原生句柄、Host／Win32 等低层接口需要相应 ABI 与生命周期知识，普通应用优先使用包的高级对象。

### 枚举

- `IO.Request.Method`: `GET = 1`, `POST = 2`, `PUT = 3`, `PATCH = 4`, `DELETE = 5`, `HEAD = 6`, `OPTIONS = 7`

### IO.Request.Response

| 成员 | 声明 | 访问器 |
|------|------|--------|
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

| 成员 | 声明 | 访问器 |
|------|------|--------|
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

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Send` | `Trusted Function IO.Request.Response Send(IO.Request.Options options)` | — |
| `Fetch` | `Trusted Function IO.Request.Response Fetch(string url, int timeoutMs = 10000)` | — |
| `Head` | `Trusted Function IO.Request.Response Head(string url, int timeoutMs = 10000)` | — |
| `Post` | `Trusted Function IO.Request.Response Post(string url, string body, string contentType = "text/plain; charset=utf-8", int timeoutMs = 10000)` | — |
| `Put` | `Trusted Function IO.Request.Response Put(string url, string body, string contentType = "text/plain; charset=utf-8", int timeoutMs = 10000)` | — |
| `Patch` | `Trusted Function IO.Request.Response Patch(string url, string body, string contentType = "text/plain; charset=utf-8", int timeoutMs = 10000)` | — |
| `DeleteRequest` | `Trusted Function IO.Request.Response DeleteRequest(string url, int timeoutMs = 10000)` | — |
