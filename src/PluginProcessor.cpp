#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    for (const auto& item : {std::pair{"inputGain", "Input Gain"}, std::pair{"outputGain", "Output Gain"}})
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{item.first, 1}, item.second,
            juce::NormalisableRange<float>{-24, 24, 0.01f}, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass", 1}, "Bypass", false));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"reduction", 1}, "Noise Reduction",
        juce::NormalisableRange<float>{0,100,0.1f},65.0f,juce::AudioParameterFloatAttributes().withLabel("%")));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"width",1},"Stereo Width",
        juce::NormalisableRange<float>{0,200,0.1f},100.0f,juce::AudioParameterFloatAttributes().withLabel("%")));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"compare",1},"Compare Original",false));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"profile",1},"RNNoise Profile",
        juce::StringArray{"General","Strong","Eco"},0));
    return p;
}
}
VocalPureProcessor::VocalPureProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "VocalPureState", makeLayout()),
      input(state.getRawParameterValue("inputGain")), output(state.getRawParameterValue("outputGain")),
      bypass(state.getRawParameterValue("bypass")), reduction(state.getRawParameterValue("reduction")),
      width(state.getRawParameterValue("width")),compare(state.getRawParameterValue("compare")),
      profile(state.getRawParameterValue("profile")) {}
AudioParameters VocalPureProcessor::parameters() const noexcept
{ return {input->load(), output->load(), bypass->load() > 0.5f, reduction->load(),width->load(),compare->load()>0.5f,int(profile->load())}; }
void VocalPureProcessor::prepareToPlay(double rate, int)
{ engine.prepare(rate, getTotalNumInputChannels(), parameters()); setLatencySamples(engine.latencySamples()); }
bool VocalPureProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    const auto channels = b.getMainInputChannelSet();
    return (channels == juce::AudioChannelSet::mono() || channels == juce::AudioChannelSet::stereo())
        && channels == b.getMainOutputChannelSet();
}
void VocalPureProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{ juce::ScopedNoDenormals guard; midi.clear(); engine.process(audio, parameters()); }
void VocalPureProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{ juce::ScopedNoDenormals guard; midi.clear(); auto p = parameters(); p.bypass = true; engine.process(audio, p); }
juce::AudioProcessorParameter* VocalPureProcessor::getBypassParameter() const
{ return state.getParameter("bypass"); }
juce::AudioProcessorEditor* VocalPureProcessor::createEditor() { return new VocalPureEditor(*this); }
void VocalPureProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto tree = state.copyState(); tree.setProperty("schemaVersion", 4, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, data);
}
void VocalPureProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType()) && xml->getIntAttribute("schemaVersion", 1) <= 4)
        {
            auto tree=juce::ValueTree::fromXml(*xml);
            // Old projects must not acquire denoising simply by reopening them.
            if(!tree.getChildWithProperty("id","reduction").isValid())
            { juce::ValueTree item("PARAM"); item.setProperty("id","reduction",nullptr);
              item.setProperty("value",0.0f,nullptr); tree.appendChild(item,nullptr); }
            for(auto id:{"width","compare","profile"}) if(!tree.getChildWithProperty("id",id).isValid())
            { juce::ValueTree item("PARAM"); item.setProperty("id",id,nullptr);
              item.setProperty("value",juce::String(id)=="width"?100.0f:0.0f,nullptr); tree.appendChild(item,nullptr); }
            state.replaceState(tree);
        }
}
void VocalPureProcessor::resetParameters()
{
    for (auto id : {"inputGain", "outputGain", "bypass", "reduction", "width", "compare", "profile"})
    { auto* p = state.getParameter(id); p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
}
void VocalPureProcessor::applyPreset(int index)
{
    if(index<0 || index>=int(factoryPresets.size())) return;
    const auto preset=factoryPresets[size_t(index)];
    for(auto item:{std::pair{"reduction",preset.reduction},std::pair{"width",preset.width},
                  std::pair{"inputGain",0.0f},std::pair{"outputGain",0.0f},std::pair{"bypass",0.0f},std::pair{"compare",0.0f},
                  std::pair{"profile",float(preset.profile)}})
    { auto* p=state.getParameter(item.first); p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(item.second)); p->endChangeGesture(); }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalPureProcessor(); }
