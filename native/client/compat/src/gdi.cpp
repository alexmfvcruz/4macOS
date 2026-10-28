// Native GDI subset (C4-P): font, bitmap and brush objects, memory device
// contexts and 32-bit DIB sections. Text is not drawn here; GDI+
// (gdiplus.cpp) draws into the DIB that a memory device context holds.
#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace fourstory { namespace text { int register_font_file(const char* path); bool unregister_font_file(const char* path); std::string resolved_family(const LOGFONT& font); } }

namespace {
struct Object { DWORD type; virtual ~Object() {} bool stock = false; };
struct Font : Object { LOGFONT lf; };
struct Bitmap : Object { int width = 0, height = 0, bits_per_pixel = 32; std::vector<BYTE> pixels; bool dib = false; };
struct Brush : Object { COLORREF color = 0; };
struct Pen : Object { COLORREF color = 0; };
struct Palette : Object {};
struct DC : Object {
    HWND window = nullptr;
    Font* font = nullptr; Bitmap* bitmap = nullptr; Brush* brush = nullptr; Pen* pen = nullptr;
    COLORREF text_color = 0, back_color = 0xFFFFFF; int back_mode = OPAQUE;
};

std::recursive_mutex& lock() { static std::recursive_mutex m; return m; }
std::set<Object*>& objects() { static std::set<Object*> s; return s; }
template<class T> T* as(HGDIOBJ handle, DWORD type) {
    Object* o = static_cast<Object*>(handle);
    if (!handle || !objects().count(o) || o->type != type) return nullptr;
    return static_cast<T*>(o);
}
DC* as_dc(HDC dc) {
    Object* o = reinterpret_cast<Object*>(dc);
    if (!dc || !objects().count(o) || (o->type != OBJ_DC && o->type != OBJ_MEMDC)) return nullptr;
    return static_cast<DC*>(o);
}
template<class T> T* make(DWORD type) { T* o = new T; o->type = type; objects().insert(o); return o; }

// Stock objects: the system font is "System" at 16 pixels, as Windows reports it.
Font* stock_font(int index) {
    static Font* fonts[32] = {};
    if (index < 0 || index >= 32) return nullptr;
    if (!fonts[index]) {
        Font* f = make<Font>(OBJ_FONT); f->stock = true;
        std::memset(&f->lf, 0, sizeof(f->lf));
        f->lf.lfHeight = 16; f->lf.lfWeight = FW_BOLD; f->lf.lfCharSet = ANSI_CHARSET;
        strcpy_s(f->lf.lfFaceName, index == DEFAULT_GUI_FONT ? "MS Shell Dlg" : "System");
        if (index == DEFAULT_GUI_FONT) { f->lf.lfHeight = -11; f->lf.lfWeight = FW_NORMAL; }
        fonts[index] = f;
    }
    return fonts[index];
}
Bitmap* stock_bitmap() {
    static Bitmap* b = nullptr;
    if (!b) { b = make<Bitmap>(OBJ_BITMAP); b->stock = true; b->width = 1; b->height = 1; b->bits_per_pixel = 1; b->pixels.assign(4, 0); }
    return b;
}
Brush* stock_brush(int index) {
    static Brush* brushes[8] = {};
    if (index < 0 || index > NULL_BRUSH) return nullptr;
    if (!brushes[index]) {
        static const COLORREF colors[] = {0xFFFFFF, 0xC0C0C0, 0x808080, 0x404040, 0x000000, 0};
        Brush* b = make<Brush>(OBJ_BRUSH); b->stock = true; b->color = colors[index]; brushes[index] = b;
    }
    return brushes[index];
}
std::vector<std::string>& font_files() { static std::vector<std::string> v; return v; }
}  // namespace

HDC CreateCompatibleDC(HDC) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = make<DC>(OBJ_MEMDC);
    dc->font = stock_font(SYSTEM_FONT); dc->bitmap = stock_bitmap(); dc->brush = stock_brush(WHITE_BRUSH);
    return reinterpret_cast<HDC>(static_cast<Object*>(dc));
}
BOOL DeleteDC(HDC handle) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    if (!dc) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    objects().erase(dc); delete dc;
    return TRUE;
}
// A window device context: selections work, pixels do not exist.
HDC GetDC(HWND window) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    if (window && !IsWindow(window)) { SetLastError(1400); return nullptr; }
    DC* dc = make<DC>(OBJ_DC);
    dc->window = window; dc->font = stock_font(SYSTEM_FONT); dc->brush = stock_brush(WHITE_BRUSH);
    return reinterpret_cast<HDC>(static_cast<Object*>(dc));
}
HDC GetWindowDC(HWND window) { return GetDC(window); }
int ReleaseDC(HWND, HDC handle) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    if (!dc || dc->type != OBJ_DC) return 0;
    objects().erase(dc); delete dc;
    return 1;
}
HGDIOBJ SelectObject(HDC handle, HGDIOBJ object) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    Object* o = static_cast<Object*>(object);
    if (!dc || !object || !objects().count(o)) { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    switch (o->type) {
    case OBJ_FONT: { Object* old = dc->font; dc->font = static_cast<Font*>(o); return old; }
    case OBJ_BRUSH: { Object* old = dc->brush; dc->brush = static_cast<Brush*>(o); return old; }
    case OBJ_PEN: { Object* old = dc->pen; dc->pen = static_cast<Pen*>(o); return old; }
    case OBJ_BITMAP: {
        // Only memory device contexts hold bitmaps.
        if (dc->type != OBJ_MEMDC) return nullptr;
        Object* old = dc->bitmap; dc->bitmap = static_cast<Bitmap*>(o); return old; }
    }
    SetLastError(ERROR_INVALID_PARAMETER);
    return nullptr;
}
// Deleting a selected object fails on Windows; the object stays valid.
BOOL DeleteObject(HGDIOBJ object) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    Object* o = static_cast<Object*>(object);
    if (!object || !objects().count(o) || o->type == OBJ_DC || o->type == OBJ_MEMDC) return FALSE;
    if (o->stock) return TRUE;
    for (Object* other : objects()) {
        if (other->type != OBJ_DC && other->type != OBJ_MEMDC) continue;
        DC* dc = static_cast<DC*>(other);
        if (dc->font == o || dc->bitmap == o || dc->brush == o || dc->pen == o) return FALSE;
    }
    objects().erase(o); delete o;
    return TRUE;
}
int GetObject(HGDIOBJ object, int size, LPVOID out) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    Object* o = static_cast<Object*>(object);
    if (!object || !objects().count(o)) { SetLastError(ERROR_INVALID_HANDLE); return 0; }
    if (o->type == OBJ_FONT) {
        if (!out) return sizeof(LOGFONT);
        const int n = std::min<int>(size, sizeof(LOGFONT));
        std::memcpy(out, &static_cast<Font*>(o)->lf, static_cast<size_t>(n));
        return n;
    }
    if (o->type == OBJ_BITMAP) {
        Bitmap* b = static_cast<Bitmap*>(o);
        if (b->dib && size >= static_cast<int>(sizeof(DIBSECTION)) && out) {
            DIBSECTION ds = {};
            ds.dsBm = {0, b->width, b->height, b->width * 4, 1, 32, b->pixels.data()};
            ds.dsBmih.biSize = sizeof(BITMAPINFOHEADER); ds.dsBmih.biWidth = b->width; ds.dsBmih.biHeight = -b->height;
            ds.dsBmih.biPlanes = 1; ds.dsBmih.biBitCount = 32; ds.dsBmih.biCompression = BI_RGB;
            std::memcpy(out, &ds, sizeof(ds));
            return sizeof(DIBSECTION);
        }
        if (!out) return sizeof(BITMAP);
        if (size < static_cast<int>(sizeof(BITMAP))) return 0;
        BITMAP bm = {0, b->width, b->height, b->width * (b->bits_per_pixel / 8 ? b->bits_per_pixel / 8 : 1), 1,
                     static_cast<WORD>(b->bits_per_pixel), b->dib ? b->pixels.data() : nullptr};
        std::memcpy(out, &bm, sizeof(bm));
        return sizeof(BITMAP);
    }
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}
HGDIOBJ GetCurrentObject(HDC handle, UINT type) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    if (!dc) return nullptr;
    switch (type) {
    case OBJ_FONT: return dc->font;
    case OBJ_BITMAP: return dc->bitmap;
    case OBJ_BRUSH: return dc->brush;
    case OBJ_PEN: return dc->pen;
    }
    return nullptr;
}
HGDIOBJ GetStockObject(int index) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    switch (index) {
    case WHITE_BRUSH: case LTGRAY_BRUSH: case GRAY_BRUSH: case DKGRAY_BRUSH: case BLACK_BRUSH: case NULL_BRUSH: return stock_brush(index);
    case OEM_FIXED_FONT: case ANSI_FIXED_FONT: case ANSI_VAR_FONT: case SYSTEM_FONT: case DEVICE_DEFAULT_FONT:
    case SYSTEM_FIXED_FONT: case DEFAULT_GUI_FONT: return stock_font(index);
    }
    return nullptr;
}
DWORD GetObjectType(HGDIOBJ object) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    Object* o = static_cast<Object*>(object);
    return object && objects().count(o) ? o->type : 0;
}
HFONT CreateFontIndirect(const LOGFONT* font) {
    if (!font) { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    std::lock_guard<std::recursive_mutex> guard(lock());
    Font* f = make<Font>(OBJ_FONT);
    f->lf = *font;
    f->lf.lfFaceName[LF_FACESIZE - 1] = 0;
    return reinterpret_cast<HFONT>(static_cast<Object*>(f));
}
HFONT CreateFont(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
                 DWORD strike_out, DWORD char_set, DWORD out_precision, DWORD clip_precision, DWORD quality,
                 DWORD pitch_and_family, LPCSTR face) {
    LOGFONT lf = {};
    lf.lfHeight = height; lf.lfWidth = width; lf.lfEscapement = escapement; lf.lfOrientation = orientation; lf.lfWeight = weight;
    lf.lfItalic = static_cast<BYTE>(italic); lf.lfUnderline = static_cast<BYTE>(underline); lf.lfStrikeOut = static_cast<BYTE>(strike_out);
    lf.lfCharSet = static_cast<BYTE>(char_set); lf.lfOutPrecision = static_cast<BYTE>(out_precision);
    lf.lfClipPrecision = static_cast<BYTE>(clip_precision); lf.lfQuality = static_cast<BYTE>(quality);
    lf.lfPitchAndFamily = static_cast<BYTE>(pitch_and_family);
    if (face) strncpy_s(lf.lfFaceName, face, _TRUNCATE);
    return CreateFontIndirect(&lf);
}
// Only 32-bit BI_RGB sections are supported (the client uses no other).
HBITMAP CreateDIBSection(HDC, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section, DWORD) {
    if (bits) *bits = nullptr;
    if (!info || usage != DIB_RGB_COLORS || section) { SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    const BITMAPINFOHEADER& h = info->bmiHeader;
    if (h.biBitCount != 32 || h.biCompression != BI_RGB || h.biWidth <= 0 || h.biHeight == 0 || h.biPlanes != 1) {
        SetLastError(ERROR_NOT_SUPPORTED); return nullptr;
    }
    const long long width = h.biWidth, height = std::llabs(static_cast<long long>(h.biHeight));
    if (width * height > (1LL << 28)) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    std::lock_guard<std::recursive_mutex> guard(lock());
    Bitmap* b = make<Bitmap>(OBJ_BITMAP);
    b->width = static_cast<int>(width); b->height = static_cast<int>(height); b->dib = true;
    // Bottom-up sections (positive height) are stored top-down too; the client
    // creates top-down sections only. See selected_bitmap.
    if (h.biHeight > 0) { objects().erase(b); delete b; SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    b->pixels.assign(static_cast<size_t>(width * height * 4), 0);
    if (bits) *bits = b->pixels.data();
    return reinterpret_cast<HBITMAP>(static_cast<Object*>(b));
}
HBITMAP CreateCompatibleBitmap(HDC, int width, int height) {
    BITMAPINFO bmi = {};
    bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
    return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
}
HBITMAP CreateBitmap(int width, int height, UINT planes, UINT bits_per_pixel, const void* bits) {
    if (planes != 1 || bits_per_pixel != 32) { SetLastError(ERROR_NOT_SUPPORTED); return nullptr; }
    void* p = nullptr;
    BITMAPINFO bmi = {};
    bmi.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
    HBITMAP b = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &p, nullptr, 0);
    if (b && bits) std::memcpy(p, bits, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    return b;
}
// SRCCOPY between two memory device contexts with 32-bit sections.
BOOL BitBlt(HDC to, int x, int y, int width, int height, HDC from, int from_x, int from_y, DWORD rop) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* d = as_dc(to); DC* s = as_dc(from);
    if (!d || !s || rop != SRCCOPY || !d->bitmap || !s->bitmap || !d->bitmap->dib || !s->bitmap->dib) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    for (int row = 0; row < height; ++row) {
        const int sy = from_y + row, dy = y + row;
        if (sy < 0 || dy < 0 || sy >= s->bitmap->height || dy >= d->bitmap->height) continue;
        for (int col = 0; col < width; ++col) {
            const int sx = from_x + col, dx = x + col;
            if (sx < 0 || dx < 0 || sx >= s->bitmap->width || dx >= d->bitmap->width) continue;
            std::memcpy(&d->bitmap->pixels[(static_cast<size_t>(dy) * d->bitmap->width + dx) * 4],
                        &s->bitmap->pixels[(static_cast<size_t>(sy) * s->bitmap->width + sx) * 4], 4);
        }
    }
    return TRUE;
}
HBRUSH CreateSolidBrush(COLORREF color) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    Brush* b = make<Brush>(OBJ_BRUSH); b->color = color;
    return reinterpret_cast<HBRUSH>(static_cast<Object*>(b));
}
COLORREF SetTextColor(HDC handle, COLORREF color) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle); if (!dc) return static_cast<COLORREF>(0xFFFFFFFF);
    COLORREF old = dc->text_color; dc->text_color = color; return old;
}
COLORREF SetBkColor(HDC handle, COLORREF color) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle); if (!dc) return static_cast<COLORREF>(0xFFFFFFFF);
    COLORREF old = dc->back_color; dc->back_color = color; return old;
}
int SetBkMode(HDC handle, int mode) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle); if (!dc) return 0;
    int old = dc->back_mode; dc->back_mode = mode; return old;
}
// 96 dots per inch, as a Windows display at 100 % scale.
int GetDeviceCaps(HDC, int index) {
    int w = 0, h = 0;
    if (fourstory::platform::backend()) fourstory::platform::backend()->screen_size(&w, &h);
    switch (index) {
    case LOGPIXELSX: case LOGPIXELSY: return 96;
    case HORZRES: return w;
    case VERTRES: return h;
    case BITSPIXEL: return 32;
    case PLANES: return 1;
    case VREFRESH: return 60;
    }
    return 0;
}
// The display gamma ramp is not accessible: both calls fail.
BOOL GetDeviceGammaRamp(HDC, LPVOID) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
BOOL SetDeviceGammaRamp(HDC, LPVOID) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
// One mode, the current one. The refresh rate is not known to the backend
// interface; 60 Hz is reported and dmFields does not contain DM_DISPLAYFREQUENCY.
BOOL EnumDisplaySettings(LPCSTR, DWORD mode, DEVMODE* out) {
    if (!out || (mode != static_cast<DWORD>(ENUM_CURRENT_SETTINGS) && mode != static_cast<DWORD>(ENUM_REGISTRY_SETTINGS) && mode != 0)) return FALSE;
    int w = 0, h = 0;
    if (!fourstory::platform::backend()) return FALSE;
    fourstory::platform::backend()->screen_size(&w, &h);
    std::memset(out, 0, sizeof(*out));
    out->dmSize = sizeof(DEVMODE); out->dmBitsPerPel = 32; out->dmPelsWidth = static_cast<DWORD>(w); out->dmPelsHeight = static_cast<DWORD>(h);
    out->dmDisplayFrequency = 60; out->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    return TRUE;
}
int AddFontResourceEx(LPCSTR file, DWORD flags, PVOID) {
    if (!file || flags != FR_PRIVATE) { SetLastError(ERROR_NOT_SUPPORTED); return 0; }
    const std::string path = fourstory::compat::native_path(file);
    const int added = fourstory::text::register_font_file(path.c_str());
    if (added > 0) font_files().push_back(path);
    return added;
}
BOOL RemoveFontResourceEx(LPCSTR file, DWORD, PVOID) {
    if (!file) return FALSE;
    const std::string path = fourstory::compat::native_path(file);
    auto it = std::find(font_files().begin(), font_files().end(), path);
    if (it == font_files().end()) return FALSE;
    font_files().erase(it);
    return fourstory::text::unregister_font_file(path.c_str()) ? TRUE : FALSE;
}
// The realized face: GDI substitutes a face that does not exist (see gdiplus.cpp);
// a vertical "@" face keeps the prefix.
int GetTextFace(HDC handle, int size, LPSTR face) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    if (!dc || !dc->font) return 0;
    std::string realized = fourstory::text::resolved_family(dc->font->lf);
    if (dc->font->lf.lfFaceName[0] == '@') realized = "@" + realized;
    const int n = static_cast<int>(realized.size());
    if (!face) return n + 1;
    if (size <= 0) return 0;
    const int copy = std::min(n, size - 1);
    std::memcpy(face, realized.c_str(), static_cast<size_t>(copy)); face[copy] = 0;
    return copy + 1;
}

namespace fourstory { namespace gdi {
bool selected_font(HDC handle, LOGFONT* font) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    if (!dc || !dc->font || !font) return false;
    *font = dc->font->lf;
    return true;
}
bool selected_bitmap(HDC handle, BYTE** bits, int* width, int* height) {
    std::lock_guard<std::recursive_mutex> guard(lock());
    DC* dc = as_dc(handle);
    if (!dc || !dc->bitmap || !dc->bitmap->dib) return false;
    *bits = dc->bitmap->pixels.data(); *width = dc->bitmap->width; *height = dc->bitmap->height;
    return true;
}
const std::vector<std::string>& private_font_files() { return font_files(); }
}}
