#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================
//  Parameter Layout
// ============================================================
juce::AudioProcessorValueTreeState::ParameterLayout ProEQProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Default frequencies for the first 8 bands (spread across the spectrum)
    static constexpr float defaultFreqs[8] = {
        50.f, 120.f, 300.f, 800.f, 2000.f, 5000.f, 10000.f, 16000.f
    };

    for (int b = 0; b < kMaxBands; ++b)
    {
        const bool enabledByDefault = (b < 8);
        const float defaultFreq = (b < 8) ? defaultFreqs[b] : 1000.f;

        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParamID::bandEnabled(b), "Band " + juce::String(b+1) + " Enable", enabledByDefault));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParamID::bandFreq(b), "Band " + juce::String(b+1) + " Freq",
            juce::NormalisableRange<float>(10.f, 22000.f, 0.01f, 0.25f), defaultFreq));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParamID::bandGain(b), "Band " + juce::String(b+1) + " Gain",
            juce::NormalisableRange<float>(-30.f, 30.f, 0.01f), 0.f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParamID::bandQ(b), "Band " + juce::String(b+1) + " Q",
            juce::NormalisableRange<float>(0.025f, 40.f, 0.001f, 0.35f), 0.707f));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParamID::bandType(b), "Band " + juce::String(b+1) + " Type",
            juce::StringArray{"Bell","LowShelf","HighShelf","LowCut","HighCut","Notch","BandPass","AllPass"}, 0));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParamID::bandMode(b), "Band " + juce::String(b+1) + " Mode",
            juce::StringArray{"Stereo","Left","Right","Mid","Side"}, 0));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParamID::bandOrder(b), "Band " + juce::String(b+1) + " Order",
            juce::StringArray{"6 dB","12 dB","18 dB","24 dB","30 dB","36 dB","42 dB","48 dB"}, 1));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParamID::bandDynamic(b), "Band " + juce::String(b+1) + " Dynamic", false));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParamID::bandThresh(b), "Band " + juce::String(b+1) + " Threshold",
            juce::NormalisableRange<float>(-60.f, 0.f, 0.1f), -18.f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParamID::bandDynRange(b), "Band " + juce::String(b+1) + " Dyn Range",
            juce::NormalisableRange<float>(0.f, 30.f, 0.1f), 6.f));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParamID::bandModel(b), "Band " + juce::String(b+1) + " Model",
            juce::StringArray{"Clean","SSL","Neve","Pultec","API","Sontec"}, 0));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParamID::bandSolo(b), "Band " + juce::String(b+1) + " Solo", false));
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParamID::phaseMode, "Phase Mode",
        juce::StringArray{"Zero Latency","Natural Phase","Linear Phase"}, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ParamID::outputGain, "Output Gain",
        juce::NormalisableRange<float>(-30.f, 30.f, 0.01f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        ParamID::analyzerPre, "Analyzer Pre/Post", false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        ParamID::monoStereo, "Mono", false));

    return layout;
}

// ============================================================
//  Constructor / Destructor
// ============================================================
ProEQProcessor::ProEQProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ProEQ", createParameterLayout())
{
    // Register as listener for all band params
    for (int b = 0; b < kMaxBands; ++b)
    {
        apvts.addParameterListener(ParamID::bandEnabled(b),  this);
        apvts.addParameterListener(ParamID::bandFreq(b),     this);
        apvts.addParameterListener(ParamID::bandGain(b),     this);
        apvts.addParameterListener(ParamID::bandQ(b),        this);
        apvts.addParameterListener(ParamID::bandType(b),     this);
        apvts.addParameterListener(ParamID::bandMode(b),     this);
        apvts.addParameterListener(ParamID::bandOrder(b),    this);
        apvts.addParameterListener(ParamID::bandDynamic(b),  this);
        apvts.addParameterListener(ParamID::bandThresh(b),   this);
        apvts.addParameterListener(ParamID::bandDynRange(b), this);
        apvts.addParameterListener(ParamID::bandModel(b),    this);
    }
    apvts.addParameterListener(ParamID::phaseMode,  this);
    apvts.addParameterListener(ParamID::outputGain, this);
}

ProEQProcessor::~ProEQProcessor()
{
    apvts.removeParameterListener(ParamID::phaseMode,  this);
    apvts.removeParameterListener(ParamID::outputGain, this);
    for (int b = 0; b < kMaxBands; ++b)
    {
        apvts.removeParameterListener(ParamID::bandEnabled(b),  this);
        apvts.removeParameterListener(ParamID::bandFreq(b),     this);
        apvts.removeParameterListener(ParamID::bandGain(b),     this);
        apvts.removeParameterListener(ParamID::bandQ(b),        this);
        apvts.removeParameterListener(ParamID::bandType(b),     this);
        apvts.removeParameterListener(ParamID::bandMode(b),     this);
        apvts.removeParameterListener(ParamID::bandOrder(b),    this);
        apvts.removeParameterListener(ParamID::bandDynamic(b),  this);
        apvts.removeParameterListener(ParamID::bandThresh(b),   this);
        apvts.removeParameterListener(ParamID::bandDynRange(b), this);
        apvts.removeParameterListener(ParamID::bandModel(b),    this);
    }
}

// ============================================================
//  prepareToPlay
// ============================================================
void ProEQProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    displaySampleRate = sampleRate;

    // Cache parameter pointers once — safe to do on any thread.
    pAnalyzerPre = apvts.getRawParameterValue(ParamID::analyzerPre);
    pPhaseMode   = apvts.getRawParameterValue(ParamID::phaseMode);
    pOutputGain  = apvts.getRawParameterValue(ParamID::outputGain);
    pMonoStereo  = apvts.getRawParameterValue(ParamID::monoStereo);

    for (int b = 0; b < kMaxBands; ++b)
    {
        pBandEnabled[b] = apvts.getRawParameterValue(ParamID::bandEnabled(b));
        pBandSolo[b]    = apvts.getRawParameterValue(ParamID::bandSolo(b));
        updateBandFromAPVTS(b);
        bands[b].prepare(sampleRate);  // full init (resets state) only here
    }

    linearPhaseEQ.prepare(sampleRate, samplesPerBlock);
    spectrumAnalyzer.prepare(sampleRate);

    // Only rebuild the kernel if LinearPhase is actually active.
    // Building it unconditionally blocks the loading thread for several
    // seconds (4097 freq evaluations * 24 bands * FFT) and makes FL Studio
    // show "not responding".
    {
        int phaseIdx = (int)*apvts.getRawParameterValue(ParamID::phaseMode);
        if ((PhaseMode)phaseIdx == PhaseMode::LinearPhase)
            rebuildLinearPhaseKernel();
    }

    if ((PhaseMode)(int)*apvts.getRawParameterValue(ParamID::phaseMode) == PhaseMode::LinearPhase)
        setLatencySamples(LinearPhaseEQ::kKernelSize);
    else
        setLatencySamples(0);
}

void ProEQProcessor::releaseResources() {}

bool ProEQProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

// ============================================================
//  processBlock
// ============================================================
void ProEQProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Guard against host calling processBlock before prepareToPlay
    if (pAnalyzerPre == nullptr) return;

    const int numSamples = buffer.getNumSamples();

    // In standalone mode, read audio from file player transport
    if (fileTransport != nullptr && fileTransport->isPlaying())
    {
        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        fileTransport->getNextAudioBlock(info);
    }

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;

    // Read cached atomic pointers — no map lookup, just a load.
    const bool      analyzerPre = (bool) pAnalyzerPre->load(std::memory_order_relaxed);
    const PhaseMode phMode      = (PhaseMode)(int) pPhaseMode->load(std::memory_order_relaxed);
    const float     outGainDb   = pOutputGain->load(std::memory_order_relaxed);
    const bool      monoMode    = pMonoStereo != nullptr &&
                                  (bool)pMonoStereo->load(std::memory_order_relaxed);

    // Pre-fader analyzer
    if (analyzerPre)
        spectrumAnalyzer.pushSamples(L, R, numSamples);

    // -------- Apply all bands --------
    if (phMode == PhaseMode::LinearPhase)
    {
        // All bands are baked into the FIR kernel; just convolve
        linearPhaseEQ.processStereo(L, R, numSamples);
    }
    else
    {
        // Check if any band is soloed (cached pointers — no map lookup)
        int soloedBand = -1;
        for (int b = 0; b < kMaxBands; ++b)
        {
            if (pBandSolo[b]->load(std::memory_order_relaxed) > 0.5f)
            {
                soloedBand = b;
                break;
            }
        }

        if (soloedBand >= 0)
        {
            bands[soloedBand].processStereo(L, R, numSamples);
        }
        else
        {
            // Only process enabled bands
            for (int b = 0; b < kMaxBands; ++b)
                if (pBandEnabled[b]->load(std::memory_order_relaxed) > 0.5f)
                    bands[b].processStereo(L, R, numSamples);
        }
    }

    // Output gain
    const float outGainLin = juce::Decibels::decibelsToGain(outGainDb);
    buffer.applyGain(outGainLin);

    // Mono summing (if toggled)
    if (monoMode && buffer.getNumChannels() >= 2)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = (L[i] + R[i]) * 0.5f;
            L[i] = mono;
            R[i] = mono;
        }
    }

    // Track peak for clipping detection
    float peak = buffer.getMagnitude(0, numSamples);
    peakSample.store(peak, std::memory_order_relaxed);

    // Post-fader analyzer
    if (!analyzerPre)
        spectrumAnalyzer.pushSamples(L, R, numSamples);
}

// ============================================================
//  State save/restore
// ============================================================
void ProEQProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml) copyXmlToBinary(*xml, destData);
}

void ProEQProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        for (int b = 0; b < kMaxBands; ++b)
            updateBandFromAPVTS(b);
        rebuildLinearPhaseKernel();
    }
}

// ============================================================
//  parameterChanged   (called from message/audio thread)
// ============================================================
void ProEQProcessor::parameterChanged(const juce::String& paramID, float /*newValue*/)
{
    // Determine if it's a band param
    for (int b = 0; b < kMaxBands; ++b)
    {
        if (paramID.startsWith("B" + juce::String(b) + "_"))
        {
            updateBandFromAPVTS(b);

            PhaseMode pm = (PhaseMode)(int)*apvts.getRawParameterValue(ParamID::phaseMode);
            if (pm == PhaseMode::LinearPhase)
                rebuildLinearPhaseKernel();
            return;
        }
    }

    if (paramID == ParamID::phaseMode)
    {
        PhaseMode pm = (PhaseMode)(int)*apvts.getRawParameterValue(ParamID::phaseMode);
        setLatencySamples(pm == PhaseMode::LinearPhase ? LinearPhaseEQ::kKernelSize : 0);
        if (pm == PhaseMode::LinearPhase)
            rebuildLinearPhaseKernel();
    }
}

// ============================================================
//  updateBandFromAPVTS
//  Only posts new params to the pending slot — the audio thread
//  applies them at the start of the next processStereo call.
//  We deliberately do NOT call prepare() here because that resets
//  the filter state (z1/z2 → 0) and causes audible clicks.
// ============================================================
void ProEQProcessor::updateBandFromAPVTS(int b)
{
    EQBand::Params p;
    p.enabled      = (bool)  *apvts.getRawParameterValue(ParamID::bandEnabled(b));
    p.freqHz       = (double) *apvts.getRawParameterValue(ParamID::bandFreq(b));
    p.gainDb       = (double) *apvts.getRawParameterValue(ParamID::bandGain(b));
    p.Q            = (double) *apvts.getRawParameterValue(ParamID::bandQ(b));
    p.type         = (FilterType)(int)*apvts.getRawParameterValue(ParamID::bandType(b));
    p.mode         = (ProcessingMode)(int)*apvts.getRawParameterValue(ParamID::bandMode(b));
    p.order        = ((int)*apvts.getRawParameterValue(ParamID::bandOrder(b)) + 1) * 2;
    p.dynamic      = (bool)  *apvts.getRawParameterValue(ParamID::bandDynamic(b));
    p.threshold    = (double) *apvts.getRawParameterValue(ParamID::bandThresh(b));
    p.dynamicRange = (double) *apvts.getRawParameterValue(ParamID::bandDynRange(b));
    p.model        = (AnalogModel)(int)*apvts.getRawParameterValue(ParamID::bandModel(b));

    bands[b].setParams(p);  // posts to pending slot; audio thread applies it
}

// ============================================================
//  rebuildLinearPhaseKernel
// ============================================================
void ProEQProcessor::rebuildLinearPhaseKernel()
{
    double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;

    linearPhaseEQ.buildKernel([this, sr](double freq) -> double {
        return getCombinedMagnitudeDb(freq);
    });
}

// ============================================================
//  getCombinedMagnitudeDb
// ============================================================
double ProEQProcessor::getCombinedMagnitudeDb(double freqHz) const
{
    double totalDb = 0.0;
    for (int b = 0; b < kMaxBands; ++b)
        totalDb += bands[b].getMagnitudeAtFreq(freqHz, displaySampleRate);
    return totalDb;
}

// ============================================================
//  createEditor
// ============================================================
juce::AudioProcessorEditor* ProEQProcessor::createEditor()
{
    return new ProEQEditor(*this);
}

// ============================================================
//  Plugin entry point
// ============================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProEQProcessor();
}
