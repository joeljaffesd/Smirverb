#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr auto dryId = "dry";
constexpr auto wetId = "wet";
constexpr auto roomMsId = "roomMs";
constexpr auto rt20Id = "rt20";
constexpr auto earlyId = "early";
constexpr auto detuneId = "detune";
constexpr auto lowCutHzId = "lowCutHz";
constexpr auto lowDampRateId = "lowDampRate";
constexpr auto highCutHzId = "highCutHz";
constexpr auto highDampRateId = "highDampRate";

juce::NormalisableRange<float> makeSkewedRange (float min, float max, float centre)
{
    juce::NormalisableRange<float> range (min, max);
    range.setSkewForCentre (centre);
    return range;
}

juce::String gainToText (float value)
{
    if (value <= 0.0f)
        return "off";

    const auto db = juce::Decibels::gainToDecibels (value);
    return juce::String (db, 2) + " dB";
}

float textToGain (const juce::String& text)
{
    const auto lower = text.trim().toLowerCase();
    if (lower == "off")
        return 0.0f;

    if (lower.containsIgnoreCase ("db"))
        return juce::Decibels::decibelsToGain (text.getFloatValue());

    return text.getFloatValue();
}

juce::String hzToText (float value)
{
    if (value >= 1000.0f)
        return juce::String (value / 1000.0f, 2) + " kHz";

    return juce::String (value, value < 100.0f ? 1 : 0) + " Hz";
}

float textToHz (const juce::String& text)
{
    const auto lower = text.toLowerCase();
    const auto value = text.getFloatValue();
    return lower.contains ("khz") ? value * 1000.0f : value;
}

juce::String secondsToText (float value)
{
    if (value < 0.1f)
        return juce::String (value * 1000.0f, 1) + " ms";

    return juce::String (value, value < 10.0f ? 2 : 1) + " s";
}

float textToSeconds (const juce::String& text)
{
    const auto lower = text.toLowerCase();
    const auto value = text.getFloatValue();
    return lower.contains ("ms") ? value * 0.001f : value;
}

juce::String ratioToPercentText (float value)
{
    return juce::String (value * 100.0f, 1) + "%";
}

float textToRatio (const juce::String& text)
{
    return text.getFloatValue() * 0.01f;
}
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
         , apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return cachedTailSeconds;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto maxBlockSize = static_cast<size_t> (juce::jmax (1, samplesPerBlock));
    reverb.configure (sampleRate, maxBlockSize, 2);
    reverb.reset();
    updateReverbParameters();
    stereoBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != out)
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (totalNumInputChannels < 1 || totalNumOutputChannels < 1)
    {
        buffer.clear();
        return;
    }

    updateReverbParameters();

    const int numSamples = buffer.getNumSamples();
    const bool isMono = (totalNumInputChannels == 1 && totalNumOutputChannels == 1);

    if (isMono)
    {
        stereoBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        stereoBuffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        float* io[2] = { stereoBuffer.getWritePointer (0), stereoBuffer.getWritePointer (1) };
        reverb.process (io, io, static_cast<size_t> (numSamples));

        buffer.copyFrom (0, 0, stereoBuffer, 0, 0, numSamples);
        buffer.addFrom  (0, 0, stereoBuffer, 1, 0, numSamples);
        buffer.applyGain (0, 0, numSamples, 0.5f);
    }
    else
    {
        float* io[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
        reverb.process (io, io, static_cast<size_t> (numSamples));
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    using Param = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Param> (
        dryId, "dry", makeSkewedRange (0.0f, 4.0f, 1.0f), 1.0f,
        "dB", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return gainToText (v); },
        [] (const juce::String& s) { return textToGain (s); }));

    params.push_back (std::make_unique<Param> (
        wetId, "wet", makeSkewedRange (0.0f, 4.0f, 1.0f), juce::Decibels::decibelsToGain (-6.1f),
        "dB", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return gainToText (v); },
        [] (const juce::String& s) { return textToGain (s); }));

    params.push_back (std::make_unique<Param> (
        roomMsId, "room", makeSkewedRange (10.0f, 200.0f, 100.0f), 80.0f,
        "ms", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " ms"; },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    params.push_back (std::make_unique<Param> (
        rt20Id, "decay", makeSkewedRange (0.01f, 30.0f, 2.0f), 1.0f,
        "s", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return secondsToText (v); },
        [] (const juce::String& s) { return textToSeconds (s); }));

    params.push_back (std::make_unique<Param> (
        earlyId, "early", makeSkewedRange (0.0f, 2.5f, 1.0f), 1.5f,
        "%", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return ratioToPercentText (v); },
        [] (const juce::String& s) { return textToRatio (s); }));

    params.push_back (std::make_unique<Param> (
        detuneId, "detune", makeSkewedRange (0.0f, 50.0f, 5.0f), 2.0f,
        "", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 2); },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    params.push_back (std::make_unique<Param> (
        lowCutHzId, "low cut", makeSkewedRange (10.0f, 500.0f, 80.0f), 80.0f,
        "Hz", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return hzToText (v); },
        [] (const juce::String& s) { return textToHz (s); }));

    params.push_back (std::make_unique<Param> (
        lowDampRateId, "low damp", makeSkewedRange (1.0f, 10.0f, 2.0f), 1.5f,
        "", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 2); },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    params.push_back (std::make_unique<Param> (
        highCutHzId, "high cut", makeSkewedRange (1000.0f, 20000.0f, 5000.0f), 12000.0f,
        "Hz", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return hzToText (v); },
        [] (const juce::String& s) { return textToHz (s); }));

    params.push_back (std::make_unique<Param> (
        highDampRateId, "high damp", makeSkewedRange (1.0f, 10.0f, 2.0f), 2.5f,
        "", juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 2); },
        [] (const juce::String& s) { return s.getFloatValue(); }));

    return { params.begin(), params.end() };
}

void AudioPluginAudioProcessor::updateReverbParameters()
{
    reverb.dry = apvts.getRawParameterValue (dryId)->load();
    reverb.wet = apvts.getRawParameterValue (wetId)->load();
    reverb.roomMs = apvts.getRawParameterValue (roomMsId)->load();
    reverb.rt20 = apvts.getRawParameterValue (rt20Id)->load();
    reverb.early = apvts.getRawParameterValue (earlyId)->load();
    reverb.detune = apvts.getRawParameterValue (detuneId)->load();
    reverb.lowCutHz = apvts.getRawParameterValue (lowCutHzId)->load();
    reverb.lowDampRate = apvts.getRawParameterValue (lowDampRateId)->load();
    reverb.highCutHz = apvts.getRawParameterValue (highCutHzId)->load();
    reverb.highDampRate = apvts.getRawParameterValue (highDampRateId)->load();

    cachedTailSeconds = static_cast<double> (apvts.getRawParameterValue (rt20Id)->load()) * 3.0;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
