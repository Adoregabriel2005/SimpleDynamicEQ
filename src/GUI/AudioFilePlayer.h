#pragma once
#include <JuceHeader.h>
#include "ProEQColors.h"

// ============================================================
//  AudioFilePlayer  — standalone-only transport with file loading
//  Renders a compact bar with: [Load] [Play/Stop] [Loop] filename waveform
// ============================================================
class AudioFilePlayer : public juce::Component,
                        public juce::ChangeListener,
                        public juce::Timer
{
public:
    AudioFilePlayer();
    ~AudioFilePlayer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ChangeListener for AudioTransportSource
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Timer for waveform / position updates
    void timerCallback() override;

    // Get the transport so the processor can mix it
    juce::AudioTransportSource& getTransport() { return transport; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    // Call from processBlock to read file audio
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    void prepareToPlay(int samplesPerBlock, double sampleRate);
    void releaseResources();

private:
    void loadFile();
    void togglePlayStop();
    void updateThumbnail();

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;

    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    juce::TextButton loadButton  { "Load" };
    juce::TextButton playButton  { "Play" };
    juce::ToggleButton loopButton { "Loop" };

    juce::Label filenameLabel;

    juce::String currentFile;
    bool         isPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFilePlayer)
};
