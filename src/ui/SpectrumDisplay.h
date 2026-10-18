#pragma once
#include "../dsp/SpectrumAnalyzer.h"
#include <juce_gui_basics/juce_gui_basics.h>
class SpectrumDisplay final : public juce::Component
{
public:
    int view=3;
    SpectrumDisplay() { in.fill(-100); out.fill(-100); setName("Live input and output spectrum"); }
    void update(const SpectrumQueue::Frame& frame)
    {
        analyzer.analyse(frame);
        for(size_t i=0;i<in.size();++i) { in[i]+=0.4f*(analyzer.input[i]-in[i]); out[i]+=0.4f*(analyzer.output[i]-out[i]); }
        repaint();
    }
    void clearDisplay() { in.fill(-100); out.fill(-100); repaint(); }
    void paint(juce::Graphics& g) override
    {
        const float diameter=float(juce::jmin(getWidth(),getHeight()))-14;
        const auto circle=juce::Rectangle<float>(diameter,diameter).withCentre(getLocalBounds().toFloat().getCentre());
        g.setColour(juce::Colours::black.withAlpha(0.2f)); g.fillEllipse(circle.translated(3,6).expanded(4));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb3b1df),circle.getX(),circle.getY(),juce::Colour(0xffb43fbd),circle.getRight(),circle.getBottom(),false));
        g.fillEllipse(circle.expanded(3));
        g.setColour(juce::Colour(0xff0a1018)); g.fillEllipse(circle);
        auto plot=circle.reduced(diameter*0.17f,diameter*0.19f);
        g.setFont(11); const double top=juce::jmin(20000.0,analyzer.rate/2);
        auto xFor=[&](double f){return plot.getX()+float(std::log(f/20)/std::log(top/20))*plot.getWidth();};
        for(int db:{-24,-48,-72,-96})
        {
            const float y=plot.getY()-float(db)/100*plot.getHeight();
            g.setColour(juce::Colour(0xff263743)); g.drawHorizontalLine(int(y),plot.getX(),plot.getRight());
        }
        for(int f:{100,1000,10000})
        { const auto x=xFor(f); g.setColour(juce::Colour(0xff263743)); g.drawVerticalLine(int(x),plot.getY(),plot.getBottom());
          g.setColour(juce::Colour(0xff8097a8)); g.drawText(f==100?"100":f==1000?"1k":"10k",int(x)-15,int(plot.getBottom())+8,30,14,juce::Justification::centred); }
        auto draw=[&](const auto& values,juce::Colour colour)
        {
            juce::Path path;
            for(int px=0;px<int(plot.getWidth());++px)
            {
                const double f=20*std::pow(top/20,double(px)/plot.getWidth());
                const double next=20*std::pow(top/20,double(px+1)/plot.getWidth());
                const int a=juce::jlimit(1,SpectrumAnalyzer::bins-1,int(f*SpectrumQueue::size/analyzer.rate));
                const int b=juce::jlimit(a,SpectrumAnalyzer::bins-1,int(next*SpectrumQueue::size/analyzer.rate));
                float db=-100; for(int k=a;k<=b;++k) db=juce::jmax(db,values[size_t(k)]);
                const float x=plot.getX()+float(px),y=plot.getY()+juce::jlimit(0.0f,1.0f,-db/100)*plot.getHeight();
                if(px==0) path.startNewSubPath(x,y); else path.lineTo(x,y);
            }
            auto fill=path; fill.lineTo(plot.getRight()-1,plot.getBottom()); fill.lineTo(plot.getX(),plot.getBottom()); fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.5f),plot.getX(),plot.getY(),colour.withAlpha(0.02f),plot.getX(),plot.getBottom(),false));
            g.fillPath(fill); g.setColour(colour); g.strokePath(path,juce::PathStrokeType(1.9f));
        };
        if(view==1 || view==3) draw(in,juce::Colour(0xffeb69e2));
        if(view==2 || view==3) draw(out,juce::Colour(0xffa4fff5));
    }
private:
    SpectrumAnalyzer analyzer;
    std::array<float,SpectrumAnalyzer::bins> in,out;
};
