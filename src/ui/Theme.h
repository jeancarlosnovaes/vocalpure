#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class VocalTheme final : public juce::LookAndFeel_V4
{
public:
    std::array<float,32> spectrumLevels {};
    void drawComboBox(juce::Graphics& g,int w,int h,bool down,int bx,int by,int bw,int bh,juce::ComboBox& box) override
    {
        if(box.getName()!="RNNoise profile")
        { juce::LookAndFeel_V4::drawComboBox(g,w,h,down,bx,by,bw,bh,box); return; }
        const auto bounds=juce::Rectangle<float>(0,0,float(w),float(h));
        g.setColour(juce::Colour(0xff8be1e6)); g.fillRoundedRectangle(bounds,float(h)*0.5f);
        g.setColour(juce::Colour(down?0xff213545:0xff101b22));
        g.fillRoundedRectangle(bounds.reduced(4),float(h)*0.45f);
        juce::Path arrow; const float x=float(w)-22,y=float(h)*0.5f;
        arrow.startNewSubPath(x-5,y-3); arrow.lineTo(x,y+3); arrow.lineTo(x+5,y-3);
        g.setColour(juce::Colour(0xffecffff)); g.strokePath(arrow,juce::PathStrokeType(2));
    }
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    { return box.getName()=="RNNoise profile"?juce::Font(juce::FontOptions(float(box.getHeight())*0.40f)):juce::LookAndFeel_V4::getComboBoxFont(box); }
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
    void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool highlighted,bool down) override
    {
        if(button.getName()!="Reset parameters")
        { juce::LookAndFeel_V4::drawButtonText(g,button,highlighted,down); return; }
        const auto centre=button.getLocalBounds().toFloat().getCentre();
        const float r=juce::jmin(float(button.getWidth()),float(button.getHeight()))*0.28f;
        juce::Path arrow;
        arrow.addCentredArc(centre.x,centre.y,r,r,0,-0.65f,4.7f,true);
        g.setColour(juce::Colour(0xffe6ffff));
        g.strokePath(arrow,juce::PathStrokeType(2.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const juce::Point<float> tip(centre.x+std::sin(-0.65f)*r,centre.y-std::cos(-0.65f)*r);
        juce::Path head;
        head.startNewSubPath(tip+juce::Point<float>(-2,-5)); head.lineTo(tip+juce::Point<float>(-3,4));
        head.lineTo(tip+juce::Point<float>(6,3)); head.closeSubPath(); g.fillPath(head);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,
                          float start,float end,juce::Slider& slider) override
    {
        const bool main=slider.getName()=="Noise reduction";
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(main?6.0f:12.0f);
        const auto centre=bounds.getCentre();
        // Reserve space for the rotating skirt at every angle, including the endpoints.
        const float radius=juce::jmin(bounds.getWidth(),bounds.getHeight())*(main?0.43f:0.5f);
        const float angle=start+position*(end-start),stroke=radius>75?7.0f:4.0f;
        const float alpha=slider.isEnabled()?1.0f:0.35f;
        const auto disc=juce::Rectangle<float>(centre.x-radius,centre.y-radius,radius*2,radius*2);
        const juce::ColourGradient rim(juce::Colour(0xff64b9c5),disc.getX(),centre.y,
                                       juce::Colour(0xffed22d3),disc.getRight(),centre.y,false);
        const auto pointerColour=rim.getColourAtPosition((std::sin(angle)+1.0f)*0.5f);
        const auto coreColour=juce::Colour(0xff797b83).interpolatedWith(juce::Colour(0xffef23da),position);
        float activity=0;
        for(auto level:spectrumLevels) activity=juce::jmax(activity,level);
        if(main)
        {
            const juce::Point<float> direction(std::sin(angle),-std::cos(angle));
            const juce::Point<float> tangent(std::cos(angle),std::sin(angle));
            const auto tip=centre+direction*(radius*1.20f);
            const auto left=centre+direction*(radius*0.70f)-tangent*(radius*0.70f);
            const auto right=centre+direction*(radius*0.70f)+tangent*(radius*0.70f);
            juce::Path skirt;
            skirt.startNewSubPath(left); skirt.lineTo(tip); skirt.lineTo(right); skirt.closeSubPath();
            juce::DropShadow(juce::Colours::black.withAlpha(0.45f),12,{4,7}).drawForPath(g,skirt);
            g.setGradientFill(juce::ColourGradient(pointerColour.darker(0.22f),left,
                                                  pointerColour,tip,false));
            g.fillPath(skirt);
            g.setColour(pointerColour.brighter(0.35f).withAlpha(0.5f));
            g.drawLine({left,tip},1.2f);
            // The dark radial notch is the position marker; the skirt sits under the rim.
            g.setColour(juce::Colour(0xff27102e));
            g.drawLine({centre+direction*(radius*0.96f),centre+direction*(radius*1.175f)},radius*0.030f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.18f)); g.fillEllipse(disc.translated(4,7).expanded(6));
        g.setGradientFill(rim);
        g.fillEllipse(disc.expanded(main?5.0f:3.0f));
        g.setColour(juce::Colour(0xff081317)); g.fillEllipse(disc);
        g.setGradientFill(juce::ColourGradient(main?coreColour.withMultipliedBrightness(0.32f+activity*0.68f):juce::Colour(0xff234657),centre.x,centre.y,
                                              juce::Colour(0xff090d17),centre.x+radius*0.89f,centre.y,true));
        g.fillEllipse(disc.reduced(main?10.0f:5.0f));
        if(main)
        {
            // Log-frequency input spectrum: each vertex follows a measured FFT band.
            std::array<juce::Point<float>,32> points;
            for(size_t i=0;i<points.size();++i)
            { const float a=float(i)*juce::MathConstants<float>::twoPi/float(points.size());
              const float r=radius*(0.22f+0.54f*spectrumLevels[i]);
              points[i]={centre.x+std::sin(a)*r,centre.y+std::cos(a)*r}; }
            for(size_t i=0;i<points.size();++i)
            {
                g.setColour(coreColour.brighter(0.7f).withAlpha(0.55f*activity));
                g.drawLine({points[i],points[(i+1)%points.size()]},1.1f);
                g.drawLine({points[i],points[(i+4)%points.size()]},0.7f);
                g.setColour(coreColour.brighter(1.2f).withAlpha(activity)); g.fillEllipse(points[i].x-1.7f,points[i].y-1.7f,3.4f,3.4f);
            }
            const auto hub=disc.withSizeKeepingCentre(radius*0.28f,radius*0.28f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff292a2e),centre.x,centre.y,
                                                   juce::Colour(0xff090c11),centre.x+hub.getWidth()*0.5f,centre.y,true));
            g.fillEllipse(hub);
            g.setColour(coreColour.withAlpha(0.3f));
            g.drawEllipse(hub,1.0f);
        }
        if(!main)
        {
        juce::Path track,arc;
        track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        arc.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
        g.setColour(juce::Colour(0xff25333c)); g.strokePath(track,juce::PathStrokeType(stroke));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa5e4ed).withAlpha(alpha),bounds.getX(),bounds.getBottom(),
                                              juce::Colour(0xfff23cec).withAlpha(alpha),bounds.getRight(),bounds.getY(),false));
        g.strokePath(arc,juce::PathStrokeType(stroke,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float dotX=centre.x+std::sin(angle)*radius,dotY=centre.y-std::cos(angle)*radius;
        g.setColour(juce::Colour(0xffc6fff3).withAlpha(alpha)); g.fillEllipse(dotX-5,dotY-5,10,10);
        }
    }
};
