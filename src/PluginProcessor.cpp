#include "PluginProcessor.h"
#include "PluginEditor.h"

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
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
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

    if (totalNumInputChannels < 2 || totalNumOutputChannels < 2)
    {
        buffer.clear();
        return;
    }

    updateReverbParameters();

    float* io[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    reverb.process (io, io, static_cast<size_t> (buffer.getNumSamples()));
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

    params.push_back (std::make_unique<Param> (dryId, "Dry", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    params.push_back (std::make_unique<Param> (wetId, "Wet", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<Param> (roomMsId, "Room (ms)", juce::NormalisableRange<float> (10.0f, 200.0f), 80.0f));
    params.push_back (std::make_unique<Param> (rt20Id, "Decay (RT20 s)", juce::NormalisableRange<float> (0.01f, 30.0f, 0.0f, 0.35f), 1.0f));
    params.push_back (std::make_unique<Param> (earlyId, "Early", juce::NormalisableRange<float> (0.0f, 2.5f), 1.5f));
    params.push_back (std::make_unique<Param> (detuneId, "Detune", juce::NormalisableRange<float> (0.0f, 50.0f), 2.0f));
    params.push_back (std::make_unique<Param> (lowCutHzId, "Low Cut (Hz)", juce::NormalisableRange<float> (10.0f, 500.0f, 0.0f, 0.4f), 80.0f));
    params.push_back (std::make_unique<Param> (lowDampRateId, "Low Damp", juce::NormalisableRange<float> (1.0f, 10.0f), 1.5f));
    params.push_back (std::make_unique<Param> (highCutHzId, "High Cut (Hz)", juce::NormalisableRange<float> (1000.0f, 20000.0f, 0.0f, 0.35f), 12000.0f));
    params.push_back (std::make_unique<Param> (highDampRateId, "High Damp", juce::NormalisableRange<float> (1.0f, 10.0f), 2.5f));

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
