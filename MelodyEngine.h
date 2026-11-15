#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace melody
{
    /** One note. Times are in beats (quarter notes) from the start of the phrase. */
    struct Note
    {
        double start = 0.0, length = 0.25;
        int pitch = 60;
        float velocity = 0.8f;
    };

    using Phrase = std::vector<Note>;

    /** Scales you can lock the ideas to. */
    enum ScaleType { major = 0, minorNatural, harmonicMinor, dorian, phrygian, mixolydian, minorPentatonic, majorPentatonic, numScaleTypes };
    juce::StringArray scaleTypeNames();
    juce::StringArray keyNames();

    /** What the analysis found in the input melody (or the key you picked). */
    struct Analysis
    {
        int root = 0;                 // 0 = C ... 11 = B
        bool minor = true;            // minor-type scale (natural/harmonic minor, dorian, phrygian, minor pentatonic)
        int scaleType = minorNatural;
        bool keyFromUser = false;     // true when the key was picked, false when detected
        juce::String keyName;         // e.g. "A minor"
        std::vector<int> scale;       // pitch classes of the detected scale, root first
        int lowest = 60, highest = 72;
        double lengthBeats = 4.0;     // phrase length, rounded up to whole bars
        double grid = 0.25;           // main rhythmic resolution in beats
        float notesPerBeat = 1.0f;
    };

    /** A generated idea: a name, a one-line explanation and the notes. */
    struct Variation
    {
        juce::String name, description;
        Phrase notes;
    };

    /** Genre feel used to steer the variations. */
    struct GenreFeel
    {
        juce::String id;
        float swing = 0.5f;           // 0.5 = straight, 0.66 = triplet swing (16th-note swing)
        bool halfTime = false;
        bool preferTriplets = false;  // trap-style rolls and triplet phrasing
        bool offbeatAccent = false;   // house/trance offbeat bounce
        bool syncopated = false;      // 2-step / garage
        bool sparse = false;          // ambient / lo-fi: fewer, longer notes
        bool extended = false;        // likes 7ths/9ths colour notes (lo-fi, house, UKG, future bass)
    };

    GenreFeel feelForGenre (const juce::String& genreId);

    /** Detects key and range. forcedRoot (0-11) / forcedScale override the detection; -1 = auto. */
    Analysis analyse (const Phrase& input, int forcedRoot = -1, int forcedScale = -1);

    /** Creates the full set of variations. The seed makes "new ideas" reproducible. */
    std::vector<Variation> generate (const Phrase& input, const Analysis& analysis,
                                     const GenreFeel& feel, juce::int64 seed);

    // MIDI file helpers -------------------------------------------------------------------
    Phrase readMidiFile (const juce::File& file);
    bool writeMidiFile (const Phrase& phrase, const juce::File& file, double bpm);

    juce::String noteName (int midiNote);
}
