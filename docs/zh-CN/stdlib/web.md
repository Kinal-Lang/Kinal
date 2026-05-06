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
