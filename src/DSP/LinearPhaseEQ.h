#pragma once
#include <vector>
#include <complex>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  LinearPhaseEQ
//  Implements linear-phase EQ via zero-phase FIR convolution
//  using the overlap-add method.
//
//  Design approach:
//   1. Sample the desired frequency response on a grid of N points.
//   2. Take the IDFT to get the FIR kernel.
//   3. Apply a Blackman window to minimise Gibbs phenomenon.
//   4. Process audio with overlap-add FFT convolution.
//
//  This introduces latency of (N/2) samples which is reported
//  back to the host for compensation.
// ============================================================
class LinearPhaseEQ
{
public:
    static constexpr int kKernelSize = 4096;   // must be power of 2
    static constexpr int kFftSize    = kKernelSize * 2;

    LinearPhaseEQ();

    void prepare(double sampleRate, int blockSize);

    // Build new FIR kernel from a magnitude response function
    // magFunc(freq_hz) → gain in dB
    template<typename MagFunc>
    void buildKernel(MagFunc magFunc)
    {
        // Step 1: sample desired magnitude on kFftSize/2+1 bins
        std::vector<std::complex<double>> H(kFftSize, {0.0, 0.0});

        for (int k = 0; k <= kFftSize / 2; ++k)
        {
            double freq = (double)k * sampleRate / (double)kFftSize;
            double magDb = magFunc(freq);
            double magLin = std::pow(10.0, magDb / 20.0);
            H[k] = { magLin, 0.0 };   // zero phase
            if (k > 0 && k < kFftSize / 2)
                H[kFftSize - k] = { magLin, 0.0 };   // conjugate symmetry
        }

        // Step 2: IDFT
        idft(H, kernel);

        // Step 3: Circular shift (linear-phase centre tap) + Blackman window
        std::vector<double> shifted(kFftSize);
        int half = kFftSize / 2;
        for (int n = 0; n < kFftSize; ++n)
            shifted[n] = kernel[(n + half) % kFftSize];

        for (int n = 0; n < kFftSize; ++n)
        {
            double w = 0.42 - 0.5 * std::cos(2.0*M_PI*n / (kFftSize-1))
                             + 0.08 * std::cos(4.0*M_PI*n / (kFftSize-1));
            kernel[n] = shifted[n] * w;
        }

        // Step 4: Compute FFT of kernel for fast convolution
        kernelSpectrum.assign(kFftSize, {0.0, 0.0});
        dft(kernel, kernelSpectrum);

        kernelReady = true;
        resetBuffers();
    }

    void processStereo(float* L, float* R, int numSamples);

    int getLatencySamples() const { return kKernelSize; }

private:
    // Simple in-place DIT Cooley-Tukey FFT
    static void fft(std::vector<std::complex<double>>& x, bool inverse);
    static void dft(const std::vector<double>& in, std::vector<std::complex<double>>& out);
    static void idft(const std::vector<std::complex<double>>& in, std::vector<double>& out);

    void processChannel(float* buf, int numSamples,
                        std::vector<double>& overlapBuf);
    void resetBuffers();

    double sampleRate  = 44100.0;
    bool   kernelReady = false;

    std::vector<double>                 kernel;
    std::vector<std::complex<double>>   kernelSpectrum;
    std::vector<double>                 overlapL, overlapR;

    // Pre-allocated working buffer for processChannel.
    // Avoids heap allocation on the audio thread every block.
    std::vector<std::complex<double>>   processBuf;
};
