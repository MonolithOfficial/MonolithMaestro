#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

//==============================================================================
/**
 * Represents a detected musical note with frequency and magnitude information.
 */
struct DetectedNote
{
    juce::String noteName;     ///< Note name without octave (e.g., "C", "A#")
    float frequency;           ///< Frequency in Hz
    float magnitude;           ///< Strength/loudness of the frequency
    int midiNoteNumber;        ///< MIDI note number (0-127)

    /** Compare notes by magnitude for sorting (descending order). */
    bool operator<(const DetectedNote& other) const
    {
        return magnitude > other.magnitude;
    }
};

//==============================================================================
/**
 * Monophonic pitch detector using FFT analysis and frequency mapping.
 *
 * Analyzes incoming audio to detect the strongest note in real-time.
 * Uses FFT to find the dominant frequency and maps it to musical notes
 * using predefined frequency ranges.
 */
class PitchDetector
{
public:
    //==============================================================================
    PitchDetector();
    ~PitchDetector() = default;

    /** Frequency range mapping for a musical note. */
    struct NoteFrequencyRange
    {
        juce::String noteName;
        float minFrequency;
        float maxFrequency;
        float centerFrequency;
        int midiNoteNumber;
    };

    //==============================================================================
    /**
     * Prepares the pitch detector for audio processing.
     *
     * @param sampleRate        Audio sample rate in Hz
     * @param expectedBlockSize Maximum samples per audio block
     */
    void prepare(double sampleRate, int expectedBlockSize);

    /**
     * Processes an audio block for pitch detection.
     * Must be called from the audio thread.
     *
     * @param audioData  Pointer to mono audio samples
     * @param numSamples Number of samples in buffer
     */
    void processAudioBlock(const float* audioData, int numSamples);

    /**
     * Gets currently detected notes, sorted by strength.
     * Thread-safe - can be called from GUI thread.
     *
     * @return Vector of detected notes
     */
    std::vector<DetectedNote> getDetectedNotes() const;

    /**
     * Resets all internal buffers and state.
     */
    void reset();

    /**
     * Checks if audio is currently active (above noise threshold).
     *
     * @return true if audio is being detected
     */
    bool isActive() const { return isActive_.load(std::memory_order_relaxed); }

    /**
     * Sets minimum magnitude threshold for peak detection.
     *
     * @param threshold Value between 0.0-1.0 (typical: 0.01-0.1)
     */
    void setMagnitudeThreshold(float threshold);

    /**
     * Sets RMS threshold below which audio is considered silence.
     *
     * @param threshold Value between 0.0-1.0 (typical: 0.001-0.01)
     */
    void setNoiseGateThreshold(float threshold);

private:
    //==============================================================================
    /** Performs FFT analysis on accumulated audio buffer. */
    void performFFTAnalysis();

    /** Updates note stability tracking and builds stable detected notes list. */
    void updateNoteStability();

    /**
     * Converts frequency in Hz to MIDI note number.
     *
     * @param frequency Frequency in Hz
     * @return MIDI note (0-127) or -1 if invalid
     */
    int frequencyToMidiNote(float frequency) const;

    /**
     * Converts MIDI note number to note name string.
     *
     * @param midiNote MIDI note number (0-127)
     * @return Note name without octave (e.g., "C", "A#")
     */
    juce::String midiNoteToName(int midiNote) const;

    /**
     * Converts MIDI note number to frequency.
     *
     * @param midiNote MIDI note number
     * @return Frequency in Hz
     */
    float midiNoteToFrequency(int midiNote) const;

    /**
     * Calculates RMS level of audio buffer.
     *
     * @param buffer     Audio samples
     * @param numSamples Number of samples
     * @return RMS level (0.0-1.0 for normalized audio)
     */
    float calculateRMS(const float* buffer, int numSamples) const;

    //==============================================================================
    // FFT Configuration
    static constexpr int fftOrder_ = 12;                      ///< FFT order (4096 samples for better low-freq resolution)
    static constexpr int fftSize_ = 1 << fftOrder_;           ///< FFT size (4096)
    static constexpr int maxNotes_ = 1;                       ///< Monophonic detection (single note)

    // FFT Processing
    std::unique_ptr<juce::dsp::FFT> fft_;                     ///< FFT processor
    juce::HeapBlock<float> fftBuffer_;                        ///< Time-domain input buffer
    juce::HeapBlock<float> fftMagnitudes_;                    ///< Frequency-domain output
    juce::HeapBlock<float> windowBuffer_;                     ///< Hann window coefficients

    // FIFO Buffer
    juce::AbstractFifo fifo_;                                 ///< Lock-free FIFO for samples
    std::vector<float> fifoBuffer_;                           ///< FIFO storage

    // Audio State
    double sampleRate_ = 44100.0;                             ///< Current sample rate
    int expectedBlockSize_ = 512;                             ///< Expected block size

    // Detection Results
    std::vector<DetectedNote> detectedNotes_;                 ///< Currently detected notes
    std::vector<DetectedNote> candidateNotes_;                ///< Candidate notes from latest frame

    // Note Stability Tracking
    struct NoteHistory {
        int midiNote = -1;
        int consecutiveFrames = 0;
        float totalMagnitude = 0.0f;
    };
    std::vector<NoteHistory> noteHistory_;                    ///< Track note stability
    static constexpr int stabilityFramesRequired_ = 2;        ///< Frames needed to confirm note (reduced for faster response)

    // Thresholds
    float magnitudeThreshold_ = 0.02f;                        ///< Min peak magnitude
    float noiseGateThreshold_ = 0.001f;                       ///< RMS silence threshold

    // Status
    std::atomic<bool> isActive_{ false };                     ///< Audio activity flag

    // Note Names
    const std::array<juce::String, 12> noteNames_ = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    // Frequency-to-Note Mapping
    std::vector<NoteFrequencyRange> frequencyMap_;

    //==============================================================================
    /** Initializes the frequency-to-note mapping table. */
    void initializeFrequencyMap();

    /**
     * Finds which note a frequency belongs to.
     *
     * @param frequency Frequency in Hz
     * @return Pointer to NoteFrequencyRange or nullptr if out of range
     */
    const NoteFrequencyRange* findNoteForFrequency(float frequency) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDetector)
};
