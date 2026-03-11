#include "AudioFilePlayer.h"
#include "KnobLookAndFeel.h"

AudioFilePlayer::AudioFilePlayer()
{
    formatManager.registerBasicFormats();

    loadButton.onClick = [this]() { loadFile(); };
    playButton.onClick = [this]() { togglePlayStop(); };
    loopButton.onClick = [this]()
    {
        if (readerSource)
            readerSource->setLooping(loopButton.getToggleState());
    };

    loadButton.setLookAndFeel(&KnobLookAndFeel::instance());
    playButton.setLookAndFeel(&KnobLookAndFeel::instance());
    loopButton.setLookAndFeel(&KnobLookAndFeel::instance());

    filenameLabel.setColour(juce::Label::textColourId, ProEQColors::textMid);
    filenameLabel.setFont(juce::Font(10.f));
    filenameLabel.setText("No file loaded", juce::dontSendNotification);

    addAndMakeVisible(loadButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(filenameLabel);

    transport.addChangeListener(this);
    startTimerHz(15);
}

AudioFilePlayer::~AudioFilePlayer()
{
    stopTimer();
    transport.removeChangeListener(this);
    transport.setSource(nullptr);
    loadButton.setLookAndFeel(nullptr);
    playButton.setLookAndFeel(nullptr);
    loopButton.setLookAndFeel(nullptr);
}

void AudioFilePlayer::loadFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load audio file...",
        juce::File{},
        formatManager.getWildcardForAllFormats());

    chooser->launchAsync(juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (!file.existsAsFile()) return;

            auto* reader = formatManager.createReaderFor(file);
            if (reader == nullptr) return;

            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            newSource->setLooping(loopButton.getToggleState());

            transport.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
            readerSource = std::move(newSource);

            currentFile = file.getFileName();
            filenameLabel.setText(currentFile, juce::dontSendNotification);

            thumbnail.setSource(new juce::FileInputSource(file));
            repaint();
        });
}

void AudioFilePlayer::togglePlayStop()
{
    if (transport.isPlaying())
    {
        transport.stop();
        playButton.setButtonText("Play");
    }
    else
    {
        transport.start();
        playButton.setButtonText("Stop");
    }
}

void AudioFilePlayer::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (!transport.isPlaying() && isPlaying)
    {
        playButton.setButtonText("Play");
        isPlaying = false;
    }
    else if (transport.isPlaying())
    {
        isPlaying = true;
    }
}

void AudioFilePlayer::timerCallback()
{
    repaint();
}

void AudioFilePlayer::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    transport.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioFilePlayer::releaseResources()
{
    transport.releaseResources();
}

void AudioFilePlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (readerSource == nullptr)
    {
        info.clearActiveBufferRegion();
        return;
    }
    transport.getNextAudioBlock(info);
}

void AudioFilePlayer::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xFF100A0E));
    g.fillAll();

    // Waveform area
    auto thumbArea = getLocalBounds().reduced(2).withTrimmedLeft(240);
    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour(ProEQColors::specLine.withAlpha(0.4f));
        thumbnail.drawChannels(g, thumbArea, 0.0, thumbnail.getTotalLength(), 1.0f);

        // Position indicator
        if (transport.getLengthInSeconds() > 0.0)
        {
            double ratio = transport.getCurrentPosition() / transport.getLengthInSeconds();
            float xPos = thumbArea.getX() + (float)(ratio * thumbArea.getWidth());
            g.setColour(ProEQColors::eqCurve.withAlpha(0.8f));
            g.drawVerticalLine((int)xPos, (float)thumbArea.getY(), (float)thumbArea.getBottom());
        }
    }
    else
    {
        g.setColour(ProEQColors::textDim);
        g.setFont(10.f);
        g.drawText("Drag & drop or click Load", thumbArea, juce::Justification::centred);
    }
}

void AudioFilePlayer::resized()
{
    int x = 4;
    loadButton.setBounds(x, 4, 50, 22);  x += 54;
    playButton.setBounds(x, 4, 50, 22);  x += 54;
    loopButton.setBounds(x, 4, 50, 22);  x += 54;
    filenameLabel.setBounds(x, 4, 80, 22);
}
