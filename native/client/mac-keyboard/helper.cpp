// Separate recovery process: no game, SDL or AppKit initialization.
#include "mode_lease.h"
int main(int argc,char** argv) {
    if(argc!=4 || std::strcmp(argv[1],"--watchdog"))return 2;
    return fourstory_keyboard_lease::watch(std::atoi(argv[2]),argv[3]);
}
