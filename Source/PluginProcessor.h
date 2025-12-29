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

    //==============================================================================
    // Recording functionality

    /** Starts recording detected notes. */
    void startRecording();

    /** Stops recording and returns the recorded notes sequence. */
    std::vector<juce::String> stopRecording();

    /** Gets the detected musical key from the last recording. */
    juce::String getDetectedKey() const { return detectedKey_; }

    /** Checks if currently recording. */
    bool isRecording() const { return isRecording_.load(); }

private:
    //==============================================================================
    std::atomic<bool> audioActive { false };           ///< Audio activity flag
    const float activityThreshold = 0.001f;            ///< RMS threshold for activity
    PitchDetector pitchDetector_;                      ///< Pitch detection engine

    // Recording state
    std::atomic<bool> isRecording_ { false };          ///< Recording active flag
    std::vector<juce::String> recordedNotes_;          ///< Recorded note sequence
    juce::String lastRecordedNote_;                    ///< Last note to avoid duplicates
    juce::String detectedKey_;                         ///< Detected musical key
    juce::CriticalSection recordingLock_;              ///< Thread safety for recording

    //==============================================================================
    /** Analyzes recorded notes and detects the musical key. */
    void analyzeKey();

    /** Converts note name to pitch class (0-11, C=0). */
    int noteToPitchClass(const juce::String& noteName) const;

    /** Converts pitch class to note name. */
    juce::String pitchClassToNoteName(int pitchClass) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonolithMaestroProcessor)
};
