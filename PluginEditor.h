#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumView.h"
#include "LevelMeter.h"
#include "Theme.h"

/** Full-window overlay shown on first load: "What are you making today?" */
class GenrePicker : public juce::Component
{
public:
    GenrePicker();
    std::function<void (int genreIndex)> onPick;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Card : public juce::Button
    {
    public:
        Card (const juce::String& name, const juce::String& summary);
        void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
    private:
        juce::String summary;
    };

    juce::OwnedArray<Card> cards;
};

/** One rounded bar of options (EQ | Compression | ...), like a modern tab switcher. */
class SegmentedControl : public juce::Component
{
public:
    void setOptions (const juce::StringArray& names);
    void setSelected (int index);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<float>) const;
    juce::Rectangle<float> segment (int index) const;

    juce::StringArray options;
    int selected = 0, hovered = -1;
};

/** Gain staging card: big peak readout, target, verdict and the meter. */
class GainPanel : public juce::Component
{
public:
    GainPanel();
    void setState (float holdDb, float targetMin, float targetMax, bool hasGuide);
    LevelMeter& getMeter() { return meter; }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LevelMeter meter;
    float hold = -100.0f, minDb = -12.0f, maxDb = -6.0f;
    bool guide = false;
};

/** The "Live advice" list: what to do right now, based on what the analyzer hears. */
class AdvicePanel : public juce::Component
{
public:
    struct Item
    {
        juce::Colour colour;
        juce::String title, body;
    };

    void setItems (std::vector<Item> newItems, bool isLive);
    void paint (juce::Graphics&) override;

private:
    std::vector<Item> items;
    bool live = false;
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
    void updateAdvice();

    MixMentorProcessor& processor;
    juce::SharedResourcePointer<theme::LookAndFeel> lookAndFeel;

    juce::ComboBox genreBox, instrumentBox;
    juce::TextButton relistenButton { "Re-listen" };
    SpectrumView spectrum;
    GainPanel gainPanel;
    SegmentedControl processTabs;
    juce::TextEditor tipText;
    AdvicePanel advice;
    GenrePicker picker;
    juce::TooltipWindow tooltipWindow { this, 300 };

    // Layout rectangles for things painted directly by the editor
    juce::Rectangle<int> headerArea, analyzerCard, guideCard;
    juce::String summaryText, guideTitle, trackName;

    int currentProcess = 0;
    int shownGenre = -2, shownInstrument = -2;

    std::vector<float> readBuffer = std::vector<float> (1 << 15);
    float peakDisplayDb = -100.0f, holdDb = -100.0f, rmsDisplayDb = -100.0f;
    int holdFrames = 0, frameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMentorEditor)
};
