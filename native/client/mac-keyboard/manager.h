#pragma once
// C ABI keeps Cocoa/IOKit headers out of the original game and SDL compatibility headers.
#ifdef FOURSTORY_KEYBOARD_OPTIONAL
#define FS_KEYBOARD_API __attribute__((weak_import))
#else
#define FS_KEYBOARD_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
enum FourstoryKeyboardState { FSKeyboardOff, FSKeyboardPending, FSKeyboardUnverified,
    FSKeyboardEnabled, FSKeyboardUnavailable, FSKeyboardUnknown };
FS_KEYBOARD_API void fourstory_keyboard_init(const char* helper_path);
FS_KEYBOARD_API void fourstory_keyboard_toggle(void);
FS_KEYBOARD_API void fourstory_keyboard_focus(int focused);
FS_KEYBOARD_API void fourstory_keyboard_observe(int function_number);
FS_KEYBOARD_API int fourstory_keyboard_state(void);
FS_KEYBOARD_API const char* fourstory_keyboard_status(void);
FS_KEYBOARD_API int fourstory_keyboard_shutdown(void);
#ifdef __cplusplus
}
#endif

#undef FS_KEYBOARD_API
