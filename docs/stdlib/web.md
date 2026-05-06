# IO.Web

`IO.Web` provides an embedded HTTP server package built on top of the bundled CivetWeb runtime bridge.

## Import

```kinal
Get IO.Web;
```

## Route Metadata

`IO.Web.Route(path, method = IO.Web.HttpMethod.GET)` marks a function as a route handler.

Handlers may declare:

- no parameters
- one `IO.Web.Context` parameter

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

## Main Types

### Server

`IO.Web.Server` is the main host object.

Common members:

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

`IO.Web.Context` provides access to the request and response objects for the current dispatch.

### Request

`IO.Web.Request` exposes:

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

`IO.Web.Response` exposes:

- `StatusCode`
- `ContentType`
- `HasSent`
- `Send(body, contentType, status = 200)`
- `Text(body, status = 200)`
- `Html(body, status = 200)`
- `Json(body, status = 200)`
- `JavaScript(body, status = 200)`

## Example

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

## Notes

- The bundled package currently targets the hosted native environment.
- The internal `"/__kinal"` prefix is reserved for package-owned endpoints.
- Static mounts and route metadata can be mixed on the same server.

## See Also

- [IO.Request](request.md)
- [Standard Library Overview](overview.md)
- [IO.Meta](meta.md) — Runtime route metadata
