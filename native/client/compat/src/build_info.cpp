// Development build label; see fourstory_build_info.h.
#include "fourstory_build_info.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>

// Channel, version and UTC build time come from build.py (build_metadata.py).
// A unit compiled without them (unit-test libraries) says so in the label.
#if defined(FOURSTORY_BUILD_CHANNEL) && defined(FOURSTORY_BUILD_VERSION) && defined(FOURSTORY_BUILD_UTC)
#define FOURSTORY_BUILD_PREFIX FOURSTORY_BUILD_CHANNEL FOURSTORY_BUILD_VERSION
#define FOURSTORY_BUILD_TIME FOURSTORY_BUILD_UTC
// Read back from the executable by tools/release/build_release.py --skip-build.
extern "C" __attribute__((used)) const char fourstory_build_stamp[] =
    "FOURSTORY_BUILD_STAMP=" FOURSTORY_BUILD_CHANNEL "|" FOURSTORY_BUILD_VERSION "|" FOURSTORY_BUILD_UTC;
#else
#define FOURSTORY_BUILD_PREFIX "unstamped"
#define FOURSTORY_BUILD_TIME "unknown"
#endif

namespace {

// First 12 lowercase hex digits of the SHA-256 of the running executable file,
// read in chunks; "hashunavailable" when the file cannot be read.
void executable_sha256_prefix(char (&out)[16]) {
    std::snprintf(out, sizeof out, "hashunavailable");
    char path[4096];
    uint32_t size = sizeof path;
    if (_NSGetExecutablePath(path, &size) != 0) return;
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    static unsigned char buffer[1 << 20];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof buffer)) != 0) {
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return;
        }
        CC_SHA256_Update(&context, buffer, static_cast<CC_LONG>(n));
    }
    close(fd);
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &context);
    for (int i = 0; i < 6; ++i) std::snprintf(out + 2 * i, 3, "%02x", digest[i]);
}

}  // namespace

const char *fourstory_build_label() {
    static char label[128];
    if (!label[0]) {
        char hash[16];
        executable_sha256_prefix(hash);
        std::snprintf(label, sizeof label, "%s-%s.%s", FOURSTORY_BUILD_PREFIX, hash, FOURSTORY_BUILD_TIME);
    }
    return label;
}

// ---- Overlay ---------------------------------------------------------------
// Objective-C is reached through the runtime functions (dlsym) because every
// unit gets the Windows prefix header, whose BOOL clashes with <objc/objc.h>,
// and the link has no AppKit/QuartzCore; SDL and MoltenVK load both.
namespace {

struct Runtime {
    void *(*get_class)(const char *) = nullptr;
    void *(*sel)(const char *) = nullptr;
    void *send = nullptr;
    bool ok = false;
    Runtime() {
        get_class = reinterpret_cast<void *(*)(const char *)>(dlsym(RTLD_DEFAULT, "objc_getClass"));
        sel = reinterpret_cast<void *(*)(const char *)>(dlsym(RTLD_DEFAULT, "sel_registerName"));
        send = dlsym(RTLD_DEFAULT, "objc_msgSend");
        ok = get_class && sel && send && get_class("CATextLayer") && get_class("CATransaction");
    }
    // objc_msgSend cast to the exact signature of the selector (arm64 ABI).
    template <typename R, typename... A> R call(void *receiver, const char *name, A... args) const {
        return reinterpret_cast<R (*)(void *, void *, A...)>(send)(receiver, sel(name), args...);
    }
};

const Runtime &runtime() {
    static Runtime r;
    return r;
}

struct Overlay {
    bool attempted = false;  // one creation attempt and one log line per window
    void *layer = nullptr;   // retained CATextLayer
    CGSize size = {-1, -1};
    CGFloat scale = -1;
};
std::map<unsigned, Overlay> g_overlays;  // SDL window ids are never reused

// Menlo 11 pt advances 6.62 pt per character; the layer is sized to the label.
const CGFloat kHeight = 16, kMargin = 6;
CGFloat label_width() { return CGFloat(std::strlen(fourstory_build_label())) * 6.7 + 8; }

void *create_layer(const Runtime &r, void *window) {
    void *view = r.call<void *>(window, "contentView");
    if (!view) return nullptr;
    r.call<void, bool>(view, "setWantsLayer:", true);
    void *host = r.call<void *>(view, "layer");
    if (!host) return nullptr;
    void *text = r.call<void *>(r.call<void *>(r.get_class("CATextLayer"), "alloc"), "init");
    if (!text) return nullptr;
    CFStringRef label = CFStringCreateWithCString(nullptr, fourstory_build_label(), kCFStringEncodingUTF8);
    CGColorRef pink = CGColorCreateSRGB(1.0, 0.41, 0.71, 1.0);  // hot pink #FF69B4
    r.call<void, const void *>(text, "setString:", label);
    r.call<void, const void *>(text, "setFont:", CFSTR("Menlo-Regular"));
    r.call<void, CGFloat>(text, "setFontSize:", 11.0);
    r.call<void, CGColorRef>(text, "setForegroundColor:", pink);
    r.call<void, const void *>(text, "setAlignmentMode:", CFSTR("right"));  // kCAAlignmentRight
    r.call<void, CGFloat>(text, "setContentsScale:", r.call<CGFloat>(window, "backingScaleFactor"));
    r.call<void, float>(text, "setShadowOpacity:", 1.0f);  // black shadow keeps it readable on bright ground
    r.call<void, CGFloat>(text, "setShadowRadius:", 1.0);
    r.call<void, CGSize>(text, "setShadowOffset:", CGSizeMake(0, 0));
    r.call<void, CGFloat>(text, "setZPosition:", 1000.0);  // above the Metal view's layer
    r.call<void, void *>(host, "addSublayer:", text);
    CFRelease(label);
    CGColorRelease(pink);
    return text;
}

}  // namespace

bool fourstory_build_overlay_update(unsigned key, void *window) {
    Overlay &o = g_overlays[key];
    const Runtime &r = runtime();
    if (!o.attempted) {
        o.attempted = true;
        void *view = r.ok && window ? r.call<void *>(window, "contentView") : nullptr;
        const CGRect bounds = view ? r.call<CGRect>(view, "bounds") : CGRectZero;
        if (view) {
            void *cls = r.get_class("CATransaction");
            r.call<void>(cls, "begin");
            r.call<void, bool>(cls, "setDisableActions:", true);
            o.layer = create_layer(r, window);
            r.call<void>(cls, "commit");
        }
        // The run log then records which executable ran.
        std::fprintf(stderr, "BUILD_LABEL %s %s (%.0fx%.0f)\n", fourstory_build_label(),
                     o.layer ? "shown" : "NOT shown", bounds.size.width, bounds.size.height);
    }
    if (!o.layer) return false;
    void *view = r.call<void *>(window, "contentView");
    if (!view) return false;
    const CGRect bounds = r.call<CGRect>(view, "bounds");
    const CGFloat scale = r.call<CGFloat>(window, "backingScaleFactor");
    if (bounds.size.width == o.size.width && bounds.size.height == o.size.height && scale == o.scale) return true;
    void *cls = r.get_class("CATransaction");
    r.call<void>(cls, "begin");
    r.call<void, bool>(cls, "setDisableActions:", true);  // no implicit move animation
    void *host = r.call<void *>(view, "layer");
    const bool flipped = host && r.call<bool>(host, "isGeometryFlipped");
    const CGFloat width = label_width();
    const CGFloat x = bounds.size.width - width - kMargin;
    const CGFloat y = flipped ? bounds.size.height - kHeight - 2 : 2;
    r.call<void, CGRect>(o.layer, "setFrame:", CGRectMake(x, y, width, kHeight));
    r.call<void, CGFloat>(o.layer, "setContentsScale:", scale);
    r.call<void>(cls, "commit");
    o.size = bounds.size;
    o.scale = scale;
    return true;
}

void fourstory_build_overlay_detach(unsigned key) {
    auto it = g_overlays.find(key);
    if (it == g_overlays.end()) return;
    if (it->second.layer) {
        const Runtime &r = runtime();
        r.call<void>(it->second.layer, "removeFromSuperlayer");
        r.call<void>(it->second.layer, "release");
    }
    g_overlays.erase(it);
}
