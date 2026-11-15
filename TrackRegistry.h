#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

/** Every Mix Mentor in the same song shares its last analysis here (plugins in one DAW run in
    the same process), so each one can warn about clashes with the other tracks: kick vs. bass,
    vocal vs. chords, and so on. */
struct TrackSnapshot
{
    juce::String trackName, instrumentId, instrumentName;
    std::vector<float> bandDb;      // 31 third-octave bands from 20 Hz, dB
    juce::int64 timeMs = 0;
};

class TrackRegistry
{
public:
    static TrackRegistry& get()
    {
        static TrackRegistry instance;
        return instance;
    }

    int newId()                                  { const juce::ScopedLock sl (lock); return ++lastId; }
    void publish (int id, const TrackSnapshot& s) { const juce::ScopedLock sl (lock); tracks[id] = s; }
    void remove (int id)                          { const juce::ScopedLock sl (lock); tracks.erase (id); }

    std::vector<TrackSnapshot> others (int id) const
    {
        const juce::ScopedLock sl (lock);
        std::vector<TrackSnapshot> out;
        for (auto& [k, v] : tracks)
            if (k != id) out.push_back (v);
        return out;
    }

    static float bandHz (int band) { return 20.0f * std::pow (2.0f, (float) band / 3.0f); }

private:
    juce::CriticalSection lock;
    std::map<int, TrackSnapshot> tracks;
    int lastId = 0;
};
