#include "PitchDetector.h"

//==============================================================================
PitchDetector::PitchDetector()
    : fft_(std::make_unique<juce::dsp::FFT>(fftOrder_))
    , fifo_(fftSize_ + 1)  // +1 because JUCE AbstractFifo holds size-1 items
{
    // Allocate FFT buffers (pre-allocation for real-time safety)
    fftBuffer_.allocate(fftSize_ * 2, true);      // *2 for complex numbers
    fftMagnitudes_.allocate(fftSize_, true);
    windowBuffer_.allocate(fftSize_, true);

    // Pre-compute Hann window: w(n) = 0.5 * (1 - cos(2π * n / (N-1)))
    for (int i = 0; i < fftSize_; ++i)
    {
        windowBuffer_[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize_ - 1.0f)));
    }

    fifoBuffer_.resize(fftSize_, 0.0f);
    detectedNotes_.reserve(maxNotes_);

    // Initialize frequency-to-note mapping
    initializeFrequencyMap();
}

//==============================================================================
void PitchDetector::prepare(double sampleRate, int expectedBlockSize)
{
    sampleRate_ = sampleRate;
    expectedBlockSize_ = expectedBlockSize;
    reset();
}

void PitchDetector::processAudioBlock(const float* audioData, int numSamples)
{
    if (audioData == nullptr || numSamples <= 0)
        return;

    // Check noise gate
    float rms = calculateRMS(audioData, numSamples);
    if (rms < noiseGateThreshold_)
    {
        isActive_.store(false, std::memory_order_relaxed);
        detectedNotes_.clear();
        return;
    }

    isActive_.store(true, std::memory_order_relaxed);

    // Write audio to FIFO
    int start1, size1, start2, size2;
    fifo_.prepareToWrite(numSamples, start1, size1, start2, size2);

    if (size1 > 0)
        std::copy(audioData, audioData + size1, fifoBuffer_.data() + start1);
    if (size2 > 0)
        std::copy(audioData + size1, audioData + size1 + size2, fifoBuffer_.data() + start2);

    fifo_.finishedWrite(size1 + size2);

    // Perform FFT when enough samples accumulated
    if (fifo_.getNumReady() >= fftSize_)
    {
        performFFTAnalysis();
    }
}

void PitchDetector::performFFTAnalysis()
{
    //==============================================================================
    // Simple Monophonic Pitch Detection:
    // 1. Find the strongest frequency peak in the FFT spectrum
    // 2. Use parabolic interpolation for sub-bin accuracy
    // 3. Map the frequency to a note using predefined frequency ranges
    // 4. No harmonic filtering - the loudest frequency is the detected note
    //==============================================================================

    // Read samples from FIFO
    int start1, size1, start2, size2;
    fifo_.prepareToRead(fftSize_, start1, size1, start2, size2);

    juce::zeromem(fftBuffer_.getData(), fftSize_ * 2 * sizeof(float));

    if (size1 > 0)
        std::copy(fifoBuffer_.data() + start1, fifoBuffer_.data() + start1 + size1, fftBuffer_.getData());
    if (size2 > 0)
        std::copy(fifoBuffer_.data() + start2, fifoBuffer_.data() + start2 + size2, fftBuffer_.getData() + size1);

    fifo_.finishedRead(size1 + size2);

    // Apply Hann window
    for (int i = 0; i < fftSize_; ++i)
    {
        fftBuffer_[i] *= windowBuffer_[i];
    }

    // Perform FFT
    fft_->performRealOnlyForwardTransform(fftBuffer_.getData());

    // Calculate magnitude spectrum: mag = sqrt(real² + imag²) / fftSize
    for (int i = 0; i < fftSize_ / 2; ++i)
    {
        float real = fftBuffer_[i * 2];
        float imag = fftBuffer_[i * 2 + 1];
        fftMagnitudes_[i] = std::sqrt(real * real + imag * imag) / fftSize_;
    }

    // Monophonic detection: find the strongest peak in the spectrum
    candidateNotes_.clear();

    // Skip first 2 bins (DC and very low frequency noise)
    // Bin 2 = ~21 Hz with 4096 FFT, allowing detection down to ~40 Hz (low E on bass)
    int strongestBin = 2;
    float strongestMagnitude = fftMagnitudes_[2];

    for (int i = 3; i < fftSize_ / 2; ++i)
    {
        if (fftMagnitudes_[i] > strongestMagnitude)
        {
            strongestMagnitude = fftMagnitudes_[i];
            strongestBin = i;
        }
    }

    // Only process if magnitude is above threshold
    if (strongestMagnitude > magnitudeThreshold_)
    {
        // Parabolic interpolation for sub-bin accuracy
        float refinedPeakIndex = static_cast<float>(strongestBin);

        if (strongestBin > 0 && strongestBin < fftSize_ / 2 - 1)
        {
            float left = fftMagnitudes_[strongestBin - 1];
            float center = fftMagnitudes_[strongestBin];
            float right = fftMagnitudes_[strongestBin + 1];

            // Parabolic interpolation: delta = 0.5 * (left - right) / (left - 2*center + right)
            float denominator = left - 2.0f * center + right;
            if (std::abs(denominator) > 0.0001f)
            {
                float delta = 0.5f * (left - right) / denominator;
                refinedPeakIndex = strongestBin + delta;
            }
        }

        // Convert bin to frequency
        float frequency = refinedPeakIndex * static_cast<float>(sampleRate_) / fftSize_;

        // Look up which note this frequency belongs to
        const auto* noteRange = findNoteForFrequency(frequency);

        if (noteRange != nullptr)
        {
            DetectedNote note;
            note.noteName = noteRange->noteName;
            note.frequency = frequency;
            note.magnitude = strongestMagnitude;
            note.midiNoteNumber = noteRange->midiNoteNumber;

            candidateNotes_.push_back(note);
        }
    }

    // Apply note stability tracking
    updateNoteStability();
}

void PitchDetector::updateNoteStability()
{
    // Update history for each candidate note
    for (const auto& candidate : candidateNotes_)
    {
        // Find if this note exists in history
        auto it = std::find_if(noteHistory_.begin(), noteHistory_.end(),
            [&candidate](const NoteHistory& h) { return h.midiNote == candidate.midiNoteNumber; });

        if (it != noteHistory_.end())
        {
            // Note is continuing - increment counter
            it->consecutiveFrames++;
            it->totalMagnitude += candidate.magnitude;
        }
        else
        {
            // New note - add to history
            NoteHistory newHistory;
            newHistory.midiNote = candidate.midiNoteNumber;
            newHistory.consecutiveFrames = 1;
            newHistory.totalMagnitude = candidate.magnitude;
            noteHistory_.push_back(newHistory);
        }
    }

    // Decay/remove notes not in current candidates
    for (auto it = noteHistory_.begin(); it != noteHistory_.end();)
    {
        bool stillPresent = std::any_of(candidateNotes_.begin(), candidateNotes_.end(),
            [&it](const DetectedNote& note) { return note.midiNoteNumber == it->midiNote; });

        if (!stillPresent)
        {
            // Note disappeared - remove from history
            it = noteHistory_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Build stable detectedNotes_ from history
    detectedNotes_.clear();
    for (const auto& history : noteHistory_)
    {
        // Only report notes that have been stable for required frames
        if (history.consecutiveFrames >= stabilityFramesRequired_)
        {
            // Find the actual note in candidates to get current frequency/magnitude
            auto candidateIt = std::find_if(candidateNotes_.begin(), candidateNotes_.end(),
                [&history](const DetectedNote& note) { return note.midiNoteNumber == history.midiNote; });

            if (candidateIt != candidateNotes_.end())
            {
                detectedNotes_.push_back(*candidateIt);
            }
        }
    }

    // Sort by magnitude
    std::sort(detectedNotes_.begin(), detectedNotes_.end());
}

std::vector<DetectedNote> PitchDetector::getDetectedNotes() const
{
    return detectedNotes_;
}

void PitchDetector::reset()
{
    juce::zeromem(fftBuffer_.getData(), fftSize_ * 2 * sizeof(float));
    juce::zeromem(fftMagnitudes_.getData(), fftSize_ * sizeof(float));
    std::fill(fifoBuffer_.begin(), fifoBuffer_.end(), 0.0f);
    fifo_.reset();
    detectedNotes_.clear();
    candidateNotes_.clear();
    noteHistory_.clear();
    isActive_.store(false, std::memory_order_relaxed);
}

void PitchDetector::setMagnitudeThreshold(float threshold)
{
    magnitudeThreshold_ = juce::jlimit(0.0f, 1.0f, threshold);
}

void PitchDetector::setNoiseGateThreshold(float threshold)
{
    noiseGateThreshold_ = juce::jlimit(0.0f, 1.0f, threshold);
}

//==============================================================================
void PitchDetector::initializeFrequencyMap()
{
    // Create frequency ranges for notes from C1 (MIDI 24, ~33 Hz) to C7 (MIDI 96)
    // This covers bass guitars (low E = 41 Hz), pianos, and most instruments
    frequencyMap_.clear();

    for (int midiNote = 24; midiNote <= 96; ++midiNote)
    {
        NoteFrequencyRange range;
        range.midiNoteNumber = midiNote;
        range.noteName = midiNoteToName(midiNote);
        range.centerFrequency = midiNoteToFrequency(midiNote);

        // Calculate boundaries as geometric mean between adjacent notes
        float lowerNoteFreq = midiNoteToFrequency(midiNote - 1);
        float upperNoteFreq = midiNoteToFrequency(midiNote + 1);

        range.minFrequency = std::sqrt(lowerNoteFreq * range.centerFrequency);
        range.maxFrequency = std::sqrt(range.centerFrequency * upperNoteFreq);

        frequencyMap_.push_back(range);
    }
}

const PitchDetector::NoteFrequencyRange* PitchDetector::findNoteForFrequency(float frequency) const
{
    for (const auto& range : frequencyMap_)
    {
        if (frequency >= range.minFrequency && frequency < range.maxFrequency)
        {
            return &range;
        }
    }
    return nullptr;  // Frequency out of range
}

int PitchDetector::frequencyToMidiNote(float frequency) const
{
    if (frequency <= 0.0f)
        return -1;

    // Formula: midiNote = 69 + 12 * log2(frequency / 440.0)
    float midiNoteFloat = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    int midiNote = static_cast<int>(std::round(midiNoteFloat));

    if (midiNote < 0 || midiNote > 127)
        return -1;

    return midiNote;
}

juce::String PitchDetector::midiNoteToName(int midiNote) const
{
    if (midiNote < 0 || midiNote > 127)
        return "Invalid";

    int noteIndex = midiNote % 12;

    // Return just the note name without octave number
    return noteNames_[noteIndex];
}

float PitchDetector::midiNoteToFrequency(int midiNote) const
{
    // Formula: frequency = 440.0 * 2^((midiNote - 69) / 12.0)
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

float PitchDetector::calculateRMS(const float* buffer, int numSamples) const
{
    if (buffer == nullptr || numSamples <= 0)
        return 0.0f;

    float sumOfSquares = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = buffer[i];
        sumOfSquares += sample * sample;
    }

    float mean = sumOfSquares / numSamples;
    return std::sqrt(mean);
}
