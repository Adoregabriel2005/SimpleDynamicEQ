#pragma once
#include "BiquadFilter.h"
#include <cmath>
#include <array>
#include <atomic>
#include <vector>

// ============================================================
//  FilterType – matches the Pro-Q 3 band types
// ============================================================
enum class FilterType : int
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowCut,        // Butterworth, selectable order
    HighCut,
    Notch,
    BandPass,
    AllPass,
    NumTypes
};

// ============================================================
//  ProcessingMode – stereo topology
// ============================================================
enum class ProcessingMode : int
{
    Stereo = 0,
    LeftOnly,
    RightOnly,
    MidOnly,
    SideOnly,
    NumModes
};

// ============================================================
//  AnalogModel – analog EQ character emulation
// ============================================================
enum class AnalogModel : int
{
    Clean = 0,     // Digital precision (no coloration)
    SSL,           // SSL 4000 E/G – tight, punchy, surgical
    Neve,          // Neve 1073/1081 – warm, musical, silky highs
    Pultec,        // Pultec EQP-1A – broad, smooth, famous low-end trick
    API,           // API 550/560 – aggressive, punchy mids
    Sontec,        // Sontec MES-432 – mastering-grade, transparent
    NumModels
};

// ============================================================
//  EQBand – one parametric band with all filter types
// ============================================================
class EQBand
{
public:
    static constexpr int kMaxOrder = 8;   // up to 8th order cut filter

    struct Params
    {
        bool         enabled       = false;
        FilterType   type          = FilterType::Bell;
        ProcessingMode mode        = ProcessingMode::Stereo;
        double       freqHz        = 1000.0;
        double       gainDb        = 0.0;
        double       Q             = 0.707;
        int          order         = 2;    // for cut filters (2/4/6/8)
        bool         dynamic       = false;
        double       threshold     = -12.0;  // dBFS
        double       dynamicRange  = 6.0;    // max dyn gain dB
        double       attack        = 20.0;   // ms
        double       release       = 200.0;  // ms
        AnalogModel  model         = AnalogModel::Clean;
    };

    EQBand();
    void prepare(double sampleRate);
    void updateCoeffs();

    // Stereo process (L/R or M/S depending on mode)
    void processStereo(float* L, float* R, int numSamples);

    Params& getParams()             { return p; }
    const Params& getParams() const { return p; }
    void   setParams(const Params& newP);

    // Returns the complex frequency response H(e^jw) magnitude in dB
    // for use in the GUI curve drawing (no dynamic component)
    double getMagnitudeAtFreq(double freq, double sampleRate) const;

    // Returns the current dynamic gain offset in dB (for GUI visualization)
    double getDynamicGainDb() const { return currentGainDb; }

private:
    // Compute biquad coefficients for a single 2nd-order section
    static BiquadFilter::Coeffs computeCoeffs(FilterType type,
                                               double freqHz,
                                               double gainDb,
                                               double Q,
                                               double sampleRate);

    // Apply analog model character to Q and gain
    static void applyAnalogModel(AnalogModel model, FilterType type,
                                  double freqHz, double& gain, double& Q);

    // Butterworth prototype decomposed into cascaded 2nd-order sections
    void buildCutFilter(FilterType type,
                        double freqHz,
                        int order,
                        double sampleRate);

    // Dynamic EQ envelope followers
    double  computeEnvelope(double absVal, double& env);

    Params                          p;
    double                          sampleRate = 44100.0;
    int                             numSections = 1;

    std::array<BiquadFilter, kMaxOrder / 2> filters;  // up to 4 sections
    std::array<BiquadFilter::Coeffs, kMaxOrder / 2> targetCoeffs;

    // Dynamic EQ state
    double envL = 0.0, envR = 0.0;
    double attackCoeff  = 0.0;
    double releaseCoeff = 0.0;
    double currentGainDb = 0.0;

    // Lock-free pending update (written on message thread, applied on audio thread)
    Params                pendingParams;
    std::atomic<bool>     paramsReady { false };
};
