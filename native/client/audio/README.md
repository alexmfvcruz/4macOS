# Native audio (C7)

The full native client now uses the existing game media interfaces:

- `CD3DSound`: lazy decoded WAV data, overlapping logical slots, mono positional
  audio, original byte cursors, pause/stop and volume/fades.
- `CT3DMusic`: Apple file playback/decoding, millisecond seek/length, loop flag,
  pause/resume and the game's existing fades and random region music selection.
- `CTShow`: audio-only handling for this release's two DirectShow WAV assets;
  DirectShow reference times remain 100 ns units. No video support is claimed.
- `CTachyonMedia`: original dispatch, IDs, registration, focus mute and settings.

`backend.mm` is isolated Objective-C++ without Win32 compatibility headers. It
uses macOS AVFoundation/AVFAudio already supplied by the OS; no package is added.
The original Windows audio sources and behavior remain unchanged.

## Position and Spatial Audio

The original camera/listener transform and 0.25 distance scale are retained.
Apple's listener faces -Z, so the backend reverses the game's +Z direction.
Distance gain uses the original default inverse-distance model and per-buffer
limits. Mono sources use the environment node; stereo WAV effects retain the
original non-positional DirectSound fallback.

Game settings has **Spatial audio (headphones)** immediately before Interface
scale and Font. Default Off gives equal-power direction panning. On selects
Apple HRTFHQ, or HRTF if HQ is unavailable, only when the environment advertises
support. HRTF adds headphone front/back/elevation cues. Existing positional
voices update when the setting changes. Unpositioned UI effects, stereo WAVs,
and music keep their normal layout. Settings/SpatialAudio persists the choice.
This does not implement head tracking, personalized HRTF, or Dolby Atmos.

Logical map/object slots do not reserve playback nodes. The backend permits
128 simultaneous output nodes; stopped voices return their player to a preconnected pool and
completed leases are reclaimed before admitting a new voice. The graph has
128 mono and 128 stereo players, built before audio starts, with a unified
128-voice playback limit. Effects are converted once to 48 kHz; playback, stop
and cleanup never attach, detach or reconnect graph nodes. There is no voice
stealing. Failure to admit a voice returns failure. Decoded inactive effects use
an LRU cache capped at 64 MiB; currently held voices/locked assets can retain
additional data. A single decoded effect is also limited to 64 MiB.

All control functions run on the game thread. Apple's renderer owns rendering
threads. Completion blocks touch only an independent atomic completion flag,
never a game object. Disabling/backgrounding audio follows existing game calls.

## Checks without launching the game

From the Codex checkout (or the equivalent paths after merge):

```sh
python3 native/client/audio/test.py --sanitize \
  --assets 'game/Data/Media'

# The dependencies come from deps (native/client/deps.py).
python3 native/client/app/build.py --sanitize --jobs 8 --channel dev --version 0.1
python3 native/client/audio/integration_test.py --sanitize
```

`test.py` uses offline rendering and never opens an output device. It checks
left/right direction, distance gain, pause/seek, overlap, reserved versus active
voice limits, cleanup, and differing front/back HRTF output. With `--assets`,
it opens all TMD files through Apple's file player and fully decodes/renders
each RIFF/WAV through the effects path, retaining the supplied filenames.

`integration_test.py` links the actual client objects with a test entry point.
It does not call the game entry point or install a video backend. It checks
Windows-style filenames, logical slot reservations, stereo fallback, the
original `CTachyonObject::GenerateSND`/`CalcSND` path, cursor units and teardown.
It requires a successful current full client build.

These checks establish source/build and offline rendering behavior. They do
not establish audible quality, physical-device latency, output switching,
headphone localization, frame-time cost or in-game settings layout.

## Isolated stutter-fix candidate

The first in-game test reported crackling/stuttering window-open effects and
lower FPS in the checked build. See `research/c7-audio-stutter.md` for current
evidence and limits. The fixed candidates live separately from the running
build. `run.py` accepts the same FOURSTORY_CLIENT_OUT override as build.py,
with the existing freshness/signature validation still required.

After closing the current game, from the **main checkout**:

```sh
FOURSTORY_CLIENT_OUT="$PWD/runtime/legacy/codex/build/client/c7-pool-plain" \
  ./4story.command test_client
```

For a matched checked-build comparison, select `c7-pool-sanitized` and add
`--checks`. Do not compare plain FPS to checked FPS as evidence of an audio
performance improvement. Keep resolution, scene, camera and spatial setting
constant between matched runs.

Tests now also cover a continuous tone during 500 effect lifecycles, invariant
graph attachment/connection counters, natural completion without polling the
Core Audio clock per object, and a 44.1 kHz head/tail fixture with preserved
one-second duration. All real WAV converted durations are checked against
file-player metadata within 2 ms. The real-asset sweep renders each prefix;
full tail preservation is verified with the dedicated impulse fixture.

## Window-open cue treatment

Native ID0x49 (ID_SND_OPEN_WND2) uses EffectProfile::WindowOpen. The original
WAV reproduces its harsh sound outside the game; a short tail fade missed the
strong middle. This profile applies a 3 kHz two-pole Butterworth low-pass,
3 ms onset ramp, 80 ms fade anchored to the audible end and a0.6 peak ceiling
without boosting quiet material. It runs once after decoding/resampling.
Original duration/source-byte cursors and on-disk assets remain unchanged;
profile variants have separate cache entries. Other sounds use Original.
Repeated UI visibility requests share one active cue without restarting it.
This is an intentional native sound treatment, not a Windows source change.

## Shared window-cue batching (2026-10-22)

Native no-index `CTachyonMedia::Play` requests for window-open ID73 and
window-close ID74 are collected until `CTachyonWnd::Present`. That shared
path is used by lobby/login/character selection and gameplay. One rendered
frame commits one cue: opening has priority over intermediate panel closes;
a close-only action retains its closing cue. The opposite cue is stopped
before playback, and repeated same-cue requests reuse its active voice.
Button clicks, indexed actor effects, ordinary effect polyphony and music
keep their existing paths. Close ID74 retains its original decoded audio.

The affected no-index UI requests return -1 because a playback slot does not
yet exist. The audited window callers ignore that return value. Deferred
state belongs to each media manager and is cleared by stop, focus loss,
unregister and release. `FlushUISound` rechecks audio enable/focus before
playing and consumes pending state even when playback is disabled.

Opt-in `FOURSTORY_AUDIO_TRACE=1` now reports `AUDIO_UI_REQUEST` and
`AUDIO_UI_COMMIT`, plus actual OPEN/CLOSE playback. This distinguishes
visibility requests from audible starts; callbacks do not log.

Candidate for user retest after closing any current client, from main:

```sh
FOURSTORY_AUDIO_TRACE=1 \
FOURSTORY_CLIENT_OUT="$PWD/runtime/legacy/codex/build/client/c7-ui-batched-sanitized" \
  ./4story.command test_client --checks
```

The real-client-object integration test exercises recursive TFrame settings
visibility, shared CFrameGroup lobby transitions, close-only behavior,
open/close exclusivity, repeated-cue cursor preservation, cancellation and
ordinary polyphony. It calls FlushUISound directly; full client compilation
checks the Present hookup, while user gameplay must verify that hookup and
audible behavior. No FPS improvement follows from these checks.
