#pragma once
#include <JuceHeader.h>
#include "ProEQColors.h"
#include "../DSP/EQBand.h"

// ============================================================
//  BandHandle  – draggable circle on the EQ curve
//  Visual design:
//   • Outer glow ring when selected
//   • Filled body with band colour gradient
//   • Filter type letter in centre
//   • Dashed ring when Dynamic EQ is active
// ============================================================
class BandHandle : public juce::Component, private juce::Timer
{
public:
    BandHandle(int bandIndex, juce::AudioProcessorValueTreeState& apvts);
    ~BandHandle() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& w) override;
    void timerCallback() override;

    void setSelected(bool s);
    bool isSelected()      const noexcept { return selected; }
    bool isBeingDragged()  const noexcept { return dragging; }
    bool isHovered()       const noexcept { return hovered; }
    int  getBandIndex()    const noexcept { return index; }

    void updatePosition(int parentWidth, int parentHeight,
                        double minFreq, double maxFreq,
                        double minDb,   double maxDb);

    static constexpr int kHandleSize = 22;

private:
    juce::String getTypeChar() const;
    juce::Colour bandColour()  const noexcept
    {
        return ProEQColors::getBandColour(index);
    }

    int    index;
    bool   selected  = false;
    bool   dragging  = false;
    bool   hovered   = false;
    float  hoverAnim = 0.f;   // 0..1 animated hover brightness

    juce::Point<float> dragStart;
    double dragStartFreq = 0.0;
    double dragStartGain = 0.0;

    juce::AudioProcessorValueTreeState& apvts;
};
