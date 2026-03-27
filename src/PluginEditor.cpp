#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setupControl (controls[0], "Dry", "dry");
    setupControl (controls[1], "Wet", "wet");
    setupControl (controls[2], "Room (ms)", "roomMs");
    setupControl (controls[3], "Decay (RT20 s)", "rt20");
    setupControl (controls[4], "Early", "early");
    setupControl (controls[5], "Detune", "detune");
    setupControl (controls[6], "Low Cut (Hz)", "lowCutHz");
    setupControl (controls[7], "Low Damp", "lowDampRate");
    setupControl (controls[8], "High Cut (Hz)", "highCutHz");
    setupControl (controls[9], "High Damp", "highDampRate");

    setSize (640, 420);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (22, 25, 30));

    auto bounds = getLocalBounds().reduced (12);
    auto title = bounds.removeFromTop (34);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("SMIRVERB", title, juce::Justification::centredLeft);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (12);
    bounds.removeFromTop (34);
    bounds.removeFromTop (8);

    constexpr int columns = 2;
    constexpr int rows = 5;
    const int cellWidth = bounds.getWidth() / columns;
    const int cellHeight = bounds.getHeight() / rows;

    for (int index = 0; index < static_cast<int> (controls.size()); ++index)
    {
        auto& control = controls[static_cast<size_t> (index)];
        const int column = index % columns;
        const int row = index / columns;
        juce::Rectangle<int> cell (bounds.getX() + column * cellWidth,
                                   bounds.getY() + row * cellHeight,
                                   cellWidth,
                                   cellHeight);
        cell = cell.reduced (8);

        auto labelArea = cell.removeFromTop (22);
        control.label.setBounds (labelArea);
        control.slider.setBounds (cell);
    }
}

void AudioPluginAudioProcessorEditor::setupControl (ParamControl& control,
                                                    const juce::String& text,
                                                    const juce::String& parameterId)
{
    control.label.setText (text, juce::dontSendNotification);
    control.label.setColour (juce::Label::textColourId, juce::Colours::white);
    control.label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (control.label);

    control.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    control.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 22);
    addAndMakeVisible (control.slider);

    control.attachment = std::make_unique<SliderAttachment> (processorRef.getAPVTS(), parameterId, control.slider);
}
