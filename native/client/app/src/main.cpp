// Native entry point of the client (C4). Windows starts TClient.exe through
// the MFC WinMain with the command line "<server ip> <port> <channel>" and the
// game directory as the working directory (Play.cmd: "TClient.exe 192.0.2.2
// 4815 1"). The native program does the same:
//
//   4story-client [--data DIRECTORY] [server-ip port channel]
//
// --data (or FOURSTORY_GAME_DIR) selects the game directory; the default is
// the working directory. The remaining arguments form m_lpCmdLine, as on
// Windows. The SDL3 backend is installed with Vulkan windows (DXVK Native).
#include "StdAfx.h"
#include <fourstory_sdl_backend.h>
#include <fourstory_bundle.h>
#include "../../mac-keyboard/manager.h"
#include <mach-o/dyld.h>
#include <limits.h>
#include <SDL3/SDL_error.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

int main(int argc, char** argv) {
    const char* data = std::getenv("FOURSTORY_GAME_DIR");
    std::string command_line;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) { data = argv[++i]; continue; }
        if (!command_line.empty()) command_line += ' ';
        command_line += argv[i];
    }
    // A packaged app prepares its own folders and settings (C8); --data and
    // FOURSTORY_GAME_DIR still win for development runs.
    if (!data && fourstory::prepare_bundle(command_line) == fourstory::BundleResult::Failed) return 4;
    if (data && chdir(data) != 0) {
        std::fprintf(stderr, "4story-client: cannot enter the game directory %s\n", data);
        return 2;
    }
    fourstory::shell::SdlOptions options;
    options.vulkan = true;
    options.virtual_game_size = true;
    if (!fourstory::shell::install_sdl_backend(options)) {
        std::fprintf(stderr, "4story-client: SDL video start failed: %s\n", SDL_GetError());
        return 3;
    }
    char executable[PATH_MAX];uint32_t executableSize=sizeof(executable);
    if(_NSGetExecutablePath(executable,&executableSize)==0){
        std::string helper(executable);helper=helper.substr(0,helper.find_last_of('/'))+"/keyboard-mode-helper";
        fourstory_keyboard_init(helper.c_str());
    }
    // The instance handle identifies the program's resource table
    // (LoadCursor, LoadIcon and FindResource with theApp.m_hInstance).
    static int module_marker = 0;
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(&module_marker);
    static std::string arguments;
    arguments = command_line;
    const int result = AfxWinMain(instance, nullptr, const_cast<LPTSTR>(arguments.c_str()), SW_SHOWNORMAL);
    int keyboardRecovery=fourstory_keyboard_shutdown();
    fourstory::shell::uninstall_sdl_backend();
    return result?result:keyboardRecovery;
}
