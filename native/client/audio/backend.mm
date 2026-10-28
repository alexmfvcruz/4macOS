// Use established APIs available before macOS 27 as well as on current macOS.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import <AVFoundation/AVFoundation.h>
#include "backend.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <list>
#include <vector>
#include <string>
#include <array>
namespace fourstory_audio {
namespace {
AVAudioEngine *engine;
bool offlineMode;
AVAudioEnvironmentNode *environment;
bool hrtfEnabled=false, hrtfAvailable=false;
AVAudio3DMixingRenderingAlgorithm hrtfAlgorithm=AVAudio3DMixingRenderingAlgorithmHRTF;
std::vector<std::weak_ptr<Voice>> voiceRegistry;
size_t voiceCount;
constexpr size_t maxVoices = 128;
float clampGain(float g) { return std::isfinite(g) ? std::max(0.f, std::min(1.f, g)) : 0.f; }
NSURL *url(const char *path) {
    if (!path) return nil;
    NSString *s = [[NSString alloc] initWithUTF8String:path];
    return s ? [NSURL fileURLWithPath:s] : nil;
}
void error(const char *operation, NSError *e) {
    std::fprintf(stderr, "[audio] %s: %s\n", operation, e ? e.localizedDescription.UTF8String : "invalid audio data/state");
}
}
struct OutputNode {
    AVAudioPlayerNode *node;
    bool leased=false;
};
namespace {
std::array<std::vector<std::shared_ptr<OutputNode>>,2> outputPools;
unsigned attachments=0,connections=0;
}
struct Clip {
    AVAudioPCMBuffer *buffer;
    uint32_t byteLength;
    unsigned sourceChannels;
};
struct Completion { std::atomic<bool> done{false}; };
struct Voice {
    ClipPtr clip;
    AVAudioPlayerNode *node;
    std::shared_ptr<OutputNode> output;
    AVAudioEngine *owner;
    AVAudioPCMBuffer *scheduled;
    std::shared_ptr<Completion> completion;
    double offset = 0;
    bool active = false, paused = false;
    float gain = 1, attenuation = 1;
    bool positional=false;
    AVAudio3DPoint location={0,0,-1};
    ~Voice() {
        if(node) { [node stop]; node.volume=0; output->leased=false; --voiceCount; }
    }
};
struct Track { AVAudioPlayer *player; ~Track() { [player stop]; } };
namespace {
struct CacheEntry { std::string path; ClipPtr clip; size_t size; double tailFade; EffectProfile profile; };
std::list<CacheEntry> clipCache;
size_t cacheBytes=0;
constexpr size_t cacheLimit=64*1024*1024;
void configure(const VoicePtr& v) {
    if(!v->node || channels(v->clip)!=1) return;
    v->node.sourceMode = v->positional ? AVAudio3DMixingSourceModePointSource : AVAudio3DMixingSourceModeBypass;
    v->node.renderingAlgorithm = hrtfEnabled && v->positional ? hrtfAlgorithm : AVAudio3DMixingRenderingAlgorithmEqualPowerPanning;
    v->node.position = v->location;
}
}
bool spatial_available() { return hrtfAvailable; }
bool spatial_enabled() { return hrtfEnabled; }
bool set_spatial(bool enabled) {
    if(enabled && !hrtfAvailable) return false;
    hrtfEnabled=enabled;
    for(auto& weak:voiceRegistry) if(auto v=weak.lock()) configure(v);
    return true;
}
bool initialize(bool offline) {
    @autoreleasepool {
        if (engine) return offline == offlineMode;
        AVAudioEngine *candidate = [[AVAudioEngine alloc] init];
        NSError *e = nil;
        if (offline) {
            AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000 channels:2];
            if (![candidate enableManualRenderingMode:AVAudioEngineManualRenderingModeOffline format:format maximumFrameCount:4096 error:&e]) {
                error("offline initialization", e); return false;
            }
        }
        AVAudioEnvironmentNode *env = [[AVAudioEnvironmentNode alloc] init];
        [candidate attachNode:env];
        AVAudioFormat *stereo = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000 channels:2];
        [candidate connect:env to:candidate.mainMixerNode format:stereo];
        env.reverbParameters.enable=NO;
        // Inputs use unit direction vectors. Distance gain is applied separately
        // to retain the original per-buffer DirectSound distance limits.
        env.distanceAttenuationParameters.referenceDistance=1;
        env.distanceAttenuationParameters.maximumDistance=1e9f;
        // Build both channel-layout pools before starting the renderer. A
        // logical slot leases a player; gameplay never attaches, disconnects,
        // or reconnects nodes on the live graph. Output PCM is always 48 kHz.
        attachments=connections=1;
        for(unsigned layout=0;layout<2;++layout) {
            AVAudioFormat *format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000 channels:layout+1];
            for(size_t i=0;i<maxVoices;++i) {
                auto output=std::make_shared<OutputNode>();
                output->node=[[AVAudioPlayerNode alloc] init];
                [candidate attachNode:output->node]; ++attachments;
                [candidate connect:output->node to:(layout==0 ? env : candidate.mainMixerNode) format:format]; ++connections;
                output->node.volume=0;
                [output->node prepareWithFrameCount:4096];
                outputPools[layout].push_back(output);
            }
        }
        if (![candidate startAndReturnError:&e]) {
            error("device initialization", e);
            for(auto& pool:outputPools) pool.clear();
            return false;
        }
        engine = candidate; environment=env; offlineMode = offline;
        auto supported=env.applicableRenderingAlgorithms;
        hrtfAvailable=[supported containsObject:@(AVAudio3DMixingRenderingAlgorithmHRTFHQ)] || [supported containsObject:@(AVAudio3DMixingRenderingAlgorithmHRTF)];
        hrtfAlgorithm=[supported containsObject:@(AVAudio3DMixingRenderingAlgorithmHRTFHQ)] ? AVAudio3DMixingRenderingAlgorithmHRTFHQ : AVAudio3DMixingRenderingAlgorithmHRTF;
        std::fprintf(stderr, "[audio] %s backend initialized\n", offline ? "offline" : "AVAudioEngine");
        return true;
    }
}
void shutdown() {
    for(auto& weak:voiceRegistry) if(auto v=weak.lock()) stop(v);
    voiceRegistry.clear(); clipCache.clear(); cacheBytes=0;
    [engine stop];
    for(auto& pool:outputPools) pool.clear();
    engine = nil; environment=nil; hrtfEnabled=hrtfAvailable=false;
}
Diagnostics diagnostics() {
    return {attachments,connections,unsigned(outputPools[0].size()+outputPools[1].size()),unsigned(voiceCount)};
}
ClipPtr load(const char *path, double tailFadeSeconds, EffectProfile profile) {
    if(!path || !*path || !std::isfinite(tailFadeSeconds) || tailFadeSeconds<0 || tailFadeSeconds>10) return {};
    @autoreleasepool {
        for(auto it=clipCache.begin();it!=clipCache.end();++it) if(it->path==path && it->tailFade==tailFadeSeconds && it->profile==profile) {
            auto result=it->clip; clipCache.splice(clipCache.begin(),clipCache,it); return result;
        }
        NSError *e = nil;
        AVAudioFile *file = [[AVAudioFile alloc] initForReading:url(path) error:&e];
        if (!file || file.length <= 0 || file.length > std::numeric_limits<uint32_t>::max()) { error("load effect", e); return {}; }
        // Bound malformed assets and accidental music loads to 64 MiB decoded.
        double size = double(file.length) * file.processingFormat.channelCount * sizeof(float);
        if (size > 64 * 1024 * 1024 || file.processingFormat.channelCount > 2) { error("effect size/channels", nil); return {}; }
        AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:file.processingFormat frameCapacity:(AVAudioFrameCount)file.length];
        if (![file readIntoBuffer:buffer error:&e] || !buffer.frameLength) { error("decode effect", e); return {}; }
        auto c = std::make_shared<Clip>(); c->sourceChannels=buffer.format.channelCount;
        // DirectSound's cursor is measured in source PCM bytes, not native float bytes.
        auto asbd = file.fileFormat.streamDescription;
        unsigned bits = asbd->mBitsPerChannel ? asbd->mBitsPerChannel : 16;
        double sourceBytes = double(buffer.frameLength) * file.processingFormat.channelCount * ((bits + 7) / 8);
        if (sourceBytes > UINT32_MAX) return {};
        c->byteLength = (uint32_t)sourceBytes;
        if(buffer.format.sampleRate!=48000) {
            AVAudioFormat *format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000 channels:c->sourceChannels];
            AVAudioConverter *converter=[[AVAudioConverter alloc] initFromFormat:buffer.format toFormat:format];
            // File conversion can read ahead: normal priming avoids live-input latency.
            converter.primeMethod=AVAudioConverterPrimeMethod_Normal;
            double capacity=std::ceil(double(buffer.frameLength)*48000/buffer.format.sampleRate)+512;
            if(!converter || capacity*c->sourceChannels*sizeof(float)>cacheLimit) { error("resample effect size",nil); return {}; }
            AVAudioPCMBuffer *converted=[[AVAudioPCMBuffer alloc] initWithPCMFormat:format frameCapacity:AVAudioFrameCount(capacity)];
            __block bool supplied=false;
            auto status=[converter convertToBuffer:converted error:&e withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount,AVAudioConverterInputStatus* inputStatus) {
                if(supplied) { *inputStatus=AVAudioConverterInputStatus_EndOfStream; return nil; }
                supplied=true; *inputStatus=AVAudioConverterInputStatus_HaveData; return buffer;
            }];
            if(status==AVAudioConverterOutputStatus_Error || !converted.frameLength) { error("resample effect",e); return {}; }
            buffer=converted;
        }
        if(tailFadeSeconds>0) {
            // Ignore digital silence when locating the audible tail. The
            // window-open asset ends with ~95 ms of silence; a file-end fade
            // would otherwise have no effect. -80 dB is below its audible tail.
            AVAudioFrameCount end=buffer.frameLength;
            while(end) {
                bool audible=false;
                for(unsigned ch=0;ch<buffer.format.channelCount;++ch)
                    audible|=std::fabs(buffer.floatChannelData[ch][end-1])>1.e-4f;
                if(audible) break;
                --end;
            }
            const auto count=std::min(end,AVAudioFrameCount(std::ceil(tailFadeSeconds*buffer.format.sampleRate)));
            if(count>1) for(unsigned ch=0;ch<buffer.format.channelCount;++ch)
                for(AVAudioFrameCount i=0;i<count;++i) {
                    const float envelope=.5f*(1.f+std::cos(float(M_PI)*i/(count-1)));
                    buffer.floatChannelData[ch][end-count+i]*=envelope;
                }
        }
        if(profile==EffectProfile::WindowOpen) {
            // The original cue's strong broadband middle reproduces the harsh
            // character even outside the game. A tail-only fade missed it.
            // Treat this one decoded cue once, never the live/master mix.
            AVAudioFrameCount end=buffer.frameLength;
            while(end) {
                bool audible=false;
                for(unsigned ch=0;ch<buffer.format.channelCount;++ch)
                    audible|=std::fabs(buffer.floatChannelData[ch][end-1])>1.e-4f;
                if(audible) break;
                --end;
            }
            // Two-pole Butterworth low-pass at 3 kHz, unity DC gain.
            const double k=std::tan(M_PI*3000./buffer.format.sampleRate);
            const double norm=1./(1.+std::sqrt(2.)*k+k*k);
            const double b0=k*k*norm,b1=2*b0,b2=b0;
            const double a1=2*(k*k-1)*norm,a2=(1-std::sqrt(2.)*k+k*k)*norm;
            const auto attack=AVAudioFrameCount(.003*buffer.format.sampleRate);
            const auto fade=std::min(end,AVAudioFrameCount(.080*buffer.format.sampleRate));
            float peak=0;
            for(unsigned ch=0;ch<buffer.format.channelCount;++ch) {
                double x1=0,x2=0,y1=0,y2=0;
                for(AVAudioFrameCount i=0;i<buffer.frameLength;++i) {
                    double x=buffer.floatChannelData[ch][i];
                    double y=b0*x+b1*x1+b2*x2-a1*y1-a2*y2;
                    x2=x1;x1=x;y2=y1;y1=y;
                    double envelope=1;
                    if(i<attack) envelope=.5*(1-std::cos(M_PI*i/attack));
                    if(i>=end) envelope=0;
                    else if(fade>1 && i>=end-fade)
                        envelope*=.5*(1+std::cos(M_PI*(i-(end-fade))/(fade-1)));
                    const float sample=float(y*envelope);
                    buffer.floatChannelData[ch][i]=sample;
                    peak=std::max(peak,std::fabs(sample));
                }
            }
            // Headroom without a compressor, gain pumping or boosting quiet cues.
            if(peak>.6f) for(unsigned ch=0;ch<buffer.format.channelCount;++ch)
                for(AVAudioFrameCount i=0;i<buffer.frameLength;++i)
                    buffer.floatChannelData[ch][i]*=.6f/peak;
        }
        c->buffer=buffer;
        size_t decodedSize=buffer.frameLength*buffer.format.channelCount*sizeof(float);
        while(!clipCache.empty() && cacheBytes+decodedSize>cacheLimit) { cacheBytes-=clipCache.back().size; clipCache.pop_back(); }
        clipCache.push_front({path,c,decodedSize,tailFadeSeconds,profile}); cacheBytes+=decodedSize;
        return c;
    }
}
unsigned channels(const ClipPtr& c) { return c ? c->sourceChannels : 0; }
uint32_t bytes(const ClipPtr& c) { return c ? c->byteLength : 0; }
double duration(const ClipPtr& c) { return c ? c->buffer.frameLength / c->buffer.format.sampleRate : 0; }
VoicePtr create(const ClipPtr& c) {
    if (!engine || !c) return {};
    auto v = std::make_shared<Voice>();
    v->clip=c; v->owner=engine;
    voiceRegistry.erase(std::remove_if(voiceRegistry.begin(),voiceRegistry.end(),[](const std::weak_ptr<Voice>& p){return p.expired();}),voiceRegistry.end());
    voiceRegistry.push_back(v);
    return v;
}
double position(const VoicePtr& v) {
    if (!v) return 0;
    if (!v->active) return v->offset;
    AVAudioTime *nodeTime = v->node.lastRenderTime;
    AVAudioTime *t = nodeTime && (nodeTime.sampleTimeValid || nodeTime.hostTimeValid) ? [v->node playerTimeForNodeTime:nodeTime] : nil;
    double p = v->offset + (t && t.sampleTime >= 0 ? double(t.sampleTime) / t.sampleRate : 0);
    return std::min(duration(v->clip), p);
}
bool playing(const VoicePtr& v) {
    // Playback completion is reported by Apple's renderer. Do not query the
    // Core Audio node clock for every object on every game/render tick.
    return v && v->active && !v->paused && !v->completion->done.load();
}
bool play(const VoicePtr& v) {
    @autoreleasepool {
        if (!v || v->owner!=engine || !v->owner.running) return false;
        if(!v->node) {
            // Logical map/object slots do not reserve output nodes. Retire
            // completed nodes before applying the simultaneous playback limit.
            for(auto& weak:voiceRegistry) if(auto old=weak.lock())
                if(old->node && !old->paused && !playing(old)) stop(old);
            if(voiceCount>=maxVoices) return false;
            auto& pool=outputPools[channels(v->clip)-1];
            auto free=std::find_if(pool.begin(),pool.end(),[](const std::shared_ptr<OutputNode>& o){return !o->leased;});
            if(free==pool.end()) return false;
            v->output=*free; v->output->leased=true; v->node=v->output->node; ++voiceCount;
            v->node.volume=v->gain*v->attenuation; configure(v);
        }
        if (v->paused) { v->paused = false; [v->node play]; return true; }
        if (playing(v)) return true;
        if(v->active) [v->node stop];
        if (v->offset >= duration(v->clip)) v->offset = 0;
        auto full = v->clip->buffer;
        auto offset = AVAudioFrameCount(v->offset * full.format.sampleRate);
        if (offset) {
            auto part = [[AVAudioPCMBuffer alloc] initWithPCMFormat:full.format frameCapacity:full.frameLength - offset];
            part.frameLength = full.frameLength - offset;
            for (unsigned ch = 0; ch < full.format.channelCount; ++ch)
                std::memcpy(part.floatChannelData[ch], full.floatChannelData[ch] + offset, part.frameLength * sizeof(float));
            v->scheduled = part;
        } else v->scheduled = full;
        auto done = std::make_shared<Completion>(); v->completion = done;
        [v->node scheduleBuffer:v->scheduled completionCallbackType:(offlineMode ? AVAudioPlayerNodeCompletionDataRendered : AVAudioPlayerNodeCompletionDataPlayedBack) completionHandler:^(AVAudioPlayerNodeCompletionCallbackType) { done->done.store(true); }];
        v->active = true; v->paused = false;
        [v->node play]; return true;
    }
}
void pause(const VoicePtr& v) { if (playing(v)) { [v->node pause]; v->paused = true; } }
void stop(const VoicePtr& v) {
    if(v) { if(v->node) { [v->node stop]; v->node.volume=0; v->output->leased=false; v->output.reset(); v->node=nil; --voiceCount; }
        v->active=v->paused=false; v->offset=0; v->scheduled=nil;
    }
}
bool seek(const VoicePtr& v, double seconds) {
    if (!v || !std::isfinite(seconds) || seconds < 0 || seconds > duration(v->clip)) return false;
    bool resume = playing(v); stop(v); v->offset = seconds;
    return !resume || play(v);
}
void volume(const VoicePtr& v, float gain) { if (v) { v->gain = clampGain(gain); v->node.volume = v->gain * v->attenuation; } }
void spatial(const VoicePtr& v, float x, float y, float z, float minimum, float maximum, bool enabled) {
    if (!v) return;
    float d = std::sqrt(x*x + y*y + z*z);
    float horizontal = std::sqrt(x*x + z*z);
    v->attenuation = enabled && d > minimum ? minimum / std::min(d, maximum) : 1.f;
    (void)horizontal;
    v->positional=enabled && channels(v->clip)==1;
    // DirectSound is left-handed (+Z forward); Apple's listener faces -Z.
    // A unit radius prevents the environment from applying distance gain twice.
    v->location=d>0 ? AVAudioMake3DPoint(x/d,y/d,-z/d) : AVAudioMake3DPoint(0,0,-1);
    configure(v);
    v->node.volume = v->gain * v->attenuation;
}
TrackPtr track(const char *path) {
    @autoreleasepool {
        NSError *e = nil;
        NSURL *fileURL=url(path);
        if(!fileURL) return {};
        AVAudioPlayer *p = [[AVAudioPlayer alloc] initWithContentsOfURL:fileURL error:&e];
        if (!p || p.duration <= 0) { error("load music/media", e); return {}; }
        auto t = std::make_shared<Track>(); t->player = p; return t;
    }
}
bool play(const TrackPtr& t, bool loop) {
    if (!engine || !t) return false;
    t->player.numberOfLoops = loop ? -1 : 0;
    return [t->player play];
}
void pause(const TrackPtr& t) { [t ? t->player : nil pause]; }
void stop(const TrackPtr& t) { if (t) { [t->player stop]; t->player.currentTime = 0; } }
bool playing(const TrackPtr& t) { return t && t->player.playing; }
double position(const TrackPtr& t) { return t ? t->player.currentTime : 0; }
double duration(const TrackPtr& t) { return t ? t->player.duration : 0; }
bool seek(const TrackPtr& t, double seconds) {
    if (!t || !std::isfinite(seconds) || seconds < 0 || seconds > duration(t)) return false;
    t->player.currentTime = seconds; return true;
}
void volume(const TrackPtr& t, float gain) { if (t) t->player.volume = clampGain(gain); }
bool render(float *stereo, unsigned frames) {
    if (!engine || !offlineMode || !stereo || frames > 4096) return false;
    @autoreleasepool {
        AVAudioPCMBuffer *b = [[AVAudioPCMBuffer alloc] initWithPCMFormat:engine.manualRenderingFormat frameCapacity:frames];
        NSError *e = nil;
        auto status = [engine renderOffline:frames toBuffer:b error:&e];
        if (status != AVAudioEngineManualRenderingStatusSuccess || b.frameLength != frames) { error("offline render", e); return false; }
        for (unsigned i=0; i<frames; ++i) { stereo[2*i] = b.floatChannelData[0][i]; stereo[2*i+1] = b.floatChannelData[1][i]; }
        return true;
    }
}
}
