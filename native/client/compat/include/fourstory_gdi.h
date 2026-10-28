// Native GDI subset (C4-P), part of <windows.h>. The client draws all text
// with GDI+ (native/client/compat/include/gdiplus.h, Core Text); GDI only
// holds fonts and the 32-bit DIB sections that GDI+ draws into. Memory device
// contexts keep their selected font and bitmap. A window device context has no
// pixels: drawing to it is not presented. See compat/README.md.
#pragma once
#include <string>
#include <vector>

struct LOGFONT {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[LF_FACESIZE];
};
static_assert(sizeof(LOGFONT) == 60, "LOGFONTA layout");
typedef LOGFONT LOGFONTA, *PLOGFONT, *LPLOGFONT, *LPLOGFONTA;
struct TEXTMETRIC {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth,
         tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    BYTE tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar, tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
};
typedef TEXTMETRIC *LPTEXTMETRIC;
struct RGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; };
struct BITMAPINFOHEADER {
    DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount; DWORD biCompression, biSizeImage;
    LONG biXPelsPerMeter, biYPelsPerMeter; DWORD biClrUsed, biClrImportant;
};
static_assert(sizeof(BITMAPINFOHEADER) == 40, "BITMAPINFOHEADER layout");
typedef BITMAPINFOHEADER *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;
struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; };
typedef BITMAPINFO *LPBITMAPINFO, *PBITMAPINFO;
#pragma pack(push, 2)
struct BITMAPFILEHEADER { WORD bfType; DWORD bfSize; WORD bfReserved1, bfReserved2; DWORD bfOffBits; };
#pragma pack(pop)
struct BITMAP { LONG bmType, bmWidth, bmHeight, bmWidthBytes; WORD bmPlanes, bmBitsPixel; LPVOID bmBits; };
typedef BITMAP *LPBITMAP, *PBITMAP;
struct DIBSECTION { BITMAP dsBm; BITMAPINFOHEADER dsBmih; DWORD dsBitfields[3]; HANDLE dshSection; DWORD dsOffset; };
// Display mode fields that the client reads; not the Windows binary layout.
struct DEVMODE {
    CHAR dmDeviceName[32]; WORD dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; DWORD dmFields;
    DWORD dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
};
typedef DEVMODE DEVMODEA, *LPDEVMODE, *PDEVMODE;
struct SIZEL_COMPAT { LONG cx, cy; };

HDC CreateCompatibleDC(HDC dc);
BOOL DeleteDC(HDC dc);
HDC GetDC(HWND window);
HDC GetWindowDC(HWND window);
int ReleaseDC(HWND window, HDC dc);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ object);
BOOL DeleteObject(HGDIOBJ object);
int GetObject(HGDIOBJ object, int size, LPVOID out);
HGDIOBJ GetCurrentObject(HDC dc, UINT type);
HGDIOBJ GetStockObject(int index);
DWORD GetObjectType(HGDIOBJ object);
HFONT CreateFontIndirect(const LOGFONT* font);
HFONT CreateFont(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
                 DWORD strike_out, DWORD char_set, DWORD out_precision, DWORD clip_precision, DWORD quality,
                 DWORD pitch_and_family, LPCSTR face);
HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section, DWORD offset);
HBITMAP CreateCompatibleBitmap(HDC dc, int width, int height);
HBITMAP CreateBitmap(int width, int height, UINT planes, UINT bits_per_pixel, const void* bits);
BOOL BitBlt(HDC to, int x, int y, int width, int height, HDC from, int from_x, int from_y, DWORD rop);
HBRUSH CreateSolidBrush(COLORREF color);
COLORREF SetTextColor(HDC dc, COLORREF color);
COLORREF SetBkColor(HDC dc, COLORREF color);
int SetBkMode(HDC dc, int mode);
int GetDeviceCaps(HDC dc, int index);
BOOL GetDeviceGammaRamp(HDC dc, LPVOID ramp);
BOOL SetDeviceGammaRamp(HDC dc, LPVOID ramp);
BOOL EnumDisplaySettings(LPCSTR device, DWORD mode, DEVMODE* out);
int AddFontResourceEx(LPCSTR file, DWORD flags, PVOID reserved);
BOOL RemoveFontResourceEx(LPCSTR file, DWORD flags, PVOID reserved);
int GetTextFace(HDC dc, int size, LPSTR face);

namespace fourstory { namespace gdi {
// The selected objects of a device context, for the GDI+ layer.
bool selected_font(HDC dc, LOGFONT* font);
// Pixels of the DIB section selected into a memory device context: top-down
// 32-bit rows, stride = width * 4. False when no 32-bit DIB is selected.
bool selected_bitmap(HDC dc, BYTE** bits, int* width, int* height);
// Font files registered with AddFontResourceEx (paths as given, in order).
const std::vector<std::string>& private_font_files();
}}
