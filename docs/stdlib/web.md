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

## Startup, Routes and Lifetime

The complete example is a Native server: visit http://127.0.0.1:8080/hello and POST a body to /echo. Wait keeps the server active; interrupt the demo from its terminal, or explicitly call Stop when embedding it in another application.

Routes are discovered from runtime-retained metadata. Prefer Public Trusted Static void handlers taking zero arguments or one IO.Web.Context. A request context belongs to the current dispatch; do not retain its native connection after the handler returns. Static directories resolve against the server process working directory; create public yourself.

Response.Json accepts text; supply serialized JSON rather than assuming arbitrary objects are serialized automatically. Applications supply authentication, access policy and deployment configuration. Enable debug/live-reload endpoints only when needed.

## Public Source Signatures

The tables list public fields, constructors, properties and methods from the current package source. Inherited members are listed under their declaring base type. Get/Set identify property accessors; signatures include declared defaults. Native handles and Host/Win32 helpers require their ABI/lifetime contracts; ordinary applications should start with the package high-level objects.

### Enums

- `IO.Web.HttpMethod`: `Unknown = 0`, `GET = 1`, `POST = 2`, `PUT = 3`, `DELETE = 4`, `PATCH = 5`, `HEAD = 6`, `OPTIONS = 7`

### IO.Web.RouteDefinition

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(int method, string path, string owner, int paramCount, IO.Type.Object.Function handler)` | — |
| `Key` | `Public Safe Function string Key()` | — |
| `Invoke` | `Public Trusted Function void Invoke(Context ctx)` | — |
| `Method` | `Public int Method;` | field |
| `Path` | `Public string Path;` | field |
| `Owner` | `Public string Owner;` | field |
| `ParamCount` | `Public int ParamCount;` | field |
| `Handler` | `Public IO.Type.Object.Function Handler;` | field |

### IO.Web.StaticMount

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string prefix, string directory, string defaultFile = "index.html")` | — |
| `Prefix` | `Public string Prefix;` | field |
| `Directory` | `Public string Directory;` | field |
| `DefaultFile` | `Public string DefaultFile;` | field |

### IO.Web.Request

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(Server server)` | — |
| `Server` | `Public Property Server Server` | Get |
| `Request` | `Public Property Request Request` | Get |
| `Response` | `Public Property Response Response` | Get |

### IO.Web.Http

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `CurrentServer` | `Public Trusted Function Server CurrentServer()` | — |
| `Current` | `Public Trusted Function Context Current()` | — |
| `Request` | `Public Trusted Function Request Request()` | — |
| `Response` | `Public Trusted Function Response Response()` | — |

### IO.Web.Server

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Run` | `Public Trusted Function void Run(int port = 8080, string host = "0.0.0.0")` | — |
