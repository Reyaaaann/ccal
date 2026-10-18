#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theory.h"

juce::AudioProcessorValueTreeState::ParameterLayout CentsHzAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                StringArray { "Cents to Hz", "Hz to Cents" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "cents", 1 }, "Cents",
                NormalisableRange<float> (-12000.0f, 12000.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "targetHz", 1 }, "Target Hz",
                NormalisableRange<float> (1.0f, 20000.0f, 0.01f, 0.3f), 440.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "refHz", 1 }, "Reference Hz",
                NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.3f), 440.0f));

    StringArray roots;
    for (int i = 0; i < 12; ++i) roots.add (Theory::noteName (i));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rootKey", 1 }, "Root Key", roots, 0));

    StringArray scaleNames;
    for (auto& s : Theory::scales()) scaleNames.add (s.name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames, 1));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "relative", 1 }, "Relative Scale Mode", false));

    return layout;
}

CentsHzAudioProcessor::CentsHzAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool CentsHzAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

// This plugin is a calculator: audio passes through untouched.
void CentsHzAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* CentsHzAudioProcessor::createEditor()
{
    return new CentsHzAudioProcessorEditor (*this);
}

void CentsHzAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CentsHzAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CentsHzAudioProcessor();
}
