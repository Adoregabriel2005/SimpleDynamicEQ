#pragma once
#include <JuceHeader.h>
#include <vector>

// ============================================================
//  FrequencyProblemDetector
//  Analyzes spectrum data for:
//    - Resonance peaks (narrow, high-energy spikes)
//    - Clipping (samples at ± 1.0)
//    - Estimated noise floor
// ============================================================
class FrequencyProblemDetector
{
public:
    struct Problem
    {
        enum Type { Resonance, Clipping, NoiseFloor };
        Type   type;
        float  freqHz    = 0.f;     // centre frequency (for Resonance)
        float  magnitude = 0.f;     // dB level
        juce::String description;
    };

    FrequencyProblemDetector() = default;

    // Call with the current spectrum magnitudes (dB) and their corresponding
    // frequency bins. Also pass peak sample for clipping detection.
    void analyze(const float* magnitudesDb, int numBins,
                 double sampleRate, float peakSample);

    const std::vector<Problem>& getProblems() const { return problems; }
    bool isClipping() const { return clipping; }
    float getNoiseFloorDb() const { return noiseFloor; }

private:
    std::vector<Problem> problems;
    bool  clipping   = false;
    float noiseFloor = -120.f;
};
