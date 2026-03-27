#pragma once

#include "PluginProcessor.h"
#include <array>

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct ParamControl
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupControl (ParamControl& control,
                       const juce::String& text,
                       const juce::String& parameterId);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

    std::array<ParamControl, 10> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
