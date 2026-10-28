// Release-bundle startup (C8). When the client runs from a packaged
// "4Story (Beta).app" whose Contents/Resources/Game holds the game data, it
// prepares the macOS user folders itself instead of relying on run.py:
//
//   ~/Library/Application Support/4Story/Settings.plist   server and options
//   ~/Library/Application Support/4Story/Runtime/         working directory
//   ~/Library/Logs/4Story/                                 logs
//   ~/Library/Caches/4Story/                               graphics and game cache
//
// The app bundle stays read-only: Runtime/ holds a symbolic link to every
// entry of Resources/Game, so the game reads its data through the links and
// writes new files into user folders. Development runs (no Resources/Game
// next to the executable) are unchanged.
#include <fourstory_bundle.h>

#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace {

std::string g_settings_path;   // set in bundle mode only

std::string executable_dir() {
    char path[PATH_MAX]; uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return std::string();
    char real[PATH_MAX];
    std::string p = realpath(path, real) ? real : path;
    return p.substr(0, p.find_last_of('/'));
}

bool is_dir(const std::string& p) { struct stat s; return stat(p.c_str(), &s) == 0 && S_ISDIR(s.st_mode); }

bool make_dirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '/') {
            std::string p = path.substr(0, i);
            if (mkdir(p.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    return is_dir(path);
}

std::string home_dir() {
    if (const char* h = std::getenv("HOME")) if (*h) return h;
    if (passwd* pw = getpwuid(getuid())) if (pw->pw_dir && *pw->pw_dir) return pw->pw_dir;
    return std::string();
}

// Points `link` at `target`, replacing a stale link. A real file or folder
// with the same name is left alone and reported.
void ensure_link(const std::string& target, const std::string& link) {
    char current[PATH_MAX];
    ssize_t n = readlink(link.c_str(), current, sizeof(current) - 1);
    if (n >= 0) {
        current[n] = 0;
        if (target == current) return;
        unlink(link.c_str());
    } else {
        struct stat s;
        if (lstat(link.c_str(), &s) == 0) {
            std::fprintf(stderr, "4Story: kept existing %s (not a link)\n", link.c_str());
            return;
        }
    }
    if (symlink(target.c_str(), link.c_str()) != 0)
        std::fprintf(stderr, "4Story: cannot link %s: %s\n", link.c_str(), std::strerror(errno));
}

// Removes links in `folder` whose names are not in `keep` (entries removed
// from the game data by an update). Real files are never removed.
void remove_stale_links(const std::string& folder, const std::set<std::string>& keep) {
    DIR* d = opendir(folder.c_str());
    if (!d) return;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] == '.' || keep.count(e->d_name)) continue;
        const std::string path = folder + "/" + e->d_name;
        struct stat s;
        if (lstat(path.c_str(), &s) == 0 && S_ISLNK(s.st_mode)) unlink(path.c_str());
    }
    closedir(d);
}

bool copy_file(const std::string& from, const std::string& to) {
    const std::string temporary = to + ".partial";
    FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) return false;
    FILE* out = std::fopen(temporary.c_str(), "wb");
    bool ok = out != nullptr;
    char buffer[65536];
    size_t n;
    while (ok && (n = std::fread(buffer, 1, sizeof(buffer), in)) > 0) ok = std::fwrite(buffer, 1, n, out) == n;
    ok = ok && !std::ferror(in);
    std::fclose(in);
    if (out && std::fclose(out) != 0) ok = false;
    if (ok && std::rename(temporary.c_str(), to.c_str()) == 0) return true;
    unlink(temporary.c_str());
    return false;
}

CFStringRef cf_key(const char* key) { return CFStringCreateWithCString(nullptr, key, kCFStringEncodingUTF8); }

CFTypeRef cf_value(CFDictionaryRef d, const char* key) {
    if (!d) return nullptr;
    CFStringRef k = cf_key(key);
    CFTypeRef v = CFDictionaryGetValue(d, k);
    CFRelease(k);
    return v;
}

std::string cf_string(CFDictionaryRef d, const char* key, const std::string& fallback) {
    CFTypeRef v = cf_value(d, key);
    if (!v || CFGetTypeID(v) != CFStringGetTypeID()) return fallback;
    char buffer[1024];
    if (!CFStringGetCString(static_cast<CFStringRef>(v), buffer, sizeof(buffer), kCFStringEncodingUTF8)) return fallback;
    return buffer;
}

long cf_number(CFDictionaryRef d, const char* key, long fallback) {
    CFTypeRef v = cf_value(d, key);
    long out = fallback;
    if (v && CFGetTypeID(v) == CFNumberGetTypeID() &&
        !CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberLongType, &out)) out = fallback;
    return out;
}

// Accepts <true/>/<false/>, 1/0 and the strings true/false/yes/no/on/off.
bool cf_bool(CFDictionaryRef d, const char* key, bool fallback) {
    CFTypeRef v = cf_value(d, key);
    if (!v) return fallback;
    if (CFGetTypeID(v) == CFBooleanGetTypeID()) return CFBooleanGetValue(static_cast<CFBooleanRef>(v));
    if (CFGetTypeID(v) == CFNumberGetTypeID()) { long n = 0; CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberLongType, &n); return n != 0; }
    const std::string s = cf_string(d, key, "");
    if (s == "true" || s == "yes" || s == "on" || s == "1") return true;
    if (s == "false" || s == "no" || s == "off" || s == "0") return false;
    return fallback;
}

CFDictionaryRef read_plist(const std::string& path) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(path.c_str()), path.size(), false);
    CFReadStreamRef stream = CFReadStreamCreateWithFile(nullptr, url);
    CFRelease(url);
    CFPropertyListRef plist = nullptr;
    if (stream && CFReadStreamOpen(stream)) {
        plist = CFPropertyListCreateWithStream(nullptr, stream, 0, kCFPropertyListImmutable, nullptr, nullptr);
        CFReadStreamClose(stream);
    }
    if (stream) CFRelease(stream);
    if (plist && CFGetTypeID(plist) != CFDictionaryGetTypeID()) { CFRelease(plist); plist = nullptr; }
    return static_cast<CFDictionaryRef>(plist);
}

bool write_atomic(const std::string& path, const void* bytes, size_t size) {
    const std::string temporary = path + ".new";
    int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    bool ok = write(fd, bytes, size) == static_cast<ssize_t>(size) && fsync(fd) == 0;
    ok = close(fd) == 0 && ok;
    if (ok && rename(temporary.c_str(), path.c_str()) == 0) return true;
    unlink(temporary.c_str());
    return false;
}

// Written when no settings file exists. Comments are not possible in a
// plist, so the key names say what they do; the README explains them.
void write_default_settings(const std::string& path) {
    struct stat s;
    if (stat(path.c_str(), &s) == 0 && s.st_size > 0) return;
    static const char text[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>ServerAddress</key>\n\t<string></string>\n"
        "\t<key>ServerPort</key>\n\t<integer>4815</integer>\n"
        "\t<key>Channel</key>\n\t<integer>1</integer>\n"
        "\t<key>WindowedMode</key>\n\t<false/>\n"
        "\t<key>WriteLogFile</key>\n\t<true/>\n"
        "</dict>\n"
        "</plist>\n";
    if (!write_atomic(path, text, sizeof(text) - 1))
        std::fprintf(stderr, "4Story: cannot write %s\n", path.c_str());
}

void redirect_output(const std::string& logs) {
    const std::string log = logs + "/4Story.log";
    rename(log.c_str(), (logs + "/4Story.previous.log").c_str());
    int fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

// Native alert before any game window exists. Returns true for the default
// button (View README), false for Quit.
bool alert(const char* title, const std::string& text, bool readme_button) {
    CFStringRef t = CFStringCreateWithCString(nullptr, title, kCFStringEncodingUTF8);
    CFStringRef m = CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8);
    CFOptionFlags response = 0;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, nullptr, nullptr, nullptr, t, m,
                                   readme_button ? CFSTR("View README") : CFSTR("Quit"),
                                   readme_button ? CFSTR("Quit") : nullptr, nullptr, &response);
    CFRelease(t);
    CFRelease(m);
    return readme_button && response == kCFUserNotificationDefaultResponse;
}

void open_file(const std::string& path) {
    const std::string command = "/usr/bin/open '" + path + "'";
    if (path.find('\'') == std::string::npos) std::system(command.c_str());
}

bool loopback_address(const std::string& a) {
    return a.compare(0, 4, "127.") == 0 || a == "localhost" || a == "::1";
}

bool valid_address(const std::string& a) {
    if (a.empty() || a.size() > 253 || loopback_address(a)) return false;
    for (char c : a)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == ':')) return false;
    return true;
}

} // namespace

namespace fourstory {

BundleResult prepare_bundle(std::string& command_line) {
    const std::string macos = executable_dir();
    if (macos.empty()) return BundleResult::NotBundle;
    const std::string contents = macos.substr(0, macos.find_last_of('/'));
    const std::string resources = contents + "/Resources";
    const std::string game = resources + "/Game";
    if (!is_dir(game)) return BundleResult::NotBundle;
    const std::string readme = resources + "/README.md";

    const std::string home = home_dir();
    const std::string support = home + "/Library/Application Support/4Story";
    const std::string runtime = support + "/Runtime";
    const std::string logs = home + "/Library/Logs/4Story";
    const std::string caches = home + "/Library/Caches/4Story";
    if (home.empty() || !make_dirs(runtime) || !make_dirs(logs) || !make_dirs(caches)) {
        alert("4Story cannot start", "4Story could not create its folders in your Library folder (" + home +
              "/Library). Check that your home folder is writable.", false);
        return BundleResult::Failed;
    }

    g_settings_path = support + "/Settings.plist";
    write_default_settings(g_settings_path);
    CFDictionaryRef settings = read_plist(g_settings_path);

    if (cf_bool(settings, "WriteLogFile", true) && !isatty(STDERR_FILENO)) redirect_output(logs);
    std::fprintf(stderr, "4Story: settings %s\n", g_settings_path.c_str());

    // Server: "<address> <port> <channel>", as TClient.exe receives it.
    if (command_line.empty()) {
        const std::string address = cf_string(settings, "ServerAddress", "");
        const long port = cf_number(settings, "ServerPort", 4815);
        const long channel = cf_number(settings, "Channel", 1);
        if (!valid_address(address) || port < 1 || port > 65535 || channel < 1 || channel > 255) {
            if (settings) CFRelease(settings);
            const std::string what = address.empty()
                ? "The server address is not set yet."
                : loopback_address(address)
                ? "The server address is " + address + ". The server does not accept this address, even on its own Mac."
                : "The server settings are not valid (address, port or channel).";
            if (alert("Set your server address", what + "\n\nOpen this file and enter the server Mac's IP address:\n" +
                      g_settings_path + "\n\nThen open 4Story again.", true)) {
                open_file(readme);
            }
            open_file(g_settings_path);
            return BundleResult::Failed;
        }
        command_line = address + " " + std::to_string(port) + " " + std::to_string(channel);
    }
    std::fprintf(stderr, "4Story: server %s\n", command_line.c_str());

    // Working directory: links to the read-only game data, plus the game's
    // own log files redirected to the Logs folder. Data/Cache is written
    // during play (custom cloaks, TClientCustomCloak.cpp), so Data/ is a real
    // folder of links and Cache/ lives in ~/Library/Caches.
    static const char* const log_files[] = {"TClient-graphics.log", "TClient-ui.log", "TClient-fonts.log",
                                             "log.txt", "npc_error.txt", "LOL.txt"};
    std::set<std::string> top, inside;
    if (DIR* d = opendir(game.c_str())) {
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.') continue;
            top.insert(e->d_name);
            if (std::strcmp(e->d_name, "Data") != 0) ensure_link(game + "/" + e->d_name, runtime + "/" + e->d_name);
        }
        closedir(d);
    }
    for (const char* name : log_files) { top.insert(name); ensure_link(logs + "/" + name, runtime + "/" + name); }
    remove_stale_links(runtime, top);

    const std::string data = runtime + "/Data";
    struct stat ds;
    if (lstat(data.c_str(), &ds) == 0 && S_ISLNK(ds.st_mode)) unlink(data.c_str());
    if (!make_dirs(data)) std::fprintf(stderr, "4Story: cannot create %s\n", data.c_str());
    if (DIR* d = opendir((game + "/Data").c_str())) {
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.' || std::strcmp(e->d_name, "Cache") == 0) continue;
            inside.insert(e->d_name);
            ensure_link(game + "/Data/" + e->d_name, data + "/" + e->d_name);
        }
        closedir(d);
    }
    inside.insert("Cache");
    remove_stale_links(data, inside);
    const std::string cache = caches + "/GameCache";
    if (!make_dirs(cache)) std::fprintf(stderr, "4Story: cannot create %s\n", cache.c_str());
    if (DIR* d = opendir((game + "/Data/Cache").c_str())) {
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.') continue;
            const std::string to = cache + "/" + e->d_name;
            struct stat cs;
            if (stat(to.c_str(), &cs) == 0) continue;
            if (!copy_file(game + "/Data/Cache/" + e->d_name, to))
                std::fprintf(stderr, "4Story: cannot copy cache file %s\n", e->d_name);
        }
        closedir(d);
    }
    ensure_link(cache, data + "/Cache");
    if (chdir(runtime.c_str()) != 0) {
        if (settings) CFRelease(settings);
        alert("4Story cannot start", "4Story could not open its working folder:\n" + runtime, false);
        return BundleResult::Failed;
    }

    // Graphics: DXVK and SDL open the Vulkan loader by name (dlopen of
    // "libvulkan.1.dylib"), which only finds an image that is already loaded.
    // Load the bundled loader by its full path first.
    const std::string vulkan = contents + "/Frameworks/libvulkan.1.dylib";
    if (!dlopen(vulkan.c_str(), RTLD_NOW | RTLD_GLOBAL))
        std::fprintf(stderr, "4Story: cannot load %s: %s\n", vulkan.c_str(), dlerror());
    setenv("SDL_VULKAN_LIBRARY", vulkan.c_str(), 1);
    setenv("VK_DRIVER_FILES", (resources + "/vulkan/icd.d/MoltenVK_icd.json").c_str(), 1);
    setenv("DXVK_WSI_DRIVER", "SDL3", 1);
    setenv("DXVK_STATE_CACHE_PATH", caches.c_str(), 1);
    setenv("DXVK_LOG_PATH", logs.c_str(), 1);
    setenv("DXVK_CONFIG_FILE", (resources + "/dxvk.conf").c_str(), 1);
    unsetenv("DXVK_CONFIG");

    // Window mode: Settings.plist decides the start mode (sdl_backend.cpp);
    // later changes in the game are written back (save_window_mode).
    if (cf_bool(settings, "WindowedMode", false)) unsetenv("FOURSTORY_START_FULLSCREEN");
    else setenv("FOURSTORY_START_FULLSCREEN", "1", 1);

    if (settings) CFRelease(settings);
    return BundleResult::Ready;
}

} // namespace fourstory

// Called by the window shell when the window enters or leaves fullscreen
// (in-game option or the green window button). Keeps Settings.plist's
// WindowedMode in step with the game. Development runs have no settings path.
extern "C" void fourstory_window_mode_saved(int fullscreen) {
    if (g_settings_path.empty()) return;
    CFDictionaryRef current = read_plist(g_settings_path);
    CFMutableDictionaryRef d = current ? CFDictionaryCreateMutableCopy(nullptr, 0, current)
                                       : CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (current) CFRelease(current);
    CFDictionarySetValue(d, CFSTR("WindowedMode"), fullscreen ? kCFBooleanFalse : kCFBooleanTrue);
    CFDataRef bytes = CFPropertyListCreateData(nullptr, d, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
    CFRelease(d);
    if (!bytes) return;
    if (!write_atomic(g_settings_path, CFDataGetBytePtr(bytes), CFDataGetLength(bytes)))
        std::fprintf(stderr, "4Story: cannot save WindowedMode to %s\n", g_settings_path.c_str());
    CFRelease(bytes);
}
