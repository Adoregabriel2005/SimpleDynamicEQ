#include "BandControlPanel.h"
#include "ProEQColors.h"
#include <array>

// -----------------------------------------------------------------------
BandControlPanel::BandControlPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    auto setupKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        s.setLookAndFeel(&KnobLookAndFeel::instance());
        addAndMakeVisible(s);

        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(10.0f));
        l.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
        addAndMakeVisible(l);
    };

    setupKnob(freqKnob,     freqLabel,     "FREQ");
    setupKnob(gainKnob,     gainLabel,     "GAIN");
    setupKnob(qKnob,        qLabel,        "Q");
    setupKnob(threshKnob,   threshLabel,   "THRESH");
    setupKnob(dynRangeKnob, dynRangeLabel, "DYN RANGE");

    // Type combo
    typeCombo.addItemList({"Bell","Low Shelf","High Shelf","Low Cut","High Cut","Notch","Band Pass","All Pass"}, 1);
    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setFont(juce::Font(10.0f));
    typeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
    addAndMakeVisible(typeCombo);
    addAndMakeVisible(typeLabel);
    typeCombo.onChange = [this]() { updateVisibility(); };

    // Mode combo
    modeCombo.addItemList({"Stereo","Left","Right","Mid","Side"}, 1);
    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setFont(juce::Font(10.0f));
    modeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
    addAndMakeVisible(modeCombo);
    addAndMakeVisible(modeLabel);

    // Order combo (cut filter slope)
    orderCombo.addItemList({"6 dB/oct","12 dB/oct","18 dB/oct","24 dB/oct",
                             "30 dB/oct","36 dB/oct","42 dB/oct","48 dB/oct"}, 1);
    orderLabel.setText("Slope", juce::dontSendNotification);
    orderLabel.setJustificationType(juce::Justification::centred);
    orderLabel.setFont(juce::Font(10.0f));
    orderLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
    addAndMakeVisible(orderCombo);
    addAndMakeVisible(orderLabel);

    // Model combo (analog model)
    modelCombo.addItemList({"Clean","SSL","Neve","Pultec","API","Sontec"}, 1);
    modelLabel.setText("Model", juce::dontSendNotification);
    modelLabel.setJustificationType(juce::Justification::centred);
    modelLabel.setFont(juce::Font(10.0f));
    modelLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
    addAndMakeVisible(modelCombo);
    addAndMakeVisible(modelLabel);

    // Buttons
    addAndMakeVisible(enableButton);
    addAndMakeVisible(dynButton);
    enableButton.setToggleState(true, juce::dontSendNotification);

    // When the Dynamic button is toggled, show/hide threshold & range knobs
    dynButton.onClick = [this]() { updateVisibility(); resized(); };

    selectBand(0);
}

BandControlPanel::~BandControlPanel()
{
    freqKnob.setLookAndFeel(nullptr);
    gainKnob.setLookAndFeel(nullptr);
    qKnob.setLookAndFeel(nullptr);
    threshKnob.setLookAndFeel(nullptr);
    dynRangeKnob.setLookAndFeel(nullptr);
}

// -----------------------------------------------------------------------
void BandControlPanel::selectBand(int b)
{
    selectedBand = b;

    // Tint all knob arcs to this band’s colour
    KnobLookAndFeel::instance().setAccentColour(ProEQColors::getBandColour(b));

    // Clear old attachments
    freqAtt.reset(); gainAtt.reset(); qAtt.reset();
    thrAtt.reset(); dynRangeAtt.reset();
    typeAtt.reset(); modeAtt.reset(); orderAtt.reset(); modelAtt.reset();
    enableAtt.reset(); dynAtt.reset();

    buildAttachments();
    updateVisibility();
    repaint();
}

void BandControlPanel::buildAttachments()
{
    int b = selectedBand;
    using SA  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA  = juce::AudioProcessorValueTreeState::ButtonAttachment;

    freqAtt     = std::make_unique<SA>(apvts, ParamID::bandFreq(b),     freqKnob);
    gainAtt     = std::make_unique<SA>(apvts, ParamID::bandGain(b),     gainKnob);
    qAtt        = std::make_unique<SA>(apvts, ParamID::bandQ(b),        qKnob);
    thrAtt      = std::make_unique<SA>(apvts, ParamID::bandThresh(b),   threshKnob);
    dynRangeAtt = std::make_unique<SA>(apvts, ParamID::bandDynRange(b), dynRangeKnob);
    typeAtt     = std::make_unique<CA>(apvts, ParamID::bandType(b),     typeCombo);
    modeAtt     = std::make_unique<CA>(apvts, ParamID::bandMode(b),     modeCombo);
    orderAtt    = std::make_unique<CA>(apvts, ParamID::bandOrder(b),    orderCombo);
    modelAtt    = std::make_unique<CA>(apvts, ParamID::bandModel(b),    modelCombo);
    enableAtt   = std::make_unique<BA>(apvts, ParamID::bandEnabled(b),  enableButton);
    dynAtt      = std::make_unique<BA>(apvts, ParamID::bandDynamic(b),  dynButton);
}

void BandControlPanel::updateVisibility()
{
    bool isDyn = dynButton.getToggleState();
    threshKnob.setVisible(isDyn);
    dynRangeKnob.setVisible(isDyn);
    threshLabel.setVisible(isDyn);
    dynRangeLabel.setVisible(isDyn);

    // Q knob: dim for cut filters where Q has no effect (Butterworth)
    int typeIdx = typeCombo.getSelectedItemIndex();
    bool isCut = (typeIdx == (int)FilterType::LowCut || typeIdx == (int)FilterType::HighCut);
    qKnob.setEnabled(!isCut);
    qKnob.setAlpha(isCut ? 0.3f : 1.0f);
}

void BandControlPanel::parameterChanged(const juce::String&, float)
{
    updateVisibility();
}

// -----------------------------------------------------------------------
void BandControlPanel::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    const auto bandCol = ProEQColors::getBandColour(selectedBand);

    // Panel background
    g.setColour(juce::Colour(ProEQColors::kPanelBg));
    g.fillRect(0, 0, W, H);

    // Thin top border with band accent colour
    g.setColour(bandCol.withAlpha(0.55f));
    g.fillRect(0, 0, W, 2);

    // Band number badge (top-left)
    const int badgeW = 48, badgeH = 18;
    g.setColour(bandCol.withAlpha(0.18f));
    g.fillRoundedRectangle(6.f, 5.f, (float)badgeW, (float)badgeH, 4.f);
    g.setColour(bandCol.withAlpha(0.7f));
    g.drawRoundedRectangle(6.5f, 5.5f, (float)badgeW - 1.f, (float)badgeH - 1.f, 4.f, 1.f);
    g.setColour(bandCol.brighter(0.3f));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("BAND " + juce::String(selectedBand + 1),
               6, 5, badgeW, badgeH, juce::Justification::centred);
}

// -----------------------------------------------------------------------
//  Dynamic range ring overlay around the Gain knob
// -----------------------------------------------------------------------
void BandControlPanel::paintOverChildren(juce::Graphics& g)
{
    bool isDyn = dynButton.getToggleState();
    if (!isDyn) return;

    // Read current gain and dynamic range from APVTS
    auto* pGain = apvts.getRawParameterValue(ParamID::bandGain(selectedBand));
    auto* pDR   = apvts.getRawParameterValue(ParamID::bandDynRange(selectedBand));
    if (!pGain || !pDR) return;

    float gainDb = pGain->load();
    float dynRange = pDR->load();
    if (dynRange < 0.1f) return;

    // Knob geometry (match LookAndFeel angles)
    auto kb = gainKnob.getBounds().toFloat();
    float cx = kb.getCentreX();
    float cy = kb.getCentreY();
    float radius = juce::jmin(kb.getWidth(), kb.getHeight()) * 0.5f - 3.f;
    float ringR = radius + 4.f;

    // Angle range — JUCE default rotary: -2.356 to 2.356 (≈ ±135°)
    constexpr float startAngle = -2.356194f;  // = -3π/4
    constexpr float endAngle   =  2.356194f;  // = +3π/4
    constexpr float angleSweep = endAngle - startAngle;

    // Gain range is -30..+30
    constexpr float minDb = -30.f, maxDb = 30.f;
    float gainNorm = (gainDb - minDb) / (maxDb - minDb);  // 0..1
    float gainAngle = startAngle + gainNorm * angleSweep;

    // Dynamic range in angle space
    float drAngle = (dynRange / (maxDb - minDb)) * angleSweep;

    float arcStart = gainAngle - drAngle;
    float arcEnd   = gainAngle + drAngle;

    // Clamp to knob range
    arcStart = juce::jlimit(startAngle, endAngle, arcStart);
    arcEnd   = juce::jlimit(startAngle, endAngle, arcEnd);

    // Draw the ring arc
    auto bandCol = ProEQColors::getBandColour(selectedBand);
    juce::Path ring;
    ring.addArc(cx - ringR, cy - ringR, ringR * 2.f, ringR * 2.f,
                arcStart, arcEnd, true);
    g.setColour(bandCol.withAlpha(0.5f));
    g.strokePath(ring, juce::PathStrokeType(3.5f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Inner glow
    juce::Path innerRing;
    innerRing.addArc(cx - ringR, cy - ringR, ringR * 2.f, ringR * 2.f,
                     arcStart, arcEnd, true);
    g.setColour(bandCol.withAlpha(0.15f));
    g.strokePath(innerRing, juce::PathStrokeType(8.f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// -----------------------------------------------------------------------
void BandControlPanel::resized()
{
    const int H   = getHeight();
    const int controlH = 70;
    const int labelH   = 14;
    const int topPad   = 24;

    auto knobRow = [&](juce::Slider& s, juce::Label& l, int x, int w)
    {
        l.setBounds(x, topPad, w, labelH);
        s.setBounds(x, topPad + labelH, w, controlH);
    };

    const int comboW   = 90;
    const int knobW    = 64;
    const int btnW     = 70;
    const int spacing  = 8;
    int x = 8;

    // Enable + Dynamic buttons
    enableButton.setBounds(x, topPad + 10, btnW, 22);  x += btnW + spacing;
    dynButton.setBounds(x, topPad + 10, btnW, 22);      x += btnW + spacing + 4;

    // Type combo
    typeLabel.setBounds(x, topPad, comboW, labelH);
    typeCombo.setBounds(x, topPad + labelH, comboW, 24);
    x += comboW + spacing;

    // Mode combo
    modeLabel.setBounds(x, topPad, comboW, labelH);
    modeCombo.setBounds(x, topPad + labelH, comboW, 24);
    x += comboW + spacing;

    // Order combo
    orderLabel.setBounds(x, topPad, comboW, labelH);
    orderCombo.setBounds(x, topPad + labelH, comboW, 24);
    x += comboW + spacing;

    // Model combo
    modelLabel.setBounds(x, topPad, comboW, labelH);
    modelCombo.setBounds(x, topPad + labelH, comboW, 24);
    x += comboW + spacing + 8;

    // Freq knob
    knobRow(freqKnob, freqLabel, x, knobW);   x += knobW + spacing;
    knobRow(gainKnob, gainLabel, x, knobW);   x += knobW + spacing;
    knobRow(qKnob,    qLabel,    x, knobW);   x += knobW + spacing;

    // Dynamic params
    knobRow(threshKnob,   threshLabel,   x, knobW);  x += knobW + spacing;
    knobRow(dynRangeKnob, dynRangeLabel, x, knobW);
}
