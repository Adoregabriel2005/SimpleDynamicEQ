#pragma once
#include <cmath>

// ============================================================
//  BiquadFilter – Direct Form II Transposed biquad filter
//  Coefficients are updated atomically so the audio thread
//  never sees a half-written state.
// ============================================================
class BiquadFilter
{
public:
    struct Coeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;   // note: a0 is normalised to 1
    };

    BiquadFilter()  { reset(); }

    void reset()
    {
        z1L = z2L = z1R = z2R = 0.0;
    }

    void setCoeffs(const Coeffs& c) noexcept { coeffs = c; }
    const Coeffs& getCoeffs() const noexcept { return coeffs; }

    inline float processSampleL(float x) noexcept
    {
        double y = coeffs.b0 * x + z1L;
        z1L = coeffs.b1 * x - coeffs.a1 * y + z2L;
        z2L = coeffs.b2 * x - coeffs.a2 * y;
        return (float)y;
    }

    inline float processSampleR(float x) noexcept
    {
        double y = coeffs.b0 * x + z1R;
        z1R = coeffs.b1 * x - coeffs.a1 * y + z2R;
        z2R = coeffs.b2 * x - coeffs.a2 * y;
        return (float)y;
    }

private:
    Coeffs coeffs;
    double z1L = 0.0, z2L = 0.0;
    double z1R = 0.0, z2R = 0.0;
};
