// Build label for the native client: <channel><version>-<sha>.<utc>, for
// example beta0.1-4a8641a0db6b.20261022T053524Z. Channel, version and the UTC
// build time are compiled in by build.py; <sha> is the first 12 lowercase hex
// digits of the SHA-256 of the running executable file, hashed once, so a
// stale or modified binary cannot show another binary's identity. It does not
// cover the bundle, game data or DMG.
//
// The overlay is a Core Animation text layer above the game's Metal layer in
// the window's content view: it does not use the game renderer, is not part of
// the back buffer (so --capture frames do not contain it) and takes no input.
#pragma once

// Computed on the first call (startup), then cached for the process.
const char *fourstory_build_label();

// Attach the pink label to the bottom-right corner of an NSWindow and keep it
// there after size or display-scale changes; main thread only. `key` is the
// SDL window id. One creation attempt and one "BUILD_LABEL" stderr line per
// window. Returns false if the label is not shown.
bool fourstory_build_overlay_update(unsigned key, void *nswindow);
void fourstory_build_overlay_detach(unsigned key);
