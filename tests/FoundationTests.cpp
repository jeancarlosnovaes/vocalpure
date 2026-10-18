#include "PluginProcessor.h"
#include "dsp/SpectrumAnalyzer.h"
#include "ui/SpectrumDisplay.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <random>
#include <chrono>
#include <limits>
#include <cstdlib>

thread_local bool trackAllocations=false;
thread_local size_t allocations=0;
void* operator new(std::size_t n) { if(trackAllocations) ++allocations; if(auto* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }

void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void set(VocalPureProcessor& p,const char* id,float value)
{ auto* a=p.state.getParameter(id); a->setValueNotifyingHost(a->convertTo0to1(value)); }
void prepare(VocalPureProcessor& p,int rate,int channels,float amount,bool bypass=false)
{
    auto layout=p.getBusesLayout();
    layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
    layout.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
    require(p.setBusesLayout(layout),"Layout rejected");
    set(p,"reduction",amount); set(p,"bypass",bypass?1.0f:0.0f); p.prepareToPlay(rate,1024);
}
std::vector<float> run(VocalPureProcessor& p,const std::vector<float>& input,const std::vector<int>& sizes,int channels=1,bool hostBypass=false)
{
    std::vector<float> result(input.size()); juce::AudioBuffer<float> b(channels,1024); juce::MidiBuffer midi;
    size_t offset=0,index=0;
    while(offset<input.size())
    {
        const int n=std::min(int(input.size()-offset),sizes[index++%sizes.size()]);
        b.setSize(channels,n,false,false,true);
        for(int c=0;c<channels;++c) std::copy_n(input.data()+offset,n,b.getWritePointer(c));
        allocations=0; trackAllocations=true;
        if(hostBypass) p.processBlockBypassed(b,midi); else p.processBlock(b,midi);
        trackAllocations=false; require(allocations==0,"C++ allocation inside processBlock");
        std::copy_n(b.getReadPointer(0),n,result.data()+offset);
        for(int i=0;i<n;++i)
        {
            require(std::isfinite(b.getSample(0,i)),"Non-finite audio");
            if(channels==2) require(b.getSample(0,i)==b.getSample(1,i),"Centre image changed");
        }
        offset+=size_t(n);
    }
    return result;
}
double energy(const std::vector<float>& v,int start)
{ double e=0; for(size_t i=size_t(start);i<v.size();++i) e+=double(v[i])*v[i]; return e; }
int peakIndex(const std::vector<float>& v)
{ return int(std::max_element(v.begin(),v.end(),[](float a,float b){return std::abs(a)<std::abs(b);})-v.begin()); }

void testResampler(int rate)
{
    if(rate==48000) return;
    FrameResampler down,up; down.prepare(rate,48000); up.prepare(48000,rate);
    const int count=rate/100; std::array<float,960> in {},out {}; std::array<float,480> middle {};
    std::vector<float> result;
    for(int frame=0;frame<5;++frame)
    { in.fill(0); if(frame==0) in[0]=1; down.process(in.data(),middle.data()); up.process(middle.data(),out.data());
      result.insert(result.end(),out.begin(),out.begin()+count); }
    require(peakIndex(result)==2*(rate/300),"Resampler group delay mismatch");
    if(rate==96000)
    {
        down.prepare(rate,48000); double e=0;
        for(int frame=0;frame<10;++frame)
        { for(int i=0;i<count;++i) in[size_t(i)]=float(std::sin(2*juce::MathConstants<double>::pi*30000*(frame*count+i)/rate));
          down.process(in.data(),middle.data()); if(frame>1) for(float x:middle) e+=double(x)*x; }
        require(e/(8*480)<1e-6,"Resampler aliases 30 kHz into speech band");
    }
}

void tests()
{
    int cases=0; std::mt19937 rng(42); std::uniform_real_distribution<float> dist(-0.08f,0.08f);
    for(int rate:{48000,44100,96000})
    {
        testResampler(rate);
        for(int channels:{1,2})
        {
            const int expected=rate==48000?960:2*(rate/100)+2*(rate/300);
            std::vector<float> signal(size_t(rate*3)); for(auto& v:signal) v=dist(rng);
            VocalPureProcessor dry; prepare(dry,rate,channels,0);
            require(dry.getLatencySamples()==expected,"Reported delay mismatch");
            auto result=run(dry,signal,{32,64,128,256,512,1024,7,137},channels);
            for(int i=0;i<int(signal.size());++i) require(result[size_t(i)]==(i<expected?0:signal[size_t(i-expected)]),"Dry not exactly delayed");
            VocalPureProcessor bypass; prepare(bypass,rate,channels,100,true);
            require(run(bypass,signal,{137,32,1024},channels)==result,"Bypass alignment mismatch");
            VocalPureProcessor host; prepare(host,rate,channels,100,true);
            require(run(host,signal,{64,512},channels,true)==result,"Host bypass mismatch");
            VocalPureProcessor a,b; prepare(a,rate,channels,100); prepare(b,rate,channels,100);
            a.engine.spectrum.enabled.store(true);
            auto wet=run(a,signal,{32,7,137,1024},channels);
            require(wet==run(b,signal,{512},channels),"Wet depends on block partition");
            std::cout<<"rate="<<rate<<" channels="<<channels<<" status="<<a.engine.status.load()<<" vad="<<a.engine.voiceProbability.load()<<" latency="<<expected
                     <<" noise_attenuation_db="<<10*std::log10(energy(result,rate)/energy(wet,rate))<<'\n';
            // Strong attenuation is checked at the model's native rate. A universal
            // quality threshold across resampled noise would overstate this model.
            require(energy(wet,rate)<energy(result,rate),"Noise energy increased");
            if(rate==48000) require(energy(wet,rate)<energy(result,rate)*0.6,"Native neural suppression regressed");
            VocalPureProcessor half; prepare(half,rate,channels,50);
            auto mixed=run(half,signal,{256,137},channels);
            for(size_t i=0;i<mixed.size();++i)
                require(std::abs(mixed[i]-(wet[i]+result[i])*0.5f)<1e-7f,"Reduction blend not linear/aligned");
            std::vector<float> impulse(size_t(rate/5),0); impulse[2000]=0.8f;
            VocalPureProcessor imp; prepare(imp,rate,channels,100);
            auto response=run(imp,impulse,{32,137,1024},channels);
            const int measured=peakIndex(response)-2000;
            std::cout<<"neural_impulse_peak_delay="<<measured<<'\n';
            require(std::abs(measured-expected)<=2,"Neural delay measurement failed");
            require(imp.engine.voiceProbability.load()>=0 && imp.engine.voiceProbability.load()<=1,"Invalid VAD");
            prepare(a,rate,channels,100); require(run(a,signal,{512},channels)==wet,"Reset retained neural history");
            juce::AudioBuffer<float> empty(channels,0); juce::MidiBuffer midi; a.processBlock(empty,midi);
            cases+=9;
        }
    }
    VocalPureProcessor p; prepare(p,48000,1,37); set(p,"inputGain",6); set(p,"outputGain",-3);
    juce::MemoryBlock saved; p.getStateInformation(saved);
    VocalPureProcessor restored; restored.setStateInformation(saved.getData(),int(saved.getSize()));
    require(restored.state.getRawParameterValue("reduction")->load()==37,"Reduction state failed");
    require(restored.state.getRawParameterValue("inputGain")->load()==6,"Gain state failed");
    restored.setStateInformation("bad",3);
    require(restored.state.getRawParameterValue("reduction")->load()==37,"Invalid state corrupted state");
    auto legacy=p.state.copyState(); legacy.removeChild(legacy.getChildWithProperty("id","reduction"),nullptr);
    legacy.setProperty("schemaVersion",1,nullptr); juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);
    restored.setStateInformation(old.getData(),int(old.getSize()));
    require(restored.state.getRawParameterValue("reduction")->load()==0,"Legacy session gained denoising");
    p.resetParameters(); require(p.state.getRawParameterValue("reduction")->load()==65,"Reset failed");
    std::vector<float> constant(8000,0.1f); prepare(p,48000,1,0); set(p,"inputGain",6); set(p,"outputGain",-3);
    auto gained=run(p,constant,{32,512}); require(std::abs(gained.back()-0.1f*juce::Decibels::decibelsToGain(3.0f))<1e-6f,"Gain failed");
    set(p,"bypass",1); auto faded=run(p,constant,{32,137});
    float previous=gained.back(); for(float v:faded) { require(std::abs(v-previous)<0.001f,"Bypass click"); previous=v; }
    require(faded.back()==0.1f,"Bypass failed to settle");
    prepare(p,32000,1,100); require(p.engine.status.load()==1,"Missing unsupported-rate status");
    require(run(p,constant,{512})==constant,"Unsupported-rate fallback failed");
    prepare(p,48000,1,100); constant[0]=std::numeric_limits<float>::quiet_NaN();
    auto safe=run(p,constant,{512}); require(p.engine.status.load()==3,"Invalid input not indicated");
    // Equal, opposite channels must remain equal and opposite; silent channel cannot acquire signal.
    for(bool opposite:{true,false})
    {
        prepare(p,48000,2,100); juce::AudioBuffer<float> stereo(2,480); juce::MidiBuffer midi;
        for(int frame=0;frame<100;++frame)
        {
            for(int i=0;i<480;++i) { float v=dist(rng); stereo.setSample(0,i,v); stereo.setSample(1,i,opposite?-v:0); }
            p.processBlock(stereo,midi);
            for(int i=0;i<480;++i) require(std::abs(stereo.getSample(1,i)+(opposite?stereo.getSample(0,i):0))<1e-5f,"Stereo polarity/crosstalk failed");
        }
    }
    std::cout<<"PASS: "<<cases<<" matrix checks plus state/migration/gains/smoothing/fallback/stereo; no C++ allocations during callback.\n";
}

void phase3Tests()
{
    SpectrumQueue queue; queue.prepare(48000,2);
    for(int i=0;i<SpectrumQueue::size*20;++i) queue.push(float(i),-float(i),float(i)*0.5f,-float(i)*0.5f);
    SpectrumQueue::Frame frame; int count=0;
    while(queue.pop(frame)) { require(frame.samples[0][0]==float(count*SpectrumQueue::size),"FIFO ordering failed"); ++count; }
    require(count==SpectrumQueue::capacity-1,"Full spectrum queue did not drop safely");
    frame.rate=48000; frame.channels=2;
    for(int i=0;i<SpectrumQueue::size;++i)
    { const float x=0.5f*float(std::sin(2*juce::MathConstants<double>::pi*64*i/SpectrumQueue::size));
      frame.samples[0][size_t(i)]=x; frame.samples[1][size_t(i)]=-x;
      frame.samples[2][size_t(i)]=x*0.5f; frame.samples[3][size_t(i)]=-x*0.5f; }
    SpectrumAnalyzer fft; fft.analyse(frame);
    require(std::abs(fft.input[64]+6.0206f)<0.05f,"Input FFT calibration or stereo cancellation error");
    require(std::abs(fft.output[64]+12.0412f)<0.05f,"Output FFT calibration error");
    SpectrumDisplay display;
    auto radial=display.radialLevels();
    require(*std::max_element(radial.begin(),radial.end())==0,"Idle radial spectrum must be silent");
    for(int i=0;i<12;++i) display.update(frame);
    radial=display.radialLevels();
    const auto peak=std::max_element(radial.begin(),radial.end());
    require(*peak>0.95f,"Radial spectrum did not react to input tone");
    const int expectedBand=int(std::log(1500.0/20)/std::log(20000.0/20)*32);
    require(std::abs(int(peak-radial.begin())-expectedBand)<=1,"Radial frequency mapping failed");
    for(auto& channel:frame.samples) channel.fill(0);
    for(int i=0;i<20;++i) display.update(frame);
    radial=display.radialLevels();
    require(*std::max_element(radial.begin(),radial.end())==0,"Radial spectrum animates in silence");
    display.clearDisplay(); radial=display.radialLevels();
    require(*std::max_element(radial.begin(),radial.end())==0,"Radial spectrum retained stale activity");
    for(float w:{0.0f,100.0f,200.0f})
    {
        VocalPureProcessor p; set(p,"width",w); prepare(p,48000,2,0);
        juce::AudioBuffer<float> b(2,512); juce::MidiBuffer midi;
        for(int block=0;block<10;++block)
        {
            for(int i=0;i<512;++i) { b.setSample(0,i,0.2f); b.setSample(1,i,0); }
            p.processBlock(b,midi);
        }
        require(std::abs(b.getSample(0,511)-(0.1f+0.1f*w/100))<1e-6f,"Width L failed");
        require(std::abs(b.getSample(1,511)-(0.1f-0.1f*w/100))<1e-6f,"Width R failed");
        require(std::abs(b.getSample(0,511)+b.getSample(1,511)-0.2f)<1e-6f,"Width changed mono sum");
        set(p,"compare",1);
        for(int block=0;block<10;++block)
        { for(int i=0;i<512;++i) { b.setSample(0,i,0.2f); b.setSample(1,i,0); } p.processBlock(b,midi); }
        require(b.getSample(0,511)==0.2f && b.getSample(1,511)==0,"Compare did not restore aligned original");
        require(p.state.getRawParameterValue("width")->load()==w,"Compare changed width parameter");
    }
    VocalPureProcessor p;
    for(int i=0;i<int(factoryPresets.size());++i)
    {
        p.applyPreset(i);
        require(p.state.getRawParameterValue("reduction")->load()==factoryPresets[size_t(i)].reduction,"Preset reduction failed");
        require(p.state.getRawParameterValue("width")->load()==100,"Preset width failed");
    }
    set(p,"width",150); set(p,"compare",1); juce::MemoryBlock state; p.getStateInformation(state);
    VocalPureProcessor restored; restored.setStateInformation(state.getData(),int(state.getSize()));
    require(restored.state.getRawParameterValue("width")->load()==150 && restored.state.getRawParameterValue("compare")->load()==1,"Phase3 state failed");
    std::cout<<"PASS: phase3 FIFO saturation/order, calibrated stereo FFT, spectrum audio invariance, width/mono sum, compare, seven presets, persistence.\n";
}

void render(const juce::File& source,const juce::File& destination,float amount)
{
    require(!destination.existsAsFile(),"Output already exists; choose a new path");
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    require(reader!=nullptr,"Cannot read input audio");
    require(reader->numChannels==1 || reader->numChannels==2,"Only mono/stereo supported");
    VocalPureProcessor p; prepare(p,int(reader->sampleRate),int(reader->numChannels),amount);
    require(p.engine.status.load()==0,"Unsupported input sample rate");
    destination.getParentDirectory().createDirectory();
    auto stream=destination.createOutputStream(); require(stream!=nullptr,"Cannot create output");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),reader->sampleRate,reader->numChannels,24,{},0));
    require(writer!=nullptr,"Cannot create WAV writer"); stream.release();
    const int delay=p.getLatencySamples(),block=512;
    const auto length=reader->lengthInSamples;
    juce::AudioBuffer<float> buffer(int(reader->numChannels),block); juce::MidiBuffer midi;
    double totalMs=0,maxMs=0; juce::int64 blocks=0;
    for(juce::int64 offset=0;offset<length+delay;offset+=block)
    {
        const int n=int(std::min<juce::int64>(block,length+delay-offset));
        buffer.setSize(int(reader->numChannels),n,false,false,true); buffer.clear();
        if(offset<length) reader->read(&buffer,0,int(std::min<juce::int64>(n,length-offset)),offset,true,true);
        auto start=std::chrono::steady_clock::now(); p.processBlock(buffer,midi);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        totalMs+=ms; maxMs=std::max(maxMs,ms); ++blocks;
        const int skip=int(std::max<juce::int64>(0,delay-offset));
        if(skip<n) require(writer->writeFromAudioSampleBuffer(buffer,skip,n-skip),"WAV write failed");
    }
    writer.reset();
    std::cout<<"RENDER rate="<<reader->sampleRate<<" channels="<<reader->numChannels<<" samples="<<length
             <<" reduction="<<amount<<" latency_samples="<<delay<<" aligned=true mean_block_ms="<<totalMs/double(blocks)
             <<" max_block_ms="<<maxMs<<" realtime_cpu_percent="<<100*totalMs/(double(length)/reader->sampleRate*1000)<<'\n';
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if(argc==5 && std::string(argv[1])=="--render")
            render(juce::File(juce::String::fromUTF8(argv[2])),juce::File(juce::String::fromUTF8(argv[3])),std::stof(argv[4]));
        else { tests(); phase3Tests(); }
        return 0;
    }
    catch(const std::exception& e) { trackAllocations=false; std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
