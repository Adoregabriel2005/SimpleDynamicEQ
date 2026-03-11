#include "SpectrumAnalyzer.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -----------------------------------------------------------------------
SpectrumAnalyzer::SpectrumAnalyzer()
    : fftEngine(kFftOrder)
{
    ringMono.resize(kFftSize * 2, 0.0f);
    magnitudes.resize(kNumBins, -120.0f);
    workBuf.resize(kNumBins, -120.0f);
    fftData.resize(kFftSize * 2, 0.0f);  // JUCE FFT uses interleaved real/imag

    // Pre-compute Hann window (float)
    window.resize(kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * (float)i / (float)(kFftSize - 1)));
}

void SpectrumAnalyzer::prepare(double sr)
{
    sampleRate      = sr;
    ringWritePos    = 0;
    samplesCollected = 0;
    std::fill(magnitudes.begin(), magnitudes.end(), -120.0f);
}

// -----------------------------------------------------------------------
//  pushSamples  (audio thread)
// -----------------------------------------------------------------------
void SpectrumAnalyzer::pushSamples(const float* L, const float* R, int numSamples)
{
    const int ringSize = kFftSize * 2;
    for (int i = 0; i < numSamples; ++i)
    {
        ringMono[ringWritePos % ringSize] = (L[i] + R[i]) * 0.5f;
        ++ringWritePos;
        ++samplesCollected;
    }

    if (samplesCollected >= kHopSize)
    {
        samplesCollected -= kHopSize;
        runFFT();
    }
}

// -----------------------------------------------------------------------
//  runFFT  (uses JUCE's optimized FFT engine)
// -----------------------------------------------------------------------
void SpectrumAnalyzer::runFFT()
{
    const int ringSize = kFftSize * 2;
    int startPos = (ringWritePos - kFftSize + ringSize) % ringSize;

    // performRealOnlyForwardTransform expects first kFftSize floats = real samples,
    // second half is scratch space. Output: interleaved (re,im) for kNumBins bins.
    for (int i = 0; i < kFftSize; ++i)
    {
        int idx = (startPos + i) % ringSize;
        fftData[i] = ringMono[idx] * window[i];
    }
    std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);

    fftEngine.performRealOnlyForwardTransform(fftData.data(), true);

    // Compute magnitude in dBFS with smoothing
    const float normFactor = 1.0f / (float)(kFftSize / 2);
    const float smoothCoeff = 0.6f;

    for (int k = 0; k < kNumBins; ++k)
    {
        float re  = fftData[k * 2];
        float im  = fftData[k * 2 + 1];
        float mag = std::sqrt(re * re + im * im) * normFactor;
        float db  = 20.0f * std::log10(std::max(mag, 1e-12f));
        workBuf[k] = smoothCoeff * magnitudes[k] + (1.0f - smoothCoeff) * db;
    }

    {
        juce::SpinLock::ScopedLockType sl(magnitudeLock);
        std::copy(workBuf.begin(), workBuf.end(), magnitudes.begin());
    }
}

// -----------------------------------------------------------------------
//  getMagnitudes  (GUI thread)
// -----------------------------------------------------------------------
void SpectrumAnalyzer::getMagnitudes(std::vector<float>& dest) const
{
    juce::SpinLock::ScopedLockType sl(magnitudeLock);
    dest = magnitudes;
}
