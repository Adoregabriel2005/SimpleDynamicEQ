#pragma once
#include <JuceHeader.h>
#include "../DSP/SpectrumAnalyzer.h"

// ============================================================
//  AnalyzerComponent  – stub
//  Spectrum rendering has been integrated into EQCurveComponent
//  (which draws the spectrum directly in its paint() method).
//  This class is kept as a stub for build-system compatibility.
// ============================================================
class AnalyzerComponent : public juce::Component
{
public:
    AnalyzerComponent(SpectrumAnalyzer& /*analyzer*/, double /*sampleRate*/) {}
    ~AnalyzerComponent() override = default;

    void paint(juce::Graphics&) override {}
    void resized() override {}
};
