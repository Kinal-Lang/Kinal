// The Win32 leaf of IO.UI. Kinal owns the visual tree and receives logical coordinates.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <imm.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <string.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>

typedef LRESULT (CALLBACK *SurfaceProc)(HWND, UINT, WPARAM, LPARAM);
enum SurfaceEvent { Paint = 1, Resize, Move, Down, Up, Leave, Wheel, Key, Character,
                    DoubleClick, CaptureLost, Focus, Blur, Tick, Context, Command, KeyUp, Closing, Destroyed, Layout, Moved, Composition, Wake, Settled, Moving, MiddleDown, MiddleUp };
struct FontEntry {
    float size;
    int weight;
    bool mono;
    IDWriteTextFormat *format;
};
struct CodeLayout { wchar_t *text; char *runs; IDWriteTextLayout *layout; int size; };
struct GradientEntry { unsigned from, to; ID2D1LinearGradientBrush *brush; };
struct Surface {
    unsigned refs;
    bool pendingResize;
    HWND window;
    SurfaceProc notify;
    ID3D11Device *gpu;
    IDXGISwapChain1 *swap;
    ID2D1DeviceContext *draw;
    ID2D1Bitmap1 *target;
    ID2D1SolidColorBrush *brush;
    GradientEntry gradients[16]; unsigned nextGradient;
    ID2D1PathGeometry *unitTriangle;
    IDWriteFactory *text;
    CodeLayout lines[128]; unsigned lineSlot;
    FontEntry fonts[24];
    unsigned nextFont;
    HCURSOR cursor;
    unsigned highSurrogate;
    int moveAnchorX,moveAnchorY;
    bool tracking;
    bool painting;
    UINT dpi;
};

template<class T> static void drop(T *&value) {
    if (value) { value->Release(); value = nullptr; }
}
static UINT windowDpi(HWND window) {
    typedef UINT (WINAPI *GetDpi)(HWND);
    GetDpi getDpi = (GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    return getDpi ? getDpi(window) : 96;
}
static int logical(int pixels, UINT dpi) { return MulDiv(pixels, 96, dpi); }
static void releaseTarget(Surface *s) {
    if (s->draw) s->draw->SetTarget(nullptr);
    drop(s->target);
}
static void releaseGraphics(Surface *s) {
    for(auto &line:s->lines){drop(line.layout);free(line.text);free(line.runs);line={};}
    for(auto &gradient:s->gradients)drop(gradient.brush);
    releaseTarget(s);
    drop(s->brush);
    drop(s->unitTriangle);
    drop(s->draw);
    drop(s->swap);
    drop(s->gpu);
}
// All references live on the window thread. A control callback can synchronously destroy its HWND.
static void releaseSurface(Surface *s) {
    if (--s->refs) return;
    releaseGraphics(s);
    for (auto &f : s->fonts) drop(f.format);
    drop(s->text);
    HeapFree(GetProcessHeap(), 0, s);
}
struct DispatchRef {
    Surface *state;
    explicit DispatchRef(Surface *s) : state(s) { ++s->refs; }
    ~DispatchRef() { releaseSurface(state); }
};
static HRESULT setup(Surface *s) {
    RECT rc;
    GetClientRect(s->window, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return S_FALSE;
    HRESULT hr = S_OK;
    if (!s->gpu) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &s->gpu, nullptr, nullptr);
        if (FAILED(hr))
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr, 0, D3D11_SDK_VERSION, &s->gpu, nullptr, nullptr);
        IDXGIDevice *device = nullptr;
        IDXGIAdapter *adapter = nullptr;
        IDXGIFactory2 *factory = nullptr;
        ID2D1Factory1 *factory2d = nullptr;
        ID2D1Device *device2d = nullptr;
        if (SUCCEEDED(hr)) hr = s->gpu->QueryInterface(IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) hr = device->GetAdapter(&adapter);
        if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = rc.right;
        desc.Height = rc.bottom;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        if (SUCCEEDED(hr)) hr = factory->CreateSwapChainForHwnd(s->gpu, s->window, &desc, nullptr, nullptr, &s->swap);
        if (SUCCEEDED(hr)) hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                                                nullptr, reinterpret_cast<void **>(&factory2d));
        if (SUCCEEDED(hr)) hr = factory2d->CreateDevice(device, &device2d);
        if (SUCCEEDED(hr)) hr = device2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &s->draw);
        if (SUCCEEDED(hr)) hr = s->draw->CreateSolidColorBrush(D2D1::ColorF(0u), &s->brush);
        drop(device2d); drop(factory2d); drop(factory); drop(adapter); drop(device);
    }
    if (SUCCEEDED(hr) && !s->target) {
        IDXGISurface *back = nullptr;
        hr = s->swap->GetBuffer(0, IID_PPV_ARGS(&back));
        D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            (float)s->dpi, (float)s->dpi);
        if (SUCCEEDED(hr)) hr = s->draw->CreateBitmapFromDxgiSurface(back, &props, &s->target);
        if (SUCCEEDED(hr)) {
            s->draw->SetTarget(s->target);
            s->draw->SetDpi((float)s->dpi, (float)s->dpi);
        }
        drop(back);
    }
    if (FAILED(hr)) releaseGraphics(s);
    return hr;
}
static Surface *surface(HWND h) {
    return reinterpret_cast<Surface *>(GetWindowLongPtrW(h, GWLP_USERDATA));
}
static LRESULT CALLBACK dispatch(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Surface *s = surface(h);
    if (msg == WM_NCCREATE) {
        s = static_cast<Surface *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
        ++s->refs;
        s->window = h;
        s->dpi = windowDpi(h);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
    }
    if (!s) return DefWindowProcW(h, msg, wp, lp);
    DispatchRef ref(s);
    int x = logical(GET_X_LPARAM(lp), s->dpi), y = logical(GET_Y_LPARAM(lp), s->dpi);
    switch (msg) {
    case WM_CLOSE:
        if (!s->notify(h, Closing, 0, 0) && IsWindow(h)) DestroyWindow(h);
        return 0;
    case WM_DESTROY: s->notify(h, Destroyed, 0, 0); return 0;
    case WM_APP + 81: return s->notify(h, Layout, 0, 0);
    case WM_ENTERSIZEMOVE: {
        RECT r;POINT cursor;
        if(GetWindowRect(h,&r)&&GetCursorPos(&cursor)) {
            int x=cursor.x-r.left,y=cursor.y-r.top;
            s->moveAnchorX=(x>=0&&x<r.right-r.left)?x:(r.right-r.left)/2;
            s->moveAnchorY=(y>=0&&y<r.bottom-r.top)?y:12;
        }
        break;
    }
    case WM_MOVING: {
        auto *rect=reinterpret_cast<const RECT*>(lp);
        if(rect)s->notify(h,Moving,rect->left+s->moveAnchorX,rect->top+s->moveAnchorY);
        break;
    }
    case WM_EXITSIZEMOVE: return s->notify(h,Settled,0,0);
    case WM_MOVE: return s->notify(h, Moved, x, y);
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) { SetCursor(s->cursor); return TRUE; }
        break;
    case WM_DPICHANGED: {
        const RECT &r = *reinterpret_cast<const RECT *>(lp);
        s->dpi = HIWORD(wp);
        SetWindowPos(h,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_NOZORDER);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        HRESULT hr = s->painting ? S_FALSE : setup(s);
        if (hr == S_OK) {
            s->draw->BeginDraw();
            s->draw->Clear(D2D1::ColorF(0xf4f5f7));
            s->painting = true;
            s->notify(h, Paint, 0, 0);
            s->painting = false;
            hr = s->draw->EndDraw();
            if (SUCCEEDED(hr) && s->window && !s->pendingResize) hr = s->swap->Present(1, 0);
        }
        EndPaint(h, &ps);
        if (!s->window) return 0;
        if (s->pendingResize && !s->painting) {
            s->pendingResize = false;
            PostMessageW(h, WM_SIZE, 0, 0);
        }
        if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
            releaseGraphics(s);
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    case WM_SIZE:
    case WM_DPICHANGED_AFTERPARENT: {
        if (s->painting) { s->pendingResize = true; return 0; }
        s->dpi = windowDpi(h);
        RECT rc;
        GetClientRect(h, &rc);
        releaseTarget(s);
        if (s->swap && rc.right && rc.bottom && FAILED(s->swap->ResizeBuffers(0, rc.right, rc.bottom, DXGI_FORMAT_UNKNOWN, 0)))
            releaseGraphics(s);
        s->notify(h, Resize, logical(rc.right, s->dpi), logical(rc.bottom, s->dpi));
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!s->tracking) {
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, h, 0};
            s->tracking = TrackMouseEvent(&track) != 0;
        }
        return s->notify(h, Move, x, y);
    }
    case WM_MOUSELEAVE: s->tracking = false; return s->notify(h, Leave, 0, 0);
    case WM_LBUTTONDOWN:
        SetFocus(h);
        if (s->window) SetCapture(h);
        return s->window ? s->notify(h, Down, x, y) : 0;
    case WM_LBUTTONUP:
        s->notify(h, Up, x, y);
        if (GetCapture() == h) ReleaseCapture();
        return 0;
    case WM_LBUTTONDBLCLK: SetCapture(h); return s->notify(h, DoubleClick, x, y);
    case WM_MBUTTONDOWN:
        SetFocus(h); SetCapture(h); return s->notify(h, MiddleDown, x, y);
    case WM_MBUTTONUP:
        s->notify(h, MiddleUp, x, y);
        if (GetCapture() == h) ReleaseCapture();
        return 0;
    case WM_RBUTTONUP: return s->notify(h, Context, x, y);
    case WM_MOUSEWHEEL: return s->notify(h, Wheel, GET_WHEEL_DELTA_WPARAM(wp), GET_KEYSTATE_WPARAM(wp));
    case WM_KEYDOWN:
        return s->notify(h, Key, wp, lp);
    case WM_KEYUP: return s->notify(h, KeyUp, wp, lp);
    case WM_CHAR:
        if (wp >= 0xd800 && wp <= 0xdbff) { s->highSurrogate = (unsigned)wp; return 0; }
        if (wp >= 0xdc00 && wp <= 0xdfff && s->highSurrogate)
            wp = 0x10000 + ((s->highSurrogate - 0xd800) << 10) + wp - 0xdc00;
        s->highSurrogate = 0;
        return s->notify(h, Character, wp, lp);
    case WM_IME_STARTCOMPOSITION: s->notify(h, Composition, 1, 0); break;
    case WM_IME_ENDCOMPOSITION: s->notify(h, Composition, 0, 0); break;
    case WM_SETFOCUS: return s->notify(h, Focus, 0, 0);
    case WM_KILLFOCUS: return s->notify(h, Blur, 0, 0);
    case WM_CAPTURECHANGED: return s->notify(h, CaptureLost, 0, 0);
    case WM_TIMER: if (wp == 1) return s->notify(h, Tick, 0, 0); break;
    case WM_APP + 72: return s->notify(h, Wake, 0, 0);
    case WM_COMMAND: return s->notify(h, Command, LOWORD(wp), HIWORD(wp));
    case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_NCDESTROY: RemovePropW(h,L"Kinal.IO.UI.Ready");
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        s->window = nullptr;
        releaseSurface(s);
        break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
extern "C" HWND kn_ui_window(const wchar_t *title, int x, int y, int w, int height, SurfaceProc proc) {
    HINSTANCE module = GetModuleHandleW(nullptr);
    WNDCLASSEXW c = {};
    c.cbSize = sizeof(c);
    c.hInstance = module;
    c.style = CS_DBLCLKS;
    c.lpfnWndProc = dispatch;
    c.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    c.lpszClassName = L"Kinal.IO.UI.Surface.2";
    if (!RegisterClassExW(&c) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    Surface *s = (Surface *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Surface));
    if (!s) return nullptr;
    s->refs = 1;
    s->notify = proc;
    s->cursor = c.hCursor;
    HWND result = CreateWindowExW(0, c.lpszClassName, title, WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                           x, y, w, height, nullptr, nullptr, module, s);
    releaseSurface(s);
    return result;
}
static ID2D1SolidColorBrush *brush(Surface *s, unsigned color) {
    s->brush->SetColor(D2D1::ColorF(color & 0xffffff, ((color >> 24) & 255) / 255.f));
    return s->brush;
}
extern "C" void kn_ui_box(HWND h, int x, int y, int w, int height, unsigned color, int radius, int stroke) {
    Surface *s = surface(h);
    if (!s || !s->painting || w <= 0 || height <= 0) return;
    float inset = stroke / 2.f;
    auto r = D2D1::RoundedRect(D2D1::RectF(x + inset, y + inset, x+w-inset, y+height-inset),
                               (float)radius, (float)radius);
    if (stroke > 0) s->draw->DrawRoundedRectangle(r, brush(s,color), (float)stroke);
    else s->draw->FillRoundedRectangle(r, brush(s,color));
}
extern "C" void kn_ui_clip(HWND h, int x, int y, int w, int height, int enable) {
    Surface *s = surface(h);
    if (!s || !s->painting) return;
    if (enable) s->draw->PushAxisAlignedClip(D2D1::RectF((float)x,(float)y,(float)(x+w),(float)(y+height)),D2D1_ANTIALIAS_MODE_ALIASED);
    else s->draw->PopAxisAlignedClip();
}
extern "C" void kn_ui_gradient(HWND h,int x,int y,int w,int height,unsigned from,unsigned to,int vertical) {
    Surface *s=surface(h);
    if(!s || !s->painting || w<=0 || height<=0)return;
    auto start=D2D1::Point2F(float(x),float(y));
    auto end=D2D1::Point2F(float(vertical?x:x+w),float(vertical?y+height:y));
    ID2D1LinearGradientBrush *ramp=nullptr;
    for(auto &entry:s->gradients)
        if(entry.brush && entry.from==from && entry.to==to){ramp=entry.brush;break;}
    if(!ramp){
        D2D1_GRADIENT_STOP stops[]={{0.f,D2D1::ColorF(from&0xffffff,(from>>24)/255.f)},
                                    {1.f,D2D1::ColorF(to&0xffffff,(to>>24)/255.f)}};
        ID2D1GradientStopCollection *colors=nullptr;
        if(FAILED(s->draw->CreateGradientStopCollection(stops,2,D2D1_GAMMA_2_2,D2D1_EXTEND_MODE_CLAMP,&colors)))return;
        auto props=D2D1::LinearGradientBrushProperties(start,end);
        HRESULT hr=s->draw->CreateLinearGradientBrush(props,colors,&ramp);
        colors->Release();
        if(FAILED(hr))return;
        auto &entry=s->gradients[s->nextGradient++%16];
        drop(entry.brush);entry={from,to,ramp};
    }else{
        ramp->SetStartPoint(start);ramp->SetEndPoint(end);
    }
    s->draw->FillRectangle(D2D1::RectF(start.x,start.y,float(x+w),float(y+height)),ramp);
}
static IDWriteTextFormat *fontFor(Surface *s,int size,int weight,int mono) {
    if (!s->text && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                           reinterpret_cast<IUnknown **>(&s->text)))) return nullptr;
    IDWriteTextFormat *fmt = nullptr;
    for (auto &f : s->fonts)
        if (f.format && f.size == size && f.weight == weight && f.mono == !!mono) { fmt = f.format; break; }
    if (!fmt) {
        FontEntry &f = s->fonts[s->nextFont++ % 24];
        drop(f.format);
        HRESULT hr = s->text->CreateTextFormat(mono ? L"Consolas" : L"Segoe UI", nullptr, (DWRITE_FONT_WEIGHT)weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (float)size, L"", &f.format);
        if (FAILED(hr)) return nullptr;
        f.size = (float)size; f.weight = weight; f.mono = !!mono;
        fmt = f.format;
        fmt->SetIncrementalTabStop((float)size*2.4f);
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return fmt;
}
extern "C" void kn_ui_text(HWND h, const wchar_t *str, int x, int y, int w, int height, int size, int weight, int mono, unsigned color, int alignment) {
    Surface *s = surface(h);
    if (!s || !s->painting || !str || w <= 0 || height <= 0) return;
    IDWriteTextFormat *fmt=fontFor(s,size,weight,mono);if(!fmt)return;
    fmt->SetTextAlignment(alignment == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER :
                          alignment == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    s->draw->DrawText(str, lstrlenW(str), fmt, D2D1::RectF((float)x,(float)y,(float)(x+w),(float)(y+height)),
                      brush(s,color), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
extern "C" void kn_ui_timer(HWND h, int active) {
    if (active) SetTimer(h, 1, 16, nullptr);
    else KillTimer(h, 1);
}
extern "C" int kn_ui_dpi(HWND h) { return (int)windowDpi(h); }
extern "C" void kn_ui_client_size(HWND window, int width, int height) {
    UINT dpi = windowDpi(window);
    RECT rect = {0, 0, MulDiv(width, dpi, 96), MulDiv(height, dpi, 96)};
    DWORD style = (DWORD)GetWindowLongPtrW(window, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(window, GWL_EXSTYLE);
    typedef BOOL (WINAPI *AdjustForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);
    auto adjust = (AdjustForDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi");
    if (adjust) adjust(&rect, style, FALSE, exStyle, dpi);
    else AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    SetWindowPos(window, nullptr, 0, 0, rect.right-rect.left, rect.bottom-rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}
extern "C" void kn_ui_dpi_bounds(HWND window, LPARAM suggested) {
    const RECT *r = reinterpret_cast<const RECT *>(suggested);
    if (r) SetWindowPos(window, nullptr, r->left, r->top, r->right-r->left, r->bottom-r->top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
}
extern "C" void kn_ui_shown(HWND window) {
    if(window) SetPropW(window,L"Kinal.IO.UI.Ready",reinterpret_cast<HANDLE>(1));
}
extern "C" void kn_ui_init() { SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); }
extern "C" void kn_ui_dark_title(HWND h, int dark) {
    BOOL value = dark != 0; DwmSetWindowAttribute(h, 20, &value, sizeof(value));
}
extern "C" void kn_ui_cursor(HWND h, int kind) {
    static const int ids[] = {32512,32644,32645,32642,32643,32646,32513,32649,32648};
    Surface *s = surface(h); if (!s) return;
    HCURSOR cursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(ids[kind>=0&&kind<9?kind:0]));
    s->cursor = cursor; SetCursor(cursor);
}
extern "C" void kn_ui_ime(HWND h, int x, int y) {
    if (!h) return;
    HIMC im = ImmGetContext(h); if (!im) return;
    COMPOSITIONFORM form = {}; form.dwStyle = CFS_POINT;
    form.ptCurrentPos = {MulDiv(x,windowDpi(h),96),MulDiv(y,windowDpi(h),96)};
    ImmSetCompositionWindow(im,&form);
    CANDIDATEFORM candidate = {}; candidate.dwStyle=CFS_CANDIDATEPOS; candidate.ptCurrentPos=form.ptCurrentPos;
    ImmSetCandidateWindow(im,&candidate); ImmReleaseContext(h,im);
}
extern "C" void kn_ui_line(HWND h,int x,int y,int endX,int endY,unsigned color,int width) {
    Surface *s=surface(h); if(s&&s->painting) s->draw->DrawLine(D2D1::Point2F(x+.5f,y+.5f),D2D1::Point2F(endX+.5f,endY+.5f),brush(s,color),(float)width);
}
extern "C" void kn_ui_triangle(HWND h, int ax, int ay, int bx, int by, int cx, int cy, unsigned color) {
    Surface *s = surface(h);
    if (!s || !s->painting) return;
    if (!s->unitTriangle) {
        ID2D1Factory *factory = nullptr;
        ID2D1GeometrySink *sink = nullptr;
        s->draw->GetFactory(&factory);
        HRESULT hr = factory->CreatePathGeometry(&s->unitTriangle);
        if (SUCCEEDED(hr)) hr = s->unitTriangle->Open(&sink);
        if (SUCCEEDED(hr)) {
            sink->BeginFigure(D2D1::Point2F(0,0), D2D1_FIGURE_BEGIN_FILLED);
            const D2D1_POINT_2F points[] = {{1,0}, {0,1}};
            sink->AddLines(points, 2);
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            hr = sink->Close();
        }
        drop(sink);
        drop(factory);
        if (FAILED(hr)) { drop(s->unitTriangle); return; }
    }
    D2D1::Matrix3x2F previous;
    s->draw->GetTransform(&previous);
    const D2D1::Matrix3x2F place(float(bx)-ax, float(by)-ay, float(cx)-ax, float(cy)-ay, float(ax), float(ay));
    s->draw->SetTransform(place * previous);
    s->draw->FillGeometry(s->unitTriangle, brush(s,color));
    s->draw->SetTransform(previous);
}
extern "C" void kn_ui_copy(HWND h,const wchar_t *value) {
    if(!value || !OpenClipboard(h)) return;
    SIZE_T bytes=(lstrlenW(value)+1)*sizeof(wchar_t); HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes);
    if(mem){void *dst=GlobalLock(mem);if(dst){memcpy(dst,value,bytes);GlobalUnlock(mem);
        if(EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,mem))mem=nullptr;}if(mem)GlobalFree(mem);}
    CloseClipboard();
}
extern "C" const char *kn_ui_paste(HWND h) {
    static char *text=nullptr; free(text);text=nullptr;
    if(OpenClipboard(h)){
        HANDLE mem=GetClipboardData(CF_UNICODETEXT);const wchar_t *src=mem?(const wchar_t*)GlobalLock(mem):nullptr;
        if(src){int n=WideCharToMultiByte(CP_UTF8,0,src,-1,nullptr,0,nullptr,nullptr);
            text=(char*)malloc(n);if(text)WideCharToMultiByte(CP_UTF8,0,src,-1,text,n,nullptr,nullptr);GlobalUnlock(mem);}
        CloseClipboard();
    }
    return text?text:"";
}

// DirectWrite uses UTF-16 positions for layout, hit testing and the LSP boundary.
extern "C" int kn_ui_text_position(HWND h,const wchar_t *str,int size,int mono,int value,int hit) {
    Surface *s=surface(h); if(!s || !str) return 0;
    const UINT32 n=lstrlenW(str);
    IDWriteTextLayout *layout=nullptr;
    if(mono) for(auto &cached:s->lines) {
        if(cached.layout && cached.size==size && !wcscmp(cached.text,str)){layout=cached.layout;layout->AddRef();break;}
    }
    if(!layout){
        IDWriteTextFormat *fmt=fontFor(s,size,400,mono); if(!fmt)return 0;
        fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        if(FAILED(s->text->CreateTextLayout(str,n,fmt,1000000.f,22.f,&layout)))return 0;
    }
    float x=0,y=0;DWRITE_HIT_TEST_METRICS metrics={};int result=0;
    if(hit){BOOL trailing=FALSE,inside=FALSE;layout->HitTestPoint((float)value,1,&trailing,&inside,&metrics);
        result=(int)(metrics.textPosition+(trailing?metrics.length:0));}
    else {layout->HitTestTextPosition(value<0?0:((UINT32)value>n?n:(UINT32)value),FALSE,&x,&y,&metrics);result=(int)(x+.5f);}
    layout->Release();return result;
}

extern "C" void kn_ui_code(HWND h,const wchar_t *str,const char *runs,int x,int y,int size,unsigned color) {
    Surface *s=surface(h);if(!s||!s->painting||!str||!*str)return;
    if(!runs)runs="";
    CodeLayout *line=nullptr;
    for(auto &cached:s->lines)if(cached.layout&&cached.size==size&&!wcscmp(cached.text,str)&&!strcmp(cached.runs,runs)){line=&cached;break;}
    if(!line){
        line=&s->lines[s->lineSlot++%128];drop(line->layout);free(line->text);free(line->runs);*line={};
        IDWriteTextFormat *fmt=fontFor(s,size,400,1);if(!fmt)return;
        fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        if(FAILED(s->text->CreateTextLayout(str,lstrlenW(str),fmt,1000000.f,22.f,&line->layout)))return;
        line->text=_wcsdup(str);line->runs=_strdup(runs);line->size=size;
        if(!line->text||!line->runs){drop(line->layout);return;}
        const char *p=runs;
        while(*p){char *end=nullptr;strtol(p,&end,10);if(end==p)break;
            long col=strtol(end,&end,10),len=strtol(end,&end,10);unsigned rgb=strtoul(end,&end,16);
            if(*end!='\n'||col<0||len<=0)break;p=end+1;
            ID2D1SolidColorBrush *ink=nullptr;
            if(SUCCEEDED(s->draw->CreateSolidColorBrush(D2D1::ColorF(rgb),&ink))){
                DWRITE_TEXT_RANGE range={(UINT32)col,(UINT32)len};line->layout->SetDrawingEffect(ink,range);ink->Release();}
        }
    }
    s->draw->DrawTextLayout(D2D1::Point2F((float)x,(float)y),line->layout,brush(s,color));
}

