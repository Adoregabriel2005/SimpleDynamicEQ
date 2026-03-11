#pragma once
#include <JuceHeader.h>
#include "DSP/EQBand.h"
#include "DSP/MidSideProcessor.h"
#include "DSP/LinearPhaseEQ.h"
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/FrequencyProblemDetector.h"

// ============================================================
//  Constants
// ============================================================
static constexpr int kMaxBands = 24;

// Phase mode
enum class PhaseMode : int { ZeroLatency = 0, NaturalPhase, LinearPhase };

// ============================================================
//  Parameter ID helpers
// ============================================================
namespace ParamID
{
    // Band params: "B<n>_<param>"  n = 0..23
    inline juce::String bandEnabled  (int b) { return "B" + juce::String(b) + "_en"; }
    inline juce::String bandFreq     (int b) { return "B" + juce::String(b) + "_fr"; }
    inline juce::String bandGain     (int b) { return "B" + juce::String(b) + "_gn"; }
    inline juce::String bandQ        (int b) { return "B" + juce::String(b) + "_q";  }
    inline juce::String bandType     (int b) { return "B" + juce::String(b) + "_ty"; }
    inline juce::String bandMode     (int b) { return "B" + juce::String(b) + "_mo"; }
    inline juce::String bandOrder    (int b) { return "B" + juce::String(b) + "_or"; }
    inline juce::String bandDynamic  (int b) { return "B" + juce::String(b) + "_dyn"; }
    inline juce::String bandThresh   (int b) { return "B" + juce::String(b) + "_thr"; }
    inline juce::String bandDynRange (int b) { return "B" + juce::String(b) + "_dr"; }
    inline juce::String bandModel    (int b) { return "B" + juce::String(b) + "_mdl"; }
    inline juce::String bandSolo     (int b) { return "B" + juce::String(b) + "_solo"; }

    // Global
    static const juce::String phaseMode      = "PhaseMode";
    static const juce::String outputGain     = "OutputGain";
    static const juce::String analyzerPre    = "AnalyzerPre";
    static const juce::String monoStereo     = "MonoStereo";
}

// ============================================================
//  PluginProcessor
// ============================================================
class ProEQProcessor : public juce::AudioProcessor,
                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    ProEQProcessor();
    ~ProEQProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return "SimpleDynamicEQ"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    void parameterChanged(const juce::String& paramID, float newValue) override;

    //==========================================================================
    // Public accessors for editor
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer; }
    FrequencyProblemDetector& getProblemDetector() { return problemDetector; }

    double getDisplaySampleRate() const { return displaySampleRate; }

    // Peak tracking for clipping detection (public for GUI access)
    std::atomic<float> peakSample { 0.f };

    // Returns the combined EQ magnitude (all bands) at a given frequency
    double getCombinedMagnitudeDb(double freqHz) const;

    // Band access for GUI highlighting
    const EQBand& getBand(int i) const { return bands[i]; }

    // Standalone file player transport (set by editor)
    void setFilePlayerSource(juce::AudioTransportSource* src) { fileTransport = src; }

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateBandFromAPVTS(int bandIndex);
    void rebuildLinearPhaseKernel();

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    std::array<EQBand, kMaxBands>   bands;
    LinearPhaseEQ                    linearPhaseEQ;
    SpectrumAnalyzer                 spectrumAnalyzer;
    FrequencyProblemDetector         problemDetector;

    double  currentSampleRate  = 44100.0;
    int     currentBlockSize   = 512;
    double  displaySampleRate  = 44100.0;

    // Cached raw parameter pointers — avoids repeated APVTS map lookups
    // in processBlock. Initialised in prepareToPlay.
    std::atomic<float>* pAnalyzerPre = nullptr;
    std::atomic<float>* pPhaseMode   = nullptr;
    std::atomic<float>* pOutputGain  = nullptr;
    std::atomic<float>* pMonoStereo  = nullptr;
    std::array<std::atomic<float>*, kMaxBands> pBandEnabled {};
    std::array<std::atomic<float>*, kMaxBands> pBandSolo    {};

    // Standalone file player source (non-owning; set via setFilePlayerSource)
    juce::AudioTransportSource* fileTransport = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProEQProcessor)
};
