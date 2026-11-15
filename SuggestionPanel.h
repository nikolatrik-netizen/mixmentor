#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Advisor.h"

/** The big right-hand panel: one card per thing to fix, with a live meter that moves while you
    turn your EQ/compressor, and a "Done, check it" box. Scrolls. */
class SuggestionPanel : public juce::Component
{
public:
    struct Info { juce::String tag, title, action, why; };   // e.g. clashes with other tracks

    SuggestionPanel();
    void setState (const std::vector<Advice>& items, const juce::StringArray& goodNotes, const std::vector<Info>& infos,
                   const juce::String& status, float passProgress, bool listeningNow, const juce::String& emptyText,
                   const juce::String& targetName);

    std::function<void (const juce::String& id)> onCheck, onReopen;
    std::function<void()> onRescan, onStartOver;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (SuggestionPanel& p) : owner (p) {}
        int layout (int width);   // returns the height
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        std::vector<Advice> items;
        std::vector<Info> infos;
        juce::StringArray good;
        juce::String emptyText;

    private:
        struct Hit { juce::Rectangle<float> area; juce::String id; bool reopen; };
        std::vector<Hit> hits;
        SuggestionPanel& owner;
        float cardHeight (const Advice&, float width) const;
    };

    Content content { *this };
    juce::Viewport viewport;
    juce::TextButton rescanButton { "scan again" }, startOverButton { "start over" };
    juce::String status, targetName;
    float progress = 0.0f;
    bool listening = false;
    int openItems = 0, doneItems = 0;
};
