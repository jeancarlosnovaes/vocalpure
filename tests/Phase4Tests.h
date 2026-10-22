#pragma once
#include "ReferenceFrameResampler.h"

void phase4Tests()
{
    // Frozen pre-optimization implementation: allow only round-off differences.
    for(auto rates:{std::pair{44100,48000},std::pair{48000,44100},std::pair{96000,48000},std::pair{48000,96000}})
    {
        FrameResampler optimized; ReferenceFrameResampler reference;
        optimized.prepare(rates.first,rates.second); reference.prepare(rates.first,rates.second);
        std::array<float,960> input {},a {},b {}; unsigned random=628;
        for(int frame=0;frame<40;++frame)
        {
            for(int i=0;i<rates.first/100;++i) { random=random*1664525u+1013904223u; input[size_t(i)]=float(random>>8)/16777216.0f-0.5f; }
            optimized.process(input.data(),a.data()); reference.process(input.data(),b.data());
            for(int i=0;i<rates.second/100;++i) require(std::abs(a[size_t(i)]-b[size_t(i)])<2e-6f,"Optimized resampler differs from reference");
        }
    }
    std::array<double,3> neuralEnergy {};
    for(int profile=0;profile<3;++profile)
    {
        RNNoiseEngine model; require(model.prepare(),"Profile model failed to prepare"); model.setProfile(profile);
        std::array<float,480> in {},out {};
        unsigned random=42;
        for(int frame=0;frame<100;++frame)
        {
            for(auto& sample:in) { random=random*1664525u+1013904223u; sample=(float(random>>8)/16777216.0f-0.5f)*0.16f; }
            require(model.processFrame(in,out).ok,"Profile inference failed");
            for(float sample:out) { require(std::isfinite(sample),"Non-finite profile output"); neuralEnergy[size_t(profile)]+=double(sample)*sample; }
        }
        require(model.inferenceCount()==(profile==2?50ul:100ul),"Eco neural cadence not halved");
        require(model.reset() && model.inferenceCount()==0,"Neural statistics did not reset");
    }
    require(neuralEnergy[1]<neuralEnergy[0]*0.95,"Strong profile did not deepen attenuation");

    for(int rate:{44100,48000,96000}) for(int profile=0;profile<3;++profile)
    {
        std::vector<float> signal(static_cast<size_t>(rate));
        for(size_t i=0;i<signal.size();++i) signal[i]=0.06f*float(std::sin(2*juce::MathConstants<double>::pi*187*double(i)/rate));
        VocalPureProcessor a,b; set(a,"profile",float(profile)); set(b,"profile",float(profile));
        prepare(a,rate,2,100); prepare(b,rate,2,100);
        require(run(a,signal,{32,7,1024,137},2)==run(b,signal,{512},2),"Profile depends on host block partition");
        prepare(a,rate,1,0);
        const auto dry=run(a,signal,{64,137}); const int delay=a.getLatencySamples();
        for(size_t i=0;i<dry.size();++i) require(dry[i]==(int(i)<delay?0:signal[i-size_t(delay)]),"Profile changed dry alignment");

        // No mid-channel collapse: independent analysis must preserve anti-phase and silence.
        for(bool opposite:{true,false})
        {
            prepare(a,rate,2,100); juce::AudioBuffer<float> audio(2,137); juce::MidiBuffer midi;
            for(int block=0;block<80;++block)
            {
                for(int i=0;i<137;++i)
                { const float v=signal[size_t(block*137+i)%signal.size()]; audio.setSample(0,i,v); audio.setSample(1,i,opposite?-v:0); }
                allocations=0; trackAllocations=true; a.processBlock(audio,midi); trackAllocations=false;
                require(allocations==0,"Profile allocated in callback");
                for(int i=0;i<137;++i) require(std::abs(audio.getSample(1,i)+(opposite?audio.getSample(0,i):0))<1e-5f,"Profile stereo polarity/crosstalk failed");
            }
        }
    }

    // Automated switches and a 30-second simulated stream; no wall-clock waiting.
    VocalPureProcessor p; prepare(p,48000,2,100);
    juce::AudioBuffer<float> audio(2,128); juce::MidiBuffer midi;
    float previous=0,maxStep=0;
    for(int block=0;block<11250;++block)
    {
        if(block%75==0) set(p,"profile",float((block/75)%3));
        for(int i=0;i<128;++i)
        { const float v=0.06f*float(std::sin(2*juce::MathConstants<double>::pi*173*(block*128+i)/48000)); audio.setSample(0,i,v); audio.setSample(1,i,v); }
        allocations=0; trackAllocations=true; p.processBlock(audio,midi); trackAllocations=false;
        require(allocations==0,"Profile switch allocated in callback");
        for(int i=0;i<128;++i)
        { const float v=audio.getSample(0,i); require(std::isfinite(v),"Profile switching failed"); maxStep=juce::jmax(maxStep,std::abs(v-previous)); previous=v; }
    }
    require(maxStep<0.04f,"Excessive discontinuity while switching profiles on a test tone");
    set(p,"profile",2); juce::MemoryBlock saved; p.getStateInformation(saved);
    VocalPureProcessor restored; restored.setStateInformation(saved.getData(),int(saved.getSize()));
    require(restored.state.getRawParameterValue("profile")->load()==2,"Profile persistence failed");
    auto old=p.state.copyState(); old.removeChild(old.getChildWithProperty("id","profile"),nullptr); old.setProperty("schemaVersion",3,nullptr);
    juce::MemoryBlock legacy; juce::AudioProcessor::copyXmlToBinary(*old.createXml(),legacy);
    restored.setStateInformation(legacy.getData(),int(legacy.getSize()));
    require(restored.state.getRawParameterValue("profile")->load()==0,"Legacy session did not select General");
    restored.applyPreset(7); require(restored.state.getRawParameterValue("profile")->load()==2,"Low CPU preset failed");
    restored.resetParameters(); require(restored.state.getRawParameterValue("profile")->load()==0,"Profile reset failed");
    std::cout<<"PASS: phase4 real profile differences, 50% Eco neural calls, variable blocks, stereo polarity/crosstalk, 30s transitions, allocations and schema migration.\n";
}
