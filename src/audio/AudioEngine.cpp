#include "AudioEngine.h"
#include <cmath>
void AudioEngine::prepare(double rate,int channels,AudioParameters p)
{
    ready=false; status.store(0); framePosition=delayPosition=0;
    channelCount=juce::jlimit(1,2,channels);
    const bool supported=rate==44100.0 || rate==48000.0 || rate==96000.0;
    resample=rate!=48000.0; frameSize=supported ? int(rate)/100 : 480;
    delay=supported ? 2*frameSize+(resample ? 2*(int(rate)/300) : 0) : 0;
    reportedLatency.store(delay);
    for(auto* s : {&inputGain,&outputGain,&active,&reduction,&width}) s->reset(rate>0 ? rate : 48000,0.02);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(p.inputDb));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(p.outputDb));
    active.setCurrentAndTargetValue(p.bypass || p.compare?0.0f:1.0f);
    width.setCurrentAndTargetValue(p.width/100.0f);
    spectrum.prepare(rate,channelCount);
    reduction.setCurrentAndTargetValue(p.reduction/100.0f);
    inputPeak.store(0); outputPeak.store(0); voiceProbability.store(0); attenuationDb.store(0);
    if(!supported) { status.store(1); return; }
    bool loaded=true;
    for(int c=0;c<channelCount;++c)
    {
        auto& s=channel[size_t(c)];
        s.inputFrame.fill(0); s.outputFrame.fill(0); s.neuralIn.fill(0); s.neuralOut.fill(0);
        s.rawDelay.assign(size_t(delay),0); s.gainedDelay.assign(size_t(delay),0);
        if(resample) { s.down.prepare(int(rate),48000); s.up.prepare(48000,int(rate)); }
        loaded=s.model.prepare() && loaded;
    }
    ready=loaded; if(!ready) status.store(2);
}
void AudioEngine::runFrame(int profile) noexcept
{
    float vad=0;
    for(int c=0;c<channelCount;++c)
    {
        auto& s=channel[size_t(c)];
        if(resample) s.down.process(s.inputFrame.data(),s.neuralIn.data());
        else std::copy_n(s.inputFrame.data(),480,s.neuralIn.data());
        s.model.setProfile(profile);
        auto result=s.model.processFrame(s.neuralIn,s.neuralOut);
        if(!result.ok) { ready=false; status.store(2); voiceProbability.store(0); return; }
        vad=juce::jmax(vad,result.voiceProbability);
        if(resample) s.up.process(s.neuralOut.data(),s.outputFrame.data());
        else std::copy_n(s.neuralOut.data(),480,s.outputFrame.data());
    }
    voiceProbability.store(vad,std::memory_order_relaxed);
}
void AudioEngine::process(juce::AudioBuffer<float>& buffer,AudioParameters p) noexcept
{
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(p.inputDb));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(p.outputDb));
    active.setTargetValue(p.bypass || p.compare?0.0f:1.0f);
    width.setTargetValue(p.width/100.0f);
    reduction.setTargetValue(ready ? p.reduction/100.0f : 0.0f);
    float peakIn=0,peakOut=0; double dryEnergy=0,wetEnergy=0;
    const int channels=juce::jmin(channelCount,buffer.getNumChannels());
    std::array<float*,2> data {buffer.getWritePointer(0),channels==2?buffer.getWritePointer(1):nullptr};
    const bool capture=spectrum.enabled.load(std::memory_order_relaxed);
    if(!capture) spectrum.discardPartial();
    for(int i=0;i<buffer.getNumSamples();++i)
    {
        const float ig=inputGain.getNextValue(),og=outputGain.getNextValue();
        const float on=active.getNextValue(),amount=reduction.getNextValue();
        const float stereoWidth=1.0f+on*(width.getNextValue()-1.0f);
        float aligned[2] {};
        for(int c=0;c<channels;++c)
        {
            auto& s=channel[size_t(c)]; auto* samples=data[size_t(c)];
            float raw=samples[i]; if(!std::isfinite(raw)) { raw=0; status.store(3); }
            peakIn=juce::jmax(peakIn,std::abs(raw));
            if(delay==0) { samples[i]=raw; aligned[c]=raw; continue; }
            const float original=s.rawDelay[size_t(delayPosition)],dry=s.gainedDelay[size_t(delayPosition)];
            aligned[c]=original;
            s.rawDelay[size_t(delayPosition)]=raw; s.gainedDelay[size_t(delayPosition)]=raw*ig;
            s.inputFrame[size_t(framePosition)]=raw*ig;
            const float wet=ready ? s.outputFrame[size_t(framePosition)] : dry;
            const float mixed=(dry+amount*(wet-dry))*og;
            samples[i]=original+on*(mixed-original);
            if(!std::isfinite(samples[i])) samples[i]=original;
        }
        if(channels==2 && stereoWidth!=1.0f && delay>0)
        {
            const float left=buffer.getSample(0,i),right=buffer.getSample(1,i);
            const float mid=(left+right)*0.5f,side=(left-right)*0.5f*stereoWidth;
            buffer.setSample(0,i,mid+side); buffer.setSample(1,i,mid-side);
        }
        for(int c=0;c<channels;++c)
        { const auto out=buffer.getSample(c,i); peakOut=juce::jmax(peakOut,std::abs(out));
          dryEnergy+=double(aligned[c])*aligned[c]; wetEnergy+=double(out)*out; }
        if(capture) spectrum.push(aligned[0],aligned[1],buffer.getSample(0,i),channels==2?buffer.getSample(1,i):0);
        if(delay>0)
        {
            if(++delayPosition==delay) delayPosition=0;
            // Previous result is read before the next frame is accepted.
            if(++framePosition==frameSize) { framePosition=0; if(ready) runFrame(p.profile); }
        }
    }
    for(int c=channels;c<buffer.getNumChannels();++c) buffer.clear(c,0,buffer.getNumSamples());
    inputPeak.store(peakIn,std::memory_order_relaxed); outputPeak.store(peakOut,std::memory_order_relaxed);
    attenuationDb.store(dryEnergy>1e-12 ? float(10*std::log10((dryEnergy+1e-20)/(wetEnergy+1e-20))) : 0,std::memory_order_relaxed);
}
