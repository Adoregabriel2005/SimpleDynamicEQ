#pragma once
#include <JuceHeader.h>
#include "ProEQColors.h"

// ============================================================
//  ProEQLookAndFeel
//  Premium dark glass knob + flat dark combo/button/label
//  Call setAccentColour() to tint knob arcs per selected band
// ============================================================
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel();

    void setAccentColour(juce::Colour c) noexcept { accent = c; }
    juce::Colour getAccentColour()  const noexcept { return accent; }

    // ---- Rotary slider ----
    void drawRotarySlider(juce::Graphics&,
                          int x, int y, int w, int h,
                          float pos, float startAngle, float endAngle,
                          juce::Slider&) override;

    // ---- Labels ----
    void drawLabel(juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont(juce::Label&) override;

    // ---- ComboBox ----
    void drawComboBox(juce::Graphics&, int w, int h, bool isDown,
                      int bx, int by, int bw, int bh,
                      juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    // ---- Buttons ----
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool highlighted, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool highlighted, bool down) override;

    // ---- Slider text box ----
    juce::Label* createSliderTextBox(juce::Slider&) override;

    static KnobLookAndFeel& instance()
    {
        static KnobLookAndFeel laf;
        return laf;
    }

private:
    juce::Colour accent { 0xFF4090E8 };
};
