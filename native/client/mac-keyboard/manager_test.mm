// Command driver for compile-time fake-platform manager tests. No synthetic keys.
#import <AppKit/AppKit.h>
#include "manager.h"
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <chrono>
#include <string>
#include <iostream>
void settle(){
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while(fourstory_keyboard_state()==FSKeyboardPending && std::chrono::steady_clock::now()<until)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,false);
}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    @autoreleasepool{
        [NSApplication sharedApplication];fourstory_keyboard_init(argv[1]);
        fourstory_keyboard_focus(1);std::puts("READY");std::fflush(stdout);
        std::string command;
        while(std::getline(std::cin,command)){
            if(command=="toggle")fourstory_keyboard_toggle();
            else if(command=="focus0")fourstory_keyboard_focus(0);
            else if(command=="focus1")fourstory_keyboard_focus(1);
            else if(command=="rapid"){fourstory_keyboard_toggle();fourstory_keyboard_focus(0);}
            else if(command=="keys")for(int i=1;i<=12;++i)fourstory_keyboard_observe(i);
            else if(command=="partial")for(int i=1;i<=8;++i)fourstory_keyboard_observe(i);
            else if(command=="quit"){int result=fourstory_keyboard_shutdown();std::printf("QUIT %d\n",result);return result;}
            else return 3;
            settle();std::printf("STATE %d\n",fourstory_keyboard_state());std::fflush(stdout);
        }
        return fourstory_keyboard_shutdown();
    }
}
