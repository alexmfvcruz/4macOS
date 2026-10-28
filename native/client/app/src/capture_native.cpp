// C4 frame capture (test instrumentation). Called before Present at the
// three original Present sites (TachyonRes.cpp loading screen, TachyonWnd.cpp
// main loop). Without FOURSTORY_C4_CAPTURE_DIR it returns at once.
//
// With FOURSTORY_C4_CAPTURE_DIR the back buffer is read back
// (GetRenderTargetData into a system-memory surface) at most every
// FOURSTORY_C4_CAPTURE_EVERY_MS milliseconds (default 1000) and written as
// binary PPM (RGB, top row first) <dir>/<kind>-<index>-<ms since start>.ppm,
// as the C3 readbacks. Only X8R8G8B8 and A8R8G8B8 back buffers are written;
// other formats and failed calls are reported on stderr and skipped.
#include <fourstory_engine_ui.h>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// C6 per-category sample counters (test instrumentation). With
// FOURSTORY_C6_PASS_LOG set, each fourstory_c6_pass call ends the running
// D3DQUERYTYPE_OCCLUSION query and begins one for the new category. Before
// Present of a main-loop frame the results are read (blocking) and, at most
// every FOURSTORY_C6_PASS_EVERY_MS milliseconds (default 1000), one line
// "C6_PASS t=<ms since the first counted frame> <category>=<samples>/<switches>
// ... <note>=<value>" goes to
// stderr. Samples are the pixels that passed the depth test when the category
// drew; later overdraw is not subtracted. Attribution is approximate: the
// per-object hook in RenderTOBJ also fires for draws outside the main object
// pass, and a category stays active until the next hook. Pooled queries live
// for the session (test runs do not reset the device). Main game thread only.
namespace {
const char* const c6_names[FOURSTORY_C6_COUNT] = {"sky", "bsp", "terrain", "bspobj", "object", "pc", "mon",
                                                  "npc", "decal", "billboard", "water", "lava", "sfx", "ui", "minimap"};
struct C6State {
    bool on = std::getenv("FOURSTORY_C6_PASS_LOG") != nullptr;
    long every = std::getenv("FOURSTORY_C6_PASS_EVERY_MS") ? std::atol(std::getenv("FOURSTORY_C6_PASS_EVERY_MS")) : 1000;
    std::vector<IDirect3DQuery9*> pool;
    std::vector<std::pair<int, IDirect3DQuery9*>> used;
    std::vector<std::pair<std::string, long>> notes;
    int active = FOURSTORY_C6_NONE;
    int object_base = FOURSTORY_C6_OBJECT;
    bool failed = false;
};
C6State& c6() { static C6State s; return s; }

void c6_end_active(C6State& s) {
    if (s.active != FOURSTORY_C6_NONE && !s.used.empty()) s.used.back().second->Issue(D3DISSUE_END);
    s.active = FOURSTORY_C6_NONE;
}

void c6_frame_end(LPDIRECT3DDEVICE9 device) {
    C6State& s = c6();
    if (!s.on) return;
    if (s.used.empty()) { s.notes.clear(); return; }
    c6_end_active(s);
    unsigned long long samples[FOURSTORY_C6_COUNT] = {};
    unsigned switches[FOURSTORY_C6_COUNT] = {};
    unsigned pending = 0;
    // One time budget for the whole frame; queries without a result by then
    // count as pending and are released, not reused while outstanding.
    const auto budget = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    for (auto& [category, query] : s.used) {
        DWORD value = 0;
        HRESULT hr = query->GetData(&value, sizeof(value), D3DGETDATA_FLUSH);
        while (hr == S_FALSE && std::chrono::steady_clock::now() < budget) hr = query->GetData(&value, sizeof(value), D3DGETDATA_FLUSH);
        ++switches[category];
        if (hr == S_OK) {
            samples[category] += value;
            s.pool.push_back(query);
        } else {
            ++pending;
            query->Release();
        }
    }
    s.used.clear();
    static const auto start = std::chrono::steady_clock::now();
    static long last = -1000000;
    const long now = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    if (now - last >= s.every) {
        last = now;
        std::string line = "C6_PASS t=" + std::to_string(now);
        for (int i = 0; i < FOURSTORY_C6_COUNT; ++i)
            line += std::string(" ") + c6_names[i] + "=" + std::to_string(samples[i]) + "/" + std::to_string(switches[i]);
        for (auto& [key, value] : s.notes) line += " " + key + "=" + std::to_string(value);
        if (pending) line += " pending=" + std::to_string(pending);
        std::fprintf(stderr, "%s\n", line.c_str());
    }
    s.notes.clear();
    (void)device;
}
}  // namespace

void fourstory_c6_pass(LPDIRECT3DDEVICE9 device, int category) {
    C6State& s = c6();
    if (!s.on || s.failed || !device || category == s.active) return;
    if (category == FOURSTORY_C6_OBJECT || category == FOURSTORY_C6_BSPOBJ) s.object_base = category;
    c6_end_active(s);
    if (category < 0 || category >= FOURSTORY_C6_COUNT) return;
    IDirect3DQuery9* query = nullptr;
    if (!s.pool.empty()) {
        query = s.pool.back();
        s.pool.pop_back();
    } else if (FAILED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query))) {
        std::fprintf(stderr, "C6_PASS occlusion query unavailable; counters off\n");
        s.failed = true;
        return;
    }
    query->Issue(D3DISSUE_BEGIN);
    s.used.emplace_back(category, query);
    s.active = category;
}

void fourstory_c6_note(const char* key, long value) {
    C6State& s = c6();
    if (s.on && !s.failed) s.notes.emplace_back(key, value);
}

// OBJ_TYPE (Lib/Own/TProtocol/include/NetCode.h): OT_PC 1, OT_MON 2, OT_NPC 3,
// OT_RECALL 7, OT_SELF 11. Other types count as map objects of the current
// object pass (object or bspobj).
int fourstory_c6_object_category(int type) {
    switch (type) {
    case 1: case 11: return FOURSTORY_C6_PC;
    case 2: case 7: return FOURSTORY_C6_MON;
    case 3: return FOURSTORY_C6_NPC;
    default: return c6().object_base;
    }
}

void fourstory_capture_frame(LPDIRECT3DDEVICE9 device, const char* kind) {
    if (std::string(kind) == "frame") c6_frame_end(device);
    static const char* dir = std::getenv("FOURSTORY_C4_CAPTURE_DIR");
    if (!dir || !device) return;
    // The loading screen presents from the load thread, the main loop from the GUI thread.
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    static const long every = std::getenv("FOURSTORY_C4_CAPTURE_EVERY_MS") ? std::atol(std::getenv("FOURSTORY_C4_CAPTURE_EVERY_MS")) : 1000;
    static const auto start = std::chrono::steady_clock::now();
    static long last = -1000000;
    static int index = 0;
    const long now = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    if (now - last < every) return;
    last = now;
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* copy = nullptr;
    D3DSURFACE_DESC desc;
    HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    if (SUCCEEDED(hr)) hr = back->GetDesc(&desc);
    if (SUCCEEDED(hr) && desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) hr = E_NOTIMPL;
    if (SUCCEEDED(hr)) hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr);
    if (SUCCEEDED(hr)) hr = device->GetRenderTargetData(back, copy);
    D3DLOCKED_RECT locked;
    if (SUCCEEDED(hr)) hr = copy->LockRect(&locked, nullptr, D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) {
        char name[64];
        std::snprintf(name, sizeof(name), "/%s-%03d-%06ld.ppm", kind, index++, now);
        const std::string path = std::string(dir) + name;
        if (FILE* f = std::fopen(path.c_str(), "wb")) {
            std::fprintf(f, "P6\n%u %u\n255\n", desc.Width, desc.Height);
            for (UINT y = 0; y < desc.Height; ++y) {
                const unsigned char* row = static_cast<const unsigned char*>(locked.pBits) + y * locked.Pitch;
                for (UINT x = 0; x < desc.Width; ++x) {
                    const unsigned char rgb[3] = {row[x * 4 + 2], row[x * 4 + 1], row[x * 4]};  // BGRA in memory
                    std::fwrite(rgb, 1, 3, f);
                }
            }
            std::fclose(f);
            std::fprintf(stderr, "C4_CAPTURE %s %ux%u %s\n", kind, desc.Width, desc.Height, path.c_str());
        }
        copy->UnlockRect();
    } else {
        std::fprintf(stderr, "C4_CAPTURE %s failed hr=0x%08x\n", kind, static_cast<unsigned>(hr));
    }
    if (copy) copy->Release();
    if (back) back->Release();
}
