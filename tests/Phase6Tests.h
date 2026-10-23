#pragma once
#include "PluginEditor.h"

void phase6WidthTests()
{
    for(int rate:{44100,48000,96000}) for(float width:{0.0f,100.0f,200.0f})
    {
        std::vector<float> centred(size_t(rate/5));
        for(size_t i=0;i<centred.size();++i)
            centred[i]=0.1f*float(std::sin(2*juce::MathConstants<double>::pi*197*double(i)/rate));
        for(int channels:{1,2})
        {
            VocalPureProcessor p; set(p,"width",width); prepare(p,rate,channels,0);
            const auto out=run(p,centred,{32,137,512,7},channels);
            const auto delay=size_t(p.getLatencySamples());
            for(size_t i=0;i<out.size();++i)
                require(std::abs(out[i]-(i<delay?0:centred[i-delay]))<1e-6f,"Width altered mono/identical stereo");
        }
        for(int original=0;original<3;++original)
        {
            VocalPureProcessor p; set(p,"width",width); set(p,"compare",original==1?1.0f:0.0f);
            prepare(p,rate,2,0,original==2);
            const int delay=p.getLatencySamples();
            juce::AudioBuffer<float> b(2,137); juce::MidiBuffer midi;
            auto left=[&](int n){return n<0?0.0f:0.1f*float(std::sin(2*juce::MathConstants<double>::pi*197*n/rate));};
            auto right=[&](int n){return n<0?0.0f:0.07f*float(std::cos(2*juce::MathConstants<double>::pi*389*n/rate));};
            for(int offset=0;offset<rate/5;offset+=137)
            {
                for(int i=0;i<137;++i) { b.setSample(0,i,left(offset+i)); b.setSample(1,i,right(offset+i)); }
                allocations=0; trackAllocations=true; p.processBlock(b,midi); trackAllocations=false;
                require(allocations==0,"Width allocated inside callback");
                for(int i=0;i<137;++i)
                {
                    const float l=left(offset+i-delay),r=right(offset+i-delay);
                    const float mid=(l+r)*0.5f,side=(l-r)*0.5f*(original?1.0f:width/100);
                    require(std::abs(b.getSample(0,i)-(mid+side))<1e-6f,"Stereo width left/Compare/Bypass mismatch");
                    require(std::abs(b.getSample(1,i)-(mid-side))<1e-6f,"Stereo width right/Compare/Bypass mismatch");
                    require(std::abs(b.getSample(0,i)+b.getSample(1,i)-(l+r))<1e-6f,"Width changed mono sum");
                }
            }
        }
    }
    for(int mode=0;mode<4;++mode)
    {
        VocalPureProcessor p; set(p,"compare",mode==2?1.0f:0.0f); prepare(p,48000,mode==0?1:2,0,mode==3);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        juce::Component* control=nullptr; juce::Label* hint=nullptr;
        for(auto* c:editor->getChildren())
        {
            if(c->getName()=="Stereo width") control=c;
            if(c->getName()=="Stereo width availability") hint=dynamic_cast<juce::Label*>(c);
        }
        require(control && hint,"Width availability components missing");
        require(control->isEnabled()==(mode==1),"Width editor availability mismatch");
        require(hint->getText()==(mode==0?"MONO: NO EFFECT":mode==1?"STEREO WIDTH":"ORIGINAL: INACTIVE"),"Width hint mismatch");
    }
    std::cout<<"PASS: phase6 width at all rates, mono/identical stereo, real stereo, mono sum, Compare/Bypass, callback allocations and editor availability.\n";
}

void snapshotEditors(const juce::File& folder)
{
    require(!folder.exists(),"Snapshot folder exists"); require(folder.createDirectory().wasOk(),"Cannot create snapshot folder");
    for(int mode=0;mode<3;++mode) for(int width:{800,1000})
    {
        VocalPureProcessor p; set(p,"compare",mode==2?1.0f:0.0f); prepare(p,48000,mode==0?1:2,0);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        editor->setSize(width,width*560/1000);
        auto picture=editor->createComponentSnapshot(editor->getLocalBounds());
        auto output=folder.getChildFile(juce::String(mode==0?"mono":mode==1?"stereo":"compare")+"-"+juce::String(width)+".png").createOutputStream();
        require(output!=nullptr,"Cannot create snapshot"); juce::PNGImageFormat png;
        require(png.writeImageToStream(picture,*output),"Cannot write snapshot");
    }
}
