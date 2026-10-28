#pragma once
namespace fourstory {
// CTGaugePannel::ShowComponent shows every child, including the hidden
// tournament panel. The original rule (CTClientGame::CalcTRSCS) shows that
// panel only when neither the minimap nor the RSCS window is visible, but it
// runs only when a tick elapses. Apply the same rule at show time; without
// game state (no main character or RSCS window yet) keep the panel hidden.
inline bool tournament_info_visible(bool parent_visible, bool have_state,
                                    bool show_minimap, bool rscs_visible) {
    return parent_visible && have_state && !show_minimap && !rscs_visible;
}
}
