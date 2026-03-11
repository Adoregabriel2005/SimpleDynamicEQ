#include "EQBand.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
static inline double linToDb(double lin) { return 20.0 * std::log10(std::max(lin, 1e-12)); }

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------
EQBand::EQBand()
{
    for (auto& f : filters) f.reset();
}

// -----------------------------------------------------------------------
//  prepare
// -----------------------------------------------------------------------
void EQBand::prepare(double sr)
{
    sampleRate = sr;
    attackCoeff  = std::exp(-1.0 / (0.001 * p.attack  * sr));
    releaseCoeff = std::exp(-1.0 / (0.001 * p.release * sr));
    updateCoeffs();
    for (auto& f : filters) f.reset();
    envL = envR = 0.0;
    currentGainDb = 0.0;
}

// -----------------------------------------------------------------------
//  setParams  – called from message thread; audio thread picks it up
//  at the start of the next processStereo call via paramsReady flag.
// -----------------------------------------------------------------------
void EQBand::setParams(const Params& newP)
{
    pendingParams = newP;
    // Release store: all writes above are visible to audio thread
    // once it reads paramsReady with acquire ordering.
    paramsReady.store(true, std::memory_order_release);
}

// -----------------------------------------------------------------------
//  updateCoeffs  – build coefficient cascade for current parameters
// -----------------------------------------------------------------------
void EQBand::updateCoeffs()
{
    if (!p.enabled) return;

    // Apply analog model character to effective Q and gain
    double effQ    = p.Q;
    double effGain = p.gainDb;
    applyAnalogModel(p.model, p.type, p.freqHz, effGain, effQ);

    if (p.type == FilterType::LowCut || p.type == FilterType::HighCut)
    {
        buildCutFilter(p.type, p.freqHz, p.order, sampleRate);
    }
    else
    {
        numSections = 1;
        targetCoeffs[0] = computeCoeffs(p.type, p.freqHz, effGain, effQ, sampleRate);
        filters[0].setCoeffs(targetCoeffs[0]);
    }
}

// -----------------------------------------------------------------------
//  computeCoeffs  – Robert Bristow-Johnson Audio EQ Cookbook
// -----------------------------------------------------------------------
BiquadFilter::Coeffs EQBand::computeCoeffs(FilterType type,
                                             double freqHz,
                                             double gainDb,
                                             double Q,
                                             double sr)
{
    const double w0    = 2.0 * M_PI * freqHz / sr;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double A     = std::pow(10.0, gainDb / 40.0);  // sqrt of linear gain
    const double alpha = sinW0 / (2.0 * Q);

    BiquadFilter::Coeffs c;

    switch (type)
    {
        case FilterType::Bell:
        {
            double a0 = 1.0 + alpha / A;
            c.b0 = (1.0 + alpha * A) / a0;
            c.b1 = (-2.0 * cosW0)    / a0;
            c.b2 = (1.0 - alpha * A) / a0;
            c.a1 = (-2.0 * cosW0)    / a0;
            c.a2 = (1.0 - alpha / A) / a0;
            break;
        }
        case FilterType::LowShelf:
        {
            double sqrtA = std::sqrt(A);
            double a0 = (A + 1.0) + (A - 1.0)*cosW0 + 2.0*sqrtA*alpha;
            c.b0 = (A * ((A+1.0) - (A-1.0)*cosW0 + 2.0*sqrtA*alpha)) / a0;
            c.b1 = (2.0*A * ((A-1.0) - (A+1.0)*cosW0))               / a0;
            c.b2 = (A * ((A+1.0) - (A-1.0)*cosW0 - 2.0*sqrtA*alpha)) / a0;
            c.a1 = (-2.0 * ((A-1.0) + (A+1.0)*cosW0))                / a0;
            c.a2 = ((A+1.0) + (A-1.0)*cosW0 - 2.0*sqrtA*alpha)       / a0;
            break;
        }
        case FilterType::HighShelf:
        {
            double sqrtA = std::sqrt(A);
            double a0 = (A + 1.0) - (A - 1.0)*cosW0 + 2.0*sqrtA*alpha;
            c.b0 = (A * ((A+1.0) + (A-1.0)*cosW0 + 2.0*sqrtA*alpha)) / a0;
            c.b1 = (-2.0*A * ((A-1.0) + (A+1.0)*cosW0))              / a0;
            c.b2 = (A * ((A+1.0) + (A-1.0)*cosW0 - 2.0*sqrtA*alpha)) / a0;
            c.a1 = (2.0 * ((A-1.0) - (A+1.0)*cosW0))                 / a0;
            c.a2 = ((A+1.0) - (A-1.0)*cosW0 - 2.0*sqrtA*alpha)       / a0;
            break;
        }
        case FilterType::Notch:
        {
            double a0 = 1.0 + alpha;
            c.b0 =  1.0        / a0;
            c.b1 = -2.0*cosW0  / a0;
            c.b2 =  1.0        / a0;
            c.a1 = -2.0*cosW0  / a0;
            c.a2 = (1.0-alpha) / a0;
            break;
        }
        case FilterType::BandPass:
        {
            double a0 = 1.0 + alpha;
            c.b0 =  (sinW0 / 2.0) / a0;
            c.b1 =  0.0;
            c.b2 = -(sinW0 / 2.0) / a0;
            c.a1 = -2.0*cosW0     / a0;
            c.a2 = (1.0 - alpha)  / a0;
            break;
        }
        case FilterType::AllPass:
        {
            double a0 = 1.0 + alpha;
            c.b0 = (1.0 - alpha) / a0;
            c.b1 = -2.0*cosW0   / a0;
            c.b2 =  1.0;
            c.a1 = -2.0*cosW0   / a0;
            c.a2 = (1.0-alpha)  / a0;
            break;
        }
        default:
            // passthrough
            c.b0 = 1.0; c.b1 = 0.0; c.b2 = 0.0;
            c.a1 = 0.0; c.a2 = 0.0;
            break;
    }
    return c;
}

// -----------------------------------------------------------------------
//  applyAnalogModel – modify Q and gain to emulate analog EQ character
//  Each model has a distinct personality:
//   SSL:    Tight Q, punchy mids, slight gain boost at edges
//   Neve:   Wider Q, warm lows, silky highs, gentle saturation curve
//   Pultec: Very wide Q for boosts, narrower for cuts (famous Pultec trick)
//   API:    Proportional Q (Q narrows with more gain), aggressive mids
//   Sontec: Mastering-grade, very smooth, minimal Q shift
// -----------------------------------------------------------------------
void EQBand::applyAnalogModel(AnalogModel model, FilterType type,
                                double freqHz, double& gain, double& Q)
{
    if (model == AnalogModel::Clean) return;
    
    const double absGain = std::abs(gain);
    const bool isBoosting = gain > 0.0;
    
    switch (model)
    {
        case AnalogModel::SSL:
        {
            // SSL: tighter Q under boost, slight presence bump
            if (type == FilterType::Bell)
                Q *= isBoosting ? (1.0 + absGain * 0.02) : 1.0;
            // Slight warmth: tiny gain asymmetry
            if (isBoosting && freqHz < 200.0)
                gain *= 1.05;  // subtle low-end push
            break;
        }
        case AnalogModel::Neve:
        {
            // Neve: wider, musical Q; warm low end
            if (type == FilterType::Bell || type == FilterType::LowShelf)
                Q *= 0.85;  // wider bandwidth = more musical
            // Neve silk: high boosts are gentler
            if (isBoosting && freqHz > 4000.0)
                gain *= 0.92;
            // Low-end warmth
            if (freqHz < 300.0)
                gain *= 1.08;
            break;
        }
        case AnalogModel::Pultec:
        {
            // Pultec: very wide curves, famous boost+cut trick
            if (type == FilterType::Bell)
            {
                Q *= isBoosting ? 0.6 : 1.2;  // wide boost, narrow cut
            }
            if (type == FilterType::LowShelf && isBoosting)
                Q *= 0.5;  // very broad low boost
            break;
        }
        case AnalogModel::API:
        {
            // API 550: proportional Q (Q tightens with more gain)
            if (type == FilterType::Bell)
                Q *= 1.0 + absGain * 0.04;
            // Aggressive mid presence
            if (freqHz > 800.0 && freqHz < 5000.0 && isBoosting)
                gain *= 1.1;
            break;
        }
        case AnalogModel::Sontec:
        {
            // Sontec: extremely smooth, barely perceptible character
            if (type == FilterType::Bell)
                Q *= 0.95;
            // Mastering-grade: gentle gain limiting for extreme values
            if (absGain > 12.0)
                gain = gain > 0 ? 12.0 + (gain - 12.0) * 0.7
                                 : -12.0 + (gain + 12.0) * 0.7;
            break;
        }
        default: break;
    }
    
    // Clamp Q to safe range
    Q = std::max(0.025, std::min(40.0, Q));
}

// -----------------------------------------------------------------------
//  buildCutFilter – Butterworth cascade (2nd-order sections)
//  Based on standard Butterworth pole decomposition
// -----------------------------------------------------------------------
void EQBand::buildCutFilter(FilterType type, double freqHz, int order, double sr)
{
    // Clamp order to even values between 2 and kMaxOrder
    int actualOrder = std::max(2, std::min(kMaxOrder, (order / 2) * 2));
    numSections = actualOrder / 2;

    for (int k = 0; k < numSections; ++k)
    {
        // Butterworth Q values for each stage
        double angle = M_PI * (2.0 * (k + 1) + actualOrder - 1.0) / (2.0 * actualOrder);
        double stageQ = -0.5 / std::cos(angle);

        BiquadFilter::Coeffs c;
        const double w0    = 2.0 * M_PI * freqHz / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * stageQ);

        if (type == FilterType::LowCut)
        {
            double a0 = 1.0 + alpha;
            c.b0 =  (1.0 + cosW0) / (2.0 * a0);
            c.b1 = -(1.0 + cosW0) / a0;
            c.b2 =  (1.0 + cosW0) / (2.0 * a0);
            c.a1 = (-2.0 * cosW0) / a0;
            c.a2 = (1.0 - alpha)  / a0;
        }
        else  // HighCut
        {
            double a0 = 1.0 + alpha;
            c.b0 =  (1.0 - cosW0) / (2.0 * a0);
            c.b1 =  (1.0 - cosW0) / a0;
            c.b2 =  (1.0 - cosW0) / (2.0 * a0);
            c.a1 = (-2.0 * cosW0) / a0;
            c.a2 = (1.0 - alpha)  / a0;
        }

        targetCoeffs[k] = c;
        filters[k].setCoeffs(c);
    }
}

// -----------------------------------------------------------------------
//  Dynamic EQ envelope follower
// -----------------------------------------------------------------------
double EQBand::computeEnvelope(double absVal, double& env)
{
    if (absVal > env)
        env = attackCoeff * env + (1.0 - attackCoeff) * absVal;
    else
        env = releaseCoeff * env + (1.0 - releaseCoeff) * absVal;
    return env;
}

// -----------------------------------------------------------------------
//  processStereo
// -----------------------------------------------------------------------
void EQBand::processStereo(float* L, float* R, int numSamples)
{
    // Apply any pending parameter update posted from the message thread.
    if (paramsReady.load(std::memory_order_acquire))
    {
        paramsReady.store(false, std::memory_order_relaxed);
        p = pendingParams;
        attackCoeff  = std::exp(-1.0 / (0.001 * p.attack  * sampleRate));
        releaseCoeff = std::exp(-1.0 / (0.001 * p.release * sampleRate));
        updateCoeffs();
    }

    if (!p.enabled) return;

    // Dynamic EQ: compute target gain once per block from peak amplitude
    if (p.dynamic && (p.type != FilterType::LowCut && p.type != FilterType::HighCut))
    {
        // Find block peak
        double blockPeak = 0.0;
        for (int i = 0; i < numSamples; ++i)
            blockPeak = std::max(blockPeak, (std::abs((double)L[i]) + std::abs((double)R[i])) * 0.5);

        double envNow    = computeEnvelope(blockPeak, envL);
        double envDb     = linToDb(envNow + 1e-12);
        double overshoot = envDb - p.threshold;

        double targetGain;
        if (overshoot > 0.0)
        {
            double ratio  = std::min(overshoot / std::max(std::abs(p.threshold), 1.0), 1.0);
            double sign   = (p.gainDb >= 0.0) ? -1.0 : 1.0;   // oppose static gain direction
            targetGain    = p.dynamicRange * ratio * sign;
        }
        else
        {
            targetGain = 0.0;
        }

        // Smooth gain transitions
        const double smoothAlpha = 0.1;
        currentGainDb += smoothAlpha * (targetGain - currentGainDb);

        // Update filter with combined static+dynamic gain
        Params dynP = p;
        dynP.gainDb = p.gainDb + currentGainDb;
        double effGain = dynP.gainDb;
        double effQ    = dynP.Q;
        applyAnalogModel(dynP.model, dynP.type, dynP.freqHz, effGain, effQ);
        filters[0].setCoeffs(computeCoeffs(dynP.type, dynP.freqHz,
                                            effGain, effQ, sampleRate));
    }

    // Switch on mode ONCE outside the sample loop so the CPU can predict
    // the branch and the inner loop stays tight with no repeated checks.
    switch (p.mode)
    {
        case ProcessingMode::Stereo:
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float s = L[i];
                float t = R[i];
                for (int k = 0; k < numSections; ++k)
                {
                    s = filters[k].processSampleL(s);
                    t = filters[k].processSampleR(t);
                }
                L[i] = s; R[i] = t;
            }
            break;
        }
        case ProcessingMode::LeftOnly:
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float s = L[i];
                for (int k = 0; k < numSections; ++k)
                    s = filters[k].processSampleL(s);
                L[i] = s;
            }
            break;
        }
        case ProcessingMode::RightOnly:
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float t = R[i];
                for (int k = 0; k < numSections; ++k)
                    t = filters[k].processSampleR(t);
                R[i] = t;
            }
            break;
        }
        case ProcessingMode::MidOnly:
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float M = (L[i] + R[i]) * 0.5f;
                float S = (L[i] - R[i]) * 0.5f;
                for (int k = 0; k < numSections; ++k)
                    M = filters[k].processSampleL(M);
                L[i] = M + S;
                R[i] = M - S;
            }
            break;
        }
        case ProcessingMode::SideOnly:
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float M = (L[i] + R[i]) * 0.5f;
                float S = (L[i] - R[i]) * 0.5f;
                for (int k = 0; k < numSections; ++k)
                    S = filters[k].processSampleL(S);
                L[i] = M + S;
                R[i] = M - S;
            }
            break;
        }
        default: break;
    }
}

// -----------------------------------------------------------------------
//  getMagnitudeAtFreq  – used for GUI curve drawing
// -----------------------------------------------------------------------
double EQBand::getMagnitudeAtFreq(double freq, double sr) const
{
    if (!p.enabled) return 0.0;  // 0 dB = no effect

    double magnitudeLinear = 1.0;

    int sections = numSections;
    for (int s = 0; s < sections; ++s)
    {
        const auto& c = targetCoeffs[s];
        // H(e^jw) evaluated at w = 2*pi*f/sr
        double w  = 2.0 * M_PI * freq / sr;
        // Using complex arithmetic: H = (b0 + b1*z^-1 + b2*z^-2) /
        //                               (1  + a1*z^-1 + a2*z^-2)
        // z = e^jw
        double cosW = std::cos(w), sinW = std::sin(w);
        double cos2W = std::cos(2.0*w), sin2W = std::sin(2.0*w);

        double numRe = c.b0 + c.b1*cosW + c.b2*cos2W;
        double numIm =        c.b1*(-sinW) + c.b2*(-sin2W);
        double denRe = 1.0  + c.a1*cosW + c.a2*cos2W;
        double denIm =        c.a1*(-sinW) + c.a2*(-sin2W);

        double numMag2 = numRe*numRe + numIm*numIm;
        double denMag2 = denRe*denRe + denIm*denIm;

        if (denMag2 < 1e-30) denMag2 = 1e-30;
        magnitudeLinear *= std::sqrt(numMag2 / denMag2);
    }

    return 20.0 * std::log10(magnitudeLinear + 1e-12);
}
