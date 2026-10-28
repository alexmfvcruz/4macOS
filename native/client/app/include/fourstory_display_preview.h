#pragma once
// Included after the game's StdAfx.h; all hooks are native-only.
class CTClientGame;
void fourstory_display_preview_attach(CTClientGame* game, FRAMEDESC_SHAREDPTR desc);
void fourstory_display_preview_begin(CTClientGame* game);
void fourstory_display_preview_tick(CTClientGame* game);

bool fourstory_display_preview_click(CTClientGame* game, UINT flags, CPoint point, bool down);

class CTClientWnd;
void fourstory_display_background_shade(CTClientWnd* window);
