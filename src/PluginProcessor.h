#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "audio/AudioEngine.h"
#include "state/FactoryPresets.h"

class VocalPureProcessor final : public juce::AudioProcessor
{
public:
    VocalPureProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VocalPure"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.04; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "RNNoise"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    void resetParameters();
    void applyPreset(int index);
    juce::AudioProcessorValueTreeState state;
    AudioEngine engine;
private:
    AudioParameters parameters() const noexcept;
    std::atomic<float>* input;
    std::atomic<float>* output;
    std::atomic<float>* bypass;
    std::atomic<float>* reduction;
    std::atomic<float>* width;
    std::atomic<float>* compare;
    std::atomic<float>* profile;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalPureProcessor)
};
