#pragma once
#include <string>
namespace fourstory {
enum class BundleResult { NotBundle, Ready, Failed };
// Prepares a packaged app run (settings plist, user folders, graphics
// environment, working directory). NotBundle outside a release bundle;
// Failed after the user was told why (the program should exit).
// Fills command_line from Settings.plist when it is empty.
BundleResult prepare_bundle(std::string& command_line);
}
// Window shell -> bundle: the window entered (1) or left (0) fullscreen.
extern "C" void fourstory_window_mode_saved(int fullscreen);
