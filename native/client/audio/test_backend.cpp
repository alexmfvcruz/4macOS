#include "backend.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
namespace a=fourstory_audio;
std::pair<double,double> energy() {
    std::vector<float> pcm(2048); double l=0,r=0;
    for(int n=0;n<12;++n) { assert(a::render(pcm.data(),1024)); for(int i=0;i<1024;++i) { if(n<4) continue; l+=pcm[2*i]*pcm[2*i]; r+=pcm[2*i+1]*pcm[2*i+1]; } }
    return {l,r};
}
int main(int argc,char** argv) {
    assert(argc>=5);
    assert(a::initialize(true));
    const auto graph=a::diagnostics(); assert(graph.pooledNodes==256 && graph.leasedNodes==0);
    // A non-native-rate file has signal at both ends: conversion must retain
    // its duration and tail, rather than merely opening/rendering its prefix.
    auto converted=a::load(argv[2]); assert(converted && a::bytes(converted)==88200);
    std::fprintf(stderr,"resampled duration=%.9f\n",a::duration(converted));
    assert(std::abs(a::duration(converted)-1)<2./48000);
    auto impulse=a::create(converted); assert(a::play(impulse));
    double headPower=0,tailPower=0; float impulsePCM[2048];
    for(int block=0;block<50;++block) {
        assert(a::render(impulsePCM,1024));
        for(int i=0;i<2048;++i) {
            if(block<2) headPower+=impulsePCM[i]*impulsePCM[i];
            if(block>=42) tailPower+=impulsePCM[i]*impulsePCM[i];
        }
    }
    assert(headPower>.01 && tailPower>.01);
    a::stop(impulse); impulse.reset(); converted.reset();
    auto capture=[](const a::ClipPtr& sound) {
        std::vector<float> result(50*2048);
        auto voice=a::create(sound); assert(a::play(voice));
        for(int i=0;i<50;++i) assert(a::render(result.data()+i*2048,1024));
        a::stop(voice); return result;
    };
    auto original=a::load(argv[3]); auto untouched=capture(original);
    auto softened=a::load(argv[3],.040); assert(softened && softened!=original);
    assert(a::duration(softened)==a::duration(original) && a::bytes(softened)==a::bytes(original));
    auto faded=capture(softened); auto unchanged=capture(a::load(argv[3]));
    assert(a::load(argv[3],.040)==softened); assert(!a::load(argv[3],-1));
    double originalTail=0,softTail=0;
    for(int frame=10000;frame<15000;++frame) assert(std::abs(untouched[2*frame]-faded[2*frame])<1.e-5);
    for(int frame=23100;frame<23900;++frame) {
        originalTail+=untouched[2*frame]*untouched[2*frame]; softTail+=faded[2*frame]*faded[2*frame];
        assert(std::abs(untouched[2*frame]-unchanged[2*frame])<1.e-5);
    }
    assert(softTail<originalTail*.4);
    std::puts("PASS audible-tail fade: trailing silence skipped, prefix/source cache unchanged, duration/bytes preserved");
    // Full-cue treatment must attenuate harsh high frequencies, retain body,
    // preserve metadata and never contaminate the original cache variant.
    auto mixed=a::load(argv[4]); auto rawMixed=capture(mixed);
    auto uiClip=a::load(argv[4],0,a::EffectProfile::WindowOpen); auto uiMixed=capture(uiClip);
    assert(uiClip!=mixed && a::bytes(uiClip)==a::bytes(mixed) && a::duration(uiClip)==a::duration(mixed));
    assert(a::load(argv[4])==mixed && a::load(argv[4],0,a::EffectProfile::WindowOpen)==uiClip);
    auto amplitude=[](const std::vector<float>& pcm,double frequency) {
        double real=0,imag=0;
        for(int i=4800;i<14400;++i) {
            real+=pcm[2*i]*std::cos(i*2*M_PI*frequency/48000);
            imag+=pcm[2*i]*std::sin(i*2*M_PI*frequency/48000);
        }
        return std::hypot(real,imag);
    };
    assert(amplitude(uiMixed,1000)>amplitude(rawMixed,1000)*.9);
    assert(amplitude(uiMixed,12000)<amplitude(rawMixed,12000)*.05);
    for(float sample:uiMixed) assert(std::isfinite(sample) && std::abs(sample)<=.60001f);
    double rawEnd=0,uiEnd=0;
    for(int i=46800;i<47900;++i) {rawEnd+=rawMixed[2*i]*rawMixed[2*i];uiEnd+=uiMixed[2*i]*uiMixed[2*i];}
    assert(uiEnd<rawEnd*.05);
    std::puts("PASS UI cue profile: high-frequency reduction, retained body, smooth tail, headroom, metadata/cache isolation");
    auto clip=a::load(argv[1]); assert(clip && a::bytes(clip)==96000); assert(std::abs(a::duration(clip)-1)<.001);
    assert(!a::load("/nonexistent/c7.wav"));
    auto v=a::create(clip); assert(v); a::volume(v,.5); a::spatial(v,-1,0,0); assert(a::play(v));
    auto left=energy(); assert(left.first>1 && left.first>left.second*10);
    a::stop(v); energy(); a::spatial(v,1,0,0); assert(a::play(v));
    auto right=energy(); assert(right.second>1 && right.second>right.first*10);
    a::stop(v); energy(); a::spatial(v,10,0,0); assert(a::play(v));
    auto far=energy(); std::fprintf(stderr,"energy left=%g/%g right=%g/%g far=%g/%g\n",left.first,left.second,right.first,right.second,far.first,far.second); assert(far.second<right.second*.02);
    a::pause(v); assert(!a::playing(v)); auto p=a::position(v); energy(); assert(std::abs(a::position(v)-p)<.001);
    assert(a::seek(v,.5)); assert(!a::seek(v,-1)); assert(a::play(v)); assert(a::playing(v));
    energy(); assert(a::position(v)>=.5);
    a::stop(v); assert(!a::playing(v)); assert(a::position(v)==0);
    auto v2=a::create(clip); assert(v2); a::volume(v,0); a::volume(v2,0); assert(a::play(v) && a::play(v2));
    assert(a::playing(v) && a::playing(v2)); a::stop(v); assert(a::playing(v2)); a::stop(v2);
    std::vector<a::VoicePtr> voices;
    for(int i=0;i<256;++i) voices.push_back(a::create(clip));
    for(int i=0;i<128;++i) { a::volume(voices[i],0); assert(a::play(voices[i])); }
    assert(!a::play(voices[128]));
    a::stop(voices[0]); assert(a::play(voices[128]));
    voices.clear(); v.reset(); v2.reset();
    auto reusable=a::create(clip); assert(reusable); reusable.reset();
    assert(a::spatial_available()); assert(a::set_spatial(true)); assert(a::spatial_enabled());
    auto h=a::create(clip); a::spatial(h,0,0,1); assert(a::play(h));
    auto front=energy(); a::stop(h); energy(); a::spatial(h,0,0,-1); assert(a::play(h));
    auto back=energy(); assert(front.first>0 && back.first>0);
    std::fprintf(stderr,"HRTF front=%g/%g back=%g/%g\n",front.first,front.second,back.first,back.second);
    assert(std::abs(front.first-back.first)>.01);
    assert(a::set_spatial(false)); a::stop(h); h.reset();
    // Keep a tone rendering while unrelated UI-style effects start and stop.
    // No graph edit may occur after initialization, including natural cleanup.
    auto sustained=a::create(clip); assert(a::play(sustained));
    for(int i=0;i<24;++i) {
        auto transient=a::create(clip); a::volume(transient,0); assert(a::play(transient));
        a::stop(transient); transient.reset();
        float pcm[2048]; assert(a::render(pcm,1024));
        double power=0; for(float sample:pcm) power+=sample*sample;
        assert(power>1);
    }
    a::stop(sustained); sustained.reset();
    for(int i=0;i<500;++i) {
        auto transient=a::create(clip); assert(a::play(transient)); a::stop(transient);
    }
    unsigned opened=0,effects=0;
    for(int i=5;i<argc;++i) {
        double decodedDuration=-1;
        std::ifstream file(argv[i],std::ios::binary); char magic[4]; file.read(magic,4);
        if(std::string(magic,4)=="RIFF") {
            auto c=a::load(argv[i]); assert(c && a::bytes(c)>0); decodedDuration=a::duration(c);
            if(std::string(argv[i]).find("/1_73.TMD")!=std::string::npos) {
                auto raw=capture(c);
                auto fixed=a::load(argv[i],0,a::EffectProfile::WindowOpen); auto processed=capture(fixed);
                assert(a::bytes(fixed)==a::bytes(c) && a::duration(fixed)==a::duration(c));
                assert(a::load(argv[i])==c);
                double rawStep=0,fixedStep=0,rawPower=0,fixedPower=0;
                for(size_t frame=1;frame<raw.size()/2;++frame) {
                    rawStep=std::max(rawStep,double(std::abs(raw[2*frame]-raw[2*frame-2])));
                    fixedStep=std::max(fixedStep,double(std::abs(processed[2*frame]-processed[2*frame-2])));
                    rawPower+=raw[2*frame]*raw[2*frame];fixedPower+=processed[2*frame]*processed[2*frame];
                }
                assert(fixedStep<rawStep*.2 && fixedPower>rawPower*.01 && fixedPower<rawPower*.5);
                std::puts("PASS real window-open cue: abrupt steps reduced, audible body retained, duration/source cache unchanged");
            }
            auto effect=a::create(c); assert(effect); assert(a::play(effect));
            float pcm[2048]; assert(a::render(pcm,1024)); a::stop(effect); ++effects;
        }
        auto t=a::track(argv[i]); if(!t) { std::fprintf(stderr,"FAILED %s\n",argv[i]); return 1; }
        assert(a::duration(t)>0);
        if(decodedDuration>=0) assert(std::abs(decodedDuration-a::duration(t))<.002); assert(a::seek(t,a::duration(t)*.5)); assert(a::position(t)>0);
        a::stop(t); assert(a::position(t)==0); ++opened;
    }
    auto completed=a::create(clip); assert(a::play(completed));
    float tail[2048]; for(int i=0;i<50;++i) assert(a::render(tail,1024));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(a::playing(completed) && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
    assert(!a::playing(completed)); completed.reset();
    auto after=a::diagnostics();
    assert(after.attachments==graph.attachments && after.connections==graph.connections);
    assert(after.pooledNodes==graph.pooledNodes && after.leasedNodes==0);
    std::puts("PASS stable audio graph: continuous tone, 500 UI lifecycles, no attach/connect/detach during playback");
    clip.reset(); a::shutdown();
    std::printf("PASS offline effects: direction, attenuation, pause, seek, overlap, voice limit/cleanup; %u real media files opened, %u WAV effects decoded/rendered\n",opened,effects);
}
