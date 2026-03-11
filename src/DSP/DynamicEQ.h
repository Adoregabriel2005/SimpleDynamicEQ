#pragma once
#include <vector>
#include <complex>
#include <cmath>

// ============================================================
//  DynamicEQ – standalone dynamic detection helper
//  (band-level analysis used inside EQBand; kept separate
//   so it can also drive a sidechain bus in the processor)
// ============================================================
class DynamicEQ
{
public:
    DynamicEQ() = default;

    void prepare(double sampleRate, double attackMs, double releaseMs)
    {
        sr       = sampleRate;
        atkCoeff = std::exp(-1.0 / (0.001 * attackMs  * sr));
        relCoeff = std::exp(-1.0 / (0.001 * releaseMs * sr));
        env      = 0.0;
    }

    // Returns gain reduction in dB (negative or zero)
    double process(double peakAbs, double threshDb, double maxCutDb) noexcept
    {
        if (peakAbs > env)
            env = atkCoeff * env + (1.0 - atkCoeff) * peakAbs;
        else
            env = relCoeff * env + (1.0 - relCoeff) * peakAbs;

        double envDb   = 20.0 * std::log10(env + 1e-12);
        double over    = envDb - threshDb;
        if (over <= 0.0) return 0.0;

        // Soft-knee proportional reduction
        double ratio = std::min(over / std::max(1.0, std::abs(threshDb)), 1.0);
        return -maxCutDb * ratio;
    }

private:
    double sr = 44100.0, atkCoeff = 0.0, relCoeff = 0.0, env = 0.0;
};
