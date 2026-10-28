// Isolated experiment. Not linked into the game. Public macOS SDK APIs only.
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <ApplicationServices/ApplicationServices.h>
#include "mode_lease.h"
namespace {
using namespace fourstory_keyboard_lease;
bool testSystemShortcuts=false;
int applicationExit=0;
int functionNumber(unsigned code) {
    const unsigned codes[]={122,120,99,118,96,97,98,100,101,109,103,111};
    for(int i=0;i<12;++i)if(codes[i]==code)return i+1;
    return 0;
}
}
@interface Experiment : NSObject<NSApplicationDelegate,NSWindowDelegate>
@property(nonatomic,strong) NSWindow* window;
@property(nonatomic,strong) NSTextField* status;
@property(nonatomic,strong) NSTextField* keys;
@property(nonatomic,strong) NSButton* enable;
@property(nonatomic) BOOL requested;
@property(nonatomic) BOOL closing;
@property(nonatomic) NSUInteger generation;
@property(nonatomic) unsigned seen;
@property(nonatomic) int originalMode;
@property(nonatomic) void* hotKeyToken;
@property(nonatomic) dispatch_queue_t queue;
- (void)startTest:(id)sender;
- (void)restore:(id)sender;
- (void)observed:(NSEvent*)event;
@end
@interface ProbeApplication : NSApplication
@end
@implementation ProbeApplication
- (void)sendEvent:(NSEvent*)event {
    if(event.type==NSEventTypeKeyDown && functionNumber(event.keyCode)) {
        [(Experiment*)self.delegate observed:event];return;
    }
    if(event.type==NSEventTypeSystemDefined && event.subtype==8)
        std::fprintf(stderr,"POC hardware event subtype=8 code=%ld (not an F-key delivery)\n",long((event.data1>>16)&0xffff));
    [super sendEvent:event];
}
@end
@implementation Experiment
- (NSTextField*)label:(NSString*)text rect:(NSRect)rect {
    NSTextField* label=[NSTextField wrappingLabelWithString:text];label.frame=rect;[self.window.contentView addSubview:label];return label;
}
- (NSButton*)button:(NSString*)title x:(CGFloat)x action:(SEL)action {
    NSButton* button=[NSButton buttonWithTitle:title target:self action:action];button.frame=NSMakeRect(x,30,190,36);[self.window.contentView addSubview:button];return button;
}
- (void)applicationDidFinishLaunching:(NSNotification*)note {
    (void)note;self.queue=dispatch_queue_create("org.fourstory.keyboard-poc",DISPATCH_QUEUE_SERIAL);
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,680,450) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=testSystemShortcuts?@"4Story: F-keys + system shortcut experiment":@"4Story: physical function-key experiment";self.window.delegate=self;self.window.releasedWhenClosed=NO;
    [self label:@"Temporary GLOBAL HID mode experiment" rect:NSMakeRect(25,390,630,32)];
    [self label:@"Click Test standard keys, then press physical F1 through F12 WITHOUT Fn. Check that brightness, volume and macOS shortcuts do not fire. Only F-key events are counted. Parameter readback is not proof of success." rect:NSMakeRect(25,290,630,90)];
    self.status=[self label:@"Original mode retained. No preference file is written." rect:NSMakeRect(25,200,630,75)];
    self.keys=[self label:@"No F1–F12 observations yet." rect:NSMakeRect(25,130,630,55)];
    if(testSystemShortcuts) self.status.stringValue=@"Optional test: suppress symbolic system shortcuts while focused (except accessibility shortcuts). Requires existing Accessibility permission. Click another app to restore. No permission prompt is requested.";
    self.enable=[self button:testSystemShortcuts?@"Test keys + shortcuts":@"Test standard keys" x:25 action:@selector(startTest:)];
    [self button:@"Restore original mode" x:245 action:@selector(restore:)];
    [self button:@"Quit and restore" x:465 action:@selector(quit:)];
    if(testSystemShortcuts){NSButton* permission=[self button:@"Request Accessibility" x:25 action:@selector(requestAccessibility:)];permission.frame=NSMakeRect(25,80,240,36);}
    [self.window center];[self.window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
- (void)requestAccessibility:(id)sender {
    (void)sender;if(!testSystemShortcuts || self.closing)return;
    NSDictionary* options=@{(__bridge NSString*)kAXTrustedCheckOptionPrompt:@YES};
    BOOL trusted=AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
    self.status.stringValue=trusted?@"Accessibility permission is available. Click Test keys + shortcuts; physical behavior is still unverified.":@"Approve 4Story Keyboard Test in macOS Accessibility settings, then return and click Test keys + shortcuts. No keyboard mode has been requested by this permission check.";
}
- (void)releaseSystemShortcuts {
    if(self.hotKeyToken){PopSymbolicHotKeyMode(self.hotKeyToken);self.hotKeyToken=nullptr;std::fprintf(stderr,"POC symbolic shortcut mode popped\n");}
}
- (void)startTest:(id)sender {
    (void)sender;if(self.closing || self.requested || !NSApp.active)return;
    if(testSystemShortcuts && !AXIsProcessTrusted()){
        self.status.stringValue=@"Accessibility permission is absent. No keyboard mode requested. This optional test needs user-approved permission; no prompt was opened.";
        std::fprintf(stderr,"POC symbolic shortcut test denied: Accessibility trust absent; no mode requested\n");return;
    }
    self.generation++;NSUInteger ticket=self.generation;self.requested=YES;self.seen=0;self.enable.enabled=NO;
    self.status.stringValue=@"Requesting standard mode. Waiting for native API readback...";
    dispatch_async(self.queue,^{Reply reply=request('E');dispatch_async(dispatch_get_main_queue(),^{
        if(ticket!=self.generation || self.closing)return;
        self.enable.enabled=YES;
        if(reply.status){self.requested=NO;self.status.stringValue=[NSString stringWithFormat:@"Request failed: 0x%08x. Not enabled. Recovery requested.",unsigned(reply.status)];}
        else {
            if(testSystemShortcuts){
                self.hotKeyToken=PushSymbolicHotKeyMode(kHIHotKeyModeAllDisabledExceptUniversalAccess);
                std::fprintf(stderr,"POC symbolic shortcut push token=%s; NOT physical proof\n",self.hotKeyToken?"present":"absent");
                if(!self.hotKeyToken){[self restore:nil];return;}
            }
            self.status.stringValue=[NSString stringWithFormat:@"Requested mode 1; readback %d. PHYSICAL BEHAVIOR UNVERIFIED. Press F1–F12 without Fn.",reply.observed];
        }
    });});
}
- (void)restore:(id)sender {
    (void)sender;if(self.closing)return;[self releaseSystemShortcuts];self.generation++;self.requested=NO;self.enable.enabled=YES;
    self.status.stringValue=@"Restoring original global HID mode...";
    NSUInteger ticket=self.generation;
    dispatch_async(self.queue,^{Reply reply=request('R');dispatch_async(dispatch_get_main_queue(),^{
        if(ticket!=self.generation || self.closing)return;
        self.status.stringValue=reply.status?[NSString stringWithFormat:@"Restore failed/unavailable: 0x%08x. Check the recovery log.",unsigned(reply.status)]:[NSString stringWithFormat:@"Original parameter restored (%d). Verify that physical hardware functions work again.",reply.observed];
    });});
}
- (void)applicationDidResignActive:(NSNotification*)note {(void)note;[self restore:nil];}
- (void)observed:(NSEvent*)event {
    int key=functionNumber(event.keyCode);if(!key)return;
    std::fprintf(stderr,"POC F%d keydown repeat=%d active=%d requested=%d\n",key,event.isARepeat,NSApp.active,self.requested);
    if(!self.requested || !NSApp.active)return;
    self.seen|=1u<<(key-1);NSMutableString* text=[NSMutableString stringWithString:@"Received: "];
    for(int i=1;i<=12;++i)if(self.seen&(1u<<(i-1)))[text appendFormat:@"F%d ",i];
    if(self.seen==0xfff)[text appendString:@"\nAll twelve delivered. Human confirmation of no Fn / no hardware action is still required."];
    self.keys.stringValue=text;
}
- (void)quit:(id)sender {(void)sender;[NSApp terminate:nil];}
- (BOOL)windowShouldClose:(NSWindow*)sender {(void)sender;[NSApp terminate:nil];return NO;}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;if(self.closing)return NSTerminateLater;
    [self releaseSystemShortcuts];self.closing=YES;self.requested=NO;self.generation++;
    dispatch_async(self.queue,^{
        Reply reply=request('R');int recovered=finish();
        dispatch_async(dispatch_get_main_queue(),^{
            if(!recovered){[NSApp replyToApplicationShouldTerminate:YES];return;}
            self.status.stringValue=@"Original mode could not be verified. Manual recovery may be required.";
            NSAlert* alert=[NSAlert new];alert.messageText=@"Keyboard restoration is not verified";
            alert.informativeText=[NSString stringWithFormat:@"Native restore status 0x%08x; helper exit %d. The original mode was %d. Check Keyboard → Keyboard Shortcuts → Function Keys before continuing. Do not force-kill a recovery helper that may still be restoring.",unsigned(reply.status),recovered,self.originalMode];
            [alert addButtonWithTitle:@"Keep window open"];[alert addButtonWithTitle:@"Quit anyway"];
            [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response){
                if(response==NSAlertSecondButtonReturn){applicationExit=2;[NSApp replyToApplicationShouldTerminate:YES];}
                else {self.closing=NO;self.enable.enabled=NO;[NSApp replyToApplicationShouldTerminate:NO];}
            }];
        });
    });
    return NSTerminateLater;
}
@end
int main(int argc,char** argv) {
    signal(SIGPIPE,SIG_IGN);
    if(argc==4 && !std::strcmp(argv[1],"--watchdog"))return watch(std::atoi(argv[2]),argv[3]);
    if(argc==2 && !std::strcmp(argv[1],"--read-only")) {
        io_connect_t c=IO_OBJECT_NULL;int status=connect(&c),mode=-1;if(!status)status=readMode(c,&mode);
        disconnect(c);std::printf("uid=%u status=0x%08x mode=%d; read-only, no physical proof\n",getuid(),unsigned(status),mode);return status?1:0;
    }
    if(argc==2 && !std::strcmp(argv[1],"--shortcut-read-only")) {
        CFArrayRef keys=nullptr;OSStatus status=CopySymbolicHotKeys(&keys);
        std::printf("Accessibility trusted=%d; CopySymbolicHotKeys status=%d; read-only, no permission prompt\n",AXIsProcessTrusted(),int(status));
        if(!status && keys)for(CFIndex i=0;i<CFArrayGetCount(keys);++i){
            CFDictionaryRef key=(CFDictionaryRef)CFArrayGetValueAtIndex(keys,i);int code=-1,mods=-1;
            CFTypeRef c=CFDictionaryGetValue(key,kHISymbolicHotKeyCode),m=CFDictionaryGetValue(key,kHISymbolicHotKeyModifiers);
            if(c && CFGetTypeID(c)==CFNumberGetTypeID())CFNumberGetValue((CFNumberRef)c,kCFNumberIntType,&code);
            if(m && CFGetTypeID(m)==CFNumberGetTypeID())CFNumberGetValue((CFNumberRef)m,kCFNumberIntType,&mods);
            int f=functionNumber(unsigned(code));
            if(f)std::printf("symbolic F%d modifiers=0x%x enabled=%d\n",f,unsigned(mods),CFDictionaryGetValue(key,kHISymbolicHotKeyEnabled)==kCFBooleanTrue);
        }
        if(keys)CFRelease(keys);return status?1:0;
    }
    testSystemShortcuts=argc==3 && !std::strcmp(argv[2],"--with-system-shortcut-test");
    if(argc<2){std::fprintf(stderr,"usage: %s LOCK_PATH [--same-value-test]\n",argv[0]);return 2;}
    Reply initial{};if(!launch(argv[0],argv[1],&initial) || initial.status) {
        std::fprintf(stderr,"POC unavailable status=0x%08x; no mode requested\n",unsigned(initial.status));finish();return 1;
    }
#ifdef FOURSTORY_KEYBOARD_PROBE_TEST
    if(argc==3 && !std::strcmp(argv[2],"--script-test")) {
        std::printf("READY %d\n",initial.original);std::fflush(stdout);
        char command=0;
        while(std::scanf(" %c",&command)==1 && command!='Q') {
            if(command=='F'){std::printf("FINISH %d\n",finish());std::fflush(stdout);continue;}
            Reply reply=request(command);std::printf("REPLY %u %d %d\n",unsigned(reply.status),reply.original,reply.observed);std::fflush(stdout);
        }
        int result=finish();std::printf("FINISH %d\n",result);return result;
    }
#endif
    if(argc==3 && !std::strcmp(argv[2],"--same-value-test")) {
        Reply reply=request('P');int recovery=finish();std::printf("same-original-value API status=0x%08x original=%d readback=%d recovery=%d; NOT physical proof\n",unsigned(reply.status),reply.original,reply.observed,recovery);return reply.status || recovery?1:0;
    }
    @autoreleasepool {
        [ProbeApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        Experiment* experiment=[Experiment new];experiment.originalMode=initial.original;NSApp.delegate=experiment;[NSApp run];
    }
    int recovery=finish();return applicationExit?applicationExit:recovery;
}
