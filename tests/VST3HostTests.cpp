#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* message)
{ if(!ok) throw std::runtime_error(message); }

static juce::AudioProcessorParameter& parameter(juce::AudioProcessor& plugin,const juce::String& name)
{
    for(auto* p:plugin.getParameters()) if(p->getName(128)==name) return *p;
    throw std::runtime_error("Missing exported parameter: "+name.toStdString());
}

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        require(argc==2,"Pass the full VST3 bundle path");
        juce::VST3PluginFormat format; juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions,juce::String::fromUTF8(argv[1]));
        require(descriptions.size()==1,"VST3 scan did not find exactly one plugin");
        require(descriptions[0]->name=="VocalPure","Unexpected plugin name");
        require(descriptions[0]->category.contains("Restoration"),"VST3 must advertise the Restoration category");
        std::cout<<"VST3 scanned: "<<descriptions[0]->name<<" "<<descriptions[0]->version<<'\n';
        for(int rate:{44100,48000,96000}) for(int channels:{1,2}) for(float width:{0.0f,0.5f,1.0f})
        {
            juce::String error;
            auto plugin=format.createInstanceFromDescription(*descriptions[0],rate,512,error);
            require(plugin!=nullptr,error.toRawUTF8());
            auto layout=plugin->getBusesLayout();
            layout.inputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
            layout.outputBuses.set(0,juce::AudioChannelSet::canonicalChannelSet(channels));
            require(plugin->setBusesLayout(layout),"Exported VST3 rejected mono/stereo layout");
            parameter(*plugin,"Noise Reduction").setValueNotifyingHost(0);
            parameter(*plugin,"Stereo Width").setValueNotifyingHost(width);
            juce::MemoryBlock saved; plugin->getStateInformation(saved);
            parameter(*plugin,"Stereo Width").setValueNotifyingHost(0.37f);
            plugin->setStateInformation(saved.getData(),int(saved.getSize()));
            require(std::abs(parameter(*plugin,"Stereo Width").getValue()-width)<1e-6f,"VST3 state restore failed");
            plugin->setRateAndBufferSizeDetails(rate,512); plugin->prepareToPlay(rate,512);
            const int delay=plugin->getLatencySamples();
            require(delay==(rate==48000?960:rate==44100?1176:2560),"Exported VST3 latency mismatch");
            juce::AudioBuffer<float> audio(channels,512); juce::MidiBuffer midi;
            auto left=[rate](int n){return n<0?0.0f:0.08f*float(std::sin(2*juce::MathConstants<double>::pi*197*n/rate));};
            auto right=[rate](int n){return n<0?0.0f:0.05f*float(std::cos(2*juce::MathConstants<double>::pi*389*n/rate));};
            for(int offset=0;offset<rate/4;offset+=512)
            {
                for(int i=0;i<512;++i)
                { audio.setSample(0,i,left(offset+i)); if(channels==2) audio.setSample(1,i,right(offset+i)); }
                plugin->processBlock(audio,midi);
                // Ignore the initial parameter ramp; validate exported audio, not shared-code calls.
                if(offset<delay+rate/20) continue;
                for(int i=0;i<512;++i)
                {
                    const float l=left(offset+i-delay),r=right(offset+i-delay);
                    const float mid=(l+r)*0.5f,side=(l-r)*width;
                    require(std::abs(audio.getSample(0,i)-(channels==1?l:mid+side))<3e-6f,"VST3 output/PDC/width left mismatch");
                    if(channels==2) require(std::abs(audio.getSample(1,i)-(mid-side))<3e-6f,"VST3 width right mismatch");
                }
            }
            plugin->releaseResources();
        }
        std::cout<<"PASS: exported VST3 scan, repeated instantiate/release, mono/stereo, three rates, reported latency, aligned dry audio, width endpoints and state restore. Not a Resolve PDC test.\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
