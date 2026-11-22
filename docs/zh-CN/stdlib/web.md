# IO.Web

`IO.Web` 提供基于内置 CivetWeb 运行时桥接的嵌入式 HTTP 服务包。

## 导入

```kinal
Get IO.Web;
```

## 路由元数据

`IO.Web.Route(path, method = IO.Web.HttpMethod.GET)` 用来标记路由处理函数。

处理函数可以声明：

- 无参数
- 一个 `IO.Web.Context` 参数

```kinal
Get IO.Web;

Public Class DemoController
{
    [IO.Web.Route("/hello")]
    Public Trusted Static Function void Hello(IO.Web.Context ctx)
    {
        ctx.Response.Text("hello");
    }
}
```

## 主要类型

### Server

`IO.Web.Server` 是主要的服务宿主对象。

常用成员：

- `UseStatic(prefix, directory, defaultFile = "index.html")`
- `RefreshRoutes()`
- `UseRoutes()`
- `Start()`
- `Wait()`
- `Stop()`
- `Run()`
- `Url()`
- `EnableDebug()`
- `EnableLiveReload()`

### Context

`IO.Web.Context` 用于访问当前派发中的请求与响应对象。

### Request

`IO.Web.Request` 提供：

- `Method`
- `MethodText`
- `Path`
- `RawPath`
- `QueryText`
- `ContentType`
- `ContentLength`
- `QueryValue(name)`
- `Header(name)`
- `BodyText()`

### Response

`IO.Web.Response` 提供：

- `StatusCode`
- `ContentType`
- `HasSent`
- `Send(body, contentType, status = 200)`
- `Text(body, status = 200)`
- `Html(body, status = 200)`
- `Json(body, status = 200)`
- `JavaScript(body, status = 200)`

## 示例

```kinal
Unit App.WebDemo;

Get IO.Web;

Public Class DemoController
{
    [IO.Web.Route("/hello")]
    Public Trusted Static Function void Hello(IO.Web.Context ctx)
    {
        ctx.Response.Text("hello");
    }

    [IO.Web.Route("/echo", IO.Web.HttpMethod.POST)]
    Public Trusted Static Function void Echo(IO.Web.Context ctx)
    {
        ctx.Response.Text(ctx.Request.BodyText());
    }
}

Trusted Static Function int Main()
{
    IO.Web.Server server = New IO.Web.Server(8080);
    server.UseStatic("/static", "public");
    server.RefreshRoutes().UseRoutes();
    server.Start().Wait();
    Return 0;
}
```

## 说明

- 当前随包实现面向 hosted native 环境。
- 内部 `"/__kinal"` 前缀保留给包自身的端点使用。
- 同一个服务实例可以同时使用静态目录挂载和路由元数据派发。

## 相关

- [IO.Request](request.md)
- [标准库概览](overview.md)
- [IO.Meta](meta.md) — 运行时路由元数据

## 启动、路由与生命周期

完整示例为 Native 服务：启动后访问 http://127.0.0.1:8080/hello，POST /echo 返回请求体。Wait 会等待服务，演示进程可通过终端中断结束；嵌入其他应用时显式调用 Stop 管理生命周期。

路由通过 Keep runtime 元数据发现，优先使用 Public Trusted Static void 处理函数以及零参数或一个 IO.Web.Context 参数。请求 Context 只在当前分发期间有效，不应在处理函数返回后继续持有其原生连接。静态资源目录按服务进程工作目录解析；public 目录需自行创建。

Response.Json 接受文本，应传入序列化后的 JSON，而不是假定任意对象会自动序列化。生产应用需自行设置认证、访问策略和部署约束；包的调试／热重载端点应按实际需要启用。

## 公开源码签名

下表逐项列出当前包源码的公开字段、构造函数、属性和方法；继承成员在基类下列出。Get/Set 表示属性访问器，函数行的参数包含声明默认值。原生句柄、Host／Win32 等低层接口需要相应 ABI 与生命周期知识，普通应用优先使用包的高级对象。

### 枚举

- `IO.Web.HttpMethod`: `Unknown = 0`, `GET = 1`, `POST = 2`, `PUT = 3`, `DELETE = 4`, `PATCH = 5`, `HEAD = 6`, `OPTIONS = 7`

### IO.Web.RouteDefinition

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(int method, string path, string owner, int paramCount, IO.Type.Object.Function handler)` | — |
| `Key` | `Public Safe Function string Key()` | — |
| `Invoke` | `Public Trusted Function void Invoke(Context ctx)` | — |
| `Method` | `Public int Method;` | field |
| `Path` | `Public string Path;` | field |
| `Owner` | `Public string Owner;` | field |
| `ParamCount` | `Public int ParamCount;` | field |
| `Handler` | `Public IO.Type.Object.Function Handler;` | field |

### IO.Web.StaticMount

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string prefix, string directory, string defaultFile = "index.html")` | — |
| `Prefix` | `Public string Prefix;` | field |
| `Directory` | `Public string Directory;` | field |
| `DefaultFile` | `Public string DefaultFile;` | field |

### IO.Web.Request

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor()` | — |
| `Method` | `Public Trusted Property IO.Web.HttpMethod Method` | Get |
| `MethodText` | `Public Trusted Property string MethodText` | Get |
| `Path` | `Public Trusted Property string Path` | Get |
| `RawPath` | `Public Trusted Property string RawPath` | Get |
| `QueryText` | `Public Trusted Property string QueryText` | Get |
| `ContentType` | `Public Trusted Property string ContentType` | Get |
| `ContentLength` | `Public Trusted Property int ContentLength` | Get |
| `QueryValue` | `Public Trusted Function string QueryValue(string name)` | — |
| `Header` | `Public Trusted Function string Header(string name)` | — |
| `BodyText` | `Public Trusted Function string BodyText()` | — |

### IO.Web.Response

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor()` | — |
| `StatusCode` | `Public Property int StatusCode` | Get/Set |
| `ContentType` | `Public Property string ContentType` | Get/Set |
| `HasSent` | `Public Trusted Property bool HasSent` | Get |
| `Send` | `Public Trusted Function void Send(string body, int status = 0, string contentType = null)` | — |
| `Text` | `Public Trusted Function void Text(string body, int status = 200)` | — |
| `Html` | `Public Trusted Function void Html(string body, int status = 200)` | — |
| `Json` | `Public Trusted Function void Json(string body, int status = 200)` | — |
| `JavaScript` | `Public Trusted Function void JavaScript(string body, int status = 200)` | — |

### IO.Web.Context

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(Server server)` | — |
| `Server` | `Public Property Server Server` | Get |
| `Request` | `Public Property Request Request` | Get |
| `Response` | `Public Property Response Response` | Get |

### IO.Web.Http

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `CurrentServer` | `Public Trusted Function Server CurrentServer()` | — |
| `Current` | `Public Trusted Function Context Current()` | — |
| `Request` | `Public Trusted Function Request Request()` | — |
| `Response` | `Public Trusted Function Response Response()` | — |

### IO.Web.Server

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(int port = 8080, string host = "0.0.0.0")` | — |
| `Host` | `Public Property string Host` | Get/Set |
| `Port` | `Public Property int Port` | Get/Set |
| `Debug` | `Public Property bool Debug` | Get/Set |
| `LiveReload` | `Public Property bool LiveReload` | Get/Set |
| `IsStarted` | `Public Safe Function bool IsStarted()` | — |
| `HasRoutes` | `Public Safe Function bool HasRoutes()` | — |
| `EnableDebug` | `Public Function Server EnableDebug(bool enabled = true)` | — |
| `EnableLiveReload` | `Public Function Server EnableLiveReload(bool enabled = true)` | — |
| `Url` | `Public Safe Function string Url()` | — |
| `ReloadVersionPath` | `Public Safe Function string ReloadVersionPath()` | — |
| `DebugRoutesPath` | `Public Safe Function string DebugRoutesPath()` | — |
| `DebugServerPath` | `Public Safe Function string DebugServerPath()` | — |
| `UseStatic` | `Public Trusted Function Server UseStatic(string prefix, string directory, string defaultFile = "index.html")` | — |
| `UseStaticMount` | `Public Trusted Function Server UseStaticMount(StaticMount mount)` | — |
| `RefreshRoutes` | `Public Trusted Function Server RefreshRoutes()` | — |
| `UseRoutes` | `Public Trusted Function Server UseRoutes()` | — |
| `FindRoute` | `Public Trusted Function RouteDefinition FindRoute(int method, string path)` | — |
| `HasAnyRouteForPath` | `Public Trusted Function bool HasAnyRouteForPath(string path)` | — |
| `ReloadVersion` | `Public Trusted Function int ReloadVersion()` | — |
| `LiveReloadScript` | `Public Trusted Function string LiveReloadScript()` | — |
| `InjectLiveReload` | `Public Trusted Function string InjectLiveReload(string html)` | — |
| `BuildRoutesDebugText` | `Public Trusted Function string BuildRoutesDebugText()` | — |
| `BuildDebugErrorPage` | `Public Trusted Function string BuildDebugErrorPage(IO.Error err)` | — |
| `BuildProductionErrorPage` | `Public Trusted Function string BuildProductionErrorPage()` | — |
| `HandleCurrentError` | `Public Trusted Function void HandleCurrentError(IO.Error err)` | — |
| `DispatchCurrentRequest` | `Public Trusted Function void DispatchCurrentRequest()` | — |
| `Start` | `Public Trusted Function Server Start()` | — |
| `Wait` | `Public Trusted Function Server Wait()` | — |
| `Stop` | `Public Trusted Function Server Stop()` | — |
| `Run` | `Public Trusted Function void Run()` | — |
| `Handle` | `Public byte* Handle;` | field |

### IO.Web.Host

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Run` | `Public Trusted Function void Run(int port = 8080, string host = "0.0.0.0")` | — |
