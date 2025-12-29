#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
MonolithMaestroProcessor::MonolithMaestroProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MonolithMaestroProcessor::~MonolithMaestroProcessor()
{
}

//==============================================================================
const juce::String MonolithMaestroProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MonolithMaestroProcessor::acceptsMidi() const
{
    return false;
}

bool MonolithMaestroProcessor::producesMidi() const
{
    return false;
}

bool MonolithMaestroProcessor::isMidiEffect() const
{
    return false;
}

double MonolithMaestroProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MonolithMaestroProcessor::getNumPrograms()
{
    return 1;
}

int MonolithMaestroProcessor::getCurrentProgram()
{
    return 0;
}

void MonolithMaestroProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String MonolithMaestroProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void MonolithMaestroProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void MonolithMaestroProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    pitchDetector_.prepare(sampleRate, samplesPerBlock);

    // Configure thresholds for accurate pitch detection
    pitchDetector_.setNoiseGateThreshold(0.001f);
    pitchDetector_.setMagnitudeThreshold(0.02f);
}

void MonolithMaestroProcessor::releaseResources()
{
}

bool MonolithMaestroProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void MonolithMaestroProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Feed first channel to pitch detector
    if (totalNumInputChannels > 0)
    {
        const float* channelData = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();

        pitchDetector_.processAudioBlock(channelData, numSamples);
        audioActive.store(pitchDetector_.isActive());
    }
    else
    {
        audioActive.store(false);
    }

    // Pass-through audio (no processing)
}

//==============================================================================
bool MonolithMaestroProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MonolithMaestroProcessor::createEditor()
{
    return new MonolithMaestroEditor(*this);
}

//==============================================================================
void MonolithMaestroProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void MonolithMaestroProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

//==============================================================================
std::vector<DetectedNote> MonolithMaestroProcessor::getDetectedNotes() const
{
    return pitchDetector_.getDetectedNotes();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonolithMaestroProcessor();
}
