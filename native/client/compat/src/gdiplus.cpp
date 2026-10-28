// Native GDI+ subset over Core Text (C4-P). Layout rules measured against
// Windows GDI+ with native/client/reference/c4-text-dump.cpp
// (research/c4-completion.md (text comparison)):
//   - Font(HDC) takes the selected LOGFONT; size = em height in pixels, UnitWorld.
//     A negative lfHeight is the em height; a positive one the cell height.
//   - A face that does not exist resolves as GDI does on a Western Windows:
//     Arial; "@" faces (vertical) resolve to the Korean UI face (Windows:
//     Malgun Gothic, here the first available of kKoreanFaces).
//   - MeasureString width = sum of glyph advances + em/3 (em/6 padding on each
//     side); a glyph advance is round(design advance x 1.03) design units.
//     Trailing spaces count only with MeasureTrailingSpaces.
//   - MeasureString height: one line = em * (cellAscent + cellDescent) / emHeight
//     + em/8; n > 1 lines = n * lineSpacing * em / emHeight + em/8.
//   - DrawString: pen x = layout.X + em/6, baseline = layout.Y + em * cellAscent / emHeight
//     (+ lineSpacing per line); pixels are premultiplied white, B=G=R=A=coverage.
// Glyph shapes come from Core Text and differ from the Windows rasterizer.
#include <windows.h>
#include <gdiplus.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Gdiplus { namespace detail {
struct Metrics { UINT16 em = 2048, ascent = 0, descent = 0, line_spacing = 0; };
struct FontFace {
    CTFontRef font = nullptr;   // at 1 pixel per em unit size 1; scaled per use
    std::string family;
    Metrics metrics;
    ~FontFace() { if (font) CFRelease(font); }
};
struct Collection { std::vector<std::string> files; std::vector<std::string> families; bool installed = false; };
}}

namespace {
using Gdiplus::detail::FontFace;
using Gdiplus::detail::Metrics;
std::recursive_mutex& lock() { static std::recursive_mutex m; return m; }
bool g_started = false;

std::string cf_to_utf8(CFStringRef s) {
    if (!s) return std::string();
    const CFIndex n = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
    std::string out(static_cast<size_t>(n), '\0');
    if (!CFStringGetCString(s, &out[0], n, kCFStringEncodingUTF8)) return std::string();
    out.resize(std::strlen(out.c_str()));
    return out;
}
CFStringRef cf_string(const std::string& utf8) { return CFStringCreateWithCString(nullptr, utf8.c_str(), kCFStringEncodingUTF8); }
std::string wide_to_utf8(const WCHAR* text) {
    std::string out;
    if (!text) return out;
    int n = 0; while (text[n]) ++n;
    std::vector<char> buffer(static_cast<size_t>(n) * 4 + 1);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, n, buffer.data(), static_cast<int>(buffer.size()), nullptr, nullptr);
    return std::string(buffer.data(), static_cast<size_t>(std::max(0, bytes)));
}
std::u16string from_wchar(const wchar_t* text) {
    std::u16string out;
    for (; text && *text; ++text) {
        char32_t c = static_cast<char32_t>(*text);
        if (c >= 0x10000) { c -= 0x10000; out += static_cast<char16_t>(0xD800 + (c >> 10)); out += static_cast<char16_t>(0xDC00 + (c & 0x3FF)); }
        else out += static_cast<char16_t>(c);
    }
    return out;
}

UINT16 be16(const UInt8* p) { return static_cast<UINT16>((p[0] << 8) | p[1]); }
SHORT sbe16(const UInt8* p) { return static_cast<SHORT>(be16(p)); }
// GDI+ cell ascent and descent are the OS/2 Windows metrics; the line spacing
// is the larger of their sum and the hhea ascender - descender + line gap.
Metrics read_metrics(CTFontRef font) {
    Metrics m;
    m.em = static_cast<UINT16>(CTFontGetUnitsPerEm(font));
    const double scale = m.em / CTFontGetSize(font);
    m.ascent = static_cast<UINT16>(std::lround(CTFontGetAscent(font) * scale));
    m.descent = static_cast<UINT16>(std::lround(CTFontGetDescent(font) * scale));
    int hhea_line = m.ascent + m.descent + static_cast<int>(std::lround(CTFontGetLeading(font) * scale));
    if (CFDataRef hhea = CTFontCopyTable(font, kCTFontTableHhea, kCTFontTableOptionNoOptions)) {
        if (CFDataGetLength(hhea) >= 10) {
            const UInt8* p = CFDataGetBytePtr(hhea);
            hhea_line = sbe16(p + 4) - sbe16(p + 6) + sbe16(p + 8);
        }
        CFRelease(hhea);
    }
    if (CFDataRef os2 = CTFontCopyTable(font, kCTFontTableOS2, kCTFontTableOptionNoOptions)) {
        if (CFDataGetLength(os2) >= 78) {
            const UInt8* p = CFDataGetBytePtr(os2);
            m.ascent = be16(p + 74); m.descent = be16(p + 76);
        }
        CFRelease(os2);
    }
    m.line_spacing = static_cast<UINT16>(std::max<int>(m.ascent + m.descent, hhea_line));
    return m;
}

bool family_exists(const std::string& family) {
    CFStringRef name = cf_string(family);
    const void* keys[] = {kCTFontFamilyNameAttribute};
    const void* values[] = {name};
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
    CFSetRef mandatory = CFSetCreate(nullptr, keys, 1, &kCFTypeSetCallBacks);
    CFArrayRef matches = CTFontDescriptorCreateMatchingFontDescriptors(desc, mandatory);
    const bool found = matches && CFArrayGetCount(matches) > 0;
    if (matches) CFRelease(matches);
    CFRelease(mandatory); CFRelease(desc); CFRelease(attrs); CFRelease(name);
    return found;
}
const char* const kKoreanFaces[] = {"Malgun Gothic", "Apple SD Gothic Neo", "AppleGothic"};

// Face resolution of Font(HDC): see the file comment.
std::string resolve_face(const char* face) {
    std::string name = face ? face : "";
    if (!name.empty() && name[0] == '@') {
        name.erase(0, 1);
        if (family_exists(name)) return name;
        for (const char* k : kKoreanFaces) if (family_exists(k)) return k;
        return "Arial";
    }
    if (!name.empty() && family_exists(name)) return name;
    return "Arial";
}

// A Core Text font of the family and style at size 1.
std::shared_ptr<FontFace> make_face(const std::string& family, INT style) {
    static std::map<std::pair<std::string, INT>, std::shared_ptr<FontFace>> cache;
    std::lock_guard<std::recursive_mutex> guard(lock());
    const auto key = std::make_pair(family, style & Gdiplus::FontStyleBoldItalic);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    CFStringRef name = cf_string(family);
    CTFontSymbolicTraits traits = 0;
    if (style & Gdiplus::FontStyleBold) traits |= kCTFontBoldTrait;
    if (style & Gdiplus::FontStyleItalic) traits |= kCTFontItalicTrait;
    CFNumberRef traits_number = CFNumberCreate(nullptr, kCFNumberSInt32Type, &traits);
    const void* tkeys[] = {kCTFontSymbolicTrait};
    const void* tvalues[] = {traits_number};
    CFDictionaryRef traits_dict = CFDictionaryCreate(nullptr, tkeys, tvalues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void* keys[] = {kCTFontFamilyNameAttribute, kCTFontTraitsAttribute};
    const void* values[] = {name, traits_dict};
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
    CTFontRef font = CTFontCreateWithFontDescriptor(desc, 64.0, nullptr);
    CFRelease(desc); CFRelease(attrs); CFRelease(traits_dict); CFRelease(traits_number); CFRelease(name);
    std::shared_ptr<FontFace> face;
    if (font) {
        const std::string actual = cf_to_utf8(CTFontCopyFamilyName(font));
        face = std::make_shared<FontFace>();
        face->font = font; face->family = actual; face->metrics = read_metrics(font);
    }
    cache[key] = face;
    return face;
}

// Em size in pixels for a LOGFONT height (Font(HDC)).
float em_from_logfont(LONG height, const Metrics& m) {
    if (height < 0) return static_cast<float>(-height);
    if (height == 0) return 12.0f;
    return static_cast<float>(height) * m.em / static_cast<float>(m.ascent + m.descent);
}

struct Glyph { CTFontRef font; CGGlyph glyph; double advance; };
// Widths in GDI+ advance units (design units of the primary font's em).
struct Line { std::vector<Glyph> glyphs; double width = 0; double trailing = 0; double units = 0, trailing_units = 0; };
const double kAdvanceScale = 1.03; // measured, research/c4-completion.md (text comparison)

// Glyphs of a UTF-16 text in one font at em size, with per-character font
// fallback for characters the font does not cover.
// Line breaks come only from LF; word wrap at the layout width is NOT IMPLEMENTED.
std::vector<Line> layout_lines(const std::u16string& text, const FontFace& face, float em) {
    std::vector<Line> lines(1);
    CTFontRef base = CTFontCreateCopyWithAttributes(face.font, em, nullptr, nullptr);
    std::vector<CTFontRef> owned{base};
    size_t i = 0;
    while (i < text.size()) {
        char16_t c = text[i];
        if (c == u'\r') { ++i; continue; }
        if (c == u'\n') { lines.emplace_back(); ++i; continue; }
        UniChar chars[2] = {c, 0}; CFIndex count = 1;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < text.size()) { chars[1] = text[i + 1]; count = 2; }
        CGGlyph glyphs[2] = {0, 0};
        CTFontRef font = base;
        if (!CTFontGetGlyphsForCharacters(base, chars, glyphs, count)) {
            CFStringRef s = CFStringCreateWithCharacters(nullptr, chars, count);
            CTFontRef fallback = CTFontCreateForString(base, s, CFRangeMake(0, count));
            CFRelease(s);
            if (fallback) { owned.push_back(fallback); font = fallback; CTFontGetGlyphsForCharacters(fallback, chars, glyphs, count); }
        }
        CGSize advance;
        const double nominal = CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, glyphs, &advance, 1);
        // GDI+: the design-unit advance times 1.03, rounded to a whole design unit.
        const double upm = CTFontGetUnitsPerEm(font);
        const double units = std::nearbyint(nominal * upm / em * kAdvanceScale);
        const double a = units * em / upm;
        const double base_upm = CTFontGetUnitsPerEm(base);
        Glyph g = {font, glyphs[0], a};
        lines.back().glyphs.push_back(g);
        lines.back().width += a;
        lines.back().units += a * base_upm / em;
        if (c == u' ') { lines.back().trailing += a; lines.back().trailing_units += a * base_upm / em; }
        else { lines.back().trailing = 0; lines.back().trailing_units = 0; }
        i += static_cast<size_t>(count);
    }
    // Fonts stay alive with the glyphs: retain each used font once per glyph list.
    for (Line& line : lines) for (Glyph& g : line.glyphs) CFRetain(g.font);
    for (CTFontRef f : owned) CFRelease(f);
    return lines;
}
void release_lines(std::vector<Line>& lines) { for (Line& line : lines) for (Glyph& g : line.glyphs) CFRelease(g.font); }

Gdiplus::RectF measure(const std::u16string& text, const Gdiplus::Font* font, const Gdiplus::PointF& origin, INT flags) {
    const FontFace& face = *font->native();
    const float em = font->native_em_pixels();
    std::vector<Line> lines = layout_lines(text, face, em);
    double units = 0;
    for (const Line& l : lines) units = std::max(units, l.units - ((flags & Gdiplus::StringFormatFlagsMeasureTrailingSpaces) ? 0 : l.trailing_units));
    release_lines(lines);
    // Single precision in the order that matches most Windows results bit for bit.
    const float scale = em / static_cast<float>(face.metrics.em), pad = em / 6.0f;
    const float width = static_cast<float>(units) * scale + pad + pad;
    const Metrics& m = face.metrics;
    const double height = lines.size() <= 1 ? em * double(m.ascent + m.descent) / m.em + em / 8.0
                                            : double(lines.size()) * em * m.line_spacing / m.em + em / 8.0;
    return Gdiplus::RectF(origin.X, origin.Y, width, static_cast<float>(height));
}
}  // namespace

namespace Gdiplus {
Status GdiplusStartup(ULONG_PTR* token, const GdiplusStartupInput* input, GdiplusStartupOutput* output) {
    if (!token || !input) return InvalidParameter;
    if (input->GdiplusVersion != 1) return UnsupportedGdiplusVersion;
    if (output) { output->NotificationHook = nullptr; output->NotificationUnhook = nullptr; }
    g_started = true; *token = 1;
    return Ok;
}
void GdiplusShutdown(ULONG_PTR) { g_started = false; }

FontCollection::FontCollection() : collection_(std::make_shared<detail::Collection>()) {}
FontCollection::~FontCollection() {}
InstalledFontCollection::InstalledFontCollection() { collection_->installed = true; }
PrivateFontCollection::PrivateFontCollection() {}
Status PrivateFontCollection::AddFontFile(const WCHAR* file_name) {
    if (!file_name) return InvalidParameter;
    const std::string path = fourstory::compat::native_path(wide_to_utf8(file_name).c_str());
    CFStringRef s = cf_string(path);
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, s, kCFURLPOSIXPathStyle, false);
    CFRelease(s);
    CFArrayRef descriptors = url ? CTFontManagerCreateFontDescriptorsFromURL(url) : nullptr;
    if (url) CFRelease(url);
    if (!descriptors || CFArrayGetCount(descriptors) == 0) { if (descriptors) CFRelease(descriptors); return FileNotFound; }
    for (CFIndex i = 0; i < CFArrayGetCount(descriptors); ++i) {
        CTFontDescriptorRef d = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(descriptors, i));
        CFStringRef family = static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(d, kCTFontFamilyNameAttribute));
        if (family) { collection_->families.push_back(cf_to_utf8(family)); CFRelease(family); }
    }
    CFRelease(descriptors);
    if (fourstory::text::register_font_file(path.c_str()) <= 0) return FileNotFound;
    collection_->files.push_back(path);
    return Ok;
}
Status PrivateFontCollection::AddFontFile(const wchar_t* file_name) {
    const std::u16string w = from_wchar(file_name);
    return AddFontFile(reinterpret_cast<const WCHAR*>(w.c_str()));
}

FontFamily::FontFamily() : status_(FontFamilyNotFound) {}
FontFamily::FontFamily(const WCHAR* name, const FontCollection* collection) {
    name_utf8_ = wide_to_utf8(name);
    if (collection) collection_ = collection->native();
    bool known = false;
    if (collection_ && !collection_->installed)
        known = std::find(collection_->families.begin(), collection_->families.end(), name_utf8_) != collection_->families.end();
    else known = family_exists(name_utf8_);
    status_ = known && make_face(name_utf8_, FontStyleRegular) ? Ok : FontFamilyNotFound;
}
FontFamily::FontFamily(const wchar_t* name, const FontCollection* collection)
    : FontFamily(reinterpret_cast<const WCHAR*>(from_wchar(name).c_str()), collection) {}
FontFamily::~FontFamily() {}
FontFamily::FontFamily(const FontFamily& other) = default;
FontFamily& FontFamily::operator=(const FontFamily& other) = default;
BOOL FontFamily::IsAvailable() const { return status_ == Ok; }
BOOL FontFamily::IsStyleAvailable(INT style) const {
    if (status_ != Ok) return FALSE;
    std::shared_ptr<detail::FontFace> f = make_face(name_utf8_, style);
    if (!f) return FALSE;
    CTFontSymbolicTraits t = CTFontGetSymbolicTraits(f->font);
    if ((style & FontStyleBold) && !(t & kCTFontBoldTrait)) return FALSE;
    if ((style & FontStyleItalic) && !(t & kCTFontItalicTrait)) return FALSE;
    return TRUE;
}
std::shared_ptr<detail::FontFace> FontFamily::native(INT style) const { return status_ == Ok ? make_face(name_utf8_, style) : nullptr; }
UINT16 FontFamily::GetEmHeight(INT style) const { auto f = native(style); return f ? f->metrics.em : 0; }
UINT16 FontFamily::GetCellAscent(INT style) const { auto f = native(style); return f ? f->metrics.ascent : 0; }
UINT16 FontFamily::GetCellDescent(INT style) const { auto f = native(style); return f ? f->metrics.descent : 0; }
UINT16 FontFamily::GetLineSpacing(INT style) const { auto f = native(style); return f ? f->metrics.line_spacing : 0; }
Status FontFamily::GetFamilyName(WCHAR* name, LANGID) const {
    if (!name) return InvalidParameter;
    name[0] = 0;
    if (status_ != Ok) return status_;
    auto f = native(FontStyleRegular);
    const std::string family = f ? f->family : name_utf8_;
    std::vector<WCHAR> wide(LF_FACESIZE, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, family.c_str(), -1, wide.data(), LF_FACESIZE)) return InsufficientBuffer;
    std::memcpy(name, wide.data(), LF_FACESIZE * sizeof(WCHAR));
    name[LF_FACESIZE - 1] = 0;
    return Ok;
}

void Font::init(const LOGFONT& lf) {
    style_ = (lf.lfWeight >= FW_SEMIBOLD ? FontStyleBold : 0) | (lf.lfItalic ? FontStyleItalic : 0) |
             (lf.lfUnderline ? FontStyleUnderline : 0) | (lf.lfStrikeOut ? FontStyleStrikeout : 0);
    const std::string family = resolve_face(lf.lfFaceName);
    std::vector<WCHAR> wide(256, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, family.c_str(), -1, wide.data(), 255)) { status_ = FontFamilyNotFound; return; }
    family_ = FontFamily(wide.data());
    face_ = family_.native(style_);
    if (!face_) { status_ = FontFamilyNotFound; return; }
    unit_ = UnitWorld;
    size_ = em_from_logfont(lf.lfHeight, face_->metrics);
    status_ = Ok;
}
Font::Font(HDC dc) {
    LOGFONT lf;
    if (!fourstory::gdi::selected_font(dc, &lf)) { status_ = InvalidParameter; return; }
    init(lf);
}
Font::Font(HDC, const LOGFONT* font) { if (!font) { status_ = InvalidParameter; return; } init(*font); }
Font::Font(const FontFamily* family, REAL size, INT style, Unit unit) {
    if (!family || family->GetLastStatus() != Ok || size <= 0) { status_ = InvalidParameter; return; }
    family_ = *family; style_ = style; size_ = size; unit_ = unit;
    face_ = family_.native(style);
    status_ = face_ ? Ok : FontFamilyNotFound;
}
Font::Font(const WCHAR* family, REAL size, INT style, Unit unit, const FontCollection* collection) {
    FontFamily f(family, collection);
    if (f.GetLastStatus() != Ok) { status_ = FontFamilyNotFound; return; }
    family_ = f; style_ = style; size_ = size; unit_ = unit;
    face_ = family_.native(style);
    status_ = face_ ? Ok : FontFamilyNotFound;
}
Font::~Font() {}
Status Font::GetFamily(FontFamily* family) const { if (!family) return InvalidParameter; *family = family_; return status_; }
REAL Font::native_em_pixels() const {
    switch (unit_) {
    case UnitPoint: return size_ * 96.0f / 72.0f;
    case UnitInch: return size_ * 96.0f;
    case UnitDocument: return size_ * 96.0f / 300.0f;
    case UnitMillimeter: return size_ * 96.0f / 25.4f;
    default: return size_;
    }
}
REAL Font::GetHeight(REAL) const {
    if (!face_) return 0;
    return native_em_pixels() * face_->metrics.line_spacing / face_->metrics.em;
}
REAL Font::GetHeight(const Graphics*) const { return GetHeight(96.0f); }
Status Font::GetLogFontA(const Graphics*, LOGFONT* font) const {
    if (!font || !face_) return InvalidParameter;
    std::memset(font, 0, sizeof(*font));
    font->lfHeight = -static_cast<LONG>(std::lround(native_em_pixels()));
    font->lfWeight = (style_ & FontStyleBold) ? FW_BOLD : FW_NORMAL;
    font->lfItalic = (style_ & FontStyleItalic) ? 1 : 0;
    strncpy_s(font->lfFaceName, face_->family.c_str(), _TRUNCATE);
    return Ok;
}

Graphics::Graphics(HDC dc) : dc_(dc) {
    LOGFONT lf;
    if (!fourstory::gdi::selected_font(dc, &lf)) status_ = InvalidParameter;
}
Graphics::~Graphics() {}
Status Graphics::MeasureString(const WCHAR* text, INT length, const Font* font, const PointF& origin, RectF* box) const {
    return MeasureString(text, length, font, origin, nullptr, box);
}
Status Graphics::MeasureString(const WCHAR* text, INT length, const Font* font, const PointF& origin, const StringFormat* format, RectF* box) const {
    if (!text || !font || !box || font->GetLastStatus() != Ok) return InvalidParameter;
    std::u16string s;
    if (length < 0) { for (int i = 0; text[i]; ++i) s += text[i]; } else s.assign(text, text + length);
    *box = measure(s, font, origin, format ? format->GetFormatFlags() : 0);
    return Ok;
}
Status Graphics::MeasureString(const WCHAR* text, INT length, const Font* font, const RectF& layout, const StringFormat* format, RectF* box,
                               INT* code_points, INT* lines) const {
    Status s = MeasureString(text, length, font, PointF(layout.X, layout.Y), format, box);
    // Word wrap is NOT IMPLEMENTED: text wider than the layout fails instead of
    // returning the size of an unwrapped line.
    if (s == Ok && layout.Width > 0 && box->Width > layout.Width && !(format && (format->GetFormatFlags() & StringFormatFlagsNoWrap))) return NotImplemented;
    if (code_points) { int n = 0; if (text) while (length < 0 ? text[n] != 0 : n < length) ++n; *code_points = n; }
    if (lines) *lines = 1;
    return s;
}
Status Graphics::MeasureString(const wchar_t* text, INT length, const Font* font, const PointF& origin, RectF* box) const {
    std::u16string w = from_wchar(text);
    if (length >= 0 && static_cast<size_t>(length) < w.size()) w.resize(static_cast<size_t>(length));
    return MeasureString(reinterpret_cast<const WCHAR*>(w.c_str()), -1, font, origin, nullptr, box);
}
Status Graphics::MeasureString(const wchar_t* text, INT length, const Font* font, const PointF& origin, const StringFormat* format, RectF* box) const {
    std::u16string w = from_wchar(text);
    if (length >= 0 && static_cast<size_t>(length) < w.size()) w.resize(static_cast<size_t>(length));
    return MeasureString(reinterpret_cast<const WCHAR*>(w.c_str()), -1, font, origin, format, box);
}

Status Graphics::DrawString(const WCHAR* text, INT length, const Font* font, const RectF& layout, const StringFormat* format, const Brush* brush) {
    if (!text || !font || !brush || font->GetLastStatus() != Ok) return InvalidParameter;
    BYTE* bits = nullptr; int width = 0, height = 0;
    if (!fourstory::gdi::selected_bitmap(dc_, &bits, &width, &height)) return NotImplemented; // window DCs have no pixels
    std::u16string s;
    if (length < 0) { for (int i = 0; text[i]; ++i) s += text[i]; } else s.assign(text, text + length);
    const FontFace& face = *font->native();
    const float em = font->native_em_pixels();
    std::vector<Line> lines = layout_lines(s, face, em);
    const Metrics& m = face.metrics;
    const double line_height = em * double(m.line_spacing) / m.em;
    const double ascent = em * double(m.ascent) / m.em;
    const double text_height = lines.size() <= 1 ? em * double(m.ascent + m.descent) / m.em : lines.size() * line_height;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(bits, static_cast<size_t>(width), static_cast<size_t>(height), 8, static_cast<size_t>(width) * 4,
                                             space, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (!ctx) { release_lines(lines); return OutOfMemory; }
    // Device coordinates: GDI+ y grows downward; Core Graphics y grows upward.
    CGContextTranslateCTM(ctx, 0, height);
    CGContextScaleCTM(ctx, 1, -1);
    if (!(format && (format->GetFormatFlags() & StringFormatFlagsNoClip)) && layout.Width > 0 && layout.Height > 0)
        CGContextClipToRect(ctx, CGRectMake(layout.X, layout.Y, layout.Width, layout.Height));
    const bool smooth = hint_ != TextRenderingHintSingleBitPerPixel && hint_ != TextRenderingHintSingleBitPerPixelGridFit;
    CGContextSetShouldAntialias(ctx, smooth);
    CGContextSetShouldSmoothFonts(ctx, false);
    CGContextSetAllowsFontSubpixelPositioning(ctx, true);
    CGContextSetShouldSubpixelPositionFonts(ctx, true);
    const ARGB argb = brush->native_color();
    CGContextSetRGBFillColor(ctx, ((argb >> 16) & 0xFF) / 255.0, ((argb >> 8) & 0xFF) / 255.0, (argb & 0xFF) / 255.0, ((argb >> 24) & 0xFF) / 255.0);
    double y0 = layout.Y;
    const StringAlignment valign = format ? format->GetLineAlignment() : StringAlignmentNear;
    if (valign == StringAlignmentCenter) y0 += (layout.Height - text_height) / 2;
    else if (valign == StringAlignmentFar) y0 += layout.Height - text_height;
    const StringAlignment align = format ? format->GetAlignment() : StringAlignmentNear;
    for (size_t li = 0; li < lines.size(); ++li) {
        const Line& line = lines[li];
        const double visible = line.width - line.trailing;
        double x = layout.X + em / 6.0;
        if (align == StringAlignmentCenter) x = layout.X + (layout.Width - visible) / 2.0;
        else if (align == StringAlignmentFar) x = layout.X + layout.Width - visible - em / 6.0;
        // Windows places the baseline on a whole pixel (measured).
        const double baseline = std::floor(y0 + ascent + li * line_height + 0.5);
        CGContextSaveGState(ctx);
        // Glyphs are drawn upright in the flipped context.
        CGContextTranslateCTM(ctx, 0, baseline);
        CGContextScaleCTM(ctx, 1, -1);
        for (const Glyph& g : line.glyphs) {
            CGPoint p = CGPointMake(x, 0);
            CTFontDrawGlyphs(g.font, &g.glyph, &p, 1, ctx);
            x += g.advance;
        }
        CGContextRestoreGState(ctx);
    }
    CGContextRelease(ctx);
    release_lines(lines);
    return Ok;
}
Status Graphics::DrawString(const WCHAR* text, INT length, const Font* font, const PointF& origin, const Brush* brush) {
    return DrawString(text, length, font, origin, nullptr, brush);
}
Status Graphics::DrawString(const WCHAR* text, INT length, const Font* font, const PointF& origin, const StringFormat* format, const Brush* brush) {
    StringFormat f(format ? format->GetFormatFlags() | StringFormatFlagsNoClip : StringFormatFlagsNoClip);
    return DrawString(text, length, font, RectF(origin.X, origin.Y, 0, 0), &f, brush);
}
Status Graphics::DrawString(const wchar_t* text, INT length, const Font* font, const RectF& layout, const StringFormat* format, const Brush* brush) {
    std::u16string w = from_wchar(text);
    if (length >= 0 && static_cast<size_t>(length) < w.size()) w.resize(static_cast<size_t>(length));
    return DrawString(reinterpret_cast<const WCHAR*>(w.c_str()), -1, font, layout, format, brush);
}
Status Graphics::Clear(const Color& color) {
    BYTE* bits = nullptr; int width = 0, height = 0;
    if (!fourstory::gdi::selected_bitmap(dc_, &bits, &width, &height)) return NotImplemented;
    const ARGB c = color.GetValue();
    const BYTE a = BYTE(c >> 24);
    const BYTE px[4] = {BYTE((c & 0xFF) * a / 255), BYTE(((c >> 8) & 0xFF) * a / 255), BYTE(((c >> 16) & 0xFF) * a / 255), a};
    for (int i = 0; i < width * height; ++i) std::memcpy(bits + i * 4, px, 4);
    return Ok;
}
}  // namespace Gdiplus

namespace fourstory { namespace text {
int register_font_file(const char* path) {
    CFStringRef s = cf_string(path ? path : "");
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, s, kCFURLPOSIXPathStyle, false);
    CFRelease(s);
    if (!url) return 0;
    CFArrayRef descriptors = CTFontManagerCreateFontDescriptorsFromURL(url);
    const int count = descriptors ? static_cast<int>(CFArrayGetCount(descriptors)) : 0;
    if (descriptors) CFRelease(descriptors);
    CFErrorRef error = nullptr;
    const bool ok = count > 0 && (CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, &error) ||
                                  (error && CFErrorGetCode(error) == kCTFontManagerErrorAlreadyRegistered));
    if (error) CFRelease(error);
    CFRelease(url);
    return ok ? count : 0;
}
bool unregister_font_file(const char* path) {
    CFStringRef s = cf_string(path ? path : "");
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, s, kCFURLPOSIXPathStyle, false);
    CFRelease(s);
    if (!url) return false;
    const bool ok = CTFontManagerUnregisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr);
    CFRelease(url);
    return ok;
}
std::string resolved_family(const LOGFONT& font) { return resolve_face(font.lfFaceName); }
}}
