#pragma once
#include "SpectrumQueue.h"
#include <juce_dsp/juce_dsp.h>
class SpectrumAnalyzer
{
public:
    static constexpr int bins=SpectrumQueue::size/2+1;
    SpectrumAnalyzer() { input.fill(-100); output.fill(-100); }
    void analyse(const SpectrumQueue::Frame& frame)
    {
        std::array<float,bins> inPower {},outPower {};
        for(int c=0;c<frame.channels;++c)
        for(int path=0;path<2;++path)
        {
            work.fill(0);
            std::copy(frame.samples[size_t(c+path*2)].begin(),frame.samples[size_t(c+path*2)].end(),work.begin());
            window.multiplyWithWindowingTable(work.data(),SpectrumQueue::size);
            fft.performFrequencyOnlyForwardTransform(work.data());
            auto& power=path==0?inPower:outPower;
            for(int k=0;k<bins;++k)
            { const float magnitude=work[size_t(k)]*(k==0?1.0f:2.0f)/SpectrumQueue::size;
              power[size_t(k)]+=magnitude*magnitude/float(frame.channels); }
        }
        for(int k=0;k<bins;++k)
        {
            input[size_t(k)]=juce::Decibels::gainToDecibels(std::sqrt(inPower[size_t(k)]),-100.0f);
            output[size_t(k)]=juce::Decibels::gainToDecibels(std::sqrt(outPower[size_t(k)]),-100.0f);
        }
        rate=frame.rate;
    }
    std::array<float,bins> input,output;
    double rate=48000;
private:
    juce::dsp::FFT fft {11};
    juce::dsp::WindowingFunction<float> window {SpectrumQueue::size,juce::dsp::WindowingFunction<float>::hann,true};
    std::array<float,SpectrumQueue::size*2> work {};
};
