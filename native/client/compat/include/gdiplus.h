// Native GDI+ subset over Core Text (C4-P): the classes and calls of the
// client text path (CD3DFont::MakeText, TComponent::GetTextExtentPoint,
// CD3DFont private fonts). Measurement and layout follow the rules measured
// against Windows GDI+ (research/c4-text-comparison.md); glyph rasterization
// is the Core Text one and differs from Windows. Unsupported calls return
// NotImplemented. Implementation: native/client/compat/src/gdiplus.cpp.
#pragma once
#include <windows.h>
#include <memory>

namespace Gdiplus {
typedef float REAL;
typedef UINT32 ARGB;
enum Status {
    Ok = 0, GenericError = 1, InvalidParameter = 2, OutOfMemory = 3, ObjectBusy = 4, InsufficientBuffer = 5,
    NotImplemented = 6, Win32Error = 7, WrongState = 8, Aborted = 9, FileNotFound = 10, ValueOverflow = 11,
    AccessDenied = 12, UnknownImageFormat = 13, FontFamilyNotFound = 14, FontStyleNotFound = 15,
    NotTrueTypeFont = 16, UnsupportedGdiplusVersion = 17, GdiplusNotInitialized = 18, PropertyNotFound = 19,
    PropertyNotSupported = 20
};
enum Unit { UnitWorld = 0, UnitDisplay = 1, UnitPixel = 2, UnitPoint = 3, UnitInch = 4, UnitDocument = 5, UnitMillimeter = 6 };
enum FontStyle { FontStyleRegular = 0, FontStyleBold = 1, FontStyleItalic = 2, FontStyleBoldItalic = 3, FontStyleUnderline = 4, FontStyleStrikeout = 8 };
enum TextRenderingHint {
    TextRenderingHintSystemDefault = 0, TextRenderingHintSingleBitPerPixelGridFit = 1, TextRenderingHintSingleBitPerPixel = 2,
    TextRenderingHintAntiAliasGridFit = 3, TextRenderingHintAntiAlias = 4, TextRenderingHintClearTypeGridFit = 5
};
enum StringAlignment { StringAlignmentNear = 0, StringAlignmentCenter = 1, StringAlignmentFar = 2 };
enum StringFormatFlags {
    StringFormatFlagsDirectionRightToLeft = 0x0001, StringFormatFlagsDirectionVertical = 0x0002,
    StringFormatFlagsNoFitBlackBox = 0x0004, StringFormatFlagsDisplayFormatControl = 0x0020,
    StringFormatFlagsNoFontFallback = 0x0400, StringFormatFlagsMeasureTrailingSpaces = 0x0800,
    StringFormatFlagsNoWrap = 0x1000, StringFormatFlagsLineLimit = 0x2000, StringFormatFlagsNoClip = 0x4000
};
enum StringTrimming { StringTrimmingNone = 0, StringTrimmingCharacter = 1, StringTrimmingWord = 2 };
enum DebugEventLevel { DebugEventLevelFatal, DebugEventLevelWarning };
typedef void (WINAPI* DebugEventProc)(DebugEventLevel level, char* message);

struct GdiplusStartupInput {
    UINT32 GdiplusVersion = 1; DebugEventProc DebugEventCallback = nullptr;
    BOOL SuppressBackgroundThread = FALSE, SuppressExternalCodecs = FALSE;
};
struct GdiplusStartupOutput { void* NotificationHook = nullptr; void* NotificationUnhook = nullptr; };
Status GdiplusStartup(ULONG_PTR* token, const GdiplusStartupInput* input, GdiplusStartupOutput* output);
void GdiplusShutdown(ULONG_PTR token);

class PointF { public: REAL X, Y; PointF() : X(0), Y(0) {} PointF(REAL x, REAL y) : X(x), Y(y) {} };
class SizeF { public: REAL Width, Height; SizeF() : Width(0), Height(0) {} SizeF(REAL w, REAL h) : Width(w), Height(h) {} };
class RectF {
public:
    REAL X, Y, Width, Height;
    RectF() : X(0), Y(0), Width(0), Height(0) {}
    RectF(REAL x, REAL y, REAL w, REAL h) : X(x), Y(y), Width(w), Height(h) {}
    RectF(const PointF& p, const SizeF& s) : X(p.X), Y(p.Y), Width(s.Width), Height(s.Height) {}
    REAL GetLeft() const { return X; } REAL GetTop() const { return Y; }
    REAL GetRight() const { return X + Width; } REAL GetBottom() const { return Y + Height; }
};
class Color {
public:
    Color() : argb_(0xFF000000u) {}
    Color(BYTE r, BYTE g, BYTE b) : argb_(0xFF000000u | (UINT32(r) << 16) | (UINT32(g) << 8) | b) {}
    Color(BYTE a, BYTE r, BYTE g, BYTE b) : argb_((UINT32(a) << 24) | (UINT32(r) << 16) | (UINT32(g) << 8) | b) {}
    Color(ARGB argb) : argb_(argb) {}
    ARGB GetValue() const { return argb_; }
    static ARGB MakeARGB(BYTE a, BYTE r, BYTE g, BYTE b) { return (ARGB(a) << 24) | (ARGB(r) << 16) | (ARGB(g) << 8) | ARGB(b); }
    BYTE GetA() const { return BYTE(argb_ >> 24); } BYTE GetR() const { return BYTE(argb_ >> 16); }
    BYTE GetG() const { return BYTE(argb_ >> 8); } BYTE GetB() const { return BYTE(argb_); }
private:
    ARGB argb_;
};

class Brush { public: virtual ~Brush() {} Status GetLastStatus() const { return Ok; } virtual ARGB native_color() const = 0; };
class SolidBrush : public Brush {
public:
    explicit SolidBrush(const Color& color) : color_(color) {}
    Status GetColor(Color* color) const { if (!color) return InvalidParameter; *color = color_; return Ok; }
    Status SetColor(const Color& color) { color_ = color; return Ok; }
    ARGB native_color() const override { return color_.GetValue(); }
private:
    Color color_;
};

class StringFormat {
public:
    StringFormat(INT flags = 0, LANGID language = 0) : flags_(flags) { (void)language; }
    Status SetAlignment(StringAlignment a) { align_ = a; return Ok; }
    Status SetLineAlignment(StringAlignment a) { line_align_ = a; return Ok; }
    Status SetFormatFlags(INT flags) { flags_ = flags; return Ok; }
    Status SetTrimming(StringTrimming t) { trimming_ = t; return Ok; }
    StringAlignment GetAlignment() const { return align_; }
    StringAlignment GetLineAlignment() const { return line_align_; }
    INT GetFormatFlags() const { return flags_; }
    StringTrimming GetTrimming() const { return trimming_; }
    Status GetLastStatus() const { return Ok; }
private:
    INT flags_ = 0;
    StringAlignment align_ = StringAlignmentNear, line_align_ = StringAlignmentNear;
    StringTrimming trimming_ = StringTrimmingCharacter;
};

namespace detail { struct FontFace; struct Collection; }
class FontCollection {
public:
    virtual ~FontCollection();
    Status GetLastStatus() const { return status_; }
    std::shared_ptr<detail::Collection> native() const { return collection_; }
protected:
    FontCollection();
    std::shared_ptr<detail::Collection> collection_;
    Status status_ = Ok;
};
class InstalledFontCollection : public FontCollection { public: InstalledFontCollection(); };
class PrivateFontCollection : public FontCollection {
public:
    PrivateFontCollection();
    Status AddFontFile(const WCHAR* file_name);
    Status AddFontFile(const wchar_t* file_name);
};

class FontFamily {
public:
    FontFamily();
    FontFamily(const WCHAR* name, const FontCollection* collection = nullptr);
    FontFamily(const wchar_t* name, const FontCollection* collection = nullptr);
    ~FontFamily();
    FontFamily(const FontFamily& other);
    FontFamily& operator=(const FontFamily& other);
    Status GetLastStatus() const { return status_; }
    BOOL IsAvailable() const;
    BOOL IsStyleAvailable(INT style) const;
    UINT16 GetEmHeight(INT style) const;
    UINT16 GetCellAscent(INT style) const;
    UINT16 GetCellDescent(INT style) const;
    UINT16 GetLineSpacing(INT style) const;
    Status GetFamilyName(WCHAR* name, LANGID language = 0) const;
    std::shared_ptr<detail::FontFace> native(INT style) const;
private:
    friend class Font;
    std::string name_utf8_;
    std::shared_ptr<detail::Collection> collection_;
    Status status_ = Ok;
};

class Graphics;
class Font {
public:
    Font(HDC dc);
    Font(HDC dc, const LOGFONT* font);
    Font(const FontFamily* family, REAL size, INT style = FontStyleRegular, Unit unit = UnitPoint);
    Font(const WCHAR* family, REAL size, INT style = FontStyleRegular, Unit unit = UnitPoint, const FontCollection* collection = nullptr);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Status GetLastStatus() const { return status_; }
    BOOL IsAvailable() const { return status_ == Ok; }
    REAL GetSize() const { return size_; }
    Unit GetUnit() const { return unit_; }
    INT GetStyle() const { return style_; }
    Status GetFamily(FontFamily* family) const;
    REAL GetHeight(const Graphics* graphics) const;
    REAL GetHeight(REAL dpi) const;
    Status GetLogFontA(const Graphics* graphics, LOGFONT* font) const;
    // Native: the em size in pixels at 96 dots per inch and the face.
    REAL native_em_pixels() const;
    std::shared_ptr<detail::FontFace> native() const { return face_; }
private:
    void init(const LOGFONT& lf);
    std::shared_ptr<detail::FontFace> face_;
    FontFamily family_;
    REAL size_ = 0; Unit unit_ = UnitPoint; INT style_ = 0;
    Status status_ = Ok;
};

class Graphics {
public:
    Graphics(HDC dc);
    ~Graphics();
    static Graphics* FromHDC(HDC dc) { return new Graphics(dc); }
    Status GetLastStatus() const { return status_; }
    Status SetTextRenderingHint(TextRenderingHint hint) { hint_ = hint; return Ok; }
    TextRenderingHint GetTextRenderingHint() const { return hint_; }
    REAL GetDpiX() const { return 96.0f; }
    REAL GetDpiY() const { return 96.0f; }
    Status MeasureString(const WCHAR* text, INT length, const Font* font, const PointF& origin, RectF* box) const;
    Status MeasureString(const WCHAR* text, INT length, const Font* font, const PointF& origin, const StringFormat* format, RectF* box) const;
    Status MeasureString(const WCHAR* text, INT length, const Font* font, const RectF& layout, const StringFormat* format, RectF* box,
                         INT* code_points = nullptr, INT* lines = nullptr) const;
    Status MeasureString(const wchar_t* text, INT length, const Font* font, const PointF& origin, RectF* box) const;
    Status MeasureString(const wchar_t* text, INT length, const Font* font, const PointF& origin, const StringFormat* format, RectF* box) const;
    Status DrawString(const WCHAR* text, INT length, const Font* font, const RectF& layout, const StringFormat* format, const Brush* brush);
    Status DrawString(const WCHAR* text, INT length, const Font* font, const PointF& origin, const Brush* brush);
    Status DrawString(const WCHAR* text, INT length, const Font* font, const PointF& origin, const StringFormat* format, const Brush* brush);
    Status DrawString(const wchar_t* text, INT length, const Font* font, const RectF& layout, const StringFormat* format, const Brush* brush);
    Status Clear(const Color& color);
private:
    HDC dc_;
    TextRenderingHint hint_ = TextRenderingHintSystemDefault;
    Status status_ = Ok;
};
}  // namespace Gdiplus

namespace fourstory { namespace text {
// Font files registered for the process (AddFontResourceEx, PrivateFontCollection).
int register_font_file(const char* path);
bool unregister_font_file(const char* path);
// Face that Font(HDC) uses for a LOGFONT face name; documents the mapping.
std::string resolved_family(const LOGFONT& font);
}}
