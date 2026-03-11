#include "KnobLookAndFeel.h"
#include <cmath>

KnobLookAndFeel::KnobLookAndFeel()
{
    using C = juce::Colour;
    // global colour tokens used by JUCE controls
    setColour(juce::Slider::rotarySliderFillColourId,    accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, C(0xFF1C2030));
    setColour(juce::Slider::thumbColourId,               C(0xFFFFFFFF));
    setColour(juce::Slider::textBoxTextColourId,         C(0xFFB8C0D0));
    setColour(juce::Slider::textBoxBackgroundColourId,   C(0xFF090B10));
    setColour(juce::Slider::textBoxOutlineColourId,      C(0x00000000));
    setColour(juce::Slider::textBoxHighlightColourId,    accent.withAlpha(0.4f));

    setColour(juce::Label::textColourId,                 C(0xFFB0B8C8));
    setColour(juce::Label::backgroundColourId,           C(0x00000000));
    setColour(juce::Label::outlineColourId,              C(0x00000000));

    setColour(juce::ComboBox::backgroundColourId,        C(0xFF0E1018));
    setColour(juce::ComboBox::textColourId,              C(0xFFB0B8C8));
    setColour(juce::ComboBox::outlineColourId,           C(0xFF252A38));
    setColour(juce::ComboBox::arrowColourId,             C(0xFF606880));
    setColour(juce::ComboBox::focusedOutlineColourId,    accent);
    setColour(juce::PopupMenu::backgroundColourId,       C(0xFF0E1018));
    setColour(juce::PopupMenu::textColourId,             C(0xFFB0B8C8));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId,  C(0xFFFFFFFF));

    setColour(juce::ToggleButton::textColourId,          C(0xFFB0B8C8));
    setColour(juce::TextButton::buttonColourId,          C(0xFF0E1018));
    setColour(juce::TextButton::buttonOnColourId,        accent.withAlpha(0.3f));
    setColour(juce::TextButton::textColourOffId,         C(0xFF8090A8));
    setColour(juce::TextButton::textColourOnId,          C(0xFFFFFFFF));
}

void KnobLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                        int x, int y, int w, int h,
                                        float pos,
                                        float startAngle, float endAngle,
                                        juce::Slider& /*slider*/)
{
    const float cx    = (float)x + (float)w * 0.5f;
    const float cy    = (float)y + (float)h * 0.5f;
    const float outer = juce::jmin(w, h) * 0.5f - 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Shadow drop
    g.setColour(juce::Colour(0x50000000));
    g.fillEllipse(cx - outer - 1.f, cy - outer - 1.f,
                  (outer + 1.f) * 2.f, (outer + 1.f) * 2.f);

    // Body gradient (glassy dark)
    {
        juce::ColourGradient bodyGrad(
            juce::Colour(0xFF252938), cx - outer * 0.25f, cy - outer * 0.35f,
            juce::Colour(0xFF0A0C14), cx + outer * 0.35f, cy + outer * 0.55f,
            true);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(cx - outer, cy - outer, outer * 2.f, outer * 2.f);
    }

    // Track arc (dim background)
    {
        const float tr = outer - 5.f;
        juce::Path track;
        track.addArc(cx - tr, cy - tr, tr * 2.f, tr * 2.f,
                     startAngle, endAngle, true);
        g.setColour(juce::Colour(0xFF181C2C));
        g.strokePath(track, juce::PathStrokeType(3.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Value arc (accent coloured)
    {
        const float ar = outer - 5.f;
        juce::Path arc;
        arc.addArc(cx - ar, cy - ar, ar * 2.f, ar * 2.f,
                   startAngle, angle, true);
        g.setColour(accent.withAlpha(0.92f));
        g.strokePath(arc, juce::PathStrokeType(3.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Outer rim bevel
    g.setColour(juce::Colour(0xFF2C3048));
    g.drawEllipse(cx - outer, cy - outer, outer * 2.f, outer * 2.f, 1.f);

    // Centre indicator dot
    const float dotR = outer * 0.11f;
    const float armL = outer * 0.50f;
    const float pdx  = cx + armL * std::sin(angle);
    const float pdy  = cy - armL * std::cos(angle);
    g.setColour(accent.brighter(0.6f));
    g.fillEllipse(pdx - dotR, pdy - dotR, dotR * 2.f, dotR * 2.f);

    // Highlight reflection
    {
        juce::ColourGradient hl(
            juce::Colours::white.withAlpha(0.09f),
            cx - outer * 0.2f, cy - outer * 0.5f,
            juce::Colours::transparentWhite,
            cx + outer * 0.1f, cy,
            true);
        g.setGradientFill(hl);
        g.fillEllipse(cx - outer, cy - outer, outer * 2.f, outer * 2.f);
    }
}

void KnobLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(juce::Colour(0x00000000));
    auto c = label.findColour(juce::Label::textColourId);
    g.setColour(c.isOpaque() ? c : juce::Colour(0xFFB0B8C8));
    g.setFont(getLabelFont(label));
    g.drawFittedText(label.getText(), label.getLocalBounds().reduced(1),
                     label.getJustificationType(), 1, 0.85f);
}

juce::Font KnobLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(10.5f, juce::Font::plain);
}

// -----------------------------------------------------------------------
//  ComboBox
// -----------------------------------------------------------------------
void KnobLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h,
                                    bool /*isDown*/,
                                    int bx, int by, int bw, int bh,
                                    juce::ComboBox& box)
{
    juce::Rectangle<float> r(0.f, 0.f, (float)w, (float)h);
    g.setColour(juce::Colour(0xFF0E1018));
    g.fillRoundedRectangle(r, 3.f);
    bool focused = box.hasKeyboardFocus(false);
    g.setColour(focused ? accent.withAlpha(0.8f) : juce::Colour(0xFF252A38));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.f, 1.f);

    const float acx = (float)(bx + bw / 2);
    const float acy = (float)(by + bh / 2);
    const float ts  = 3.8f;
    juce::Path arrow;
    arrow.addTriangle(acx - ts, acy - ts * 0.5f,
                      acx + ts, acy - ts * 0.5f,
                      acx,      acy + ts * 0.7f);
    g.setColour(juce::Colour(0xFF5A6280));
    g.fillPath(arrow);
}

juce::Font KnobLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(10.5f, juce::Font::plain);
}

void KnobLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

// -----------------------------------------------------------------------
//  Buttons
// -----------------------------------------------------------------------
void KnobLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& btn,
                                        bool highlighted, bool /*down*/)
{
    const float bh  = (float)btn.getHeight();
    const float r   = bh * 0.28f;
    const float lcy = bh * 0.5f;
    const float lx  = 5.f;

    g.setColour(juce::Colour(0xFF0C0E14));
    g.fillEllipse(lx, lcy - r, r * 2.f, r * 2.f);

    if (btn.getToggleState())
    {
        g.setColour(accent.withAlpha(0.28f));
        g.fillEllipse(lx - 3.f, lcy - r - 3.f, (r + 3.f)*2.f, (r + 3.f)*2.f);
        g.setColour(accent.brighter(0.35f));
        g.fillEllipse(lx, lcy - r, r * 2.f, r * 2.f);
    }
    else
    {
        g.setColour(juce::Colour(0xFF202438));
        g.fillEllipse(lx, lcy - r, r * 2.f, r * 2.f);
    }

    g.setColour(juce::Colour(0xFF2C3050));
    g.drawEllipse(lx, lcy - r, r * 2.f, r * 2.f, 1.f);

    g.setFont(10.5f);
    g.setColour(highlighted ? juce::Colour(0xFFFFFFFF)
                : (btn.getToggleState() ? juce::Colour(0xFFDDE4F0)
                                        : juce::Colour(0xFF7888A8)));
    g.drawFittedText(btn.getButtonText(),
                     (int)(lx + r * 2.f + 6.f), 0,
                     btn.getWidth() - (int)(lx + r * 2.f + 8.f),
                     btn.getHeight(),
                     juce::Justification::centredLeft, 1);
}

void KnobLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& btn,
                                            const juce::Colour&,
                                            bool highlighted, bool down)
{
    juce::Rectangle<float> r(btn.getLocalBounds().toFloat());
    auto col = juce::Colour(0xFF0E1018);
    if (btn.getToggleState()) col = accent.withAlpha(0.18f);
    if (highlighted)          col = col.brighter(0.08f);
    if (down)                 col = col.darker(0.08f);
    g.setColour(col);
    g.fillRoundedRectangle(r, 3.f);
    auto border = btn.getToggleState()
                  ? accent.withAlpha(0.55f) : juce::Colour(0xFF252A38);
    g.setColour(border);
    g.drawRoundedRectangle(r.reduced(0.5f), 3.f, 1.f);
}

void KnobLookAndFeel::drawButtonText(juce::Graphics& g,
                                      juce::TextButton& btn,
                                      bool highlighted, bool /*down*/)
{
    g.setFont(10.5f);
    auto col = btn.getToggleState()
               ? (highlighted ? juce::Colours::white : accent.brighter(0.3f))
               : (highlighted ? juce::Colour(0xFFCDD4E8) : juce::Colour(0xFF7888A8));
    g.setColour(col);
    g.drawFittedText(btn.getButtonText(), btn.getLocalBounds().reduced(2),
                     juce::Justification::centred, 1);
}

juce::Label* KnobLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setColour(juce::Label::textColourId,              juce::Colour(0xFFB8C4D8));
    l->setColour(juce::Label::backgroundColourId,        juce::Colour(0xFF090B12));
    l->setColour(juce::Label::outlineColourId,           juce::Colour(0x00000000));
    l->setColour(juce::TextEditor::textColourId,         juce::Colour(0xFFB8C4D8));
    l->setColour(juce::TextEditor::backgroundColourId,   juce::Colour(0xFF090B12));
    l->setFont(juce::Font(10.f, juce::Font::plain));
    return l;
}
