#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumView.h"
#include "LevelMeter.h"

/** Full-window overlay shown on first load: "What genre are you making?" */
class GenrePicker : public juce::Component
{
public:
    GenrePicker();
    std::function<void (int genreIndex)> onPick;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
};

class MixMentorEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit MixMentorEditor (MixMentorProcessor&);
    ~MixMentorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshContent();
    void updateGainStatus();
    void selectProcess (int index);

    MixMentorProcessor& processor;

    juce::Label titleLabel, summaryLabel, trackLabel, tipHeader, gainStatus;
    juce::ComboBox genreBox, instrumentBox;
    juce::OwnedArray<juce::TextButton> processButtons;
    juce::TextEditor tipText;
    SpectrumView spectrum;
    LevelMeter meter;
    GenrePicker picker;
    juce::TooltipWindow tooltipWindow { this, 250 };

    int currentProcess = 0;
    int shownGenre = -2, shownInstrument = -2;

    std::vector<float> readBuffer = std::vector<float> (1 << 15);
    float peakDisplayDb = -100.0f, holdDb = -100.0f, rmsDisplayDb = -100.0f;
    int holdFrames = 0, frameCounter = 0;
    juce::String lastTrackName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMentorEditor)
};
