#pragma once
#include <JuceHeader.h>
#include "KnobLookAndFeel.h"
#include "../PluginProcessor.h"

// ============================================================
//  BandControlPanel
//  Bottom panel showing knobs for the selected band
// ============================================================
class BandControlPanel : public juce::Component,
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    BandControlPanel(juce::AudioProcessorValueTreeState& apvts);
    ~BandControlPanel() override;

    void selectBand(int bandIndex);
    int  getSelectedBand() const { return selectedBand; }

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void parameterChanged(const juce::String& paramID, float) override;

private:
    void buildAttachments();
    void updateVisibility();

    juce::AudioProcessorValueTreeState& apvts;
    int selectedBand = 0;

    // Knobs
    juce::Slider freqKnob, gainKnob, qKnob, threshKnob, dynRangeKnob;
    juce::Label  freqLabel, gainLabel, qLabel, threshLabel, dynRangeLabel;

    // Combos / buttons
    juce::ComboBox typeCombo, modeCombo, orderCombo, modelCombo;
    juce::Label    typeLabel, modeLabel, orderLabel, modelLabel;

    juce::ToggleButton enableButton {"Enable"};
    juce::ToggleButton dynButton    {"Dynamic"};

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt, thrAtt, dynRangeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, modeAtt, orderAtt, modelAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   enableAtt, dynAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlPanel)
};
