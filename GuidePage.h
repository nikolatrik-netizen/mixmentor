#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "KnowledgeBase.h"

/** The tips and theory pages: a designed page instead of plain text.
    Tips: a header with the key numbers, numbered tip cards for your instrument (numbers
    highlighted), the genre's pro tricks for this process, and common mistakes.
    Theory: each section as a card, bullets and progressions laid out clearly. Scrolls. */
class GuidePage : public juce::Component
{
public:
    GuidePage();
    void showTips (const juce::String& processName, const juce::String& instrumentName, const juce::String& genreName,
                   const juce::String& tipText, const std::vector<mm::Trick>& tricks, const juce::StringArray& mistakes);
    void showTheory (const juce::String& genreName, const juce::String& bpm, const juce::String& theoryText);
    void resized() override;

private:
    class Content : public juce::Component
    {
    public:
        int layout (int width);
        void paint (juce::Graphics&) override;

        bool theory = false;
        juce::String title, subtitle;
        juce::StringArray keyNumbers;
        juce::StringArray tips;                        // paragraphs
        std::vector<mm::Trick> tricks;
        juce::StringArray mistakes;
        struct Section { juce::String heading; juce::StringArray lines; };
        std::vector<Section> sections;

    private:
        float paintOrMeasure (juce::Graphics* g, float width);
    };

    Content content;
    juce::Viewport viewport;
};
