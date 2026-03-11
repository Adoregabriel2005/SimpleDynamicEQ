#pragma once
#include <JuceHeader.h>

// ============================================================
//  ProEQColors  — dark wine / burgundy design palette
// ============================================================
namespace ProEQColors
{
    // ---- Integer ARGB constants (kXxx form, for juce::Colour(uint32_t)) ----
    static constexpr uint32_t kBackground    = 0xFF0E0A0C;
    static constexpr uint32_t kGridMinor     = 0xFF16101A;
    static constexpr uint32_t kGridMajor     = 0xFF221820;
    static constexpr uint32_t kZeroDbLine    = 0xFF3A2030;
    static constexpr uint32_t kGridLabel     = 0xFF50354A;
    static constexpr uint32_t kSpectrumFillA = 0xFF4A1028;
    static constexpr uint32_t kSpectrumFillB = 0x00200010;
    static constexpr uint32_t kSpectrumLine  = 0xFFC04060;
    static constexpr uint32_t kSpectrumPeak  = 0xFFE06888;
    static constexpr uint32_t kEqCurve       = 0xFFF0C850;
    static constexpr uint32_t kHeaderBg      = 0xFF0A0608;
    static constexpr uint32_t kPanelBg       = 0xFF0A0608;

    // ---- Dark background tier ----
    static const juce::Colour background   { 0xFF0C0810 };   // deepest dark-wine
    static const juce::Colour bgCurve      { 0xFF0E0A10 };   // curve area
    static const juce::Colour bgHeader     { 0xFF0A0608 };   // top bar
    static const juce::Colour bgPanel      { 0xFF0A0608 };   // bottom panel
    static const juce::Colour bgPanelDark  { 0xFF060406 };   // inset areas

    // ---- Grid ----
    static const juce::Colour gridMinor    { 0xFF16101A };
    static const juce::Colour gridMajor    { 0xFF221820 };
    static const juce::Colour gridZero     { 0xFF3A2030 };   // 0 dB line
    static const juce::Colour gridLabel    { 0xFF50354A };

    // ---- Spectrum analyzer ----
    static const juce::Colour specFillTop  { 0xFF4A1028 };   // wine gradient top
    static const juce::Colour specFillBot  { 0x00200010 };   // transparent bottom
    static const juce::Colour specLine     { 0xFFC04060 };   // wine-rose outline
    static const juce::Colour specPeak     { 0xFFE06888 };   // peak hold rose

    // ---- EQ combined curve ----
    static const juce::Colour eqCurve      { 0xFFF0C850 };   // warm gold
    static const juce::Colour eqGlow       { 0x15D09030 };   // behind-glow

    // ---- Controls ----
    static const juce::Colour knobBg       { 0xFF141018 };
    static const juce::Colour knobTrack    { 0xFF281828 };
    static const juce::Colour knobText     { 0xFFBCA8B8 };
    static const juce::Colour buttonBg     { 0xFF180E18 };
    static const juce::Colour buttonFg     { 0xFF886880 };
    static const juce::Colour comboBg      { 0xFF130E14 };
    static const juce::Colour comboFg      { 0xFFB0A0AC };
    static const juce::Colour separator    { 0xFF1C1420 };
    static const juce::Colour textDim      { 0xFF605060 };
    static const juce::Colour textMid      { 0xFF907888 };
    static const juce::Colour textBright   { 0xFFD8C8D4 };

    // ---- Band colours (8 cycling) ----
    static const juce::Colour bandPalette[8] =
    {
        juce::Colour(0xFFE05555),   // 0  red
        juce::Colour(0xFFE08830),   // 1  orange
        juce::Colour(0xFFD4C030),   // 2  yellow
        juce::Colour(0xFF40C060),   // 3  green
        juce::Colour(0xFF28B8A0),   // 4  teal
        juce::Colour(0xFF3890E8),   // 5  blue
        juce::Colour(0xFF8060D8),   // 6  purple
        juce::Colour(0xFFD050A0),   // 7  pink
    };

    inline juce::Colour getBandColour(int index) noexcept
    {
        return bandPalette[((index % 8) + 8) % 8];
    }
}
