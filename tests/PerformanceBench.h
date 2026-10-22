#pragma once
#include "PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

// Offline callback timing; deadline overruns here are not measured device underruns.
inline void benchmark(const char* destination,int profiles=1)
{
    std::ofstream csv(destination);
    if(!csv) throw std::runtime_error("Cannot create benchmark CSV");
    csv<<"rate,channels,profile,block,capture,mean_us,p99_us,max_us,cpu_percent,over_budget,calls\n";
    for(int rate:{44100,48000,96000}) for(int channels:{1,2})
    for(int profile=0;profile<profiles;++profile) for(int block:{32,64,128,256,512,1024}) for(bool capture:{false,true})
    {
        VocalPureProcessor p;
        auto layout=p.getBusesLayout();
        layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
        layout.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
        if(!p.setBusesLayout(layout)) throw std::runtime_error("Benchmark layout rejected");
        p.state.getParameter("reduction")->setValueNotifyingHost(1);
        if(auto* parameter=p.state.getParameter("profile")) parameter->setValueNotifyingHost(parameter->convertTo0to1(float(profile)));
        p.prepareToPlay(rate,block); p.engine.spectrum.enabled.store(capture);
        juce::AudioBuffer<float> audio(channels,block); juce::MidiBuffer midi;
        std::vector<double> durations; durations.reserve(size_t(rate/block+1));
        const int warm=rate/5,measured=rate;
        unsigned random=729;
        for(int offset=-warm;offset<measured;offset+=block)
        {
            for(int c=0;c<channels;++c) for(int i=0;i<block;++i)
            {
                random=random*1664525u+1013904223u;
                const float noise=(float(random>>8)/16777216.0f-0.5f)*0.035f;
                const double t=double(offset+i)/rate;
                audio.setSample(c,i,noise+0.12f*float(std::sin(2*juce::MathConstants<double>::pi*(c?187:137)*t)));
            }
            // Draining excludes display FFT cost and models a visible editor receiving snapshots.
            SpectrumQueue::Frame frame; while(p.engine.spectrum.pop(frame)) {}
            const auto begin=std::chrono::steady_clock::now();
            p.processBlock(audio,midi);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
            if(offset>=0) durations.push_back(us);
        }
        double total=0; size_t over=0;
        for(auto us:durations) { total+=us; if(us>1e6*block/rate) ++over; }
        std::sort(durations.begin(),durations.end());
        const double mean=total/double(durations.size());
        csv<<rate<<','<<channels<<','<<profile<<','<<block<<','<<int(capture)<<','<<mean<<','
           <<durations[size_t(0.99*double(durations.size()-1))]<<','<<durations.back()<<','
           <<mean*rate/(1e4*block)<<','<<over<<','<<durations.size()<<'\n';
    }
    if(!csv) throw std::runtime_error("Benchmark CSV write failed");
    std::cout<<"Benchmark saved: "<<destination<<'\n';
}
