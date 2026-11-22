# IO.Request

`IO.Request` 是 Kinal 的对外 HTTP 客户端包。它通过 Kinal FFI 包装内置的 CivetWeb client API，提供同步、面向文本响应的请求接口，公开表面以属性和 `dict` 请求头为主。

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
- `https://` 走 CivetWeb 的 OpenSSL client 路径。运行进程需要能加载 OpenSSL 3 运行时：
  - Windows：`libssl-3-x64.dll` / `libcrypto-3-x64.dll`，或对应的 ARM64 版本
  - Linux：`libssl.so.3` 和 `libcrypto.so.3`
- 如果 OpenSSL 运行时缺失，`IO.Request` 会直接给出运行时错误，不会悄悄降级成明文 HTTP。
- 当前桥接层还没有暴露 CA 或对端证书校验选项。也就是说 `IO.Request` 现在的 HTTPS 已经加密传输，但还不会校验服务端证书。
- 这个包是同步接口。
- 发送前会校验请求头名称和值。
- `Options.Headers` 和 `Response.Headers` 都使用 `dict`。

## 相关

- [IO.Web](web.md) — 内嵌 HTTP 服务包
- [IO.System](system.md) — 更底层的系统与 FFI 接口

## 默认值、错误与内存

默认方法 GET，超时 10000 毫秒。Send 是同步调用；检查响应状态需要 IsSuccess 或 EnsureSuccess，后者对非 2xx 抛出 IO.Error。配置、连接与解析失败同样可能抛出 IO.Error。BodyDict 每次访问都解析文本并要求对象根。

Header 名称会去除首尾空白，但 dict 键不会统一大小写，Header(name) 也按该键查询；应使用实际返回的名称（如 Content-Type）。HTTP 名称语义与 dict 的大小写敏感存储应分别考虑。

整个响应体会读入内存，目前没有响应大小上限或流式 API。BodyLength 是字节数，不能用于推断 Unicode 字符数。当前执行路径为 hosted Native；使用前确保原生资产与 OpenSSL 位数匹配。

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
