#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Advisor.h"

/** The numbers under the curve: one box per frequency region, how far it is from the target. */
class BandNumbers : public juce::Component
{
public:
    void setReadings (std::vector<Advisor::RegionReading> r, bool hasTarget, const juce::String& targetName);
    void paint (juce::Graphics&) override;

private:
    std::vector<Advisor::RegionReading> readings;
    bool target = false;
    juce::String name;
};

/** "What to do": compact cards (title, the move with numbers, a meter with the green goal zone,
    "Done, check it"). Click a card to open the in-depth advice for that issue. Scrolls. */
class SuggestionPanel : public juce::Component
{
public:
    struct Info { juce::String tag, title, action, why; };   // e.g. clashes with other tracks

    SuggestionPanel();
    void setState (const std::vector<Advice>& items, const juce::StringArray& goodNotes, const std::vector<Info>& infos,
                   const juce::String& status, float passProgress, bool listeningNow, const juce::String& emptyText,
                   const juce::String& targetName);

    std::function<void (const juce::String& id)> onCheck, onReopen, onSelect;
    std::function<void()> onRescan, onStartOver;
    juce::String getSelected() const { return content.selected; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (SuggestionPanel& p) : owner (p) {}
        int layout (int width);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        std::vector<Advice> items;
        std::vector<Info> infos;
        juce::StringArray good;
        juce::String emptyText, selected;
        bool userCollapsed = false;

    private:
        struct Hit { juce::Rectangle<float> area; juce::String id; int what; };   // 0 check, 1 reopen, 2 select
        std::vector<Hit> hits;
        SuggestionPanel& owner;
        float cardHeight (const Advice&, float width) const;
        float detailHeight (const Advice&, float width) const;
    };

    Content content { *this };
    juce::Viewport viewport;
    juce::TextButton rescanButton { "scan again" }, startOverButton { "start over" };
    juce::String status, targetName;
    float progress = 0.0f;
    bool listening = false;
    int openItems = 0, doneItems = 0;
};
