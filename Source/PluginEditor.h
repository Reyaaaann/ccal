#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MainContent;

class CentsHzAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CentsHzAudioProcessorEditor (CentsHzAudioProcessor&);
    ~CentsHzAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    std::unique_ptr<MainContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CentsHzAudioProcessorEditor)
};
