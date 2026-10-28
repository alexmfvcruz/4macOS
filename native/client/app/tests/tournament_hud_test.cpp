#include "../include/fourstory_tournament_hud.h"
#include <cassert>
int main() {
    using fourstory::tournament_info_visible;
    // Q-C7A4: ordinary world entry shows the HUD with the minimap visible;
    // the tournament panel must stay hidden before any tick elapses.
    assert(!tournament_info_visible(true, true, true, false));
    // No game state yet: hidden until CalcTRSCS decides.
    assert(!tournament_info_visible(true, false, false, false));
    // Tournament battle or watching: minimap hidden, RSCS closed -> shown.
    assert(tournament_info_visible(true, true, false, false));
    // RSCS window open: hidden, as in CalcTRSCS.
    assert(!tournament_info_visible(true, true, false, true));
    // Hiding the HUD hides the panel.
    assert(!tournament_info_visible(false, true, false, false));
    return 0;
}
