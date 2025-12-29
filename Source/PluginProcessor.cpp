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

        // Capture notes during recording
        if (isRecording_.load())
        {
            auto detectedNotes = pitchDetector_.getDetectedNotes();
            if (!detectedNotes.empty())
            {
                // Get the strongest note (first in sorted vector)
                const auto& strongestNote = detectedNotes[0];
                const auto& noteName = strongestNote.noteName;

                // Only record if different from last note (avoid duplicates)
                if (noteName != lastRecordedNote_)
                {
                    const juce::ScopedLock lock(recordingLock_);
                    recordedNotes_.push_back(noteName);
                    lastRecordedNote_ = noteName;
                }
            }
        }
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
// Recording Implementation

void MonolithMaestroProcessor::startRecording()
{
    const juce::ScopedLock lock(recordingLock_);
    recordedNotes_.clear();
    lastRecordedNote_.clear();
    detectedKey_.clear();
    isRecording_.store(true);
}

std::vector<juce::String> MonolithMaestroProcessor::stopRecording()
{
    isRecording_.store(false);

    const juce::ScopedLock lock(recordingLock_);

    // Analyze key if we have notes
    if (!recordedNotes_.empty())
    {
        analyzeKey();
    }
    else
    {
        detectedKey_ = "No notes recorded";
    }

    return recordedNotes_;
}

void MonolithMaestroProcessor::analyzeKey()
{
    if (recordedNotes_.empty())
    {
        detectedKey_ = "Unknown";
        return;
    }

    // Count pitch class occurrences (0-11, where C=0)
    std::array<int, 12> pitchClassCounts = {0};

    for (const auto& note : recordedNotes_)
    {
        int pc = noteToPitchClass(note);
        if (pc >= 0 && pc < 12)
        {
            pitchClassCounts[pc]++;
        }
    }

    // Define major and minor scale patterns (intervals from root)
    const std::array<int, 7> majorScale = {0, 2, 4, 5, 7, 9, 11};
    const std::array<int, 7> minorScale = {0, 2, 3, 5, 7, 8, 10};

    // Score each possible key
    int bestScore = -1000;
    juce::String bestKey = "Unknown";

    for (int root = 0; root < 12; ++root)
    {
        // Score major key
        int majorScore = 0;
        for (int interval : majorScale)
        {
            int pc = (root + interval) % 12;
            majorScore += pitchClassCounts[pc] * 2; // Notes in scale get bonus
        }

        // Penalty for notes NOT in scale
        for (int pc = 0; pc < 12; ++pc)
        {
            bool inScale = false;
            for (int interval : majorScale)
            {
                if (pc == (root + interval) % 12)
                {
                    inScale = true;
                    break;
                }
            }
            if (!inScale)
            {
                majorScore -= pitchClassCounts[pc]; // Penalty for chromatic notes
            }
        }

        if (majorScore > bestScore)
        {
            bestScore = majorScore;
            bestKey = pitchClassToNoteName(root) + " Major";
        }

        // Score minor key
        int minorScore = 0;
        for (int interval : minorScale)
        {
            int pc = (root + interval) % 12;
            minorScore += pitchClassCounts[pc] * 2;
        }

        for (int pc = 0; pc < 12; ++pc)
        {
            bool inScale = false;
            for (int interval : minorScale)
            {
                if (pc == (root + interval) % 12)
                {
                    inScale = true;
                    break;
                }
            }
            if (!inScale)
            {
                minorScore -= pitchClassCounts[pc];
            }
        }

        if (minorScore > bestScore)
        {
            bestScore = minorScore;
            bestKey = pitchClassToNoteName(root) + " Minor";
        }
    }

    detectedKey_ = bestKey;
}

int MonolithMaestroProcessor::noteToPitchClass(const juce::String& noteName) const
{
    if (noteName.isEmpty())
        return -1;

    // Extract note name without octave (e.g., "C5" -> "C", "F#4" -> "F#")
    juce::String noteOnly;
    for (int i = 0; i < noteName.length(); ++i)
    {
        if (noteName[i] >= '0' && noteName[i] <= '9')
            break;
        noteOnly += noteName[i];
    }

    // Map to pitch class (0-11)
    if (noteOnly == "C") return 0;
    if (noteOnly == "C#" || noteOnly == "Db") return 1;
    if (noteOnly == "D") return 2;
    if (noteOnly == "D#" || noteOnly == "Eb") return 3;
    if (noteOnly == "E") return 4;
    if (noteOnly == "F") return 5;
    if (noteOnly == "F#" || noteOnly == "Gb") return 6;
    if (noteOnly == "G") return 7;
    if (noteOnly == "G#" || noteOnly == "Ab") return 8;
    if (noteOnly == "A") return 9;
    if (noteOnly == "A#" || noteOnly == "Bb") return 10;
    if (noteOnly == "B") return 11;

    return -1;
}

juce::String MonolithMaestroProcessor::pitchClassToNoteName(int pitchClass) const
{
    const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    if (pitchClass >= 0 && pitchClass < 12)
        return names[pitchClass];

    return "?";
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonolithMaestroProcessor();
}
