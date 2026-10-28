#pragma once
#include <cstdint>
namespace fourstory {
// Unsigned elapsed time also handles the Win32 tick counter wrapping.
class PreviewTimer {
    std::uint32_t start_ = 0;
    bool active_ = false, released_ = false;
public:
    void start(std::uint32_t now, bool controlHeld) {
        start_ = now; active_ = true; released_ = !controlHeld;
    }
    void stop() { active_ = false; }
    bool active() const { return active_; }
    unsigned remaining(std::uint32_t now) const {
        const auto elapsed = std::uint32_t(now - start_);
        return elapsed >= 8000 ? 0 : (8000 - elapsed + 999) / 1000;
    }
    enum Result { Waiting, Keep, Undo };
    Result update(std::uint32_t now, bool controlHeld) {
        if (!active_) return Waiting;
        // The undo key must be pressed during the preview, before its deadline.
        if (!remaining(now)) { stop(); return Keep; }
        if (!controlHeld) released_ = true;
        if (controlHeld && released_) { stop(); return Undo; }
        return Waiting;
    }
};
}
