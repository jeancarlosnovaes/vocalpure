#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class VocalTheme final : public juce::LookAndFeel_V4
{
public:
    VocalTheme()
    {
        setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffdfe9f0));
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff6acbd5));
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff102331));
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
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(12);
        const auto centre=bounds.getCentre(); const float radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*0.5f;
        const float angle=start+position*(end-start),stroke=radius>75?7.0f:4.0f;
        const float alpha=slider.isEnabled()?1.0f:0.35f;
        const bool main=slider.getName()=="Noise reduction";
        const auto disc=juce::Rectangle<float>(centre.x-radius,centre.y-radius,radius*2,radius*2);
        g.setColour(juce::Colours::black.withAlpha(0.18f)); g.fillEllipse(disc.translated(4,7).expanded(6));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb2e6e6),disc.getX(),disc.getY(),
                                              juce::Colour(0xffa33fc4),disc.getRight(),disc.getBottom(),false));
        g.fillEllipse(disc.expanded(main?5.0f:3.0f));
        g.setColour(juce::Colour(0xff081317)); g.fillEllipse(disc);
        g.setGradientFill(juce::ColourGradient(main?juce::Colour(0xffb920cf):juce::Colour(0xff234657),centre.x,centre.y,
                                              juce::Colour(0xff090d17),centre.x+radius*0.89f,centre.y,true));
        g.fillEllipse(disc.reduced(main?10.0f:5.0f));
        if(main)
        {
            // Static original mesh ornament, not an indication of neural activity.
            std::array<juce::Point<float>,15> points;
            for(size_t i=0;i<points.size();++i)
            { const float a=float(i)*juce::MathConstants<float>::twoPi/float(points.size());
              const float r=radius*(0.47f+0.22f*std::sin(float(i)*2.3f));
              points[i]={centre.x+std::sin(a)*r,centre.y+std::cos(a)*r}; }
            for(size_t i=0;i<points.size();++i)
            {
                g.setColour(juce::Colour(0xfff7a0fb).withAlpha(0.24f));
                g.drawLine({points[i],points[(i+1)%points.size()]},1.1f);
                g.drawLine({points[i],points[(i+4)%points.size()]},0.7f);
                g.setColour(juce::Colour(0xffffcafc).withAlpha(0.55f)); g.fillEllipse(points[i].x-1.7f,points[i].y-1.7f,3.4f,3.4f);
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff17132a),centre.x,centre.y,
                                                   juce::Colour(0xee090d17),centre.x+radius*0.3f,centre.y,true));
            g.fillEllipse(disc.withSizeKeepingCentre(radius*0.72f,radius*0.72f));
        }
        juce::Path track,arc;
        track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
        g.setColour(juce::Colour(0xff25333c)); g.strokePath(track,juce::PathStrokeType(stroke));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa5e4ed).withAlpha(alpha),bounds.getX(),bounds.getBottom(),
                                              juce::Colour(0xfff23cec).withAlpha(alpha),bounds.getRight(),bounds.getY(),false));
        g.strokePath(arc,juce::PathStrokeType(stroke,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float dotX=centre.x+std::sin(angle)*radius,dotY=centre.y-std::cos(angle)*radius;
        g.setColour(juce::Colour(0xffc6fff3).withAlpha(alpha)); g.fillEllipse(dotX-5,dotY-5,10,10);
        if(main)
        {
            juce::Path pointer;
            const juce::Point<float> direction(std::sin(angle),-std::cos(angle)),tangent(std::cos(angle),std::sin(angle));
            pointer.startNewSubPath(centre+direction*(radius+10));
            pointer.lineTo(centre+direction*(radius-15)+tangent*10);
            pointer.lineTo(centre+direction*(radius-15)-tangent*10); pointer.closeSubPath();
            g.setColour(juce::Colour(0xfffd53ed)); g.fillPath(pointer);
            g.setColour(juce::Colour(0xfffceaff)); g.setFont(radius*0.27f);
            g.drawText(juce::String(int(std::round(slider.getValue())))+"%",bounds,juce::Justification::centred);
        }
        if(slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true))
        { g.setColour(juce::Colour(0xff6aebda).withAlpha(0.35f)); g.drawEllipse(bounds.reduced(12),1.2f); }
    }
};
