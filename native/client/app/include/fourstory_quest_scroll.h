#pragma once
namespace fourstory {
// Quest panes pass their extent through both INT and WORD conversions. Keep
// fractional layout values, but clamp an empty pane and the WORD ceiling first.
inline float quest_scroll_extent(float extent) {
    if(!(extent>0.f)) return 0.f; // Includes negative extents and NaN.
    return extent<65535.f ? extent : 65535.f;
}
}
