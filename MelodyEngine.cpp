#include "MelodyEngine.h"
#include "MelodyModel.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace melody
{
    namespace
    {
        const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        // Each scale as 7 "parent" notes; pentatonic scales use 5 of them.
        const int scaleNotes[numScaleTypes][7] = {
            { 0, 2, 4, 5, 7, 9, 11 },   // major
            { 0, 2, 3, 5, 7, 8, 10 },   // natural minor
            { 0, 2, 3, 5, 7, 8, 11 },   // harmonic minor
            { 0, 2, 3, 5, 7, 9, 10 },   // dorian
            { 0, 1, 3, 5, 7, 8, 10 },   // phrygian
            { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian
            { 0, 2, 3, 5, 7, 8, 10 },   // minor pentatonic (parent: natural minor)
            { 0, 2, 4, 5, 7, 9, 11 },   // major pentatonic (parent: major)
        };
        const bool scaleUses[numScaleTypes][7] = {
            { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 },
            { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1 },
            { 1, 0, 1, 1, 1, 0, 1 },    // minor pentatonic: 1 b3 4 5 b7
            { 1, 1, 1, 0, 1, 1, 0 },    // major pentatonic: 1 2 3 5 6
        };
        const char* scaleLabels[numScaleTypes] = { "major", "minor", "harmonic minor", "dorian", "phrygian",
                                                   "mixolydian", "minor pentatonic", "major pentatonic" };

        int degreeOf (int step) { return ((step % 7) + 7) % 7; }

        /** Converts between MIDI pitch and "scale steps" so melodies can be moved musically.
            Every pitch that comes out of fromStep() or snap() is in the scale. */
        struct ScaleMap
        {
            int root = 0;
            int mode = 1;            // statistics to use: 0 major-type, 1 minor-type
            int pcs[7] = {};
            bool uses[7] = {};

            bool usesDegree (int step) const { return uses[degreeOf (step)]; }

            /** Nearest allowed scale step (ties go down). */
            int allowedStep (int step) const
            {
                for (int d = 0; d < 4; ++d)
                {
                    if (usesDegree (step - d)) return step - d;
                    if (usesDegree (step + d)) return step + d;
                }
                return step;
            }

            int fromStep (int step) const
            {
                step = allowedStep (step);
                const int octave = (int) std::floor (step / 7.0);
                return root + octave * 12 + pcs[degreeOf (step)];
            }

            /** The scale step of the scale note nearest to this pitch. */
            int toStep (int pitch) const
            {
                int bestStep = 0, bestDist = 1000;
                const int rel = pitch - root;
                const int base = (int) std::floor (rel / 12.0) * 7;
                for (int s = base - 7; s <= base + 14; ++s)
                {
                    if (! usesDegree (s)) continue;
                    const int pitchOfStep = root + (int) std::floor (s / 7.0) * 12 + pcs[degreeOf (s)];
                    const int dist = std::abs (pitchOfStep - pitch);
                    if (dist < bestDist || (dist == bestDist && pitchOfStep < pitch)) { bestDist = dist; bestStep = s; }
                }
                return bestStep;
            }

            int snap (int pitch) const { return fromStep (toStep (pitch)); }
            bool isChordTone (int step) const { const int d = degreeOf (step); return d == 0 || d == 2 || d == 4; }
        };

        ScaleMap scaleFor (const Analysis& a)
        {
            ScaleMap m;
            m.root = a.root;
            const int t = juce::jlimit (0, (int) numScaleTypes - 1, a.scaleType);
            m.mode = (t == major || t == mixolydian || t == majorPentatonic) ? 0 : 1;
            for (int i = 0; i < 7; ++i) { m.pcs[i] = scaleNotes[t][i]; m.uses[i] = scaleUses[t][i]; }
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
            if (p.empty()) return p;
            const int lo = juce::jmax (24, a.lowest - 7), hi = juce::jmin (108, a.highest + 12);
            // Move the whole idea by octaves first (keeps its shape), and only fold single notes
            // if it's still out of range.
            int top = 0, bottom = 127;
            for (auto& n : p) { top = juce::jmax (top, n.pitch); bottom = juce::jmin (bottom, n.pitch); }
            int shift = 0;
            while (top + shift > hi && bottom + shift - 12 >= lo - 5) shift -= 12;
            while (bottom + shift < lo && top + shift + 12 <= hi + 5) shift += 12;
            for (auto& n : p)
            {
                n.pitch += shift;
                while (n.pitch < lo - 5) n.pitch += 12;
                while (n.pitch > hi + 5) n.pitch -= 12;
                n.pitch = m.snap (n.pitch);
            }
            return p;
        }

        // ---- the variations ---------------------------------------------------------------

        // ---- the trained note model -----------------------------------------------------

        struct Slot { double start = 0.0, length = 0.25; float velocity = 0.8f; };

        /** Where a note falls in the bar: 0 bar or half-bar, 1 other beats, 2 8ths, 3 16ths/other. */
        int metricOf (double start)
        {
            double b = std::fmod (start, 4.0);
            if (b < 0) b += 4.0;
            auto near = [] (double x, double y) { return std::abs (x - y) < 0.02; };
            if (near (b, 0.0) || near (b, 2.0) || near (b, 4.0)) return 0;
            const double f = b - std::floor (b);
            if (f < 0.02 || f > 0.98) return 1;
            if (near (f, 0.5)) return 2;
            return 3;
        }

        std::vector<Slot> slotsOf (const Phrase& p)
        {
            std::vector<Slot> out;
            for (auto& n : p) out.push_back ({ n.start, n.length, n.velocity });
            return out;
        }

        std::vector<int> contourOf (const Phrase& p)
        {
            std::vector<int> c;
            for (size_t i = 1; i < p.size(); ++i)
                c.push_back (p[i].pitch > p[i - 1].pitch ? 1 : p[i].pitch < p[i - 1].pitch ? -1 : 0);
            return c;
        }

        struct LineSpec
        {
            int lo = 0, hi = 14;                 // allowed range in scale steps
            int startStep = -1000;               // -1000 = let the model choose
            const std::vector<int>* contour = nullptr;
            float contourWeight = 0.0f;          // how strongly to follow the contour (0..1)
            bool endOnRoot = true;               // false = end on any stable note (1st, 3rd, 5th)
        };

        /** Writes notes for the given rhythm with the statistics learned from real melodies, plus the
            classic rules: resolve big leaps by step the other way, land strong beats on stable notes,
            one clear high point, end on a stable note. Many candidates are written and the most
            natural ones kept. */
        std::vector<int> modelSteps (const std::vector<Slot>& slots, const ScaleMap& m, const LineSpec& spec, juce::Random& rnd)
        {
            const int n = (int) slots.size();
            if (n == 0) return {};
            const int mode = m.mode;

            struct Candidate { float score; std::vector<int> steps; };
            std::vector<Candidate> pool;

            for (int attempt = 0; attempt < 220; ++attempt)
            {
                std::vector<int> st;
                st.reserve ((size_t) n);
                float logp = 0.0f;

                // First note
                int first = spec.startStep;
                if (first == -1000)
                {
                    const int mid = (spec.lo + spec.hi) / 2;
                    float w[7] = {};
                    float total = 0.0f;
                    for (int d = 0; d < 7; ++d) { w[d] = m.uses[d] ? model::firstProb[mode][d] : 0.0f; total += w[d]; }
                    float r = rnd.nextFloat() * total;
                    int deg = 0;
                    for (; deg < 6; ++deg) { r -= w[deg]; if (r <= 0.0f) break; }
                    first = (int) std::floor ((mid - deg) / 7.0 + 0.5) * 7 + deg;
                }
                st.push_back (juce::jlimit (spec.lo, spec.hi, m.allowedStep (first)));

                for (int i = 1; i < n; ++i)
                {
                    const int cur = st.back();
                    const int deg = degreeOf (cur);
                    const int mc = metricOf (slots[(size_t) i].start);
                    const bool last = i == n - 1;
                    const int prevMove = i >= 2 ? st[(size_t) i - 1] - st[(size_t) i - 2] : 0;
                    const bool twoRepeats = i >= 3 && st[(size_t) i - 1] == st[(size_t) i - 2] && st[(size_t) i - 2] == st[(size_t) i - 3];

                    float w[15] = {};
                    float total = 0.0f;
                    for (int k = 0; k < 15; ++k)
                    {
                        const int move = k - 7;
                        const int next = cur + move;
                        if (next < spec.lo || next > spec.hi || ! m.usesDegree (next)) continue;
                        float x = model::stepProb[mode][deg][mc][k];

                        if (std::abs (prevMove) >= 3)                               // after a leap...
                            x *= (move * prevMove < 0 && std::abs (move) <= 2) ? 2.5f   // ...step back the other way
                               : (move * prevMove > 0 ? 0.25f : 1.0f);
                        if (move == 0 && twoRepeats) x *= 0.15f;                    // no long note repeats
                        if (mc == 0 && ! m.isChordTone (next)) x *= 0.6f;           // stable notes on strong beats

                        if (spec.contour != nullptr && (size_t) (i - 1) < spec.contour->size())
                        {
                            const int c = (*spec.contour)[(size_t) i - 1];
                            const int dir = move > 0 ? 1 : move < 0 ? -1 : 0;
                            if (dir == c) x *= 1.0f + spec.contourWeight * 2.0f;
                            else if (c != 0 && dir == -c) x *= 1.0f - spec.contourWeight * 0.7f;
                        }

                        if (last)
                        {
                            const int d = degreeOf (next);
                            x *= spec.endOnRoot ? (d == 0 ? 8.0f : (d == 2 || d == 4) ? 0.6f : 0.03f)
                                                : (d == 0 || d == 2 || d == 4 ? 3.0f : 0.1f);
                            x *= 1.0f / (1.0f + 0.5f * (float) std::abs (move));   // arrive smoothly
                        }
                        w[k] = x;
                        total += x;
                    }

                    if (total <= 0.0f) { st.push_back (cur); logp -= 4.0f; continue; }
                    float r = rnd.nextFloat() * total;
                    int pick = 7;
                    for (int k = 0; k < 15; ++k) { if (w[k] <= 0.0f) continue; pick = k; r -= w[k]; if (r <= 0.0f) break; }
                    logp += std::log (juce::jmax (1.0e-4f, model::stepProb[mode][deg][mc][pick]));
                    st.push_back (cur + pick - 7);
                }

                // Score the whole line
                float score = logp / (float) juce::jmax (1, n - 1);
                const int top = *std::max_element (st.begin(), st.end());
                const int low = *std::min_element (st.begin(), st.end());
                int peaks = 0;
                for (int i = 0; i < n; ++i)
                    if (st[(size_t) i] == top && (i == 0 || st[(size_t) i - 1] != top)) ++peaks;
                if (peaks == 1) score += 0.35f;                         // one clear high point
                if (top - low >= 3 && top - low <= 9) score += 0.25f;    // uses some range, but singable
                if (top - low < 2 && n > 4) score -= 0.6f;               // too flat
                int repeats = 0;
                for (int i = 1; i < n; ++i) if (st[(size_t) i] == st[(size_t) i - 1]) ++repeats;
                if (n > 4 && repeats > n * 0.4f) score -= 0.5f;          // stuck on one note
                for (int i = 2; i < n; ++i)                              // two big leaps in a row the same way
                {
                    const int m1 = st[(size_t) i - 1] - st[(size_t) i - 2], m2 = st[(size_t) i] - st[(size_t) i - 1];
                    if (std::abs (m1) >= 3 && std::abs (m2) >= 3 && m1 * m2 > 0) score -= 0.4f;
                }
                pool.push_back ({ score, std::move (st) });
            }

            std::sort (pool.begin(), pool.end(), [] (const Candidate& x, const Candidate& y) { return x.score > y.score; });
            const int keep = juce::jmin (5, (int) pool.size());
            return pool[(size_t) rnd.nextInt (keep)].steps;
        }

        LineSpec specFor (const Analysis& a, const ScaleMap& m)
        {
            LineSpec spec;
            spec.lo = m.toStep (a.lowest) - 2;
            spec.hi = m.toStep (a.highest) + 2;
            while (spec.hi - spec.lo < 9) { --spec.lo; ++spec.hi; }   // room for a real melody
            while (spec.hi - spec.lo > 14) { ++spec.lo; --spec.hi; }  // ...but singable
            return spec;
        }

        Phrase phraseFrom (const std::vector<Slot>& slots, const std::vector<int>& steps, const ScaleMap& m)
        {
            Phrase out;
            for (size_t i = 0; i < slots.size() && i < steps.size(); ++i)
                out.push_back ({ slots[i].start, slots[i].length, m.fromStep (steps[i]), slots[i].velocity });
            return out;
        }

        /** Same rhythm, new notes written by the trained model, loosely following your melody's shape. */
        Phrase newNotes (const Phrase& in, const Analysis& a, const ScaleMap& m, juce::Random& rnd)
        {
            auto spec = specFor (a, m);
            const auto contour = contourOf (in);
            spec.contour = &contour;
            spec.contourWeight = 0.35f;
            spec.startStep = m.toStep (in.front().pitch) + (rnd.nextBool() ? 0 : (rnd.nextBool() ? 2 : -2));
            const auto slots = slotsOf (in);
            return phraseFrom (slots, modelSteps (slots, m, spec, rnd), m);
        }

        /** A new rhythm from the most common one-bar rhythms in real songs (repeating like A A B A),
            with notes from the trained model. */
        Phrase freshRhythm (const Phrase& in, const Analysis& a, const ScaleMap& m, juce::Random& rnd)
        {
            const int bars = juce::jmax (1, (int) std::round (a.lengthBeats / 4.0));
            const float wantPerBar = juce::jlimit (2.0f, 12.0f, (float) in.size() / (float) bars);

            auto pickPattern = [&] (float density, int avoid)
            {
                float total = 0.0f;
                std::vector<float> w ((size_t) model::numBarRhythms);
                for (int k = 0; k < model::numBarRhythms; ++k)
                {
                    const int count = juce::countNumberOfBits ((juce::uint32) model::barRhythms[k]);
                    const bool startsOnOne = (model::barRhythms[k] & 1) != 0;
                    float x = (float) model::barRhythmCounts[k] / (1.0f + 1.5f * std::abs ((float) count - density));
                    if (! startsOnOne) x *= 0.5f;
                    if (k == avoid) x = 0.0f;
                    w[(size_t) k] = x; total += x;
                }
                float r = rnd.nextFloat() * total;
                for (int k = 0; k < model::numBarRhythms; ++k) { r -= w[(size_t) k]; if (r <= 0.0f) return k; }
                return 0;
            };

            const int A = pickPattern (wantPerBar, -1);
            const int B = pickPattern (wantPerBar, A);
            const int C = pickPattern (juce::jmax (2.0f, wantPerBar * 0.6f), -1);   // calmer ending
            std::vector<int> form;
            for (int b = 0; b < bars; ++b)
                form.push_back (bars == 1 ? A : b == bars - 1 ? (bars >= 4 ? C : B) : (b % 4 == 2 ? B : A));

            std::vector<Slot> slots;
            std::vector<int> barOf;
            for (int b = 0; b < bars; ++b)
            {
                const unsigned short mask = model::barRhythms[form[(size_t) b]];
                for (int k = 0; k < 16; ++k)
                    if ((mask >> k) & 1)
                    {
                        const float vel = (k % 4 == 0) ? 0.85f : 0.7f;
                        slots.push_back ({ b * 4.0 + k * 0.25, 0.25, vel });
                        barOf.push_back (b);
                    }
            }
            if (slots.empty()) return in;
            for (size_t i = 0; i < slots.size(); ++i)
            {
                const double next = i + 1 < slots.size() ? slots[i + 1].start : bars * 4.0;
                slots[i].length = juce::jlimit (0.25, 2.0, next - slots[i].start) * 0.95;
            }

            auto spec = specFor (a, m);
            spec.startStep = m.toStep (in.front().pitch);
            auto steps = modelSteps (slots, m, spec, rnd);

            // Repetition makes it catchy: bars that share bar 1's rhythm also reuse its notes
            // (all but the very last note, which keeps its resolution).
            std::vector<int> barOneSteps;
            for (size_t i = 0; i < slots.size(); ++i) if (barOf[i] == 0) barOneSteps.push_back (steps[i]);
            for (int b = 1; b < bars; ++b)
            {
                if (form[(size_t) b] != form[0]) continue;
                size_t k = 0;
                for (size_t i = 0; i < slots.size(); ++i)
                    if (barOf[i] == b && k < barOneSteps.size() && i + 1 < slots.size())
                        steps[i] = barOneSteps[k++];
            }
            return phraseFrom (slots, steps, m);
        }

        /** Keep the original as the "call" and write an answer phrase that resolves home. */
        Phrase callResponse (const Phrase& in, const Analysis& a, const ScaleMap& m, juce::Random& rnd)
        {
            Phrase out = in;
            auto spec = specFor (a, m);
            auto contour = contourOf (in);
            // The answer opens like the call (so it's recognisable) and then turns the other way.
            for (size_t i = contour.size() / 2; i < contour.size(); ++i) contour[i] = -contour[i];
            spec.contour = &contour;
            spec.contourWeight = 0.45f;
            spec.startStep = m.toStep (in.front().pitch) - 2;
            spec.endOnRoot = true;
            auto slots = slotsOf (in);
            for (auto& sl : slots) sl.start += a.lengthBeats;
            auto answer = phraseFrom (slots, modelSteps (slots, m, spec, rnd), m);
            if (! answer.empty()) answer.back().length = juce::jmax (answer.back().length, 0.5);
            out.insert (out.end(), answer.begin(), answer.end());
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

    juce::StringArray scaleTypeNames()
    {
        juce::StringArray names;
        for (auto* l : scaleLabels) names.add (l);
        return names;
    }

    juce::StringArray keyNames()
    {
        juce::StringArray names;
        for (auto* n : noteNames) names.add (n);
        return names;
    }

    Analysis analyse (const Phrase& input, int forcedRoot, int forcedScale)
    {
        Analysis a;
        if (input.empty())
        {
            if (forcedRoot >= 0) a.root = forcedRoot;
            if (forcedScale >= 0) a.scaleType = forcedScale;
            a.keyName = "-";
            return a;
        }

        // Key detection: correlate a duration-weighted pitch-class histogram with the
        // Krumhansl-Kessler major and minor key profiles.
        static const double majorProfile[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double minorProfile[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        // Long notes, notes on the beat and the last note say most about the home key.
        double hist[12] = {};
        for (size_t i = 0; i < input.size(); ++i)
        {
            const auto& n = input[i];
            double w = juce::jlimit (0.1, 2.0, n.length);
            if (std::abs (n.start - std::round (n.start)) < 0.05) w *= 1.5;
            if (std::abs (std::fmod (n.start, 4.0)) < 0.05) w *= 1.5;
            if (i + 1 == input.size()) w *= 2.0;
            hist[((n.pitch % 12) + 12) % 12] += w;
        }

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
        a.scaleType = a.minor ? minorNatural : major;
        if (forcedRoot >= 0 && forcedRoot < 12)   { a.root = forcedRoot; a.keyFromUser = true; }
        if (forcedScale >= 0 && forcedScale < numScaleTypes) { a.scaleType = forcedScale; a.keyFromUser = true; }
        a.minor = ! (a.scaleType == major || a.scaleType == mixolydian || a.scaleType == majorPentatonic);
        a.keyName = juce::String (noteNames[a.root]) + " " + scaleLabels[a.scaleType];
        for (int i = 0; i < 7; ++i)
            if (scaleUses[a.scaleType][i])
                a.scale.push_back ((a.root + scaleNotes[a.scaleType][i]) % 12);

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

        juce::Random r1 = rnd (1), r2 = rnd (2), r3 = rnd (3), r4 = rnd (4), r5 = rnd (5);

        out.push_back ({ "new notes", "same rhythm, new notes in " + a.keyName + ", following your melody's shape",
                         newNotes (input, a, m, r1) });
        out.push_back ({ "fresh rhythm", "a new rhythm from real hit melodies, repeating like A A B A",
                         freshRhythm (input, a, m, r5) });
        out.push_back ({ "call & response", "your melody, then an answer phrase that lands on the root",
                         callResponse (input, a, m, r2) });
        out.push_back ({ "genre groove", "re-shaped in the genre's rhythm and swing",
                         genreGroove (input, a, m, feel, r3) });
        out.push_back ({ "motif sequence", "the opening motif repeated and moved up the scale",
                         sequence (input, a, m) });
        out.push_back ({ "embellished", feel.extended ? "passing notes and jazzy 7th/9th colour tones"
                                                      : "passing and neighbour notes between your notes",
                         embellished (input, m, feel, r4) });
        out.push_back ({ "simplified", "only the strongest notes: a cleaner, more singable hook",
                         simplified (input) });
        out.push_back ({ "hook lift", "an octave up with stable notes on the beat, for a chorus or drop",
                         hookLift (input, m) });
        out.push_back ({ "inversion", "the melody mirrored upside-down in the same key",
                         inversion (input, m) });
        for (auto& v : out)   // everything is forced into the key and a sensible range
            v.notes = clampRange (v.notes, a, m);
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
