#pragma once
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/SpectrumDisplay.h"
#include "ui/LevelMeter.h"
#include <juce_gui_extra/juce_gui_extra.h>
class VocalPureEditor final : public juce::AudioProcessorEditor,private juce::Timer
{
public:
    explicit VocalPureEditor(VocalPureProcessor&);
    ~VocalPureEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void saveUndo();
    VocalPureProcessor& processor;
    VocalTheme theme;
    juce::Slider input,output,reduction,width;
    juce::ComboBox presets,views,profiles;
    juce::ToggleButton bypass {"BYPASS"},compare {"COMPARE ORIGINAL"};
    juce::TextButton reset {"RESET"},undo {"UNDO"},settings {"INFO"};
    juce::Label status,voice,delta;
    SpectrumDisplay spectrum;
    LevelMeter inputMeter {"INPUT"},outputMeter {"OUTPUT"};
    juce::AudioProcessorValueTreeState::SliderAttachment inputAttachment,outputAttachment,reductionAttachment,widthAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment,compareAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment profileAttachment;
    juce::TooltipWindow tooltips {this};
    static constexpr std::array<const char*,7> ids {"inputGain","outputGain","bypass","reduction","width","compare","profile"};
    std::array<float,7> undoValues {};
    int selectedPreset=-1,emptyTicks=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalPureEditor)
};
