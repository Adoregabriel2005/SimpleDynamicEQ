#include "PluginEditor.h"
#include <BinaryData.h>

static constexpr int kHeaderH   = 44;
static constexpr int kBandRowH  = 26;
static constexpr int kControlH  = 120;
static constexpr int kPlayerH   = 30;

// -----------------------------------------------------------------------
ProEQEditor::ProEQEditor(ProEQProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      eqCurve(p),
      bandPanel(p.getAPVTS())
{
    // ---- Phase mode combo ----
    phaseModeCombo.addItemList({"Zero Latency", "Natural Phase", "Linear Phase"}, 1);
    phaseModeCombo.setLookAndFeel(&KnobLookAndFeel::instance());
    addAndMakeVisible(phaseModeCombo);
    phaseModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), ParamID::phaseMode, phaseModeCombo);

    // ---- Output gain knob ----
    outputGainLabel.setText("OUT", juce::dontSendNotification);
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF687080));
    outputGainLabel.setFont(juce::Font(9.f));
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    outputGainKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    outputGainKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    outputGainKnob.setLookAndFeel(&KnobLookAndFeel::instance());
    addAndMakeVisible(outputGainKnob);
    outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), ParamID::outputGain, outputGainKnob);

    // ---- Analyzer PRE toggle ----
    analyzerPreButton.setLookAndFeel(&KnobLookAndFeel::instance());
    addAndMakeVisible(analyzerPreButton);
    analyzerPreAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), ParamID::analyzerPre, analyzerPreButton);

    // ---- Mono/Stereo toggle ----
    monoStereoButton.setLookAndFeel(&KnobLookAndFeel::instance());
    addAndMakeVisible(monoStereoButton);
    monoStereoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), ParamID::monoStereo, monoStereoButton);

    // ---- Centre header info label ----
    headerInfoLabel.setColour(juce::Label::textColourId,   juce::Colour(0xFFB8C4D8));
    headerInfoLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x00000000));
    headerInfoLabel.setFont(juce::Font(11.5f));
    headerInfoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(headerInfoLabel);

    // ---- Preset combo ----
    {
        presetCombo.addItem("-- Presets --", 1);
        auto presets = FactoryPresets::getAll();
        juce::String lastCat;
        for (int i = 0; i < (int)presets.size(); ++i)
        {
            if (lastCat != presets[(size_t)i].category)
            {
                lastCat = presets[(size_t)i].category;
                presetCombo.addSeparator();
            }
            presetCombo.addItem(juce::String(presets[(size_t)i].name), i + 2);
        }
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        presetCombo.onChange = [this]()
        {
            int id = presetCombo.getSelectedId();
            if (id >= 2)
                loadPreset(id - 2);
        };
        presetCombo.setLookAndFeel(&KnobLookAndFeel::instance());
        addAndMakeVisible(presetCombo);
    }

    // ---- Band selector buttons row ----
    for (int b = 0; b < kMaxBands; ++b)
    {
        auto* btn = bandButtons.add(new juce::TextButton(juce::String(b + 1)));
        btn->setClickingTogglesState(false);
        btn->onClick = [this, b]() { selectBand(b); };
        btn->setLookAndFeel(&KnobLookAndFeel::instance());
        addAndMakeVisible(btn);
    }

    // ---- Main areas ----
    addAndMakeVisible(eqCurve);
    addAndMakeVisible(bandPanel);

    // ---- Standalone file player ----
    isStandalone = (juce::PluginHostType().getPluginLoadedAs()
                    == juce::AudioProcessor::wrapperType_Standalone);
    if (isStandalone)
    {
        filePlayer = std::make_unique<AudioFilePlayer>();
        addAndMakeVisible(*filePlayer);
        audioProcessor.setFilePlayerSource(&filePlayer->getTransport());
    }

    selectBand(0);
    updateBandButtons();

    // Must be called AFTER all child components are created, because
    // setSize triggers resized() which accesses bandButtons etc.
    setSize(920, 580);
    setResizable(true, true);
    setResizeLimits(720, 440, 1800, 1000);

    startTimerHz(20);  // header readout refresh
}

ProEQEditor::~ProEQEditor()
{
    stopTimer();
    audioProcessor.setFilePlayerSource(nullptr);  // disconnect before destroying
    filePlayer.reset();
    outputGainKnob.setLookAndFeel(nullptr);
    phaseModeCombo.setLookAndFeel(nullptr);
    analyzerPreButton.setLookAndFeel(nullptr);
    monoStereoButton.setLookAndFeel(nullptr);
    presetCombo.setLookAndFeel(nullptr);
    for (auto* b : bandButtons) b->setLookAndFeel(nullptr);
}

// -----------------------------------------------------------------------
//  Timer – refresh the header band-info text
// -----------------------------------------------------------------------
void ProEQEditor::timerCallback()
{
    updateHeaderInfo();
    updateBandButtons();   // refresh enabled/color state
}

// -----------------------------------------------------------------------
//  Band selection
// -----------------------------------------------------------------------
void ProEQEditor::selectBand(int b)
{
    selectedBand = b;
    bandPanel.selectBand(b);
    updateHeaderInfo();

    // Highlight the chosen button
    auto selCol   = ProEQColors::getBandColour(b);
    for (int i = 0; i < kMaxBands; ++i)
    {
        bool isSel = (i == b);
        bandButtons[i]->setColour(juce::TextButton::buttonOnColourId,
                                   selCol.withAlpha(0.35f));
        bandButtons[i]->setToggleState(isSel, juce::dontSendNotification);
    }
}

// -----------------------------------------------------------------------
//  Update each band button's colour to match its band palette colour
// -----------------------------------------------------------------------
void ProEQEditor::updateBandButtons()
{
    auto& apvts = audioProcessor.getAPVTS();
    for (int b = 0; b < kMaxBands; ++b)
    {
        bool en = (bool)*apvts.getRawParameterValue(ParamID::bandEnabled(b));
        auto col = en ? ProEQColors::getBandColour(b) : juce::Colour(0xFF252830);
        bandButtons[b]->setColour(juce::TextButton::buttonColourId, col.withAlpha(en ? 0.22f : 1.f));
        bandButtons[b]->setColour(juce::TextButton::textColourOffId,
                                   en ? col.brighter(0.5f) : juce::Colour(0xFF3A3E4A));
    }
}

// -----------------------------------------------------------------------
//  Build the "Band N: freq  gain  Q  type  mode" info string
// -----------------------------------------------------------------------
juce::String ProEQEditor::buildBandInfoString() const
{
    auto& apvts = audioProcessor.getAPVTS();
    int b = selectedBand;

    bool en = (bool)*apvts.getRawParameterValue(ParamID::bandEnabled(b));
    if (!en) return "Band " + juce::String(b + 1) + "  \u2014  Off";

    double freq   = *apvts.getRawParameterValue(ParamID::bandFreq(b));
    double gain   = *apvts.getRawParameterValue(ParamID::bandGain(b));
    double q      = *apvts.getRawParameterValue(ParamID::bandQ(b));
    int    typeI  = (int)*apvts.getRawParameterValue(ParamID::bandType(b));
    int    modeI  = (int)*apvts.getRawParameterValue(ParamID::bandMode(b));

    static const char* typeNames[] = { "Bell","Lo Shelf","Hi Shelf","Lo Cut","Hi Cut","Notch","Band Pass","All Pass" };
    static const char* modeNames[] = { "Stereo","Left","Right","Mid","Side" };

    auto fStr = freq >= 1000.0 ? (juce::String(freq / 1000.0, 2) + " kHz")
                               : (juce::String((int)freq) + " Hz");
    auto gStr = (gain >= 0.0 ? "+" : "") + juce::String(gain, 1) + " dB";
    auto qStr = "Q " + juce::String(q, 2);
    auto tStr = (typeI >= 0 && typeI < 8) ? typeNames[typeI] : "";
    auto mStr = (modeI >= 0 && modeI < 5) ? modeNames[modeI] : "";

    return "Band " + juce::String(b + 1) + "   \u00b7   " +
           fStr + "   \u00b7   " + gStr + "   \u00b7   " + qStr +
           "   \u00b7   " + tStr + "   \u00b7   " + mStr;
}

void ProEQEditor::updateHeaderInfo()
{
    headerInfoLabel.setText(buildBandInfoString(), juce::dontSendNotification);
}

// -----------------------------------------------------------------------
//  Load factory preset
// -----------------------------------------------------------------------
void ProEQEditor::loadPreset(int presetIndex)
{
    auto presets = FactoryPresets::getAll();
    if (presetIndex < 0 || presetIndex >= (int)presets.size()) return;

    auto& apvts = audioProcessor.getAPVTS();
    auto& preset = presets[(size_t)presetIndex];

    // Disable all bands first
    for (int b = 0; b < kMaxBands; ++b)
    {
        if (auto* ep = apvts.getParameter(ParamID::bandEnabled(b)))
            ep->setValueNotifyingHost(0.f);
    }

    // Apply preset bands
    for (auto& bp : preset.bands)
    {
        int b = bp.bandIndex;
        if (b < 0 || b >= kMaxBands) continue;

        if (auto* ep = apvts.getParameter(ParamID::bandEnabled(b)))
            ep->setValueNotifyingHost(bp.enabled ? 1.f : 0.f);
        if (auto* fp = apvts.getParameter(ParamID::bandFreq(b)))
            fp->setValueNotifyingHost(fp->convertTo0to1(bp.freq));
        if (auto* gp = apvts.getParameter(ParamID::bandGain(b)))
            gp->setValueNotifyingHost(gp->convertTo0to1(bp.gain));
        if (auto* qp = apvts.getParameter(ParamID::bandQ(b)))
            qp->setValueNotifyingHost(qp->convertTo0to1(bp.Q));
        if (auto* tp = apvts.getParameter(ParamID::bandType(b)))
            tp->setValueNotifyingHost(tp->convertTo0to1((float)bp.type));
        if (auto* mp = apvts.getParameter(ParamID::bandModel(b)))
            mp->setValueNotifyingHost(mp->convertTo0to1((float)bp.model));
    }
}

// -----------------------------------------------------------------------
//  Paint
// -----------------------------------------------------------------------
void ProEQEditor::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    auto bandCol = ProEQColors::getBandColour(selectedBand);

    // Background image
    auto bgImage = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    if (bgImage.isValid())
        g.drawImage(bgImage, 0, 0, W, H, 0, 0, bgImage.getWidth(), bgImage.getHeight());
    else
    {
        // Fallback solid colour
        g.setColour(juce::Colour(ProEQColors::kBackground));
        g.fillRect(0, 0, W, H);
    }

    // Semi-transparent overlay to keep UI legible
    g.setColour(juce::Colour(0xCC0E0A0C));
    g.fillRect(0, 0, W, H);

    // Header background
    g.setColour(juce::Colour(ProEQColors::kHeaderBg));
    g.fillRect(0, 0, W, kHeaderH);

    // Thin accent line at bottom of header
    g.setColour(bandCol.withAlpha(0.4f));
    g.fillRect(0, kHeaderH - 1, W, 1);

    // Plugin logo (left)
    g.setFont(juce::Font(17.f, juce::Font::bold));
    g.setColour(juce::Colour(ProEQColors::kEqCurve));
    g.drawText("SimpleDynamicEQ", 12, 0, 130, kHeaderH, juce::Justification::centredLeft);

    // Subtitle / BETA badge
    g.setFont(juce::Font(9.5f, juce::Font::bold));
    g.setColour(juce::Colour(0xFFFF9020));
    g.drawText("BETA", 144, 6, 40, 14, juce::Justification::left);

    // Band selector row background
    g.setColour(juce::Colour(0xFF0A0C12));
    g.fillRect(0, kHeaderH, W, kBandRowH);
    g.setColour(juce::Colour(0xFF161820));
    g.fillRect(0, kHeaderH + kBandRowH - 1, W, 1);
}

// -----------------------------------------------------------------------
//  Layout
// -----------------------------------------------------------------------
void ProEQEditor::resized()
{
    const int W         = getWidth();
    const int H         = getHeight();
    const int playerH   = (isStandalone && filePlayer) ? kPlayerH : 0;
    const int curveTop  = kHeaderH + kBandRowH;
    const int curveH    = H - curveTop - kControlH - playerH;

    // ---- Header layout (left → right) ----
    // Logo "SimpleDynamicEQ" is painted directly (0..190px), skip it.
    int hx = 190;  // start after logo area

    // Phase mode combo
    phaseModeCombo.setBounds(hx, 12, 110, 20);
    hx += 118;

    // Preset combo
    presetCombo.setBounds(hx, 12, 140, 20);
    hx += 148;

    // Centre info label (fills the gap between presets and right controls)
    const int rightControlsW = 200;  // Pre + Mono + OUT knob area
    int infoW = W - hx - rightControlsW;
    if (infoW < 80) infoW = 80;
    headerInfoLabel.setBounds(hx, 0, infoW, kHeaderH);

    // Right-aligned controls
    int rx = W - 52;
    outputGainLabel.setBounds(rx, 2,  44, 12);
    outputGainKnob.setBounds (rx, 12, 44, 30);
    rx -= 58;
    monoStereoButton.setBounds(rx, 12, 52, 20);
    rx -= 50;
    analyzerPreButton.setBounds(rx, 12, 44, 20);

    // Band selector buttons
    const int btnW = (W - 4) / kMaxBands;
    for (int b = 0; b < kMaxBands; ++b)
        bandButtons[b]->setBounds(2 + b * btnW, kHeaderH + 2, btnW - 1, kBandRowH - 4);

    // EQ curve
    eqCurve.setBounds(0, curveTop, W, curveH);

    // Control panel
    bandPanel.setBounds(0, H - kControlH - playerH, W, kControlH);

    // File player (standalone only)
    if (isStandalone && filePlayer)
        filePlayer->setBounds(0, H - playerH, W, playerH);
}
