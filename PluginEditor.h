#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumView.h"
#include "Theme.h"

/** Full-window overlay shown on first load: "what are you making today?" */
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
        Card (const juce::String& name, const juce::String& bpm, const juce::String& summary);
        void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
    private:
        juce::String bpm, summary;
    };

    juce::OwnedArray<Card> cards;
};

/** Row of options with one selected, like a soft | hard switch. */
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

/** Gain staging: a trim knob (really changes the volume) inside a ring that shows the peak level
    against the genre's target range, plus an "auto-match" button. */
class GainGauge : public juce::Component
{
public:
    explicit GainGauge (juce::AudioProcessorValueTreeState& params);
    void setState (float peakDb, float holdDb, float targetMin, float targetMax, bool hasGuide);
    std::function<void()> onAutoMatch;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Point<float> centre() const { return { (float) getWidth() * 0.5f, 74.0f }; }

    juce::Slider knob;
    juce::TextButton autoButton { "auto-match target" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float peak = -100.0f, hold = -100.0f, minDb = -12.0f, maxDb = -6.0f;
    bool guide = false;
};

/** The "live advice" list: what to do right now, based on what the analyzer hears. */
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

/** The genre's 30-step workflow as a checklist. Click a step to tick it off. */
class WorkflowList : public juce::Component
{
public:
    void setSteps (const std::vector<mm::WorkflowStep>& steps, juce::uint32 doneBits);
    void setWidthAndLayout (int width);
    std::function<void (int stepIndex)> onToggle;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Row { juce::Rectangle<float> bounds; juce::TextLayout layout; bool phaseHeader = false; int step = -1; juce::String phase; };
    std::vector<mm::WorkflowStep> steps;
    std::vector<Row> rows;
    juce::uint32 done = 0;
    int hovered = -1;
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
    bool isWorkflowTab() const;
    bool isTheoryTab() const;
    juce::uint32 workflowBits() const;

    MixMentorProcessor& processor;
    juce::SharedResourcePointer<theme::LookAndFeel> lookAndFeel;

    juce::ComboBox genreBox, instrumentBox;
    juce::TextButton relistenButton { "re-listen" }, guideButton { "?" };
    SpectrumView spectrum;
    GainGauge gauge { processor.parameters };
    SegmentedControl processTabs;
    juce::TextEditor tipText;
    WorkflowList workflow;
    juce::Viewport workflowView;
    AdvicePanel advice;
    GenrePicker picker;
    juce::TooltipWindow tooltipWindow { this, 350 };

    // Layout rectangles for things painted directly by the editor
    juce::Rectangle<int> sidebar, topBar, guidePanel, elementsArea, gaugeTextArea;
    juce::String summaryText, guideTitle, bpmText, genreName;
    juce::StringArray elements;

    int currentProcess = 0;
    int shownGenre = -2, shownInstrument = -2;

    std::vector<float> readBuffer = std::vector<float> (1 << 15);
    float peakDisplayDb = -100.0f, holdDb = -100.0f;
    int holdFrames = 0, frameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMentorEditor)
};
