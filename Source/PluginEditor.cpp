#include "PluginEditor.h"

//==============================================================================
MonolithMaestroEditor::MonolithMaestroEditor(MonolithMaestroProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    startTimer(50);  // Update display at 20Hz
    setSize(500, 800);
}

MonolithMaestroEditor::~MonolithMaestroEditor()
{
    stopTimer();
}

//==============================================================================
void MonolithMaestroEditor::paint(juce::Graphics& g)
{
    // Background gradient
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        juce::Colour(0xff2a2a2a), bounds.getTopLeft(),
        juce::Colour(0xff1a1a1a), bounds.getBottomRight(),
        false);

    g.setGradientFill(gradient);
    g.fillRect(bounds);

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(32.0f, juce::Font::bold));
    g.drawText("MONOLITH MAESTRO", 0, 20, getWidth(), 40, juce::Justification::centred);

    g.setFont(juce::Font(14.0f));
    g.setColour(juce::Colour(0xff888888));
    g.drawText("Real-Time Pitch Detection", 0, 60, getWidth(), 20, juce::Justification::centred);

    // Detected notes or idle message
    if (currentNotes_.empty())
    {
        g.setColour(juce::Colour(0xff666666));
        g.setFont(juce::Font(24.0f));
        g.drawText("Waiting for audio input...", 0, getHeight() / 2 - 30, getWidth(), 60, juce::Justification::centred);
    }
    else
    {
        int noteHeight = 80;
        int startY = 120;
        int spacing = 10;

        for (size_t i = 0; i < currentNotes_.size(); ++i)
        {
            const auto& note = currentNotes_[i];
            int yPos = startY + i * (noteHeight + spacing);

            // Note card background
            juce::Rectangle<float> noteCard(40, yPos, getWidth() - 80, noteHeight);
            g.setColour(juce::Colour(0xff333333));
            g.fillRoundedRectangle(noteCard, 8.0f);

            // Magnitude bar
            float barWidth = (getWidth() - 120) * juce::jlimit(0.0f, 1.0f, note.magnitude * 10.0f);
            juce::Rectangle<float> magBar(50, yPos + 10, barWidth, 10);
            g.setColour(juce::Colour(0xff4CAF50));
            g.fillRoundedRectangle(magBar, 4.0f);

            // Note name
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(36.0f, juce::Font::bold));
            g.drawText(note.noteName, 50, yPos + 25, 120, 40, juce::Justification::centredLeft);

            // Frequency
            g.setFont(juce::Font(16.0f));
            g.setColour(juce::Colour(0xffaaaaaa));
            juce::String freqText;
            freqText << juce::String(note.frequency, 1) << " Hz";
            g.drawText(freqText, 180, yPos + 30, 150, 30, juce::Justification::centredLeft);

            // MIDI note number
            g.setFont(juce::Font(14.0f));
            g.setColour(juce::Colour(0xff888888));
            juce::String midiText;
            midiText << "MIDI: " << note.midiNoteNumber;
            g.drawText(midiText, getWidth() - 150, yPos + 30, 100, 30, juce::Justification::centredRight);
        }
    }

    // Status indicator
    bool isActive = processorRef.isAudioActive();
    g.setFont(juce::Font(12.0f));
    g.setColour(isActive ? juce::Colour(0xff4CAF50) : juce::Colour(0xff666666));
    juce::String status = isActive ? "LISTENING" : "IDLE";
    g.drawText(status, 0, getHeight() - 30, getWidth(), 20, juce::Justification::centred);
}

void MonolithMaestroEditor::resized()
{
}

void MonolithMaestroEditor::timerCallback()
{
    currentNotes_ = processorRef.getDetectedNotes();
    repaint();
}
