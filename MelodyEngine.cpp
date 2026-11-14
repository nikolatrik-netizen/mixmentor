#include "MelodyEngine.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace melody
{
    namespace
    {
        const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };
        const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        /** Converts between MIDI pitch and "scale steps" so melodies can be moved musically. */
        struct ScaleMap
        {
            int root = 0;
            std::vector<int> pcs;   // scale pitch classes relative to root (7 notes)

            int toStep (int pitch) const
            {
                const int rel = pitch - root;
                const int octave = (int) std::floor (rel / 12.0);
                const int pc = rel - octave * 12;
                int idx = 0;
                for (int i = 0; i < (int) pcs.size(); ++i)
                    if (pcs[(size_t) i] <= pc) idx = i;
                return octave * 7 + idx;
            }

            int fromStep (int step) const
            {
                const int octave = (int) std::floor (step / 7.0);
                const int idx = step - octave * 7;
                return root + octave * 12 + pcs[(size_t) idx];
            }

            int snap (int pitch) const { return fromStep (toStep (pitch)); }
            bool isChordTone (int step) const { const int d = ((step % 7) + 7) % 7; return d == 0 || d == 2 || d == 4; }
        };

        ScaleMap scaleFor (const Analysis& a)
        {
            ScaleMap m;
            m.root = a.root;
            for (int i = 0; i < 7; ++i)
                m.pcs.push_back (a.minor ? minorScale[i] : majorScale[i]);
            return m;
        }

        void sortByTime (Phrase& p)
        {
            std::sort (p.begin(), p.end(), [] (const Note& x, const Note& y)
                       { return std::abs (x.start - y.start) > 1.0e-9 ? x.start < y.start : x.pitch < y.pitch; });
        }

        bool isStrongBeat (double t)    { return std::abs (t - std::round (t)) < 0.01; }

        /** Keep a melody a clean single line: no overlaps, nothing past the end, no duplicates. */
        Phrase tidy (Phrase p, double totalLength)
        {
            sortByTime (p);
            Phrase out;
            for (auto& n : p)
            {
                if (n.start >= totalLength - 1.0e-6) continue;
                if (! out.empty() && std::abs (out.back().start - n.start) < 1.0e-6) continue;   // same onset
                out.push_back (n);
            }
            for (size_t i = 0; i < out.size(); ++i)
            {
                const double limit = i + 1 < out.size() ? out[i + 1].start : totalLength;
                out[i].length = juce::jlimit (1.0 / 32.0, juce::jmax (1.0 / 32.0, limit - out[i].start), out[i].length);
            }
            return out;
        }
        bool isEighth (double t)        { return std::abs (t * 2.0 - std::round (t * 2.0)) < 0.01; }

        Phrase clampRange (Phrase p, const Analysis& a, const ScaleMap& m)
        {
            const int lo = juce::jmax (24, a.lowest - 7), hi = juce::jmin (108, a.highest + 7);
            for (auto& n : p)
            {
                while (n.pitch < lo) n.pitch += 12;
                while (n.pitch > hi) n.pitch -= 12;
                n.pitch = m.snap (n.pitch);
            }
            return p;
        }

        // ---- the variations ---------------------------------------------------------------

        /** Same rhythm, new notes: a guided random walk through the scale. */
        Phrase newNotes (const Phrase& in, const Analysis& a, const ScaleMap& m, juce::Random& rnd)
        {
            Phrase out = in;
            int step = m.toStep (in.front().pitch);
            const int lo = m.toStep (a.lowest) - 2, hi = m.toStep (a.highest) + 2;

            for (size_t i = 0; i < out.size(); ++i)
            {
                if (i > 0)
                {
                    const int moves[] = { -2, -1, -1, 1, 1, 2, 0, 3, -3 };
                    step += moves[rnd.nextInt (9)];
                    if (step < lo) step += 2;
                    if (step > hi) step -= 2;
                    // Strong beats prefer chord tones (1st, 3rd, 5th of the key).
                    if (isStrongBeat (out[i].start) && ! m.isChordTone (step))
                        step += (rnd.nextBool() ? 1 : -1);
                }
                out[i].pitch = m.fromStep (step);
            }

            // End on the root or the 5th so the phrase resolves.
            const int last = m.toStep (out.back().pitch);
            const int rootStep = (int) std::round (last / 7.0) * 7;
            out.back().pitch = m.fromStep (rnd.nextBool() ? rootStep : rootStep + 4);
            return out;
        }

        /** Mirror the melody upside-down around its first note, in the scale. */
        Phrase inversion (const Phrase& in, const ScaleMap& m)
        {
            Phrase out = in;
            const int axis = m.toStep (in.front().pitch);
            for (auto& n : out)
                n.pitch = m.fromStep (axis - (m.toStep (n.pitch) - axis));
            return out;
        }

        /** Play it backwards in time. */
        Phrase retrograde (const Phrase& in, const Analysis& a)
        {
            Phrase out;
            for (auto& n : in)
            {
                Note r = n;
                r.start = juce::jmax (0.0, a.lengthBeats - (n.start + n.length));
                out.push_back (r);
            }
            sortByTime (out);
            return out;
        }

        /** Keep the original as the "call" and write an answer phrase that resolves home. */
        Phrase callResponse (const Phrase& in, const Analysis& a, const ScaleMap& m, juce::Random& rnd)
        {
            Phrase out = in;
            const double offset = a.lengthBeats;
            Phrase answer = newNotes (in, a, m, rnd);

            // Answer starts a third lower and moves in contrary direction to the call's ending.
            for (auto& n : answer)
            {
                n.start += offset;
                n.pitch = m.fromStep (m.toStep (n.pitch) - 2);
            }
            const int last = m.toStep (answer.back().pitch);
            answer.back().pitch = m.fromStep ((int) std::round (last / 7.0) * 7);   // land on the root
            answer.back().length = juce::jmax (answer.back().length, 0.5);
            out.insert (out.end(), answer.begin(), answer.end());
            return out;
        }

        /** Keep only the important notes: strong positions and long notes. */
        Phrase simplified (const Phrase& in)
        {
            Phrase keep;
            for (auto& n : in)
                if (isEighth (n.start) || n.length >= 0.5)
                    keep.push_back (n);
            if (keep.size() < 3)
                keep = in;

            // Thin out every other note if it's still busy, then let notes ring until the next one.
            if (keep.size() > 8)
            {
                Phrase thin;
                for (size_t i = 0; i < keep.size(); ++i)
                    if (i % 2 == 0 || isStrongBeat (keep[i].start))
                        thin.push_back (keep[i]);
                keep = thin;
            }
            for (size_t i = 0; i + 1 < keep.size(); ++i)
                keep[i].length = juce::jmax (keep[i].length, keep[i + 1].start - keep[i].start);
            return keep;
        }

        /** Add passing notes, neighbour notes and (for some genres) jazzy colour tones. */
        Phrase embellished (const Phrase& in, const ScaleMap& m, const GenreFeel& feel, juce::Random& rnd)
        {
            Phrase out;
            for (size_t i = 0; i < in.size(); ++i)
            {
                const auto& n = in[i];
                const bool hasNext = i + 1 < in.size();
                const double gap = hasNext ? in[i + 1].start - n.start : n.length;
                const int s1 = m.toStep (n.pitch);

                if (hasNext && gap >= 0.5)
                {
                    const int s2 = m.toStep (in[i + 1].pitch);
                    Note first = n;
                    first.length = gap * 0.5;
                    out.push_back (first);

                    Note pass = n;
                    pass.start = n.start + gap * 0.5;
                    pass.length = gap * 0.5;
                    pass.velocity = n.velocity * 0.8f;
                    if (std::abs (s2 - s1) >= 2)
                        pass.pitch = m.fromStep (s1 + (s2 > s1 ? 1 : -1));          // step towards the next note
                    else if (feel.extended && rnd.nextFloat() < 0.5f)
                        pass.pitch = m.fromStep (s1 + (rnd.nextBool() ? 6 : 8));    // 7th or 9th colour tone
                    else
                        pass.pitch = m.fromStep (s1 + (rnd.nextBool() ? 1 : -1));   // neighbour note
                    out.push_back (pass);
                }
                else
                {
                    out.push_back (n);
                }
            }
            return out;
        }

        /** Repeat the opening motif, moving it up the scale (a melodic sequence). */
        Phrase sequence (const Phrase& in, const Analysis& a, const ScaleMap& m)
        {
            // Motif = notes in the first beat or two (at least two notes).
            double motifLen = 1.0;
            while (motifLen < a.lengthBeats)
            {
                int count = 0;
                for (auto& n : in) if (n.start < motifLen) ++count;
                if (count >= 2) break;
                motifLen *= 2.0;
            }
            Phrase motif;
            for (auto& n : in) if (n.start < motifLen) motif.push_back (n);
            if (motif.empty()) return in;

            Phrase out;
            const int shifts[] = { 0, 2, 4, 2, 0, -1, 1, 0 };
            int rep = 0;
            for (double t = 0.0; t < a.lengthBeats - 0.001; t += motifLen, ++rep)
            {
                for (auto n : motif)
                {
                    n.start += t;
                    n.pitch = m.fromStep (m.toStep (n.pitch) + shifts[rep % 8]);
                    if (n.start < a.lengthBeats) out.push_back (n);
                }
            }
            return out;
        }

        /** Re-shape the rhythm in the genre's groove. */
        Phrase genreGroove (const Phrase& in, const Analysis& a, const ScaleMap& m,
                            const GenreFeel& feel, juce::Random& rnd)
        {
            Phrase out;

            if (feel.sparse)
            {
                // Ambient / lo-fi: fewer notes, longer and slightly laid back.
                for (size_t i = 0; i < in.size(); ++i)
                {
                    if (in.size() > 4 && i % 2 == 1 && ! isStrongBeat (in[i].start)) continue;
                    Note n = in[i];
                    n.length = juce::jmax (n.length * 2.0, 1.0);
                    if (feel.id == "lofi") n.start += 0.03;   // lazy, behind the beat
                    out.push_back (n);
                }
            }
            else if (feel.preferTriplets)
            {
                // Trap-style: some notes become triplet stutters (rolls).
                for (auto& n : in)
                {
                    if (n.length >= 0.5 && rnd.nextFloat() < 0.45f)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            Note t = n;
                            t.start = n.start + k / 6.0;
                            t.length = 1.0 / 6.0;
                            t.velocity = n.velocity * (k == 0 ? 1.0f : 0.75f);
                            out.push_back (t);
                        }
                    }
                    else out.push_back (n);
                }
            }
            else if (feel.offbeatAccent)
            {
                // House / trance: short stabs pushed onto the offbeat 8ths.
                for (auto& n : in)
                {
                    Note s = n;
                    s.start = std::floor (n.start) + 0.5;
                    s.length = 0.25;
                    if (! out.empty() && std::abs (out.back().start - s.start) < 0.01) continue;
                    out.push_back (s);
                }
            }
            else if (feel.syncopated)
            {
                // UK garage / 2-step: push some notes a 16th early for a skippy feel.
                for (auto n : in)
                {
                    if (isStrongBeat (n.start) && n.start > 0.0 && rnd.nextFloat() < 0.6f)
                        n.start -= 0.25;
                    n.length = juce::jmin (n.length, 0.5);
                    out.push_back (n);
                }
            }
            else
            {
                // Default: a little rhythmic displacement (anticipate strong beats).
                for (auto n : in)
                {
                    if (isStrongBeat (n.start) && n.start > 0.0 && rnd.nextFloat() < 0.35f)
                        n.start -= 0.5;
                    out.push_back (n);
                }
            }

            // Apply the genre's 16th-note swing: delay every second 16th.
            if (feel.swing > 0.505f)
            {
                for (auto& n : out)
                {
                    const double sixteenth = n.start * 4.0;
                    const double frac = sixteenth - std::floor (sixteenth);
                    const int idx = (int) std::floor (sixteenth + 0.001);
                    if (frac < 0.01 && idx % 2 == 1)
                        n.start += (feel.swing - 0.5) * 0.5;   // swing % of an 8th note
                }
            }

            sortByTime (out);
            for (auto& n : out) { n.start = juce::jmax (0.0, n.start); n.pitch = m.snap (n.pitch); }
            juce::ignoreUnused (a);
            return out;
        }

        /** Lift the hook: answer the original an octave up with bigger leaps (chorus energy). */
        Phrase hookLift (const Phrase& in, const ScaleMap& m)
        {
            Phrase out = in;
            for (size_t i = 0; i < out.size(); ++i)
            {
                int step = m.toStep (out[i].pitch) + 7;           // up an octave
                if (isStrongBeat (out[i].start) && ! m.isChordTone (step))
                    step += 1;                                     // land strong beats on chord tones
                if (i == out.size() - 1)
                    step = (int) std::round (step / 7.0) * 7;      // finish on the root
                out[i].pitch = m.fromStep (step);
                out[i].velocity = juce::jmin (1.0f, out[i].velocity * 1.1f);
            }
            return out;
        }
    }

    //==============================================================================
    GenreFeel feelForGenre (const juce::String& id)
    {
        GenreFeel f;
        f.id = id;
        if (id == "hiphop")          { f.swing = 0.54f; f.halfTime = true; f.preferTriplets = true; }
        else if (id == "lofi")       { f.swing = 0.60f; f.sparse = true; f.extended = true; }
        else if (id == "pop")        { f.swing = 0.50f; }
        else if (id == "house")      { f.swing = 0.52f; f.offbeatAccent = true; f.extended = true; }
        else if (id == "ukg")        { f.swing = 0.57f; f.syncopated = true; f.extended = true; }
        else if (id == "techno")     { f.swing = 0.50f; f.offbeatAccent = true; }
        else if (id == "trance")     { f.swing = 0.50f; f.offbeatAccent = true; }
        else if (id == "dnb")        { f.swing = 0.50f; f.syncopated = true; }
        else if (id == "dubstep")    { f.swing = 0.50f; f.halfTime = true; f.preferTriplets = true; }
        else if (id == "futurebass") { f.swing = 0.50f; f.halfTime = true; f.preferTriplets = true; f.extended = true; }
        else if (id == "ambient")    { f.swing = 0.50f; f.sparse = true; }
        return f;
    }

    Analysis analyse (const Phrase& input)
    {
        Analysis a;
        if (input.empty())
        {
            a.keyName = "-";
            return a;
        }

        // Key detection: correlate a duration-weighted pitch-class histogram with the
        // Krumhansl-Kessler major and minor key profiles.
        static const double majorProfile[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double minorProfile[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        double hist[12] = {};
        for (auto& n : input)
            hist[((n.pitch % 12) + 12) % 12] += juce::jmax (0.1, n.length);

        auto correlate = [&hist] (const double* profile, int root)
        {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += hist[i]; my += profile[i]; }
            mx /= 12.0; my /= 12.0;
            double num = 0, dx = 0, dy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double x = hist[(i + root) % 12] - mx, y = profile[i] - my;
                num += x * y; dx += x * x; dy += y * y;
            }
            return dx > 0 && dy > 0 ? num / std::sqrt (dx * dy) : 0.0;
        };

        double best = -2.0;
        for (int r = 0; r < 12; ++r)
        {
            const double cMaj = correlate (majorProfile, r), cMin = correlate (minorProfile, r);
            if (cMaj > best) { best = cMaj; a.root = r; a.minor = false; }
            if (cMin > best) { best = cMin; a.root = r; a.minor = true; }
        }
        a.keyName = juce::String (noteNames[a.root]) + (a.minor ? " minor" : " major");
        for (int i = 0; i < 7; ++i)
            a.scale.push_back ((a.root + (a.minor ? minorScale[i] : majorScale[i])) % 12);

        a.lowest = 127; a.highest = 0;
        double end = 0.0;
        for (auto& n : input)
        {
            a.lowest = juce::jmin (a.lowest, n.pitch);
            a.highest = juce::jmax (a.highest, n.pitch);
            end = juce::jmax (end, n.start + n.length);
        }
        a.lengthBeats = juce::jmax (4.0, std::ceil (end / 4.0 - 0.01) * 4.0);
        a.notesPerBeat = (float) input.size() / (float) a.lengthBeats;

        // Most common gap between note starts = the main rhythmic grid.
        std::map<int, int> gaps;
        for (size_t i = 1; i < input.size(); ++i)
        {
            const double g = input[i].start - input[i - 1].start;
            if (g > 0.01) ++gaps[(int) std::round (g * 12.0)];
        }
        int bestCount = 0;
        for (auto& [g, c] : gaps)
            if (c > bestCount) { bestCount = c; a.grid = g / 12.0; }
        return a;
    }

    std::vector<Variation> generate (const Phrase& rawInput, const Analysis& a, const GenreFeel& feel, juce::int64 seed)
    {
        std::vector<Variation> out;
        if (rawInput.empty())
            return out;

        Phrase input = rawInput;
        sortByTime (input);
        const auto m = scaleFor (a);
        auto rnd = [seed] (int k) { return juce::Random (seed * 7919 + k * 104729 + 17); };

        juce::Random r1 = rnd (1), r2 = rnd (2), r3 = rnd (3), r4 = rnd (4);

        out.push_back ({ "new notes", "same rhythm, new notes from " + a.keyName + ", resolving home",
                         clampRange (newNotes (input, a, m, r1), a, m) });
        out.push_back ({ "call & response", "your melody, then an answer phrase that lands on the root",
                         clampRange (callResponse (input, a, m, r2), a, m) });
        out.push_back ({ "genre groove", "re-shaped in the genre's rhythm and swing",
                         clampRange (genreGroove (input, a, m, feel, r3), a, m) });
        out.push_back ({ "motif sequence", "the opening motif repeated and moved up the scale",
                         clampRange (sequence (input, a, m), a, m) });
        out.push_back ({ "embellished", feel.extended ? "passing notes and jazzy 7th/9th colour tones"
                                                      : "passing and neighbour notes between your notes",
                         clampRange (embellished (input, m, feel, r4), a, m) });
        out.push_back ({ "simplified", "only the strongest notes: a cleaner, more singable hook",
                         simplified (input) });
        out.push_back ({ "hook lift", "an octave up with chord tones on the beat, for a chorus or drop",
                         hookLift (input, m) });
        out.push_back ({ "inversion", "the melody mirrored upside-down in the same key",
                         clampRange (inversion (input, m), a, m) });
        out.push_back ({ "reversed", "played backwards: same notes, new shape",
                         retrograde (input, a) });
        for (auto& v : out)   // call & response is twice as long (call + answer); everything else keeps the length
            v.notes = tidy (v.notes, v.name == "call & response" ? a.lengthBeats * 2.0 : a.lengthBeats);
        return out;
    }

    //==============================================================================
    Phrase readMidiFile (const juce::File& file)
    {
        Phrase p;
        juce::FileInputStream in (file);
        juce::MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in))
            return p;

        const short tf = mf.getTimeFormat();
        const double tpq = tf > 0 ? (double) tf : 960.0;

        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*mf.getTrack (t));
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* ev = seq.getEventPointer (i);
                if (! ev->message.isNoteOn()) continue;
                Note n;
                n.pitch = ev->message.getNoteNumber();
                n.velocity = ev->message.getFloatVelocity();
                n.start = ev->message.getTimeStamp() / tpq;
                const double endTick = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp()
                                                                    : ev->message.getTimeStamp() + tpq * 0.25;
                n.length = juce::jmax (1.0 / 32.0, endTick / tpq - n.start);
                p.push_back (n);
            }
        }
        sortByTime (p);
        if (! p.empty())
        {
            const double shift = std::floor (p.front().start / 4.0) * 4.0;   // start at the first bar with notes
            for (auto& n : p) n.start -= shift;
        }
        return p;
    }

    bool writeMidiFile (const Phrase& phrase, const juce::File& file, double bpm)
    {
        constexpr int tpq = 960;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (20.0, bpm))), 0.0);
        for (auto& n : phrase)
        {
            const double on = n.start * tpq, off = (n.start + n.length) * tpq;
            seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, juce::jlimit (0.05f, 1.0f, n.velocity)), on);
            seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), juce::jmax (on + 1.0, off - 1.0));
        }
        seq.updateMatchedPairs();

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (tpq);
        mf.addTrack (seq);

        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && mf.writeTo (out);
    }

    juce::String noteName (int midiNote)
    {
        return juce::String (noteNames[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
    }
}
