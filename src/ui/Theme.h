#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class VocalTheme final : public juce::LookAndFeel_V4
{
public:
    VocalTheme()
    {
        setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffdfe9f0));
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff304457));
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff182734));
        setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff365162));
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff213545));
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff29665f));
        setColour(juce::ToggleButton::textColourId,juce::Colour(0xffdfe9f0));
        setColour(juce::ToggleButton::tickColourId,juce::Colour(0xff59ead2));
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff182734));
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,
                          float start,float end,juce::Slider& slider) override
    {
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(10);
        const auto centre=bounds.getCentre(); const float radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*0.5f;
        const float angle=start+position*(end-start),stroke=radius>75?7.0f:4.0f;
        const float alpha=slider.isEnabled()?1.0f:0.35f;
        juce::Path track,arc;
        track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
        g.setColour(juce::Colour(0xff263c4c)); g.strokePath(track,juce::PathStrokeType(stroke));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff48dfcc).withAlpha(alpha),bounds.getX(),bounds.getBottom(),
                                              juce::Colour(0xffbd74e7).withAlpha(alpha),bounds.getRight(),bounds.getY(),false));
        g.strokePath(arc,juce::PathStrokeType(stroke,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float dotX=centre.x+std::sin(angle)*radius,dotY=centre.y-std::cos(angle)*radius;
        g.setColour(juce::Colour(0xffc6fff3).withAlpha(alpha)); g.fillEllipse(dotX-5,dotY-5,10,10);
        if(slider.getName()=="Noise reduction")
        {
            g.setColour(juce::Colour(0xffedf8f6)); g.setFont(radius*0.42f);
            g.drawText(juce::String(int(std::round(slider.getValue())))+"%",bounds,juce::Justification::centred);
        }
        if(slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true))
        { g.setColour(juce::Colour(0xff6aebda).withAlpha(0.35f)); g.drawEllipse(bounds.reduced(12),1.2f); }
    }
};
