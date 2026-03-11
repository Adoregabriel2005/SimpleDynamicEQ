#pragma once
#include <JuceHeader.h>
#include "BandHandle.h"
#include "ProEQColors.h"
#include "../PluginProcessor.h"

// ============================================================
//  EQCurveComponent
//  Full-featured EQ display:
//   • Real-time FFT spectrum with gradient fill + peak hold
//   • Per-band individual coloured curves + fill areas
//   • Combined EQ curve with glow (gold)
//   • Fine grid with dB and frequency axis labels
//   • Mouse hover crosshair with freq/gain readout
//   • paintOverChildren for drag-info popups on top of handles
//   • Double-click empty area to create a new Bell band
// ============================================================
class EQCurveComponent : public juce::Component,
                          public juce::Timer,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr double kMinFreq =  18.0;
    static constexpr double kMaxFreq =  22000.0;
    static constexpr double kMinDb   = -30.0;
    static constexpr double kMaxDb   =  30.0;

    explicit EQCurveComponent(ProEQProcessor& proc);
    ~EQCurveComponent() override;

    // juce::Component
    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& w) override;

    // Timer
    void timerCallback() override;

    // APVTS listener
    void parameterChanged(const juce::String& paramID, float) override;

    // Coordinate helpers (used by BandHandle for positioning)
    float  freqToX(double freq)   const noexcept;
    float  gainToY(double gainDb) const noexcept;
    double xToFreq(float x)       const noexcept;
    double yToGain(float y)       const noexcept;

    static juce::Colour getBandColour(int index) noexcept
    { return ProEQColors::getBandColour(index); }

private:
    //-- Drawing layers (called in paint order) --
    void drawBackground      (juce::Graphics& g);
    void drawSpectrumAnalyzer(juce::Graphics& g);
    void drawGrid            (juce::Graphics& g);
    void drawPerBandCurves   (juce::Graphics& g);
    void drawDynamicArrows   (juce::Graphics& g);  // dynamic EQ gain indicators
    void drawCombinedCurve   (juce::Graphics& g);
    void drawAxisLabels      (juce::Graphics& g);  // freq labels at bottom
    void drawProblemOverlay  (juce::Graphics& g);  // resonance/clip markers

    //-- Overlay (paintOverChildren) --
    void drawHoverOverlay    (juce::Graphics& g);
    void drawDragInfoPopup   (juce::Graphics& g, int bandIdx);

    //-- Helpers --
    void updateHandles();
    void invalidateCurves();
    int  findFreeBand()  const;
    void createBandAt(float x, float y);
    void showContextMenu(float x, float y);

    float specDbToY(float specDb) const noexcept;

    ProEQProcessor& proc;

    // Band handle child components
    std::array<std::unique_ptr<BandHandle>, kMaxBands> handles;

    // Spectrum data
    std::vector<float> specMagnitudes;
    std::vector<float> peakHold;

    // Mouse state
    bool   hoverActive = false;
    float  hoverX      = 0.f;
    float  hoverY      = 0.f;

    // Resize guard
    int  lastWidth  = 0;
    int  lastHeight = 0;

    // Dynamic arrow drag state
    int   dynArrowDragBand      = -1;    // which band (-1 = none)
    bool  dynArrowDragIsUp      = false; // true = upper arrow
    float dynArrowDragStartY    = 0.f;
    float dynArrowDragStartRange = 0.f;

    // ---- Magnitude curve cache (avoids recalculating trig every frame) ----
    static constexpr int kCurveRes = 256;   // number of sample points
    struct CachedCurve
    {
        std::array<float, kCurveRes> dB {};         // dB per sample point
    };
    std::array<CachedCurve, kMaxBands> bandCurveCache;
    std::array<float, kCurveRes>       combinedCurveCache {};
    std::array<double, kCurveRes>      curveFreqs {};      // precomputed frequencies
    bool                                curveCacheDirty = true;
    int                                 curveCacheWidth = 0;

    void rebuildCurveCache();

    // Problem detection frame counter
    int  problemFrameCounter = 0;
};
