#include "BandHandle.h"
#include "../PluginProcessor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BandHandle::BandHandle(int bandIdx, juce::AudioProcessorValueTreeState& a)
    : index(bandIdx), apvts(a)
{
    setSize(kHandleSize, kHandleSize);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    // Expand hit area slightly beyond visible size
    setInterceptsMouseClicks(true, false);
}

BandHandle::~BandHandle()
{
    stopTimer();
}

void BandHandle::setSelected(bool s)
{
    selected = s;
    repaint();
}

// -----------------------------------------------------------------------
juce::String BandHandle::getTypeChar() const
{
    auto* p = apvts.getRawParameterValue(ParamID::bandType(index));
    if (!p) return "";
    static const char* chars[] = { "~", "L", "H", "<", ">", "N", "B", "A" };
    int t = juce::jlimit(0, 7, (int)*p);
    return chars[t];
}

// -----------------------------------------------------------------------
void BandHandle::paint(juce::Graphics& g)
{
    const float W   = (float)getWidth();
    const float H   = (float)getHeight();
    const float cx  = W * 0.5f;
    const float cy  = H * 0.5f;
    auto        col = bandColour();
    const float brR = W * 0.5f - 1.f;   // body radius

    // ---- Outer glow ring (when selected or hovered) ----
    if (selected || hoverAnim > 0.01f)
    {
        float alpha = selected ? 0.55f : hoverAnim * 0.35f;
        float glowR = brR + (selected ? 5.f : 3.f * hoverAnim);
        g.setColour(col.withAlpha(alpha));
        g.drawEllipse(cx - glowR, cy - glowR, glowR * 2.f, glowR * 2.f,
                      selected ? 2.0f : 1.5f);
        // Second softer ring
        g.setColour(col.withAlpha(alpha * 0.4f));
        float gr2 = glowR + 3.f;
        g.drawEllipse(cx - gr2, cy - gr2, gr2 * 2.f, gr2 * 2.f, 1.f);
    }

    // ---- Dynamic mode dashed inner indicator ----
    bool isDyn = false;
    if (auto* pd = apvts.getRawParameterValue(ParamID::bandDynamic(index)))
        isDyn = *pd > 0.5f;
    if (isDyn)
    {
        g.setColour(col.withAlpha(0.5f));
        const int segments = 8;
        for (int i = 0; i < segments; ++i)
        {
            float a0 = (float)i / segments * juce::MathConstants<float>::twoPi;
            float a1 = a0 + juce::MathConstants<float>::twoPi / segments * 0.55f;
            juce::Path seg;
            seg.addArc(cx - brR + 2.f, cy - brR + 2.f,
                       (brR - 2.f) * 2.f, (brR - 2.f) * 2.f,
                       a0, a1, true);
            g.strokePath(seg, juce::PathStrokeType(1.5f,
                juce::PathStrokeType::mitered, juce::PathStrokeType::square));
        }
    }

    // ---- Body: radial gradient ----
    {
        juce::ColourGradient body(
            col.brighter(0.35f).withAlpha(0.95f), cx - brR * 0.3f, cy - brR * 0.3f,
            col.darker(0.25f).withAlpha(0.9f),    cx + brR * 0.4f, cy + brR * 0.5f,
            true);
        g.setGradientFill(body);
        g.fillEllipse(cx - brR, cy - brR, brR * 2.f, brR * 2.f);
    }

    // ---- Rim ----
    g.setColour(col.brighter(0.3f).withAlpha(0.7f));
    g.drawEllipse(cx - brR, cy - brR, brR * 2.f, brR * 2.f, 1.2f);

    // ---- Highlight reflection (top-left arc) ----
    {
        juce::ColourGradient hl(
            juce::Colours::white.withAlpha(0.30f), cx - brR * 0.4f, cy - brR * 0.55f,
            juce::Colours::transparentWhite,       cx, cy,
            true);
        g.setGradientFill(hl);
        g.fillEllipse(cx - brR, cy - brR, brR * 2.f, brR * 2.f);
    }

    // ---- Filter type character ----
    g.setFont(juce::Font(8.5f, juce::Font::bold));
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawFittedText(getTypeChar(), (int)(cx - 6.f), (int)(cy - 6.f), 12, 12,
                     juce::Justification::centred, 1);

    // ---- Solo indicator (yellow "S" badge top-right) ----
    if (auto* ps = apvts.getRawParameterValue(ParamID::bandSolo(index)))
    {
        if (*ps > 0.5f)
        {
            float sx = cx + brR * 0.35f, sy = cy - brR * 0.7f;
            g.setColour(juce::Colour(0xFFFFCC00));
            g.fillEllipse(sx - 4.f, sy - 4.f, 8.f, 8.f);
            g.setFont(juce::Font(7.f, juce::Font::bold));
            g.setColour(juce::Colour(0xFF1A1200));
            g.drawText("S", (int)(sx - 4.f), (int)(sy - 4.f), 8, 8,
                       juce::Justification::centred);
        }
    }
}

// -----------------------------------------------------------------------
void BandHandle::mouseDown(const juce::MouseEvent& e)
{
    // Right-click → context menu with Solo / Make Dynamic / Delete
    if (e.mods.isRightButtonDown())
    {
        bool isSoloed  = false;
        bool isDynamic = false;
        if (auto* ps = apvts.getRawParameterValue(ParamID::bandSolo(index)))
            isSoloed = *ps > 0.5f;
        if (auto* pd = apvts.getRawParameterValue(ParamID::bandDynamic(index)))
            isDynamic = *pd > 0.5f;

        juce::PopupMenu menu;
        menu.addItem(1, isSoloed ? "Unsolo Band" : "Solo Band");
        menu.addSeparator();
        menu.addItem(2, isDynamic ? "Remove Dynamic" : "Make Dynamic");
        menu.addSeparator();
        menu.addItem(3, "Delete Band " + juce::String(index + 1));

        menu.showMenuAsync(juce::PopupMenu::Options{},
            [this](int result)
            {
                if (result == 1)  // Toggle solo
                {
                    if (auto* sp = apvts.getParameter(ParamID::bandSolo(index)))
                    {
                        bool cur = sp->getValue() > 0.5f;
                        sp->setValueNotifyingHost(cur ? 0.f : 1.f);
                    }
                }
                else if (result == 2)  // Toggle dynamic
                {
                    if (auto* dp = apvts.getParameter(ParamID::bandDynamic(index)))
                    {
                        bool cur = dp->getValue() > 0.5f;
                        dp->setValueNotifyingHost(cur ? 0.f : 1.f);
                    }
                }
                else if (result == 3)  // Delete band
                {
                    if (auto* ep = apvts.getParameter(ParamID::bandEnabled(index)))
                        ep->setValueNotifyingHost(0.f);
                }
            });
        return;
    }

    auto* pFreq   = apvts.getRawParameterValue(ParamID::bandFreq(index));
    auto* pGain   = apvts.getRawParameterValue(ParamID::bandGain(index));
    dragStart     = e.getEventRelativeTo(getParentComponent()).position;
    dragStartFreq = pFreq ? (double)*pFreq : 1000.0;
    dragStartGain = pGain ? (double)*pGain : 0.0;
    dragging      = true;
    setSelected(true);
}

void BandHandle::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
}

void BandHandle::mouseDrag(const juce::MouseEvent& e)
{
    if (auto* parent = getParentComponent())
    {
        const double W  = parent->getWidth();
        const double H  = parent->getHeight();

        auto eInParent = e.getEventRelativeTo(parent);
        float dx = (float)(eInParent.position.x - dragStart.x);
        float dy = (float)(eInParent.position.y - dragStart.y);

        // Horizontal → frequency (log scale)
        constexpr double logMin = 4.169925;   // log2(18)
        constexpr double logMax = 14.42471;   // log2(22000)
        double curLog  = std::log2(std::max(dragStartFreq, 18.0));
        double newLog  = juce::jlimit(logMin, logMax,
                                      curLog + (double)dx / W * (logMax - logMin));
        double newFreq = std::pow(2.0, newLog);

        // Vertical → gain (linear in dB)
        // Ctrl held = fine adjustment (÷5)
        double scale   = e.mods.isCtrlDown() ? 0.2 : 1.0;
        double newGain = juce::jlimit(-30.0, 30.0,
                                      dragStartGain - (double)dy / H * 60.0 * scale);

        if (auto* pF = apvts.getParameter(ParamID::bandFreq(index)))
            pF->setValueNotifyingHost(pF->convertTo0to1((float)newFreq));

        if (auto* pG = apvts.getParameter(ParamID::bandGain(index)))
            pG->setValueNotifyingHost(pG->convertTo0to1((float)newGain));
    }
}

void BandHandle::mouseEnter(const juce::MouseEvent&)
{
    hovered = true;
    startTimerHz(60);
}

void BandHandle::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    // keep timer running to animate out
}

void BandHandle::timerCallback()
{
    float target = (hovered || selected) ? 1.f : 0.f;
    hoverAnim += (target - hoverAnim) * 0.25f;
    if (!hovered && !selected && hoverAnim < 0.01f)
    {
        hoverAnim = 0.f;
        stopTimer();
    }
    repaint();
}

void BandHandle::mouseWheelMove(const juce::MouseEvent& e,
                                 const juce::MouseWheelDetails& w)
{
    if (e.mods.isAltDown())
    {
        // Alt + scroll wheel → adjust dynamic range
        // Also enable dynamic mode if not already on
        if (auto* dynP = apvts.getParameter(ParamID::bandDynamic(index)))
        {
            if (dynP->getValue() < 0.5f)
                dynP->setValueNotifyingHost(1.f);
        }
        auto* pDR  = apvts.getParameter(ParamID::bandDynRange(index));
        auto* pDRv = apvts.getRawParameterValue(ParamID::bandDynRange(index));
        if (!pDR || !pDRv) return;
        double newDR = juce::jlimit(0.0, 30.0, (double)*pDRv + w.deltaY * 1.0);
        pDR->setValueNotifyingHost(pDR->convertTo0to1((float)newDR));
    }
    else
    {
        // Normal scroll wheel → adjust Q
        auto* pQ   = apvts.getParameter(ParamID::bandQ(index));
        auto* pQv  = apvts.getRawParameterValue(ParamID::bandQ(index));
        if (!pQ || !pQv) return;
        double newQ = juce::jlimit(0.025, 40.0, (double)*pQv + w.deltaY * 0.4);
        pQ->setValueNotifyingHost(pQ->convertTo0to1((float)newQ));
    }
}

void BandHandle::mouseDoubleClick(const juce::MouseEvent&)
{
    if (auto* pGain = apvts.getParameter(ParamID::bandGain(index)))
        pGain->setValueNotifyingHost(pGain->getDefaultValue());
}

void BandHandle::updatePosition(int parentWidth, int parentHeight,
                                  double minFreq, double maxFreq,
                                  double minDb,   double maxDb)
{
    double freq = (double)*apvts.getRawParameterValue(ParamID::bandFreq(index));
    double gain = (double)*apvts.getRawParameterValue(ParamID::bandGain(index));

    double logMin  = std::log2(minFreq);
    double logMax  = std::log2(maxFreq);
    double logFreq = std::log2(std::max(freq, minFreq));
    double normX   = (logFreq - logMin) / (logMax - logMin);
    double normY   = 1.0 - (gain - minDb) / (maxDb - minDb);

    int cx = juce::roundToInt(normX * parentWidth);
    int cy = juce::roundToInt(normY * parentHeight);
    setCentrePosition(cx, cy);
}
