#include "LinearPhaseEQ.h"
#include <algorithm>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------
LinearPhaseEQ::LinearPhaseEQ()
{
    kernel.resize(kFftSize, 0.0);
    kernelSpectrum.resize(kFftSize, {0.0, 0.0});
    processBuf.resize(kFftSize, {0.0, 0.0});  // pre-allocate; never resize again
}

// -----------------------------------------------------------------------
//  prepare
// -----------------------------------------------------------------------
void LinearPhaseEQ::prepare(double sr, int /*blockSize*/)
{
    sampleRate = sr;
    resetBuffers();
}

void LinearPhaseEQ::resetBuffers()
{
    overlapL.assign(kFftSize, 0.0);
    overlapR.assign(kFftSize, 0.0);
}

// -----------------------------------------------------------------------
//  FFT  (in-place Cooley-Tukey DIT, radix-2)
// -----------------------------------------------------------------------
void LinearPhaseEQ::fft(std::vector<std::complex<double>>& x, bool inverse)
{
    int N = (int)x.size();
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; ++i)
    {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }
    for (int len = 2; len <= N; len <<= 1)
    {
        double ang = 2.0 * M_PI / len * (inverse ? -1.0 : 1.0);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j)
            {
                auto u = x[i + j];
                auto v = x[i + j + len/2] * w;
                x[i + j]          = u + v;
                x[i + j + len/2]  = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse)
        for (auto& v : x) v /= (double)N;
}

void LinearPhaseEQ::dft(const std::vector<double>& in,
                         std::vector<std::complex<double>>& out)
{
    int N = (int)in.size();
    out.resize(N);
    for (int i = 0; i < N; ++i) out[i] = { in[i], 0.0 };
    fft(out, false);
}

void LinearPhaseEQ::idft(const std::vector<std::complex<double>>& in,
                          std::vector<double>& out)
{
    auto tmp = in;
    fft(tmp, true);   // inverse=true already divides by N
    out.resize(tmp.size());
    for (size_t i = 0; i < tmp.size(); ++i) out[i] = tmp[i].real();
}

// -----------------------------------------------------------------------
//  processChannel  – overlap-add with pre-computed kernel spectrum
// -----------------------------------------------------------------------
void LinearPhaseEQ::processChannel(float* buf, int numSamples,
                                    std::vector<double>& overlap)
{
    if (!kernelReady) return;

    // Use the pre-allocated buffer — zero it out for this block
    // (no heap allocation on the audio thread).
    std::fill(processBuf.begin(), processBuf.end(), std::complex<double>{0.0, 0.0});
    auto& X = processBuf;

    int validLen = std::min(numSamples, kFftSize / 2);
    for (int i = 0; i < validLen; ++i) X[i] = { (double)buf[i], 0.0 };

    // Forward FFT
    fft(X, false);

    // Multiply with kernel spectrum
    for (int k = 0; k < kFftSize; ++k) X[k] *= kernelSpectrum[k];

    // Inverse FFT
    fft(X, true);

    // Overlap-add output: output[i] = IFFT[i] + saved_tail[i]
    for (int i = 0; i < validLen; ++i)
        buf[i] = (float)(X[i].real() + overlap[i]);

    // Build new overlap tail: tail[j] = IFFT[validLen+j] + old_tail[validLen+j]
    // (the old tail beyond validLen hasn't been consumed yet)
    const int tailLen = kFftSize - validLen;
    for (int i = 0; i < tailLen; ++i)
    {
        double oldTail = (validLen + i < kFftSize) ? overlap[validLen + i] : 0.0;
        overlap[i] = X[validLen + i].real() + oldTail;
    }
    for (int i = tailLen; i < kFftSize; ++i) overlap[i] = 0.0;
}

// -----------------------------------------------------------------------
//  processStereo
// -----------------------------------------------------------------------
void LinearPhaseEQ::processStereo(float* L, float* R, int numSamples)
{
    processChannel(L, numSamples, overlapL);
    processChannel(R, numSamples, overlapR);
}
