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

    MonolithMaestroProcessor& processorRef;            ///< Reference to processor
    std::vector<DetectedNote> currentNotes_;           ///< Currently detected notes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonolithMaestroEditor)
};
