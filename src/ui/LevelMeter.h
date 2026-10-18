#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter(juce::String title):name(std::move(title)) {}
    void update(float peak) { db=juce::jmax(juce::Decibels::gainToDecibels(peak,-100.0f),db-1.5f); repaint(); }
    void paint(juce::Graphics& g) override
    {
        auto area=getLocalBounds().toFloat(); g.setFont(12); g.setColour(juce::Colour(0xffc1d1dd));
        g.drawText(name+"  "+juce::String(db,1)+" dBFS",area.removeFromTop(22),juce::Justification::centredLeft);
        area.removeFromBottom(6); g.setColour(juce::Colour(0xff263c4c)); g.fillRoundedRectangle(area,3);
        area.setWidth(area.getWidth()*juce::jlimit(0.0f,1.0f,(db+60)/60));
        g.setColour(db>=0?juce::Colour(0xffff6582):juce::Colour(0xff53ddc9)); g.fillRoundedRectangle(area,3);
    }
private:
    juce::String name; float db=-100;
};
