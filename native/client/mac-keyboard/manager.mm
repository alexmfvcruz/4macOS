// Native manager. All public entry points and Cocoa callbacks run on the UI thread.
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <ApplicationServices/ApplicationServices.h>
#include "manager.h"
#include "mode_lease.h"
#include <atomic>
#include <vector>
namespace {
using namespace fourstory_keyboard_lease;
dispatch_queue_t queue;
std::string helperPath,lockPath,statusText="Mac Keyboard Mode: Disabled\nOriginal keyboard behavior retained. Click to enable.";
std::atomic<unsigned> generation{0};
int state=FSKeyboardOff;
bool focused=false,requested=false,closing=false,consented=false;
unsigned seen=0;
void* shortcutToken=nullptr;
id resignObserver,sleepObserver,wakeObserver;
// Test shims are compile-time only: the test binary cannot request real HID or
// shortcut/Accessibility changes, and production has no environment bypass.
bool trusted(){
#ifdef FOURSTORY_KEYBOARD_MANAGER_TEST
    return std::getenv("FOURSTORY_KEYBOARD_TEST_TRUSTED")!=nullptr;
#else
    return AXIsProcessTrusted();
#endif
}
bool active(){
#ifdef FOURSTORY_KEYBOARD_MANAGER_TEST
    return true;
#else
    return NSApp.active;
#endif
}
void requestPermission(){
#ifndef FOURSTORY_KEYBOARD_MANAGER_TEST
    NSDictionary* options=@{(__bridge NSString*)kAXTrustedCheckOptionPrompt:@YES};
    AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
#endif
}
void* pushShortcuts(){
#ifdef FOURSTORY_KEYBOARD_MANAGER_TEST
    return reinterpret_cast<void*>(1);
#else
    return PushSymbolicHotKeyMode(kHIHotKeyModeAllDisabledExceptUniversalAccess);
#endif
}
void setState(int value,const char* message){state=value;statusText=message;}
void popShortcuts(){if(shortcutToken){
#ifndef FOURSTORY_KEYBOARD_MANAGER_TEST
    PopSymbolicHotKeyMode(shortcutToken);
#endif
    shortcutToken=nullptr;
}}
void restore() {
    popShortcuts();requested=false;seen=0;unsigned ticket=++generation;
    if(!queue)return;
    setState(FSKeyboardPending,"Mac Keyboard Mode: Restoring\nRestoring the previous keyboard mode...");
    dispatch_async(queue,^{
        Reply result=request('R');int recovered=finish();
        dispatch_async(dispatch_get_main_queue(),^{
            if(closing || generation!=ticket)return;
            if(recovered)setState(FSKeyboardUnknown,"Mac Keyboard Mode: Unknown\nRestoration could not be verified. Check macOS Keyboard settings. See the client log for recovery details.");
            else setState(FSKeyboardOff,"Mac Keyboard Mode: Disabled\nOriginal keyboard behavior restored. Click to enable.");
            std::fprintf(stderr,"MAC_KEYBOARD restore request=0x%08x recovery=%d\n",unsigned(result.status),recovered);
        });
    });
}
void enable() {
    if(closing || !focused || !active())return;
    if(!trusted()){
        requestPermission();
        setState(FSKeyboardUnavailable,"Mac Keyboard Mode: Permission needed\nApprove this game in macOS Accessibility settings, then click again. No keyboard mode was changed.");return;
    }
    requested=true;seen=0;unsigned ticket=++generation;
    setState(FSKeyboardPending,"Mac Keyboard Mode: Enabling\nRequesting temporary function-key mode...");
    dispatch_async(queue,^{
        Reply reply{};bool ok=false;
        if(generation==ticket){
            // Finish any earlier lease before snapshotting a fresh original state.
            int recovered=finish();
            if(!recovered && launch(helperPath.c_str(),lockPath.c_str(),&reply) && !reply.status && generation==ticket){reply=request('E');ok=!reply.status;}
            if(!ok)finish();
        }
        dispatch_async(dispatch_get_main_queue(),^{
            if(closing || generation!=ticket)return;
            if(!ok){requested=false;setState(FSKeyboardUnavailable,"Mac Keyboard Mode: Unavailable\nThe native request failed, another game holds the keyboard lease, or recovery is pending. See the client log.");return;}
            if(!focused || !active() || !trusted()){restore();return;}
            shortcutToken=pushShortcuts();
            if(!shortcutToken){restore();return;}
            setState(FSKeyboardUnverified,"Mac Keyboard Mode: Active\nFunction-key mode is active. Press F1 through F12 without Fn to complete the event check. System shortcuts are suppressed while focused. Click to restore.");
            std::fprintf(stderr,"MAC_KEYBOARD requested mode=%d; shortcut token present; physical verification pending\n",reply.observed);
        });
    });
}
}
extern "C" void fourstory_keyboard_init(const char* helper_path) {
    if(queue)return;
    helperPath=helper_path?helper_path:"";
#ifndef FOURSTORY_KEYBOARD_MANAGER_TEST
    // Direct executable launches can leave Dock/switcher artwork stale even
    // when the bundle advertises the correct icon. Publish it to the running app.
    NSBundle* bundle=[NSBundle mainBundle];
    NSString* iconName=[bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
    if(iconName){
        NSURL* iconURL=[bundle.resourceURL URLByAppendingPathComponent:iconName];
        NSImage* icon=[[NSImage alloc] initWithContentsOfURL:iconURL];
        if(icon.valid){NSApp.applicationIconImage=icon;std::fprintf(stderr,"APP_ICON published bundle icon to Dock and app switcher\n");}
    }
#endif
    std::fprintf(stderr,"MAC_KEYBOARD bundle=%s Accessibility=%d (no prompt)\n",[[NSBundle mainBundle].bundleIdentifier UTF8String],trusted());
    size_t n=confstr(_CS_DARWIN_USER_TEMP_DIR,nullptr,0);
    std::vector<char> directory(n);
    if(!n || confstr(_CS_DARWIN_USER_TEMP_DIR,directory.data(),n)!=n){setState(FSKeyboardUnavailable,"Mac Keyboard Mode: Unavailable\nA private per-user recovery lease is unavailable.");return;}
    lockPath=std::string(directory.data())+"fourstory-keyboard-mode.lock";
    queue=dispatch_queue_create("org.fourstory.keyboard-manager",DISPATCH_QUEUE_SERIAL);
#ifdef FOURSTORY_KEYBOARD_MANAGER_TEST
    const char* testLock=std::getenv("FOURSTORY_KEYBOARD_TEST_LOCK");
    if(!testLock)std::abort();lockPath=testLock;consented=true;
#else
    resignObserver=[[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidResignActiveNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){fourstory_keyboard_focus(0);}];
    sleepObserver=[[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceWillSleepNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){fourstory_keyboard_focus(0);}];
    wakeObserver=[[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidWakeNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){fourstory_keyboard_focus(NSApp.active?1:0);}];
#endif
}
extern "C" void fourstory_keyboard_focus(int value) {
    focused=value!=0;
    if(!focused && (requested || shortcutToken))restore();
}
extern "C" void fourstory_keyboard_toggle() {
    if(closing || !queue)return;
    if(requested || state==FSKeyboardPending){restore();return;}
    if(!focused || !active())return;
#ifndef FOURSTORY_KEYBOARD_MANAGER_TEST
    // After a failure (often missing Accessibility access) explain it again.
    if(state==FSKeyboardUnavailable)consented=false;
#endif
    if(!consented){
        NSAlert* alert=[NSAlert new];alert.messageText=@"Use F1–F12 without Fn?";
        alert.informativeText=@"Use F1–F12 for game skills instead of macOS shortcuts.\n\nYour normal keys return when you switch apps, sleep, or quit. Turn this on again when you return to the game.\n\nRequires macOS Accessibility access.";
        [alert addButtonWithTitle:@"Enable"];[alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:@"Open Accessibility Settings"];
        // No HID change until the sheet is dismissed and the game is focused.
        unsigned ticket=++generation;setState(FSKeyboardPending,"Mac Keyboard Mode: Confirmation\nReview the temporary global mode and shortcut scope.");
        auto complete=^(NSModalResponse response){
            if(closing || generation!=ticket)return;
            setState(FSKeyboardOff,"Mac Keyboard Mode: Disabled\nOriginal keyboard behavior retained. Click to enable.");
            if(response==NSAlertFirstButtonReturn){consented=true;enable();}
            else if(response==NSAlertThirdButtonReturn)
                [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
        };
        if(NSApp.keyWindow)[alert beginSheetModalForWindow:NSApp.keyWindow completionHandler:complete];
        else {setState(FSKeyboardUnavailable,"Mac Keyboard Mode: Unavailable\nNo active game window is available for confirmation.");}
        return;
    }
    enable();
}
extern "C" void fourstory_keyboard_observe(int f) {
    if(!requested || !focused || state!=FSKeyboardUnverified || f<1 || f>12)return;
    seen|=1u<<(f-1);
    if(seen==0xfff){setState(FSKeyboardEnabled,"Mac Keyboard Mode: Enabled\nF1–F12 events verified in this session. System shortcuts are suppressed while focused. Click to restore your previous mode.");std::fprintf(stderr,"MAC_KEYBOARD all twelve F-key events observed in active session\n");}
}
extern "C" int fourstory_keyboard_state(){return state;}
extern "C" const char* fourstory_keyboard_status(){return statusText.c_str();}
extern "C" int fourstory_keyboard_shutdown(){
    closing=true;++generation;requested=false;popShortcuts();
    if(resignObserver)[[NSNotificationCenter defaultCenter] removeObserver:resignObserver];
    if(sleepObserver)[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:sleepObserver];
    if(wakeObserver)[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:wakeObserver];
    __block int recovered=0;
    if(queue)dispatch_sync(queue,^{recovered=finish();});
    if(recovered){
        std::fprintf(stderr,"MAC_KEYBOARD RESTORE NOT VERIFIED recovery=%d; check Keyboard settings\n",recovered);
#ifndef FOURSTORY_KEYBOARD_MANAGER_TEST
        NSAlert* alert=[NSAlert new];alert.messageText=@"Keyboard restoration is not verified";
        alert.informativeText=@"Check macOS Keyboard → Keyboard Shortcuts → Function Keys to restore your preferred mode. A recovery helper may still be finishing; do not force-kill it.";
        [alert runModal];
#endif
    }
    return recovered;
}
