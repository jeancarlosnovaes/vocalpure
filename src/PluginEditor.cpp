#include "PluginEditor.h"
VocalPureEditor::VocalPureEditor(VocalPureProcessor& p)
 : AudioProcessorEditor(p),processor(p),inputAttachment(p.state,"inputGain",input),
   outputAttachment(p.state,"outputGain",output),reductionAttachment(p.state,"reduction",reduction),
   widthAttachment(p.state,"width",width),bypassAttachment(p.state,"bypass",bypass),compareAttachment(p.state,"compare",compare)
{
    setLookAndFeel(&theme);
    for(auto* slider:{&input,&output,&reduction,&width})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,22);
        slider->setTextValueSuffix(" dB"); slider->setDoubleClickReturnValue(true,0);
        slider->setTooltip("Drag to adjust. Shift: fine adjustment. Double-click: default. Arrow keys and mouse wheel supported.");
        addAndMakeVisible(slider);
    }
    input.setName("Input gain"); output.setName("Output gain"); reduction.setName("Noise reduction"); width.setName("Stereo width");
    reduction.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); reduction.setTextValueSuffix(" %"); reduction.setDoubleClickReturnValue(true,65);
    reduction.setTooltip("Drag to adjust reduction: grey 0%, pink 100%. The moving centre shows the real input spectrum. Double-click: 65%.");
    width.setTextValueSuffix(" %"); width.setDoubleClickReturnValue(true,100);
    width.setTooltip("Mid/side width. 0% mono, 100% original width, 200% double side. No effect on mono input.");
    for(size_t i=0;i<factoryPresets.size();++i) presets.addItem(factoryPresets[i].name,int(i)+1);
    presets.setTextWhenNothingSelected("CUSTOM / SELECT PRESET"); presets.setName("Factory presets");
    presets.onChange=[this] { if(presets.getSelectedId()>0) { saveUndo(); selectedPreset=presets.getSelectedId()-1; processor.applyPreset(selectedPreset); } };
    views.addItem("COMPARE",3); views.addItem("INPUT",1); views.addItem("OUTPUT",2); views.setSelectedId(3);
    views.setName("Spectrum view"); views.onChange=[this]{spectrum.view=views.getSelectedId(); spectrum.repaint();};
    reset.setName("Reset parameters"); reset.setTooltip("Restore default settings");
    reset.onClick=[this]{saveUndo(); processor.resetParameters(); selectedPreset=-1; presets.setSelectedId(0,juce::dontSendNotification);};
    undo.setEnabled(false); undo.setTooltip("Undo the last preset or reset. Slider automation is managed by the host.");
    undo.onClick=[this]
    {
        for(size_t i=0;i<ids.size();++i) { auto* parameter=processor.state.getParameter(ids[i]); parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(undoValues[i])); parameter->endChangeGesture(); }
        undo.setEnabled(false); selectedPreset=-1; presets.setSelectedId(0,juce::dontSendNotification);
    };
    settings.onClick=[this]
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"VocalPure 0.3.0",
            "Local RNNoise 0.1.1 processing.\n\nSupported rates: 44.1, 48 and 96 kHz. Latency: 20 ms at 48 kHz; 26.67 ms otherwise.\n\n"
            "Compare and Bypass play the time-aligned original without changing your settings. Width uses mid/side; mono is unaffected.\n\n"
            "Spectrum: magenta input / pale cyan output, stereo power average. LEVEL CHANGE measures overall signal level, not isolated noise. VAD may give false positives.\n\n"
            "Seven presets change real intensity values. Additional neural profiles and Low CPU are not available yet.","OK",this);
    };
    bypass.setTooltip("Pass the latency-aligned original; smooth transition. Neural state remains active.");
    compare.setTooltip("Listen to the aligned original, then switch back without losing reduction, width or gain settings.");
    for(juce::Component* c:std::initializer_list<juce::Component*>{&presets,&views,&bypass,&compare,&reset,&undo,&settings,
                          &status,&voice,&delta,&spectrum,&inputMeter,&outputMeter}) addAndMakeVisible(c);
    for(auto* label:{&status,&voice,&delta}) { label->setColour(juce::Label::textColourId,juce::Colour(0xffa8becc)); label->setFont(juce::Font(juce::FontOptions(13))); }
    voice.setColour(juce::Label::textColourId,juce::Colour(0xffe6ffff));
    status.setJustificationType(juce::Justification::centredLeft); voice.setJustificationType(juce::Justification::centred);
    delta.setJustificationType(juce::Justification::centred);
    setResizable(true,true); setResizeLimits(800,448,1500,840); getConstrainer()->setFixedAspectRatio(1000.0/560);
    setSize(1000,560);
    SpectrumQueue::Frame stale; while(processor.engine.spectrum.pop(stale)) {}
    processor.engine.spectrum.enabled.store(true); startTimerHz(30);
}
VocalPureEditor::~VocalPureEditor() { stopTimer(); processor.engine.spectrum.enabled.store(false); setLookAndFeel(nullptr); }
void VocalPureEditor::saveUndo()
{ for(size_t i=0;i<ids.size();++i) undoValues[i]=processor.state.getRawParameterValue(ids[i])->load(); undo.setEnabled(true); }
void VocalPureEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff087c9b));
    g.addTransform(juce::AffineTransform::scale(float(getWidth())/1000,float(getHeight())/560));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff27d4de),480,45,juce::Colour(0xff056487),220,560,false));
    g.fillRect(0,42,1000,518);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x004cbdd8),420,100,juce::Colour(0x995247bd),1000,550,false));
    g.fillRect(0,42,1000,518);
    // Original static network texture, independent of audio and model state.
    const std::array<juce::Point<float>,16> nodes {{{20,160},{140,130},{230,205},{340,105},{440,170},{600,95},{730,170},{930,110},
        {75,350},{190,440},{350,360},{490,460},{650,390},{800,460},{950,330},{990,220}}};
    for(size_t i=0;i<nodes.size();++i)
    {
        g.setColour(juce::Colour(0xffb0f4f5).withAlpha(0.10f));
        g.drawLine({nodes[i],nodes[(i+3)%nodes.size()]},1); g.drawLine({nodes[i],nodes[(i+6)%nodes.size()]},0.6f);
        g.setColour(juce::Colour(0xffd9f9ff).withAlpha(0.17f)); g.fillEllipse(nodes[i].x-2.5f,nodes[i].y-2.5f,5,5);
    }
    juce::Path shadow; shadow.startNewSubPath(445,380); shadow.lineTo(618,230); shadow.lineTo(905,490); shadow.lineTo(560,490); shadow.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x77521588),500,350,juce::Colour(0x00481983),870,540,false)); g.fillPath(shadow);
    g.setColour(juce::Colour(0xff10151c)); g.fillRect(0,0,1000,42);
    g.setColour(juce::Colour(0xff7bdfe4)); g.fillRect(0,42,1000,2);
    auto text=[&](juce::String t,int x,int y,int w,int h,float font,juce::Colour colour,
                 juce::Justification align=juce::Justification::centredLeft)
    { g.setColour(colour); g.setFont(font); g.drawText(t,x,y,w,h,align); };
    const juce::Colour dark(0xff073c50),white(0xffecffff);
    text("V O C A L P U R E",42,65,400,40,28,dark);
    text("LOCAL NEURAL AUDIO",734,72,240,30,17,dark);
    text("0.3.0",44,107,120,18,11,dark);
    text("NEURAL ENGINE",40,219,235,26,17,white,juce::Justification::centred);
    g.setColour(juce::Colour(0xff8be1e6)); g.fillRoundedRectangle(40,252,235,57,28);
    g.setColour(juce::Colour(0xff101b22)); g.fillRoundedRectangle(44,256,227,49,24);
    text("RNNoise",44,263,227,34,25,white,juce::Justification::centred);
    text("ANALYSIS",40,332,144,22,15,white,juce::Justification::centred);
    text("WIDTH",196,332,94,22,15,white,juce::Justification::centred);
    g.setColour(juce::Colour(0xff8be1e6)); g.fillRoundedRectangle(40,366,144,52,25);
    g.setColour(juce::Colour(0xff10232d)); g.fillRoundedRectangle(44,370,136,44,22);
    text(processor.getTotalNumInputChannels()==1?"MONO":"DUAL",44,380,136,24,20,white,juce::Justification::centred);
    text("NOISE REDUCTION",324,110,400,24,15,white,juce::Justification::centred);
    text("LIVE SPECTRUM",747,180,226,22,14,white,juce::Justification::centred);
    g.setColour(juce::Colour(0xff101e2c).withAlpha(0.95f)); g.fillRoundedRectangle(20,489,960,51,12);
    g.setColour(juce::Colour(0xffeb69e2)); g.fillEllipse(788,449,5,5); text("IN",800,442,30,18,10,white);
    g.setColour(juce::Colour(0xffa4fff5)); g.fillEllipse(851,449,5,5); text("OUT",864,442,40,18,10,white);
}
void VocalPureEditor::resized()
{
    const float sx=float(getWidth())/1000,sy=float(getHeight())/560;
    auto place=[&](juce::Component& c,int x,int y,int w,int h){c.setBounds(juce::Rectangle<float>(float(x)*sx,float(y)*sy,float(w)*sx,float(h)*sy).toNearestInt());};
    place(undo,16,7,82,28); place(presets,112,7,330,28); place(compare,469,7,225,28);
    place(settings,732,7,74,28); place(bypass,845,7,140,28);
    place(reset,131,155,52,46); place(width,196,354,94,105);
    place(reduction,324,126,400,340); place(voice,365,466,315,20);
    place(spectrum,747,211,226,226); place(views,780,459,165,26);
    place(inputMeter,37,497,187,37); place(input,243,491,85,43);
    place(outputMeter,365,497,187,37); place(output,571,491,85,43);
    input.setSliderStyle(juce::Slider::LinearHorizontal); output.setSliderStyle(juce::Slider::LinearHorizontal);
    place(delta,693,503,265,25); place(status,28,540,935,20);
}
void VocalPureEditor::timerCallback()
{
    SpectrumQueue::Frame frame; bool found=false;
    // Drain bounded queue to the newest complete snapshot; FFT runs only here.
    for(int i=0;i<SpectrumQueue::capacity;++i) { if(!processor.engine.spectrum.pop(frame)) break; found=true; }
    if(found) { spectrum.update(frame); emptyTicks=0; } else if(++emptyTicks==15) spectrum.clearDisplay();
    if(found || emptyTicks==15) { theme.spectrumLevels=spectrum.radialLevels(); reduction.repaint(); }
    const float in=processor.engine.inputPeak.load(),out=processor.engine.outputPeak.load();
    inputMeter.update(emptyTicks<15?in:0); outputMeter.update(emptyTicks<15?out:0);
    voice.setText("VOICE PROBABILITY  "+juce::String(emptyTicks<15?int(processor.engine.voiceProbability.load()*100):0)+"%",juce::dontSendNotification);
    const bool dry=processor.state.getRawParameterValue("bypass")->load()>0.5f || processor.state.getRawParameterValue("compare")->load()>0.5f;
    delta.setText(dry?"LISTENING TO ORIGINAL":emptyTicks>=15 || in<1e-7f?"NO SIGNAL":
                  "LEVEL CHANGE  "+juce::String(processor.engine.attenuationDb.load(),1)+" dB",juce::dontSendNotification);
    const int error=processor.engine.status.load(); juce::String message;
    if(error==1) message="Unsupported rate - original audio passed through";
    else if(error==2) message="Neural engine failed - aligned dry audio";
    else if(error==3) message="Invalid input samples removed";
    else message="RNNoise ready  /  "+juce::String(processor.engine.reportedLatency.load())+" samples latency  /  "+(in<1e-7f || emptyTicks>=15?"No audio input":"Audio active");
    status.setText(message,juce::dontSendNotification);
    width.setEnabled(processor.getTotalNumInputChannels()==2);
    if(selectedPreset>=0)
    {
        const auto preset=factoryPresets[size_t(selectedPreset)];
        if(std::abs(reduction.getValue()-preset.reduction)>0.1 || std::abs(width.getValue()-preset.width)>0.1
           || std::abs(input.getValue())>0.01 || std::abs(output.getValue())>0.01)
        { selectedPreset=-1; presets.setSelectedId(0,juce::dontSendNotification); }
    }
}
