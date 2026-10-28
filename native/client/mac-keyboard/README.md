# Isolated Mac function-key experiment

This is not integrated into the game. Do not interpret API readback or simulated
backend tests as proof that physical function keys work.

Build with existing Apple command-line tools (no downloads):

```sh
python3 native/client/mac-keyboard/build_probe.py
runtime/legacy/mac-keyboard-poc/keyboard-probe --read-only
runtime/legacy/mac-keyboard-poc/keyboard-probe runtime/legacy/mac-keyboard-poc/probe.lock
```

The window does not change mode until **Test standard keys** is clicked. This
requests a temporary **global** live HID setting, not an app-specific preference.
Press the physical F1–F12 keys without Fn and check that hardware/system actions
do not occur. Only function-key observations and numeric hardware events are
logged; ordinary text is not logged. Readback alone leaves the status unverified.
Use **Restore original mode** and check that the original behavior returns.
Switching away restores; switching back does not automatically enable again.

A separate helper snapshots the original numeric mode, holds a checkout-local
lease, and conditionally restores after normal exit or loss of the parent IPC
connection, including parent SIGKILL. Distinguishable external mode changes are
preserved. An IPC timeout invalidates the channel and triggers helper recovery.
Failed recovery remains an error across repeated quit attempts; a warning offers
manual recovery and explicit quit. This is an experiment, not a guarantee if
both processes are killed, the system fails, or an external writer makes an
indistinguishable change. A production lease must be shared across checkouts.

The public IOKit parameter APIs are used. No persistent preference writes,
private APIs, service restarts, event taps, Accessibility/Input Monitoring
requests, administrator elevation, or third-party dependencies are used.

Tests using a separately compiled file-backed fake HID adapter:

```sh
python3 native/client/mac-keyboard/test_probe.py
```

These cover original 0/1 restoration, external changes, parent death, delayed
reply/timeout, repeated recovery failure and lease contention. The fake adapter
and script command interface are excluded from the real build. These tests do
not touch the system's keyboard mode.

Read-only original UI-template inspection:

```sh
python3 native/client/mac-keyboard/measure_layout.py /path/to/TClientCmd.tif
```

See `research/mac-keyboard/feasibility.md` for evidence and remaining gates.


Optional investigation of system shortcut conflicts (not proven yet):

```sh
runtime/legacy/mac-keyboard-poc/keyboard-probe --shortcut-read-only
runtime/legacy/mac-keyboard-poc/keyboard-probe runtime/legacy/mac-keyboard-poc/probe.lock --with-system-shortcut-test
```

The optional window refuses to request keyboard mode without existing
Accessibility trust. It does not request permission or open Settings. With
user-approved permission, its Test button requests HID mode and public Carbon
symbolic-shortcut suppression while focused. This affects more system shortcuts
than just F9–F11; Universal Access shortcuts are preserved. Click another app
with the mouse to test restoration. The permission and broad scope are optional
and must be explained before use. A token/readback is not physical proof.


The build also creates `runtime/legacy/mac-keyboard-poc/4Story Keyboard Test.app`, signed
ad-hoc using the installed codesign command. Launch that bundle for an isolated
permission identity (rather than granting a terminal broader permission):

```sh
open -n 'runtime/legacy/mac-keyboard-poc/4Story Keyboard Test.app' --args "$PWD/runtime/legacy/mac-keyboard-poc/probe.lock" --with-system-shortcut-test
```

Click **Request Accessibility** only after agreeing to that optional test.
Approve the named app yourself in macOS settings. Do not rebuild/re-sign during
permission testing; ad-hoc signature changes may invalidate trust. This is not
a production distribution/signing solution.

Production manager logic tests (compile-time fake HID/permission/shortcut layer;
no real keyboard or authorization requests):

```sh
python3 native/client/mac-keyboard/test_manager.py
```

Production source and remaining runtime limits are documented in
`research/mac-keyboard/implementation.md`. Keyboard mode has now been physically
validated by the user both in the isolated experiment and in the first game run.
The checked game build instruments the manager and recovery helper too.
