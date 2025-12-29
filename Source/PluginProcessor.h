#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchDetector.h"

//==============================================================================
/**
 * Main audio processor for Monolith Maestro VST3 plugin.
 *
 * Performs real-time pitch detection on incoming audio and provides
 * detected note information to the GUI.
 */
class MonolithMaestroProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MonolithMaestroProcessor();
    ~MonolithMaestroProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Checks if audio is currently active (above noise threshold). */
    bool isAudioActive() const { return audioActive; }

    /** Gets currently detected notes from the pitch detector. */
    std::vector<DetectedNote> getDetectedNotes() const;

private:
    //==============================================================================
    std::atomic<bool> audioActive { false };           ///< Audio activity flag
    const float activityThreshold = 0.001f;            ///< RMS threshold for activity
    PitchDetector pitchDetector_;                      ///< Pitch detection engine

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonolithMaestroProcessor)
};
