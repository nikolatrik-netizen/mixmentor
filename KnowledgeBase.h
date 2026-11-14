#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace mm
{
    /** A frequency range with genre-specific advice, drawn on the analyzer. */
    struct Zone
    {
        float lowHz = 0.0f, highHz = 0.0f;
        juce::String kind;   // "boost", "cut" or "watch"
        juce::String label;
        juce::String tip;
        float targetMinDb = -12.0f, targetMaxDb = 12.0f;   // healthy range vs. the track's average level
        juce::String tooMuch, tooLittle;                    // optional advice templates (see AdviceBuilder)
    };

    /** Everything the plugin knows about one instrument inside one genre. */
    struct Guide
    {
        float peakMinDb = -12.0f, peakMaxDb = -6.0f;
        std::vector<Zone> zones;
        std::map<juce::String, juce::String> tips;   // process id -> tip text
    };

    struct Genre
    {
        juce::String id, name, summary;
        std::map<juce::String, Guide> guides;        // instrument id -> guide
    };

    struct Instrument
    {
        juce::String id, name;
        juce::StringArray keywords;                  // used to guess from the DAW track name
    };

    struct Process
    {
        juce::String id, name;
    };

    /** Loads Resources/knowledge.json (compiled into the binary) once, read-only afterwards. */
    class KnowledgeBase
    {
    public:
        static const KnowledgeBase& get();

        const std::vector<Genre>&      genres() const      { return genres_; }
        const std::vector<Instrument>& instruments() const { return instruments_; }
        const std::vector<Process>&    processes() const   { return processes_; }

        const Guide* findGuide (int genreIndex, int instrumentIndex) const;

        int indexOfGenre (const juce::String& id) const;
        int indexOfInstrument (const juce::String& id) const;

        /** Returns the instrument index whose keywords appear in a track name, or -1. */
        int guessInstrumentFromTrackName (const juce::String& trackName) const;

    private:
        KnowledgeBase();

        std::vector<Genre> genres_;
        std::vector<Instrument> instruments_;
        std::vector<Process> processes_;
    };
}
