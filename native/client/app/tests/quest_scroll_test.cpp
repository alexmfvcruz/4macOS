#include "../include/fourstory_quest_scroll.h"
#include <cassert>
#include <cstdint>
#include <limits>
int main() {
    // The reported J-window extent, empty left/right panes, and normal content.
    assert(fourstory::quest_scroll_extent(-82.f)==0);
    assert(fourstory::quest_scroll_extent(-374.f)==0);
    assert(fourstory::quest_scroll_extent(-292.f)==0);
    assert(fourstory::quest_scroll_extent(273.5f)==273.5f);
    const float values[]={-82,-374,-292,0,.25f,1,273.5f,65535,65536,1e30f,
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()};
    for(float value:values) {
        float extent=fourstory::quest_scroll_extent(value);
        // Exercise the real pane's two conversions under UBSan.
        auto word=static_cast<std::uint16_t>(extent);
        auto integer=static_cast<int>(extent);
        assert(integer>=0 && integer<=65535 && word==integer);
    }
}
