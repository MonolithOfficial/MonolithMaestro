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
    detectedNotes_.reserve(maxPolyphony_);
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

    // Find peaks and convert to notes
    std::vector<float> magnitudes(fftMagnitudes_.getData(), fftMagnitudes_.getData() + fftSize_ / 2);
    auto peakIndices = findPeaks(magnitudes);

    detectedNotes_.clear();
    std::vector<int> filteredPeakIndices;

    // Harmonic filtering: Remove peaks that are harmonics (2x, 3x, 4x) of lower peaks
    // Process peaks by frequency (low to high) to preserve fundamentals
    std::vector<int> peaksByFrequency = peakIndices;
    std::sort(peaksByFrequency.begin(), peaksByFrequency.end());

    for (const auto& peakIndex : peaksByFrequency)
    {
        float frequency = peakIndex * static_cast<float>(sampleRate_) / fftSize_;

        // Check if this peak is a harmonic of any lower peak
        bool isHarmonic = false;
        for (const auto& lowerPeakIndex : filteredPeakIndices)
        {
            float lowerFreq = lowerPeakIndex * static_cast<float>(sampleRate_) / fftSize_;

            // Check 2nd, 3rd, 4th harmonics with 10% tolerance
            for (int harmonic = 2; harmonic <= 4; ++harmonic)
            {
                float expectedHarmonic = lowerFreq * harmonic;
                float tolerance = lowerFreq * 0.1f;

                if (std::abs(frequency - expectedHarmonic) < tolerance)
                {
                    isHarmonic = true;
                    break;
                }
            }
            if (isHarmonic)
                break;
        }

        if (!isHarmonic)
        {
            filteredPeakIndices.push_back(peakIndex);
        }
    }

    // Convert filtered peaks to notes with parabolic interpolation for accuracy
    for (const auto& peakIndex : filteredPeakIndices)
    {
        // Parabolic interpolation for sub-bin accuracy
        float refinedPeakIndex = static_cast<float>(peakIndex);

        if (peakIndex > 0 && peakIndex < static_cast<int>(magnitudes.size()) - 1)
        {
            float left = magnitudes[peakIndex - 1];
            float center = magnitudes[peakIndex];
            float right = magnitudes[peakIndex + 1];

            // Parabolic interpolation: delta = 0.5 * (left - right) / (left - 2*center + right)
            float denominator = left - 2.0f * center + right;
            if (std::abs(denominator) > 0.0001f)  // Avoid division by zero
            {
                float delta = 0.5f * (left - right) / denominator;
                refinedPeakIndex = peakIndex + delta;
            }
        }

        float frequency = refinedPeakIndex * static_cast<float>(sampleRate_) / fftSize_;
        int midiNote = frequencyToMidiNote(frequency);

        if (midiNote < 0 || midiNote > 127)
            continue;

        float magnitude = magnitudes[peakIndex];

        DetectedNote note {
            midiNoteToName(midiNote),
            frequency,
            magnitude,
            midiNote
        };

        detectedNotes_.push_back(note);

        if (detectedNotes_.size() >= maxPolyphony_)
            break;
    }

    // Sort by magnitude (strongest first)
    std::sort(detectedNotes_.begin(), detectedNotes_.end());

    // Filter out weak notes: only keep notes within 40% of the strongest note's magnitude
    // This ensures only genuinely played notes are shown, not weak harmonics/artifacts
    if (!detectedNotes_.empty())
    {
        float strongestMagnitude = detectedNotes_[0].magnitude;
        float relativeThreshold = strongestMagnitude * 0.4f;  // 40% of strongest

        // Keep only notes above relative threshold
        detectedNotes_.erase(
            std::remove_if(detectedNotes_.begin(), detectedNotes_.end(),
                [relativeThreshold](const DetectedNote& note) {
                    return note.magnitude < relativeThreshold;
                }),
            detectedNotes_.end()
        );
    }
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
std::vector<int> PitchDetector::findPeaks(const std::vector<float>& magnitudes)
{
    std::vector<std::pair<float, int>> peaks;

    // Find local maxima (skip first 4 bins to avoid DC and low-frequency noise)
    for (int i = 4; i < static_cast<int>(magnitudes.size()) - 1; ++i)
    {
        if (magnitudes[i] > magnitudeThreshold_ &&
            magnitudes[i] > magnitudes[i - 1] &&
            magnitudes[i] > magnitudes[i + 1])
        {
            peaks.emplace_back(magnitudes[i], i);
        }
    }

    // Sort by magnitude (descending)
    std::sort(peaks.begin(), peaks.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // Extract top N peak indices
    std::vector<int> peakIndices;
    peakIndices.reserve(maxPolyphony_);

    int numPeaks = std::min(static_cast<int>(peaks.size()), maxPolyphony_);
    for (int i = 0; i < numPeaks; ++i)
    {
        peakIndices.push_back(peaks[i].second);
    }

    return peakIndices;
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
    int octave = (midiNote / 12);  // Scientific pitch notation: C5 = middle C

    juce::String result = noteNames_[noteIndex];
    result << octave;
    return result;
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
