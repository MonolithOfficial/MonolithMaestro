#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
 * GUI editor for Monolith Maestro plugin.
 *
 * Displays detected notes in real-time with visual feedback.
 */
class MonolithMaestroEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit MonolithMaestroEditor(MonolithMaestroProcessor&);
    ~MonolithMaestroEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    /** Updates detected notes display (called by timer). */
    void timerCallback() override;

    /** Handles record button click. */
    void recordButtonClicked();

    /** Handles copy button click. */
    void copyButtonClicked();

    /** Updates the recorded notes display text. */
    void updateRecordedNotesDisplay(const std::vector<juce::String>& notes, const juce::String& key);

    MonolithMaestroProcessor& processorRef;            ///< Reference to processor
    std::vector<DetectedNote> currentNotes_;           ///< Currently detected notes

    // Recording UI components
    juce::TextButton recordButton_;                    ///< Record/Stop button
    juce::TextEditor recordedNotesDisplay_;            ///< Display area for recorded notes
    juce::TextButton copyButton_;                      ///< Copy to clipboard button

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonolithMaestroEditor)
};
