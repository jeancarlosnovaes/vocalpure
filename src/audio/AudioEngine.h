#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../ai/RNNoiseEngine.h"
#include "FrameResampler.h"
#include "../dsp/SpectrumQueue.h"
#include <atomic>
#include <array>
#include <vector>
struct AudioParameters { float inputDb,outputDb; bool bypass; float reduction=65.0f,width=100.0f; bool compare=false; int profile=0; };
class AudioEngine
{
public:
    void prepare(double rate,int channels,AudioParameters);
    void process(juce::AudioBuffer<float>&,AudioParameters) noexcept;
    int latencySamples() const noexcept { return delay; }
    std::atomic<float> inputPeak {0},outputPeak {0},voiceProbability {0};
    std::atomic<float> attenuationDb {0}; // Overall signal difference, not noise-only dB.
    // 0 ready, 1 unsupported rate, 2 model failure, 3 invalid input sanitised.
    std::atomic<int> status {0},reportedLatency {0};
    SpectrumQueue spectrum;
private:
    struct Channel
    {
        RNNoiseEngine model;
        FrameResampler down,up;
        std::array<float,960> inputFrame {},outputFrame {};
        std::array<float,480> neuralIn {},neuralOut {};
        std::vector<float> rawDelay,gainedDelay;
    };
    void runFrame(int profile) noexcept;
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<int>::is_always_lock_free);
    std::array<Channel,2> channel;
    int channelCount=0,frameSize=480,framePosition=0,delay=0,delayPosition=0;
    bool resample=false,ready=false;
    juce::SmoothedValue<float> inputGain,outputGain,active,reduction,width;
};
