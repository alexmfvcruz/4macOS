// SDL3 platform backend of the native window system (C4-P). It creates one SDL
// window per top-level HWND and reports SDL input as the abstract events of
// fourstory_user32.h; the Win32 rules (message order, key state, double
// clicks, character messages) live in the compatibility layer. Coordinates are
// SDL window coordinates (points on macOS; no high-pixel-density request).
#pragma once
#include <windows.h>

namespace fourstory { namespace shell {
struct SdlOptions {
    bool virtual_game_size = false; // Resizable presentation; input stays in game-resolution coordinates.
    bool vulkan = false;           // SDL_WINDOW_VULKAN for DXVK Native (C3/C4 device)
    bool high_pixel_density = false;
    int caption_height = 28;       // AdjustWindowRect model of a titled macOS window
};
// Initializes SDL video (SDL_Init) and installs the backend. Returns false and
// leaves no backend installed when SDL cannot start; SDL_GetError has the text.
bool install_sdl_backend(const SdlOptions& options = SdlOptions());
void uninstall_sdl_backend();
// Game modes use native desktop fullscreen while retaining a fixed backbuffer.
bool set_game_fullscreen(HWND window, bool fullscreen);
bool game_fullscreen(HWND window);
// Windows virtual-key code of an SDL scancode and key code (exposed for tests).
UINT sdl_to_vk(int scancode, unsigned int keycode, bool* extended);
}}
