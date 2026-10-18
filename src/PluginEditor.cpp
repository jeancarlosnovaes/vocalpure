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
    reduction.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,22); reduction.setTextValueSuffix(" %"); reduction.setDoubleClickReturnValue(true,65);
    reduction.setTooltip("Real RNNoise processing. 0%: aligned dry signal. 100%: neural output. Intermediate values blend both.");
    width.setTextValueSuffix(" %"); width.setDoubleClickReturnValue(true,100);
    width.setTooltip("Mid/side width. 0% mono, 100% original width, 200% double side. No effect on mono input.");
    for(size_t i=0;i<factoryPresets.size();++i) presets.addItem(factoryPresets[i].name,int(i)+1);
    presets.setTextWhenNothingSelected("CUSTOM / SELECT PRESET"); presets.setName("Factory presets");
    presets.onChange=[this] { if(presets.getSelectedId()>0) { saveUndo(); selectedPreset=presets.getSelectedId()-1; processor.applyPreset(selectedPreset); } };
    views.addItem("COMPARE",3); views.addItem("INPUT",1); views.addItem("OUTPUT",2); views.setSelectedId(3);
    views.setName("Spectrum view"); views.onChange=[this]{spectrum.view=views.getSelectedId(); spectrum.repaint();};
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
            "Spectrum: purple input / cyan output, stereo power average. LEVEL CHANGE measures overall signal level, not isolated noise. VAD may give false positives.\n\n"
            "Seven presets change real intensity values. Additional neural profiles and Low CPU are not available yet.","OK",this);
    };
    bypass.setTooltip("Pass the latency-aligned original; smooth transition. Neural state remains active.");
    compare.setTooltip("Listen to the aligned original, then switch back without losing reduction, width or gain settings.");
    for(juce::Component* c:std::initializer_list<juce::Component*>{&presets,&views,&bypass,&compare,&reset,&undo,&settings,
                          &status,&voice,&delta,&spectrum,&inputMeter,&outputMeter}) addAndMakeVisible(c);
    for(auto* label:{&status,&voice,&delta}) { label->setColour(juce::Label::textColourId,juce::Colour(0xffa8becc)); label->setFont(juce::Font(juce::FontOptions(13))); }
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
    g.fillAll(juce::Colour(0xff0b121c));
    g.addTransform(juce::AffineTransform::scale(float(getWidth())/1000,float(getHeight())/560));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff172c38),0,0,juce::Colour(0xff211b32),1000,560,false));
    g.fillRect(0,0,1000,560);
    g.setColour(juce::Colour(0xff10202b));
    for(auto r:{juce::Rectangle<float>(20,100,220,344),{254,100,362,344},{630,100,350,344},{20,460,960,72}}) g.fillRoundedRectangle(r,12);
    auto text=[&](juce::String t,int x,int y,int w,int h,float font,juce::Colour colour)
    { g.setColour(colour); g.setFont(font); g.drawText(t,x,y,w,h,juce::Justification::centredLeft); };
    const juce::Colour white(0xffe0eff0),muted(0xff8ca6b7),cyan(0xff56ddc9);
    text("VOCALPURE",28,23,235,36,28,white); text("0.3.0   /   LOCAL NEURAL AUDIO",30,61,340,18,11,muted);
    text("NEURAL ENGINE",38,118,180,22,12,muted); text("RNNoise",38,149,180,32,23,white);
    text("Speech model / dual channel",38,185,185,22,12,muted); text("STEREO WIDTH",38,224,180,20,12,muted);
    text("NOISE REDUCTION",330,121,230,24,15,white);
    text("LIVE SPECTRUM",648,118,170,22,12,white);
    text("INPUT",53,470,80,18,11,muted); text("GAIN",265,470,65,18,11,muted);
    text("OUTPUT",393,470,90,18,11,muted); text("GAIN",605,470,65,18,11,muted);
    g.setColour(juce::Colour(0xffb586df)); g.fillEllipse(654,417,6,6); text("Original",666,409,80,22,11,muted);
    g.setColour(cyan); g.fillEllipse(750,417,6,6); text("Processed",762,409,100,22,11,muted);
}
void VocalPureEditor::resized()
{
    const float sx=float(getWidth())/1000,sy=float(getHeight())/560;
    auto place=[&](juce::Component& c,int x,int y,int w,int h){c.setBounds(juce::Rectangle<float>(float(x)*sx,float(y)*sy,float(w)*sx,float(h)*sy).toNearestInt());};
    place(presets,465,29,230,32); place(settings,710,29,65,32); place(bypass,800,29,150,32);
    place(width,72,246,116,126); place(reset,38,392,84,30); place(undo,137,392,84,30);
    place(reduction,292,158,286,250); place(voice,282,411,306,22);
    place(views,820,114,140,29); place(spectrum,642,152,324,246);
    place(inputMeter,43,486,200,40); place(outputMeter,383,486,200,40);
    place(input,261,480,85,48); place(output,601,480,85,48);
    input.setSliderStyle(juce::Slider::LinearHorizontal); output.setSliderStyle(juce::Slider::LinearHorizontal);
    place(compare,750,488,210,30); place(delta,708,465,255,22); place(status,28,536,935,20);
}
void VocalPureEditor::timerCallback()
{
    SpectrumQueue::Frame frame; bool found=false;
    // Drain bounded queue to the newest complete snapshot; FFT runs only here.
    for(int i=0;i<SpectrumQueue::capacity;++i) { if(!processor.engine.spectrum.pop(frame)) break; found=true; }
    if(found) { spectrum.update(frame); emptyTicks=0; } else if(++emptyTicks==15) spectrum.clearDisplay();
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
