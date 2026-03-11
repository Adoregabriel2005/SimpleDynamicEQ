#include "EQCurveComponent.h"
#include "../DSP/FrequencyProblemDetector.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -----------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------
static juce::String fmtFreq(double hz)
{
    if (hz >= 1000.0) return juce::String(hz / 1000.0, 2) + " kHz";
    return juce::String((int)hz) + " Hz";
}
static juce::String fmtGain(double db)
{
    return (db >= 0.0 ? "+" : "") + juce::String(db, 1) + " dB";
}
static juce::String fmtQ(double q)
{
    return "Q " + juce::String(q, 2);
}

// -----------------------------------------------------------------------
//  Constructor / Destructor
// -----------------------------------------------------------------------
EQCurveComponent::EQCurveComponent(ProEQProcessor& p) : proc(p)
{
    specMagnitudes.resize(1025, -90.f);
    peakHold.resize(1025, -90.f);

    auto& apvts = proc.getAPVTS();
    for (int b = 0; b < kMaxBands; ++b)
    {
        handles[b] = std::make_unique<BandHandle>(b, apvts);
        addChildComponent(*handles[b]);

        apvts.addParameterListener(ParamID::bandEnabled(b), this);
        apvts.addParameterListener(ParamID::bandFreq(b),    this);
        apvts.addParameterListener(ParamID::bandGain(b),    this);
        apvts.addParameterListener(ParamID::bandQ(b),       this);
        apvts.addParameterListener(ParamID::bandSolo(b),    this);
        apvts.addParameterListener(ParamID::bandDynamic(b), this);
    }

    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    startTimerHz(30);
}

EQCurveComponent::~EQCurveComponent()
{
    stopTimer();
    auto& apvts = proc.getAPVTS();
    for (int b = 0; b < kMaxBands; ++b)
    {
        apvts.removeParameterListener(ParamID::bandEnabled(b), this);
        apvts.removeParameterListener(ParamID::bandFreq(b),    this);
        apvts.removeParameterListener(ParamID::bandGain(b),    this);
        apvts.removeParameterListener(ParamID::bandQ(b),       this);
        apvts.removeParameterListener(ParamID::bandSolo(b),    this);
        apvts.removeParameterListener(ParamID::bandDynamic(b), this);
    }
}

// -----------------------------------------------------------------------
//  Layout
// -----------------------------------------------------------------------
void EQCurveComponent::resized()
{
    if (getWidth() != lastWidth || getHeight() != lastHeight)
    {
        lastWidth  = getWidth();
        lastHeight = getHeight();
    }
    updateHandles();
}

// -----------------------------------------------------------------------
//  Timer: advance spectrum / peak hold / repaint
// -----------------------------------------------------------------------
void EQCurveComponent::timerCallback()
{
    // Pull latest FFT from processor
    proc.getSpectrumAnalyzer().getMagnitudes(specMagnitudes);

    // Ensure peakHold matches specMagnitudes size (defensive against first-frame mismatch)
    if (peakHold.size() != specMagnitudes.size())
        peakHold.assign(specMagnitudes.size(), -90.f);

    // Decay peak hold at ~9 dB/s  (30fps => 0.3dB/frame)
    constexpr float kDecay = 0.3f;
    const size_t nBins = specMagnitudes.size();
    for (size_t i = 0; i < nBins; ++i)
    {
        if (specMagnitudes[i] > peakHold[i])
            peakHold[i] = specMagnitudes[i];
        else
            peakHold[i] = std::max(peakHold[i] - kDecay, -90.f);
    }

    // Run problem detection every ~10 frames (3 Hz)
    if (++problemFrameCounter >= 10)
    {
        problemFrameCounter = 0;
        float peak = proc.peakSample.load(std::memory_order_relaxed);
        proc.getProblemDetector().analyze(
            specMagnitudes.data(), (int)specMagnitudes.size(),
            proc.getDisplaySampleRate(), peak);
    }

    repaint();
}

void EQCurveComponent::parameterChanged(const juce::String& /*paramID*/, float)
{
    curveCacheDirty = true;
    juce::MessageManager::callAsync([this] { updateHandles(); repaint(); });
}

// -----------------------------------------------------------------------
//  Coordinate helpers
// -----------------------------------------------------------------------
float EQCurveComponent::freqToX(double freq) const noexcept
{
    const double logMin  = std::log2(kMinFreq);
    const double logMax  = std::log2(kMaxFreq);
    const double logFreq = std::log2(juce::jlimit(kMinFreq, kMaxFreq, freq));
    return (float)((logFreq - logMin) / (logMax - logMin) * getWidth());
}

float EQCurveComponent::gainToY(double gainDb) const noexcept
{
    return (float)((1.0 - (gainDb - kMinDb) / (kMaxDb - kMinDb)) * getHeight());
}

double EQCurveComponent::xToFreq(float x) const noexcept
{
    const double logMin = std::log2(kMinFreq);
    const double logMax = std::log2(kMaxFreq);
    return std::pow(2.0, logMin + (double)x / (double)getWidth() * (logMax - logMin));
}

double EQCurveComponent::yToGain(float y) const noexcept
{
    return kMaxDb - (double)y / (double)getHeight() * (kMaxDb - kMinDb);
}

float EQCurveComponent::specDbToY(float specDb) const noexcept
{
    // -90 dBFS → bottom,  0 dBFS → top
    float norm = juce::jlimit(-90.f, 0.f, specDb) / -90.f;   // 0=top,1=bottom
    return norm * (float)getHeight();
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 1 – Background
// -----------------------------------------------------------------------
void EQCurveComponent::drawBackground(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();

    // Base dark fill
    g.setColour(juce::Colour(ProEQColors::kBackground));
    g.fillRect(0, 0, W, H);

    // Subtle radial centre glow (makes it feel "lit from inside")
    juce::ColourGradient radial(
        juce::Colour(0xFF131820), (float)W * 0.5f, (float)H * 0.55f,
        juce::Colour(0xFF0D0F14), (float)W * 0.05f, 0.f, true);
    g.setGradientFill(radial);
    g.fillRect(0, 0, W, H);
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 2 – Spectrum Analyzer (FFT fill + peak hold)
// -----------------------------------------------------------------------
void EQCurveComponent::drawSpectrumAnalyzer(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    if (specMagnitudes.empty()) return;

    const int    nBins  = (int)specMagnitudes.size();  // 1025
    const double sr     = proc.getDisplaySampleRate();
    const double nyq    = sr * 0.5;
    const double logMin = std::log2(kMinFreq);
    const double logMax = std::log2(kMaxFreq);

    // --- Build spectrum fill path ---
    juce::Path fillPath;
    juce::Path linePath;

    bool started = false;
    for (int px = 0; px < W; ++px)
    {
        double freq   = xToFreq((float)px);
        double binfD  = freq / nyq * (double)(nBins - 1);
        int    bin0   = (int)binfD;
        float  frac   = (float)(binfD - bin0);
        int    bin1   = std::min(bin0 + 1, nBins - 1);

        float mag     = specMagnitudes[bin0] * (1.f - frac) + specMagnitudes[bin1] * frac;
        float y       = specDbToY(mag);

        if (!started)
        {
            fillPath.startNewSubPath((float)px, (float)H);
            fillPath.lineTo((float)px, y);
            linePath.startNewSubPath((float)px, y);
            started = true;
        }
        else
        {
            fillPath.lineTo((float)px, y);
            linePath.lineTo((float)px, y);
        }
    }
    fillPath.lineTo((float)W, (float)H);
    fillPath.closeSubPath();

    // Gradient fill: teal at top → transparent at bottom
    juce::ColourGradient grad(
        juce::Colour(ProEQColors::kSpectrumFillA),  0.f, 0.f,
        juce::Colour(ProEQColors::kSpectrumFillB),  0.f, (float)H, false);
    g.setGradientFill(grad);
    g.fillPath(fillPath);

    // Spectrum outline
    g.setColour(juce::Colour(ProEQColors::kSpectrumLine));
    g.strokePath(linePath, juce::PathStrokeType(1.2f));

    // --- Peak hold line ---
    juce::Path peakPath;
    bool peakStarted = false;
    for (int px = 0; px < W; ++px)
    {
        double freq  = xToFreq((float)px);
        double binfD = freq / nyq * (double)(nBins - 1);
        int    bin0  = (int)binfD;
        float  frac  = (float)(binfD - bin0);
        int    bin1  = std::min(bin0 + 1, nBins - 1);

        float pk = peakHold[bin0] * (1.f - frac) + peakHold[bin1] * frac;
        float y  = specDbToY(pk);

        if (!peakStarted) { peakPath.startNewSubPath((float)px, y); peakStarted = true; }
        else               peakPath.lineTo((float)px, y);
    }
    g.setColour(juce::Colour(ProEQColors::kSpectrumPeak));
    g.strokePath(peakPath, juce::PathStrokeType(1.0f));
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 3 – Grid
// -----------------------------------------------------------------------
void EQCurveComponent::drawGrid(juce::Graphics& g)
{
    const int   W          = getWidth();
    const int   H          = getHeight();
    const float fW         = (float)W;
    const float fH         = (float)H;
    const auto  colMinor   = juce::Colour(ProEQColors::kGridMinor);
    const auto  colMajor   = juce::Colour(ProEQColors::kGridMajor);
    const auto  colZero    = juce::Colour(ProEQColors::kZeroDbLine);

    // ---- Horizontal dB lines ----
    static constexpr float kDbMajor[] = { -24.f, -12.f, 0.f, 12.f, 24.f };
    static constexpr float kDbMinor[] = { -18.f, -6.f, 6.f, 18.f };

    for (float db : kDbMinor)
    {
        g.setColour(colMinor);
        g.drawHorizontalLine(juce::roundToInt(gainToY(db)), 0.f, fW);
    }
    for (float db : kDbMajor)
    {
        auto col = (db == 0.0f) ? colZero : colMajor;
        g.setColour(col);
        g.drawHorizontalLine(juce::roundToInt(gainToY(db)), 0.f, fW);
    }

    // ---- Vertical frequency lines ----
    // Minor
    static const double kFreqMinor[] = {
        30, 40, 60, 70, 80, 90,
        150, 300, 400, 600, 700, 800, 900,
        1500, 3000, 4000, 6000, 7000, 8000, 9000,
        15000
    };
    g.setColour(colMinor);
    for (double f : kFreqMinor)
        g.drawVerticalLine(juce::roundToInt(freqToX(f)), 0.f, fH);

    // Major
    static const double kFreqMajor[] = { 20, 50, 100, 200, 500,
                                          1000, 2000, 5000, 10000, 20000 };
    g.setColour(colMajor);
    for (double f : kFreqMajor)
        g.drawVerticalLine(juce::roundToInt(freqToX(f)), 0.f, fH);
}

// -----------------------------------------------------------------------
//  rebuildCurveCache – pre-compute magnitude samples for all bands
//  at kCurveRes points.  Called only when parameters change.
// -----------------------------------------------------------------------
void EQCurveComponent::rebuildCurveCache()
{
    const double sr     = proc.getDisplaySampleRate();
    const double logMin = std::log2(kMinFreq);
    const double logMax = std::log2(kMaxFreq);
    auto& apvts         = proc.getAPVTS();

    // Pre-compute frequency at each sample point
    for (int i = 0; i < kCurveRes; ++i)
    {
        double t = (double)i / (double)(kCurveRes - 1);
        curveFreqs[i] = std::pow(2.0, logMin + t * (logMax - logMin));
    }

    // Per-band + combined
    std::fill(combinedCurveCache.begin(), combinedCurveCache.end(), 0.f);

    for (int b = 0; b < kMaxBands; ++b)
    {
        bool en = (bool)*apvts.getRawParameterValue(ParamID::bandEnabled(b));
        if (!en)
        {
            std::fill(bandCurveCache[b].dB.begin(), bandCurveCache[b].dB.end(), 0.f);
            continue;
        }
        const auto& band = proc.getBand(b);
        for (int i = 0; i < kCurveRes; ++i)
        {
            float db = (float)band.getMagnitudeAtFreq(curveFreqs[i], sr);
            bandCurveCache[b].dB[i] = db;
            combinedCurveCache[i] += db;
        }
    }

    curveCacheDirty  = false;
    curveCacheWidth  = getWidth();
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 4 – Per-band individual coloured curves (cached)
// -----------------------------------------------------------------------
void EQCurveComponent::drawPerBandCurves(juce::Graphics& g)
{
    if (curveCacheDirty || curveCacheWidth != getWidth())
        rebuildCurveCache();

    const int    W        = getWidth();
    const float  zeroY    = gainToY(0.0);

    for (int b = 0; b < kMaxBands; ++b)
    {
        if (!handles[b]->isVisible()) continue;

        const auto colour = ProEQColors::getBandColour(b);
        const auto& cache = bandCurveCache[b].dB;

        juce::Path fillPath, strokePath;

        for (int px = 0; px < W; ++px)
        {
            // Map pixel to cache index (linear interpolation)
            float ci  = (float)px / (float)(W - 1) * (float)(kCurveRes - 1);
            int   i0  = juce::jlimit(0, kCurveRes - 2, (int)ci);
            float frc = ci - (float)i0;
            float db  = cache[i0] * (1.f - frc) + cache[i0 + 1] * frc;
            db = juce::jlimit((float)(kMinDb - 6.0), (float)(kMaxDb + 6.0), db);
            float y   = gainToY(db);

            if (px == 0)
            {
                fillPath.startNewSubPath(0.f, zeroY);
                fillPath.lineTo(0.f, y);
                strokePath.startNewSubPath(0.f, y);
            }
            else
            {
                fillPath.lineTo((float)px, y);
                strokePath.lineTo((float)px, y);
            }
        }
        fillPath.lineTo((float)W, fillPath.getCurrentPosition().y);
        fillPath.lineTo((float)W, zeroY);
        fillPath.closeSubPath();

        g.setColour(colour.withAlpha(0.13f));
        g.fillPath(fillPath);

        float alpha = handles[b]->isHovered() ? 0.85f : 0.55f;
        float thick = handles[b]->isHovered() ? 1.8f  : 1.1f;
        g.setColour(colour.withAlpha(alpha));
        g.strokePath(strokePath, juce::PathStrokeType(thick,
                                   juce::PathStrokeType::curved,
                                   juce::PathStrokeType::rounded));
    }
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 4b – Dynamic EQ interactive arrows
//  Two draggable triangles (▲ ▼) per dynamic band showing the
//  dynamic range limits.  A horizontal dash shows the current
//  real-time dynamic gain offset.
// -----------------------------------------------------------------------
void EQCurveComponent::drawDynamicArrows(juce::Graphics& g)
{
    auto& apvts = proc.getAPVTS();

    for (int b = 0; b < kMaxBands; ++b)
    {
        if (!handles[b]->isVisible()) continue;

        bool isDyn = (bool)*apvts.getRawParameterValue(ParamID::bandDynamic(b));
        if (!isDyn) continue;

        float freq = *apvts.getRawParameterValue(ParamID::bandFreq(b));
        float gain = *apvts.getRawParameterValue(ParamID::bandGain(b));
        float dr   = *apvts.getRawParameterValue(ParamID::bandDynRange(b));

        const auto colour = ProEQColors::getBandColour(b);
        float ax      = freqToX(freq);
        float handleY = gainToY(gain);
        float upY     = gainToY(gain + dr);   // higher gain → lower Y on screen
        float downY   = gainToY(gain - dr);   // lower gain  → higher Y on screen

        // Thin vertical line showing the full dynamic range
        if (dr >= 0.1f)
        {
            g.setColour(colour.withAlpha(0.30f));
            g.drawLine(ax, upY, ax, downY, 1.5f);
        }

        // ▲ Up arrow at gain + dynRange
        if (dr >= 0.1f)
        {
            constexpr float aw = 6.f, ah = 5.f;
            juce::Path arrow;
            arrow.addTriangle(ax - aw, upY + ah,
                              ax + aw, upY + ah,
                              ax, upY);
            bool dragging = (dynArrowDragBand == b && dynArrowDragIsUp);
            g.setColour(colour.withAlpha(dragging ? 0.95f : 0.7f));
            g.fillPath(arrow);
            g.setColour(colour.brighter(0.3f).withAlpha(0.5f));
            g.strokePath(arrow, juce::PathStrokeType(0.8f));
        }

        // ▼ Down arrow at gain - dynRange
        if (dr >= 0.1f)
        {
            constexpr float aw = 6.f, ah = 5.f;
            juce::Path arrow;
            arrow.addTriangle(ax - aw, downY - ah,
                              ax + aw, downY - ah,
                              ax, downY);
            bool dragging = (dynArrowDragBand == b && !dynArrowDragIsUp);
            g.setColour(colour.withAlpha(dragging ? 0.95f : 0.7f));
            g.fillPath(arrow);
            g.setColour(colour.brighter(0.3f).withAlpha(0.5f));
            g.strokePath(arrow, juce::PathStrokeType(0.8f));
        }

        // Current dynamic gain indicator (horizontal dash + dB label)
        double dynGain = proc.getBand(b).getDynamicGainDb();
        if (std::abs(dynGain) > 0.1)
        {
            float dynY = gainToY(gain + (float)dynGain);
            g.setColour(colour.withAlpha(0.85f));
            g.drawLine(ax - 8.f, dynY, ax + 8.f, dynY, 2.f);

            juce::String dbStr = (dynGain >= 0 ? "+" : "") + juce::String(dynGain, 1);
            g.setFont(juce::Font(9.f, juce::Font::bold));
            g.setColour(colour.brighter(0.3f).withAlpha(0.85f));
            g.drawText(dbStr, (int)(ax + 10.f), (int)(dynY - 7.f), 40, 14,
                       juce::Justification::left);
        }
    }
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 5 – Combined EQ curve (gold with glow, uses cache)
// -----------------------------------------------------------------------
void EQCurveComponent::drawCombinedCurve(juce::Graphics& g)
{
    if (curveCacheDirty || curveCacheWidth != getWidth())
        rebuildCurveCache();

    const int W = getWidth();

    juce::Path curvePath;
    for (int px = 0; px < W; ++px)
    {
        float ci  = (float)px / (float)(W - 1) * (float)(kCurveRes - 1);
        int   i0  = juce::jlimit(0, kCurveRes - 2, (int)ci);
        float frc = ci - (float)i0;
        float db  = combinedCurveCache[i0] * (1.f - frc) + combinedCurveCache[i0 + 1] * frc;
        db = juce::jlimit((float)(kMinDb - 6.0), (float)(kMaxDb + 6.0), db);
        float y   = gainToY(db);
        if (px == 0) curvePath.startNewSubPath(0.f, y);
        else          curvePath.lineTo((float)px, y);
    }

    // Outer glow (wide, very transparent)
    g.setColour(juce::Colour(0x10F0A020));
    g.strokePath(curvePath, juce::PathStrokeType(10.f, juce::PathStrokeType::curved));

    // Inner glow
    g.setColour(juce::Colour(0x28F0C040));
    g.strokePath(curvePath, juce::PathStrokeType(4.f, juce::PathStrokeType::curved));

    // Solid line
    g.setColour(juce::Colour(ProEQColors::kEqCurve));
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
}

// -----------------------------------------------------------------------
//  DRAWING LAYER 6 – Axis labels (bottom row)
// -----------------------------------------------------------------------
void EQCurveComponent::drawAxisLabels(juce::Graphics& g)
{
    const int H = getHeight();
    const auto col = juce::Colour(ProEQColors::kGridLabel);
    g.setColour(col);
    g.setFont(juce::Font(9.5f));

    static const double kLabFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    static const char*  kLabText[]  = { "20","50","100","200","500","1k","2k","5k","10k","20k" };

    for (int i = 0; i < 10; ++i)
    {
        float x = freqToX(kLabFreqs[i]);
        g.drawText(kLabText[i], (int)x - 16, H - 16, 32, 14,
                   juce::Justification::centred, false);
    }

    // dB labels
    static constexpr float kDbLabels[] = { -24.f, -12.f, 0.f, 12.f, 24.f };
    for (float db : kDbLabels)
    {
        float y = gainToY(db);
        g.drawText(juce::String((int)db), 3, (int)y - 7, 26, 14,
                   juce::Justification::left, false);
    }
}

// -----------------------------------------------------------------------
//  paint  (layers 1-6 in order; handles are children → paint on top)
// -----------------------------------------------------------------------
void EQCurveComponent::paint(juce::Graphics& g)
{
    drawBackground(g);
    drawSpectrumAnalyzer(g);
    drawGrid(g);
    drawPerBandCurves(g);
    drawDynamicArrows(g);
    drawCombinedCurve(g);
    drawAxisLabels(g);
    drawProblemOverlay(g);
}

// -----------------------------------------------------------------------
//  paintOverChildren  (draws on top of all child BandHandles)
// -----------------------------------------------------------------------
void EQCurveComponent::paintOverChildren(juce::Graphics& g)
{
    // 1. Hover crosshair
    if (hoverActive) drawHoverOverlay(g);

    // 2. Drag info popup for any handle currently being dragged
    for (int b = 0; b < kMaxBands; ++b)
        if (handles[b]->isVisible() && handles[b]->isBeingDragged())
            drawDragInfoPopup(g, b);
}

// -----------------------------------------------------------------------
//  Hover overlay: vertical line + freq & gain readout
// -----------------------------------------------------------------------
void EQCurveComponent::drawHoverOverlay(juce::Graphics& g)
{
    const int   H     = getHeight();
    const float gx    = hoverX;

    // Vertical line
    g.setColour(juce::Colour(0x30AABBCC));
    g.drawVerticalLine(juce::roundToInt(gx), 0.f, (float)H);

    // Frequency label box at top
    juce::String freqStr = fmtFreq(xToFreq(gx));
    auto font = juce::Font(10.f);
    g.setFont(font);
    int tw = font.getStringWidth(freqStr) + 10;
    float bx = juce::jlimit(0.f, (float)(getWidth() - tw), gx - (float)tw * 0.5f);
    juce::Rectangle<int> box((int)bx, 4, tw, 16);

    g.setColour(juce::Colour(0xCC101418));
    g.fillRoundedRectangle(box.toFloat(), 3.f);
    g.setColour(juce::Colour(0xFFAABBCC));
    g.drawText(freqStr, box, juce::Justification::centred);

    // Gain readout at mouse Y
    juce::String gainStr = fmtGain(yToGain(hoverY));
    int gw = font.getStringWidth(gainStr) + 10;
    float gy2 = juce::jlimit(0.f, (float)(H - 16), hoverY - 8.f);
    juce::Rectangle<int> gbox(6, (int)gy2, gw, 15);

    g.setColour(juce::Colour(0xCC101418));
    g.fillRoundedRectangle(gbox.toFloat(), 3.f);
    g.setColour(juce::Colour(0xFF88AACC));
    g.drawText(gainStr, gbox, juce::Justification::centred);
}

// -----------------------------------------------------------------------
//  Drag info popup (shown while user drags a band handle)
// -----------------------------------------------------------------------
void EQCurveComponent::drawDragInfoPopup(juce::Graphics& g, int bandIdx)
{
    auto& apvts   = proc.getAPVTS();
    auto  bHandle = handles[bandIdx].get();
    if (!bHandle) return;

    float hx = (float)bHandle->getX() + bHandle->getWidth()  * 0.5f;
    float hy = (float)bHandle->getY() + bHandle->getHeight() * 0.5f;

    double freq   = *apvts.getRawParameterValue(ParamID::bandFreq(bandIdx));
    double gainDb = *apvts.getRawParameterValue(ParamID::bandGain(bandIdx));
    double q      = *apvts.getRawParameterValue(ParamID::bandQ(bandIdx));

    juce::String info = fmtFreq(freq) + "  " + fmtGain(gainDb) + "  " + fmtQ(q);

    auto   font  = juce::Font(10.5f);
    g.setFont(font);
    int    tw    = font.getStringWidth(info) + 18;
    int    th    = 20;
    float  bx    = juce::jlimit(2.f, (float)(getWidth() - tw - 2),
                                  hx - (float)tw * 0.5f);
    float  by    = hy - 36.f;
    if (by < 4.f) by = hy + 22.f;

    auto col = ProEQColors::getBandColour(bandIdx);

    // Shadow
    g.setColour(juce::Colour(0xAA000000));
    g.fillRoundedRectangle(bx + 2.f, by + 2.f, (float)tw, (float)th, 4.f);

    // Background
    g.setColour(juce::Colour(0xEE111520));
    g.fillRoundedRectangle(bx, by, (float)tw, (float)th, 4.f);

    // Coloured border
    g.setColour(col.withAlpha(0.7f));
    g.drawRoundedRectangle(bx, by, (float)tw, (float)th, 4.f, 1.f);

    // Text
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.drawText(info, (int)bx, (int)by, tw, th, juce::Justification::centred);
}

// -----------------------------------------------------------------------
//  updateHandles
// -----------------------------------------------------------------------
void EQCurveComponent::updateHandles()
{
    if (getWidth() == 0 || getHeight() == 0) return;
    auto& apvts = proc.getAPVTS();
    for (int b = 0; b < kMaxBands; ++b)
    {
        bool en = (bool)*apvts.getRawParameterValue(ParamID::bandEnabled(b));
        handles[b]->setVisible(en);
        if (en)
            handles[b]->updatePosition(getWidth(), getHeight(),
                                        kMinFreq, kMaxFreq, kMinDb, kMaxDb);
    }
}

void EQCurveComponent::invalidateCurves() { repaint(); }

// -----------------------------------------------------------------------
//  Mouse interaction
// -----------------------------------------------------------------------
void EQCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    hoverActive = true;
    hoverX = (float)e.x;
    hoverY = (float)e.y;
    repaint();
}

void EQCurveComponent::mouseEnter(const juce::MouseEvent& e)
{
    hoverActive = true;
    hoverX = (float)e.x;
    hoverY = (float)e.y;
}

void EQCurveComponent::mouseExit(const juce::MouseEvent& e)
{
    hoverActive = false;
    repaint();
}

void EQCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showContextMenu((float)e.x, (float)e.y);
        return;
    }

    // Check if click hits a dynamic arrow
    auto& apvts = proc.getAPVTS();
    float mx = (float)e.x;
    float my = (float)e.y;

    for (int b = 0; b < kMaxBands; ++b)
    {
        if (!handles[b]->isVisible()) continue;
        bool isDyn = (bool)*apvts.getRawParameterValue(ParamID::bandDynamic(b));
        if (!isDyn) continue;

        float freq = *apvts.getRawParameterValue(ParamID::bandFreq(b));
        float gain = *apvts.getRawParameterValue(ParamID::bandGain(b));
        float dr   = *apvts.getRawParameterValue(ParamID::bandDynRange(b));
        if (dr < 0.1f) continue;

        float ax  = freqToX(freq);
        float upY   = gainToY(gain + dr);
        float downY = gainToY(gain - dr);

        constexpr float hitR = 12.f;

        if (std::abs(mx - ax) < hitR && std::abs(my - upY) < hitR)
        {
            dynArrowDragBand      = b;
            dynArrowDragIsUp      = true;
            dynArrowDragStartY    = my;
            dynArrowDragStartRange = dr;
            return;
        }
        if (std::abs(mx - ax) < hitR && std::abs(my - downY) < hitR)
        {
            dynArrowDragBand      = b;
            dynArrowDragIsUp      = false;
            dynArrowDragStartY    = my;
            dynArrowDragStartRange = dr;
            return;
        }
    }
}

void EQCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dynArrowDragBand < 0) return;

    float dy = (float)e.y - dynArrowDragStartY;
    float dbPerPixel = (float)(kMaxDb - kMinDb) / (float)getHeight();

    // Up arrow: dragging up (neg dy) → increase range
    // Down arrow: dragging down (pos dy) → increase range
    float newRange = dynArrowDragIsUp
        ? dynArrowDragStartRange - dy * dbPerPixel
        : dynArrowDragStartRange + dy * dbPerPixel;

    newRange = juce::jlimit(0.f, 30.f, newRange);

    auto& apvts = proc.getAPVTS();
    if (auto* p = apvts.getParameter(ParamID::bandDynRange(dynArrowDragBand)))
        p->setValueNotifyingHost(p->convertTo0to1(newRange));
}

void EQCurveComponent::mouseUp(const juce::MouseEvent&)
{
    dynArrowDragBand = -1;
}

void EQCurveComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Double-click on empty area → create Bell band here
    createBandAt((float)e.x, (float)e.y);
}

void EQCurveComponent::mouseWheelMove(const juce::MouseEvent&,
                                       const juce::MouseWheelDetails&) {}

// -----------------------------------------------------------------------
//  Band creation helpers
// -----------------------------------------------------------------------
int EQCurveComponent::findFreeBand() const
{
    auto& apvts = proc.getAPVTS();
    for (int b = 0; b < kMaxBands; ++b)
        if (!(bool)*apvts.getRawParameterValue(ParamID::bandEnabled(b)))
            return b;
    return -1;
}

void EQCurveComponent::createBandAt(float x, float y)
{
    int b = findFreeBand();
    if (b < 0) return;

    auto& apvts = proc.getAPVTS();
    double freq   = xToFreq(x);
    double gainDb = yToGain(y);
    freq   = juce::jlimit(kMinFreq, kMaxFreq, freq);
    gainDb = juce::jlimit(kMinDb,   kMaxDb,   gainDb);

    if (auto* fp = apvts.getParameter(ParamID::bandFreq(b)))
        fp->setValueNotifyingHost(fp->convertTo0to1((float)freq));
    if (auto* gp = apvts.getParameter(ParamID::bandGain(b)))
        gp->setValueNotifyingHost(gp->convertTo0to1((float)gainDb));
    if (auto* ep = apvts.getParameter(ParamID::bandEnabled(b)))
        ep->setValueNotifyingHost(1.f);
    if (auto* tp = apvts.getParameter(ParamID::bandType(b)))
        tp->setValueNotifyingHost(tp->convertTo0to1((float)FilterType::Bell));

    updateHandles();
}

// -----------------------------------------------------------------------
//  Problem overlay: resonance markers, clipping indicator
// -----------------------------------------------------------------------
void EQCurveComponent::drawProblemOverlay(juce::Graphics& g)
{
    auto& detector = proc.getProblemDetector();
    auto& problems = detector.getProblems();

    if (problems.empty()) return;

    const float W = (float)getWidth();
    const float H = (float)getHeight();
    auto font = juce::Font(9.5f, juce::Font::bold);
    g.setFont(font);

    for (auto& p : problems)
    {
        if (p.type == FrequencyProblemDetector::Problem::Resonance)
        {
            float xPos = freqToX(p.freqHz);
            // Red vertical line
            g.setColour(juce::Colour(0x40FF2020));
            g.drawVerticalLine((int)xPos, 0.f, H);

            // Red triangle marker at top
            juce::Path tri;
            tri.addTriangle(xPos - 5.f, 2.f, xPos + 5.f, 2.f, xPos, 12.f);
            g.setColour(juce::Colour(0xCCFF3030));
            g.fillPath(tri);

            // Label
            int tw = font.getStringWidth(p.description) + 8;
            float lx = juce::jlimit(0.f, W - (float)tw, xPos - (float)tw * 0.5f);
            g.setColour(juce::Colour(0xBB200808));
            g.fillRoundedRectangle(lx, 14.f, (float)tw, 14.f, 3.f);
            g.setColour(juce::Colour(0xFFFF6060));
            g.drawText(p.description, (int)lx, 14, tw, 14, juce::Justification::centred);
        }
        else if (p.type == FrequencyProblemDetector::Problem::Clipping)
        {
            // Red CLIP badge top-right
            g.setColour(juce::Colour(0xDDFF1010));
            g.fillRoundedRectangle(W - 50.f, 4.f, 46.f, 18.f, 4.f);
            g.setColour(juce::Colours::white);
            g.drawText("CLIP", (int)(W - 50), 4, 46, 18, juce::Justification::centred);
        }
    }
}

void EQCurveComponent::showContextMenu(float x, float y)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Add Bell band here");
    menu.addItem(2, "Add Low Shelf here");
    menu.addItem(3, "Add High Shelf here");
    menu.addItem(4, "Add Low Cut here");
    menu.addItem(5, "Add High Cut here");

    auto callback = [this, x, y](int result)
    {
        if (result == 0) return;
        static const FilterType types[] = {
            FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf,
            FilterType::LowCut, FilterType::HighCut
        };
        int b = findFreeBand();
        if (b < 0) return;

        auto& apvts = proc.getAPVTS();
        double freq   = xToFreq(x);
        double gainDb = (result == 4 || result == 5) ? 0.0 : yToGain(y);
        freq   = juce::jlimit(kMinFreq, kMaxFreq, freq);
        gainDb = juce::jlimit(kMinDb,   kMaxDb,   gainDb);

        if (auto* fp = apvts.getParameter(ParamID::bandFreq(b)))
            fp->setValueNotifyingHost(fp->convertTo0to1((float)freq));
        if (auto* gp = apvts.getParameter(ParamID::bandGain(b)))
            gp->setValueNotifyingHost(gp->convertTo0to1((float)gainDb));
        if (auto* ep = apvts.getParameter(ParamID::bandEnabled(b)))
            ep->setValueNotifyingHost(1.f);
        if (auto* tp = apvts.getParameter(ParamID::bandType(b)))
            tp->setValueNotifyingHost(tp->convertTo0to1((float)types[result - 1]));

        updateHandles();
    };

    menu.showMenuAsync(juce::PopupMenu::Options{}, callback);
}
