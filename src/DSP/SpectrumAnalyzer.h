#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  SpectrumAnalyzer
//  Continuous FFT-based spectrum analyser using a ring buffer
//  and a Hann window.  Uses JUCE's optimized FFT engine.
// ============================================================
class SpectrumAnalyzer
{
public:
    static constexpr int kFftOrder = 11;                    // 2048-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kNumBins  = kFftSize / 2 + 1;

    SpectrumAnalyzer();

    void prepare(double sampleRate);

    // Called from audio thread; pushes samples into ring buffer
    void pushSamples(const float* L, const float* R, int numSamples);

    // Called from GUI thread; copies latest magnitudes (in dBFS)
    void getMagnitudes(std::vector<float>& dest) const;

    double getFreqForBin(int bin) const
    {
        return (double)bin * sampleRate / (double)kFftSize;
    }

private:
    void runFFT();

    double sampleRate = 44100.0;

    // Ring buffer
    std::vector<float>  ringMono;
    int                 ringWritePos = 0;
    int                 samplesCollected = 0;
    static constexpr int kHopSize = kFftSize / 2;

    // Hann window (float for JUCE FFT)
    std::vector<float> window;

    // Shared magnitude array.
    mutable juce::SpinLock     magnitudeLock;
    mutable std::vector<float> magnitudes;
    std::vector<float>         workBuf;

    // JUCE FFT engine + working buffer (float, interleaved real/imag)
    juce::dsp::FFT              fftEngine;
    std::vector<float>          fftData;   // size = kFftSize * 2
};
