#pragma once
#include <memory>
#include <cstdint>
namespace fourstory_audio {
struct Clip;
struct Voice;
struct Track;
using ClipPtr = std::shared_ptr<Clip>;
using VoicePtr = std::shared_ptr<Voice>;
using TrackPtr = std::shared_ptr<Track>;
// All control calls belong to the game thread. Audio rendering uses Apple's
// render threads; no callback accesses game objects or compatibility headers.
bool initialize(bool offline = false);
void shutdown();
struct Diagnostics { unsigned attachments, connections, pooledNodes, leasedNodes; };
// Counters allow offline regression tests to prove that effects do not rebuild
// the live graph. No game state or audio data is exposed.
Diagnostics diagnostics();
enum class EffectProfile { Original, WindowOpen };
// WindowOpen is an explicit native-only treatment for the harsh UI cue.
// Source data, source-byte cursors and duration remain unchanged.
ClipPtr load(const char *path, double tailFadeSeconds = 0,
             EffectProfile profile = EffectProfile::Original);
uint32_t bytes(const ClipPtr&);
unsigned channels(const ClipPtr&);
bool spatial_available();
bool spatial_enabled();
bool set_spatial(bool enabled);
double duration(const ClipPtr&);
VoicePtr create(const ClipPtr&);
bool play(const VoicePtr&);
void pause(const VoicePtr&);
void stop(const VoicePtr&);
bool playing(const VoicePtr&);
double position(const VoicePtr&);
bool seek(const VoicePtr&, double seconds);
void volume(const VoicePtr&, float gain);
void spatial(const VoicePtr&, float x, float y, float z, float minimum = 1, float maximum = 1e9f, bool enabled = true);
// Music/DirectShow WAV path: AVAudioPlayer streams/decodes the file instead
// of keeping an entire decoded music library in memory.
TrackPtr track(const char *path);
bool play(const TrackPtr&, bool loop);
void pause(const TrackPtr&);
void stop(const TrackPtr&);
bool playing(const TrackPtr&);
double position(const TrackPtr&);
double duration(const TrackPtr&);
bool seek(const TrackPtr&, double seconds);
void volume(const TrackPtr&, float gain);
// Standalone test only. Returns interleaved stereo PCM, never opens a device.
bool render(float *stereo, unsigned frames);
}
