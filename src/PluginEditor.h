#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/EQCurveComponent.h"
#include "GUI/BandControlPanel.h"
#include "GUI/KnobLookAndFeel.h"
#include "GUI/ProEQColors.h"
#include "GUI/AudioFilePlayer.h"
#include "GUI/FactoryPresets.h"

// ============================================================
//  ProEQEditor  – main plugin window
// ============================================================
class ProEQEditor  : public juce::AudioProcessorEditor,
                     public juce::Timer
{
public:
    explicit ProEQEditor(ProEQProcessor&);
    ~ProEQEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;   // updates the header band-info readout

private:
    void selectBand(int bandIndex);
    void updateBandButtons();
    void updateHeaderInfo();
    void loadPreset(int presetIndex);
    juce::String buildBandInfoString() const;

    ProEQProcessor& audioProcessor;

    // EQ display
    EQCurveComponent eqCurve;

    // Band selector row  (24 compact dot-buttons)
    juce::OwnedArray<juce::TextButton> bandButtons;

    // Bottom control panel
    BandControlPanel bandPanel;

    // Top-bar controls
    juce::ComboBox     phaseModeCombo;
    juce::Slider       outputGainKnob;
    juce::Label        outputGainLabel;
    juce::ToggleButton analyzerPreButton {"Pre"};
    juce::ToggleButton monoStereoButton  {"Mono"};
    juce::Label        headerInfoLabel;   // centre: selected-band info
    juce::ComboBox     presetCombo;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> phaseModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   outputGainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   analyzerPreAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   monoStereoAtt;

    int selectedBand = 0;

    // Standalone-only audio file player
    bool isStandalone = false;
    std::unique_ptr<AudioFilePlayer> filePlayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProEQEditor)
};
