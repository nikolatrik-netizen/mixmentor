#include "Advisor.h"
#include <algorithm>
#include <cmath>

namespace
{
    /** Starting compressor settings per instrument (ratio, attack, release, gain reduction).
        Typical values from mixing guides and compression cheat sheets. */
    struct CompRecipe { const char* ratio; const char* attack; const char* release; const char* reduction; const char* note; };

    CompRecipe recipeFor (const juce::String& inst)
    {
        if (inst == "kick")   return { "4:1", "20-30 ms", "50-80 ms (let it recover before the next kick)", "3-6 dB",
                                       "The slow attack lets the beater click through, so the kick gets punchier, not flatter." };
        if (inst == "snare")  return { "4:1", "5-15 ms", "80-120 ms", "3-6 dB",
                                       "A slightly faster attack than on the kick controls the crack without killing it." };
        if (inst == "hats")   return { "2:1", "1-5 ms", "30-60 ms", "1-3 dB",
                                       "Hats rarely need much. A transient shaper (less attack) or a soft clipper is often cleaner." };
        if (inst == "bass")   return { "3:1 to 4:1", "15-25 ms", "100-150 ms, or timed to the note length", "3-5 dB",
                                       "Bass should be very even so every note sits at the same level under the kick." };
        if (inst == "vocal")  return { "3:1", "10-30 ms", "50-150 ms (or auto)", "3-6 dB",
                                       "For a very jumpy vocal use two compressors: a fast one catching peaks (2-3 dB), then a slower, smoother one (2-3 dB). Ride the volume of single loud words with automation." };
        if (inst == "master") return { "2:1", "30 ms", "auto, or about 100-200 ms", "1-2 dB",
                                       "Gentle glue on the mix bus, then a limiter does the loudness." };
        return { "2:1", "15-25 ms", "100-150 ms", "2-4 dB", "Gentle compression keeps it steady without squashing it." };
    }

    juce::String recipeText (const CompRecipe& r)
    {
        return juce::String ("Compressor: ratio ") + r.ratio + ", attack " + r.attack + ", release " + r.release
             + ", aim for " + r.reduction + " of gain reduction. " + r.note;
    }

    /** Loudness targets for the loudest part of a finished master. These are integrated-LUFS ranges from
        mastering guides; the loudest section usually reads 2-4 LU above the song's integrated value. */
    juce::Range<float> loudnessTarget (const juce::String& genre)
    {
        if (genre == "hiphop")                                   return { -10.0f, -7.0f };
        if (genre == "house" || genre == "techno" || genre == "trance" || genre == "dnb"
            || genre == "dubstep" || genre == "futurebass" || genre == "ukg") return { -9.0f, -6.0f };
        if (genre == "pop")                                      return { -11.0f, -8.0f };
        if (genre == "lofi")                                     return { -14.0f, -10.0f };
        if (genre == "ambient")                                  return { -16.0f, -12.0f };
        return { -11.0f, -8.0f };
    }


    juce::String hzText (float hz)
    {
        if (hz >= 1000.0f) return juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + " kHz";
        return juce::String (juce::roundToInt (hz)) + " Hz";
    }


    struct Region { float upTo; const char* name; const char* tooMuch; const char* tooLittle; };
    const Region regions[] = {
        { 60.0f,    "Sub",          "eats headroom and booms on big systems, and your limiter has to work harder.",
                                    "the track feels thin on big speakers and good headphones." },
        { 120.0f,   "Bass",         "sounds boomy and the kick and bass blur into one.",
                                    "lacks weight and punch." },
        { 300.0f,   "Low-mids",     "sounds muddy and cloudy, and it masks the other parts.",
                                    "sounds thin and loses warmth and body." },
        { 800.0f,   "Mids",         "sounds boxy and honky, like a cardboard box.",
                                    "sounds hollow and scooped." },
        { 2000.0f,  "Upper-mids",   "sounds nasal and gets tiring.",
                                    "parts lose definition and sit behind the mix." },
        { 5000.0f,  "Presence",     "sounds harsh and tiring at high volume.",
                                    "sounds dull, and vocals and leads don't cut through." },
        { 10000.0f, "Brilliance",   "sounds brittle and sibilant.",
                                    "lacks clarity and sparkle." },
        { 1.0e9f,   "Air",          "sounds fizzy and hissy.",
                                    "sounds closed-in and lacks air." },
    };
    const Region& regionFor (float hz)
    {
        for (auto& r : regions) if (hz < r.upTo) return r;
        return regions[7];
    }

    float smoothedLevel (const Analyzer& an, float f)
    {
        double acc = 0.0;
        for (int j = -2; j <= 2; ++j) acc += std::pow (10.0, an.levelAt (f * std::pow (2.0f, (float) j / 24.0f)) / 10.0);
        return (float) (10.0 * std::log10 (acc / 5.0 + 1.0e-20));
    }

    juce::String capitalise (const juce::String& s) { return s.substring (0, 1).toUpperCase() + s.substring (1); }
}

//==============================================================================
void Advisor::setContext (const juce::String& g, const juce::String& gName, const juce::String& inst, const juce::String& instName)
{
    const bool changed = g != genreId || inst != instrumentId;
    genreId = g; genreName = gName; instrumentId = inst; instrumentName = instName;
    if (reference.valid)              target = reference;
    else if (instrumentId == "master") target = TargetCurve::forGenre (genreId, genreName);
    else                              target = {};
    if (changed) clear();
}

void Advisor::setReference (const TargetCurve& ref)
{
    reference = ref;
    setContext (genreId, genreName, instrumentId, instrumentName);
    clear();
}

int Advisor::openCount() const
{
    int n = 0;
    for (auto& a : items) n += a.status == Advice::open ? 1 : 0;
    return n;
}

float Advisor::measure (const Advice& a, const LiveListener::Pass& pass, int fromBeat, int toBeat)
{
    if (a.kind == Advice::kRange)     return LiveListener::rangeVsRest (pass.beats, fromBeat, toBeat, a.lowHz, a.highHz);
    if (a.kind == Advice::kResonance) return LiveListener::peakVsSurroundings (pass.beats, fromBeat, toBeat, a.focusHz);
    const auto& d = pass.dynamics;
    const auto& m = a.metric;
    if (m == "clip" || m == "truepeak")   return d.peakDb;
    if (m == "crestHi" || m == "crestLo") return d.crestDb;
    if (m == "swing")                     return d.swingDb;
    if (m == "lowwidth")                  return d.lowCorrelation;
    if (m == "phase")                     return d.correlation;
    if (m == "width")                     return d.sideDb;
    if (m == "loudLo" || m == "loudHi")   return d.loudnessLufs;
    return 0.0f;
}

void Advisor::describeRange (Advice& a, float excess, float tol) const
{
    const auto& reg = regionFor (a.focusHz);
    // enough to land comfortably inside the target range, not just on its edge
    const float amount = (float) juce::roundToInt (juce::jlimit (1.5f, 9.0f, excess + 0.5f * tol) * 2.0f) / 2.0f;
    const bool whole = std::abs (amount - std::round (amount)) < 0.01f;
    const auto amt = juce::String (amount, whole ? 0 : 1);
    const bool cut = a.direction > 0;
    a.title = juce::String (reg.name) + (cut ? " too loud" : " too quiet");
    if (a.highHz >= 9000.0f)
        a.action = "EQ: high shelf " + juce::String (cut ? "-" : "+") + amt + " dB from " + hzText (a.lowHz);
    else if (a.lowHz <= 40.0f && cut)
        a.action = "EQ: low shelf -" + amt + " dB below " + hzText (a.highHz) + " (or raise your high-pass a little)";
    else
    {
        const float q = juce::jlimit (0.5f, 4.0f, a.focusHz / juce::jmax (1.0f, a.highHz - a.lowHz));
        a.action = "EQ: " + juce::String (cut ? "cut " : "boost ") + amt + " dB at " + hzText (a.focusHz) + ", Q " + juce::String (q, 1);
    }
    a.why = hzText (a.lowHz) + " - " + hzText (a.highHz) + " is " + juce::String (excess, 1) + " dB "
          + (cut ? "louder" : "quieter") + " than "
          + (target.fromReference ? "in your reference \"" + target.name + "\" (level-matched)" : "the typical range of " + target.name)
          + ". It " + (cut ? reg.tooMuch : reg.tooLittle);
}

void Advisor::addTargetIssues (const LiveListener::Pass& pass, std::vector<Advice>& found)
{
    if (! target.valid || pass.spectrum == nullptr) return;
    const auto& an = *pass.spectrum;
    const auto g = TargetCurve::grid();
    std::vector<float> meas (g.size());
    float top = -400.0f;
    for (size_t i = 0; i < g.size(); ++i) { meas[i] = smoothedLevel (an, g[i]); top = juce::jmax (top, meas[i]); }
    const float floorDb = top - 50.0f;

    // Level-match: same average as the target between 100 Hz and 8 kHz.
    double off = 0.0; int n = 0;
    for (size_t i = 0; i < g.size(); ++i)
        if (g[i] >= 100.0f && g[i] <= 8000.0f && meas[i] > floorDb) { off += meas[i] - target.levelAt (g[i]); ++n; }
    if (n < 5) return;
    off /= n;

    std::vector<float> dev (g.size()), tol (g.size());
    for (size_t i = 0; i < g.size(); ++i) { dev[i] = meas[i] - target.levelAt (g[i]) - (float) off; tol[i] = target.toleranceAt (g[i]); }

    // Excess beyond the tolerance (signed: + too loud, - too quiet), ignoring the extremes.
    std::vector<float> ex (g.size(), 0.0f);
    for (size_t k = 0; k < g.size(); ++k)
    {
        if (g[k] < 30.0f || g[k] > 18000.0f) continue;
        if (dev[k] > tol[k])       ex[k] = dev[k] - tol[k];
        else if (dev[k] < -tol[k]) ex[k] = dev[k] + tol[k];
    }

    // Regions: start at the worst point, grow while the excess stays above 35 % of that peak
    // (max 1.5 octaves), so separate problems (a sub boom and a 250 Hz build-up) stay separate.
    for (int guard = 0; guard < 12; ++guard)
    {
        size_t peak = 0; float best = 0.0f;
        for (size_t k = 0; k < g.size(); ++k) if (std::abs (ex[k]) > best) { best = std::abs (ex[k]); peak = k; }
        if (best <= 0.0f) break;
        const int s = ex[peak] > 0.0f ? 1 : -1;
        size_t lo = peak, hi = peak;
        auto grows = [&] (size_t k) { return (float) s * ex[k] > 0.35f * best; };
        while (hi - lo < 8 && lo > 0 && grows (lo - 1)) --lo;
        while (hi - lo < 8 && hi + 1 < g.size() && grows (hi + 1)) ++hi;
        double sum = 0.0;
        for (size_t k = lo; k <= hi; ++k) { sum += std::abs (ex[k]); ex[k] = 0.0f; }
        const int len = (int) (hi - lo + 1);
        const float excess = (float) (sum / len);
        if (len < 2 && best < 3.0f) continue;   // a lone, small blip isn't worth a suggestion

        Advice a;
        a.kind = Advice::kRange;
        a.direction = s;
        a.lowHz = g[lo] * std::pow (2.0f, -1.0f / 12.0f);
        a.highHz = g[hi] * std::pow (2.0f, 1.0f / 12.0f);
        a.focusHz = g[peak];
        a.severity = excess * std::sqrt ((float) len) + 2.0f;
        a.id = "t:" + juce::String (juce::roundToInt (3.0f * std::log2 (a.focusHz / 20.0f))) + (s > 0 ? "+" : "-");
        double tolSum = 0.0;
        for (size_t k = lo; k <= hi; ++k) tolSum += tol[k];
        describeRange (a, excess, (float) (tolSum / len));
        a.detected = measure (a, pass);
        a.goal = a.detected - (float) s * excess;
        found.push_back (a);
    }
}

void Advisor::addZoneIssues (const LiveListener::Pass& pass, const mm::Guide& guide, std::vector<Advice>& found)
{
    if (pass.spectrum == nullptr) return;
    float ref = 0.0f;
    const auto readings = pass.spectrum->readZones (guide.zones, ref);
    for (size_t z = 0; z < guide.zones.size() && z < readings.size(); ++z)
    {
        const auto& zone = guide.zones[z];
        const auto& r = readings[z];

        if (! target.valid)
        {
            // Build-ups and holes against the neighbouring frequencies (an instrument's own body,
            // the "boost" zones, is meant to stick out, so only a weak body counts there).
            const float limit = zone.kind == "boost" ? 1000.0f : zone.kind == "watch" ? 3.5f : 2.5f;
            const bool tooMuch = r.bumpDb > limit || (r.relativeDb > zone.targetMaxDb + 6.0f && r.bumpDb > 1.0f);
            const bool weakBody = zone.kind == "boost" && r.bumpDb < -5.0f;
            const bool tooLittle = ! tooMuch && (weakBody || r.relativeDb < zone.targetMinDb - 3.0f);
            if (tooMuch || tooLittle)
            {
                Advice a;
                a.kind = Advice::kRange;
                a.direction = tooMuch ? 1 : -1;
                a.lowHz = zone.lowHz; a.highHz = zone.highHz;
                a.focusHz = r.peakHz > 0.0f ? r.peakHz : std::sqrt (zone.lowHz * zone.highHz);
                const float excess = tooMuch ? (r.bumpDb > limit ? r.bumpDb - limit + 0.5f : r.relativeDb - zone.targetMaxDb - 3.0f)
                                             : (weakBody ? -r.bumpDb - 2.0f : zone.targetMinDb - r.relativeDb);
                a.severity = excess + 1.0f;
                a.id = "z:" + zone.label + (tooMuch ? "+" : "-");
                const auto amt = juce::String (juce::roundToInt (juce::jlimit (2.0f, 8.0f, excess + 1.0f)));
                const float q = juce::jlimit (0.5f, 4.0f, a.focusHz / juce::jmax (1.0f, a.highHz - a.lowHz));
                a.title = capitalise (zone.label) + (tooMuch ? ": too much" : ": too weak");
                if (zone.label.containsIgnoreCase ("rumble") || zone.label.containsIgnoreCase ("low cut"))
                    a.action = "High-pass filter: raise it to about " + hzText (zone.highHz) + " (12-24 dB/oct)";
                else if (zone.label.containsIgnoreCase ("sibilance"))
                    a.action = "De-esser at about " + hzText (a.focusHz) + ", 3-6 dB of reduction on the S sounds";
                else if (! tooMuch && zone.tooLittle.containsIgnoreCase ("saturation"))
                    a.action = "Add saturation (or EQ: boost " + amt + " dB at " + hzText (a.focusHz) + ", Q " + juce::String (q, 1) + ")";
                else
                    a.action = "EQ: " + juce::String (tooMuch ? "cut " : "boost ") + amt + " dB at " + hzText (a.focusHz) + ", Q " + juce::String (q, 1);
                const auto& reg = regionFor (a.focusHz);
                a.why = hzText (zone.lowHz) + " - " + hzText (zone.highHz) + " " + (tooMuch ? "sticks out " : "is weak ")
                      + "compared with the frequencies around it. "
                      + (zone.tip.isNotEmpty() ? zone.tip : juce::String ("It ") + (tooMuch ? reg.tooMuch : reg.tooLittle));
                a.detected = measure (a, pass);
                a.goal = a.detected - (float) a.direction * juce::jmax (1.0f, excess);
                found.push_back (a);
            }
        }

        // Narrow resonances that ring in every note
        if (r.resonanceHz > 0.0f && r.resonanceDb >= 12.0f)
        {
            Advice a;
            a.kind = Advice::kResonance;
            a.direction = 1;
            a.focusHz = r.resonanceHz;
            a.lowHz = r.resonanceHz / 1.06f; a.highHz = r.resonanceHz * 1.06f;
            const float amount = juce::jlimit (3.0f, 10.0f, std::round (r.resonanceDb - 6.0f));
            a.severity = r.resonanceDb - 8.0f;
            a.id = "r:" + juce::String (juce::roundToInt (6.0f * std::log2 (r.resonanceHz / 20.0f)));
            a.title = "Ringing at " + hzText (r.resonanceHz);
            a.action = "EQ: narrow cut -" + juce::String (juce::roundToInt (amount)) + " dB at " + hzText (r.resonanceHz) + ", Q 6-8";
            a.why = "One narrow frequency sticks out " + juce::String (juce::roundToInt (r.resonanceDb)) + " dB above its surroundings in "
                    "every note, so it rings and gets tiring. To find it exactly: boost a narrow band a lot, sweep around "
                  + hzText (r.resonanceHz) + " until it whistles the loudest, then turn that boost into a cut.";
            a.detected = measure (a, pass);
            a.goal = a.detected - 0.5f * amount;   // a narrow cut lowers this 1/6-octave measure by part of its depth
            found.push_back (a);
        }
    }
}

void Advisor::addDynamicsIssues (const DynamicsReading& d, std::vector<Advice>& found)
{
    if (! d.valid) return;
    const auto& inst = instrumentId;
    const bool drum = inst == "kick" || inst == "snare" || inst == "hats";
    const bool master = inst == "master";
    const bool sustained = ! drum && ! master;
    const bool centred = inst == "kick" || inst == "snare" || inst == "bass" || inst == "vocal";
    const auto recipe = recipeText (recipeFor (inst));
    auto num = [] (float v, int dec = 1) { return dec == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec); };

    auto add = [&] (float severity, const juce::String& metric, int dir, float value, float limit,
                    const juce::String& title, const juce::String& action, const juce::String& why, const juce::String& unit = "dB")
    {
        Advice a;
        a.kind = Advice::kMetric; a.metric = metric; a.direction = dir;
        a.severity = severity; a.id = "m:" + metric;
        a.title = title; a.action = action; a.why = why; a.unit = unit;
        a.detected = a.value = value; a.goal = limit;
        found.push_back (a);
    };

    if (master ? d.peakDb > -1.0f : (d.clippedSamples > 0 && d.peakDb > -0.1f))
    {
        if (master)
            add (12.0f, "truepeak", 1, d.peakDb, -1.0f, "Peaks above -1 dB", "Limiter: output ceiling -1.0 dB (true peak)",
                 "Converting to MP3/AAC for streaming can push peaks above -1 dB into distortion.");
        else
            add (20.0f, "clip", 1, d.peakDb, -0.5f, "Clipping", "Turn down about " + num (juce::jmax (3.0f, d.peakDb + 4.0f), 0) + " dB before Mix Mentor (or use the trim)",
                 "The track hits 0 dBFS and clips: harsh, crunchy distortion. Aim for peaks around -6 to -3 dB. Want that crunch? Use a clipper plugin on purpose.");
    }

    const float crestMax = master ? (genreId == "ambient" || genreId == "lofi" ? 18.0f : genreId == "pop" ? 14.0f : 13.0f)
                         : inst == "kick" || inst == "snare" ? 20.0f : inst == "hats" ? 22.0f
                         : inst == "bass" ? 15.0f : inst == "vocal" ? 18.0f : 17.0f;
    const float crestMin = master ? 6.0f : inst == "kick" || inst == "snare" ? 7.0f : inst == "bass" ? 3.5f : 5.0f;
    if (d.crestDb > crestMax)
        add (d.crestDb - crestMax + 3.0f, "crestHi", 1, d.crestDb, crestMax,
             master ? "Too dynamic for " + genreName : juce::String ("Peaks stick out"),
             master ? juce::String ("Glue compressor 2:1, then a limiter") : juce::String ("Compressor: ") + recipeFor (inst).ratio + ", attack " + recipeFor (inst).attack,
             (master ? "Peaks stand " + num (d.crestDb) + " dB above the average level; punchy masters usually sit around 8-12 dB. "
                     : "The peaks are " + num (d.crestDb) + " dB above the body of the sound, so it jumps out and then disappears. ") + recipe);
    else if (d.crestDb < crestMin)
        add (crestMin - d.crestDb + 4.0f, "crestLo", -1, d.crestDb, crestMin, "Squashed",
             drum ? juce::String ("Less compression, or attack 30 ms") : juce::String ("Less compression / limiting"),
             "Peaks are only " + num (d.crestDb) + " dB above the average, so it sounds flat. "
             + (drum ? juce::String ("Slow the compressor's attack so the transient gets through, or add attack with a transient shaper.")
                     : juce::String ("Back off the compressor or limiter until it breathes again.")));

    const float swingMax = inst == "bass" ? 6.0f : inst == "vocal" ? 8.0f : 10.0f;
    if (sustained && d.swingDb > swingMax)
        add (d.swingDb - swingMax + 2.0f, "swing", 1, d.swingDb, swingMax, "Uneven level",
             juce::String ("Compressor: ") + recipeFor (inst).ratio + ", attack " + recipeFor (inst).attack + ", release " + recipeFor (inst).release,
             "Some notes are much louder than others (" + num (d.swingDb) + " dB between loud and quiet moments). " + recipe
             + " Then automate single notes that still jump out.");

    if (master)
    {
        const auto t = loudnessTarget (genreId);
        if (d.loudnessLufs < t.getStart() - 1.0f)
            add (t.getStart() - d.loudnessLufs, "loudLo", -1, d.loudnessLufs, t.getStart(), "Quieter than " + genreName,
                 "Limiter: more input gain (ceiling -1 dB)",
                 "Finished " + genreName + " masters usually sit around " + num (t.getStart(), 0) + " to " + num (t.getEnd(), 0)
                 + " LUFS, and the loudest part reads 2-4 LU above that.", "LUFS");
        else if (d.loudnessLufs > t.getEnd() + 3.0f)
            add (d.loudnessLufs - t.getEnd(), "loudHi", 1, d.loudnessLufs, t.getEnd() + 2.0f, "Pushed very loud", "Limiter: less input gain",
                 "Streaming services turn loud masters down anyway, so all that limiting only costs punch.", "LUFS");
    }

    if (d.stereo)
    {
        if (d.correlation < 0.0f)
            add (15.0f, "phase", -1, d.correlation, 0.3f, "Phase problem", "Reduce stereo wideners / unison spread",
                 "Left and right partly cancel each other, so in mono (clubs, phones) this part gets thin or disappears. Check that "
                 "one side isn't polarity-flipped. Aim for a correlation above +0.3.", "");
        else if (d.lowSideDb > -18.0f && d.lowCorrelation < 0.9f)
            add (10.0f, "lowwidth", -1, d.lowCorrelation, 0.95f, "Low end in stereo", "Make everything below 120 Hz mono",
                 "Low end should be mono: stereo lows lose punch in mono and on club systems. Use a stereo imager or a mid/side EQ "
                 "(high-pass the side at 120 Hz). In Studio One: Splitter in frequency mode, Binaural Pan width 0 on the low band.", "");
        if (centred && d.sideDb > -8.0f && d.correlation >= 0.0f)
            add (6.0f, "width", 1, d.sideDb, -14.0f, "Too wide for a centre part", "Pan centre, reduce chorus / wideners",
                 "The " + instrumentName.toLowerCase() + " is normally the centre of the mix. Save the width for chords, pads and effects.");
    }

    // Reference comparison of punch
    if (reference.valid && reference.crestDb > 0.0f && std::abs (d.crestDb - reference.crestDb) > 3.0f && ! master)
    {
        const bool more = d.crestDb > reference.crestDb;
        add (std::abs (d.crestDb - reference.crestDb), more ? "crestHi" : "crestLo", more ? 1 : -1, d.crestDb,
             reference.crestDb + (more ? 1.5f : -1.5f), more ? juce::String ("Spikier than your reference") : juce::String ("Flatter than your reference"),
             more ? juce::String ("More compression") : juce::String ("Less compression"),
             "Your reference has " + num (reference.crestDb) + " dB between peaks and average, yours " + num (d.crestDb) + " dB. " + recipe);
    }
}

void Advisor::scan (const LiveListener::Pass& pass, const mm::Guide* guide)
{
    std::vector<Advice> found;
    addTargetIssues (pass, found);
    if (guide != nullptr) addZoneIssues (pass, *guide, found);
    addDynamicsIssues (pass.dynamics, found);

    // one item per id (keep the most severe)
    std::sort (found.begin(), found.end(), [] (const Advice& a, const Advice& b) { return a.severity > b.severity; });
    std::vector<Advice> unique;
    for (auto& f : found)
    {
        bool dup = false;
        for (auto& u : unique) dup = dup || u.id == f.id;
        if (! dup) unique.push_back (f);
    }

    std::vector<Advice> next;
    for (auto& f : unique)
    {
        Advice* old = nullptr;
        for (auto& e : items) if (e.id == f.id) old = &e;
        if (old != nullptr && old->status == Advice::done)
        {
            // Done stays done unless it clearly got worse again since it was confirmed.
            const float margin = f.unit.isEmpty() ? 0.15f : 2.0f;
            if ((float) f.direction * (f.detected - old->goal) < margin) continue;
        }
        if (old != nullptr && old->checking) f.checking = true;
        f.value = f.detected;
        if ((int) next.size() < 7) next.push_back (f);
    }
    // Keep the done items in the list (shown under "done")
    for (auto& e : items)
        if (e.status == Advice::done)
        {
            bool again = false;
            for (auto& n : next) again = again || n.id == e.id;
            if (! again) next.push_back (e);
        }
    items = next;

    goodNotes.clear();
    bool dynIssue = false, stereoIssue = false, toneIssue = false;
    for (auto& a : items)
        if (a.status == Advice::open)
        {
            if (a.kind != Advice::kMetric) toneIssue = true;
            else if (a.metric == "phase" || a.metric == "lowwidth" || a.metric == "width") stereoIssue = true;
            else dynIssue = true;
        }
    if (! toneIssue)   goodNotes.add (target.valid ? "tonal balance matches " + target.name : juce::String ("frequency balance"));
    if (! dynIssue)    goodNotes.add ("dynamics");
    if (! stereoIssue) goodNotes.add (pass.dynamics.stereo ? "stereo and phase" : "mono (fine)");
    ++scans;
}

void Advisor::updateLive (const LiveListener& listener)
{
    const auto* cur = listener.currentPass();
    const auto* prev = listener.lastPass();
    if (cur == nullptr || prev == nullptr || ! cur->sameBarsAsBefore) return;
    const int b = listener.getCurrentBeat();   // beats [0, b) are complete
    if (b < 1) return;
    for (auto& a : items)
    {
        if (a.status != Advice::open || a.kind == Advice::kMetric) continue;
        const float full = measure (a, *prev);
        const float delta = measure (a, *cur, 0, b) - measure (a, *prev, 0, b);
        a.value = full + delta;
        a.liveThisPass = true;
    }
}

void Advisor::updateAfterPass (const LiveListener::Pass& pass)
{
    for (auto& a : items)
    {
        if (a.status == Advice::done) continue;
        a.value = measure (a, pass);
        a.liveThisPass = false;
        const float slack = a.unit.isEmpty() ? 0.03f : 0.5f;
        a.goalReachedLastPass = a.toGo() <= slack;
        if (a.checking)
        {
            a.checking = false;
            if (a.goalReachedLastPass)
            {
                a.status = Advice::done;
                a.checkMessage = "Confirmed on the last loop";
            }
            else
                a.checkMessage = "Not there yet: " + juce::String (a.toGo(), a.unit.isEmpty() ? 2 : 1) + " " + a.unit + " to go";
        }
    }
}

void Advisor::requestCheck (const juce::String& id)
{
    for (auto& a : items)
        if (a.id == id && a.status == Advice::open) { a.checking = true; a.checkMessage = "Checking on the next loop..."; }
}

void Advisor::reopen (const juce::String& id)
{
    for (auto& a : items)
        if (a.id == id) { a.status = Advice::open; a.checking = false; a.checkMessage = {}; }
}

juce::String Advisor::toXmlString() const
{
    juce::XmlElement x ("Advisor");
    x.setAttribute ("genre", genreId);
    x.setAttribute ("instrument", instrumentId);
    x.setAttribute ("scans", scans);
    x.setAttribute ("good", goodNotes.joinIntoString ("|"));
    if (reference.valid) x.setAttribute ("reference", reference.toString());
    for (auto& a : items)
    {
        auto* e = x.createNewChildElement ("Item");
        e->setAttribute ("id", a.id); e->setAttribute ("kind", a.kind); e->setAttribute ("dir", a.direction);
        e->setAttribute ("title", a.title); e->setAttribute ("action", a.action); e->setAttribute ("why", a.why);
        e->setAttribute ("unit", a.unit); e->setAttribute ("lo", a.lowHz); e->setAttribute ("hi", a.highHz);
        e->setAttribute ("focus", a.focusHz); e->setAttribute ("metric", a.metric); e->setAttribute ("sev", a.severity);
        e->setAttribute ("det", a.detected); e->setAttribute ("goal", a.goal); e->setAttribute ("val", a.value);
        e->setAttribute ("status", a.status); e->setAttribute ("msg", a.checkMessage);
    }
    return x.toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
}

void Advisor::fromXmlString (const juce::String& s)
{
    auto x = juce::parseXML (s);
    if (x == nullptr || ! x->hasTagName ("Advisor")) return;
    items.clear();
    scans = x->getIntAttribute ("scans");
    goodNotes = juce::StringArray::fromTokens (x->getStringAttribute ("good"), "|", "");
    reference = TargetCurve::fromString (x->getStringAttribute ("reference"));
    genreId = x->getStringAttribute ("genre");
    instrumentId = x->getStringAttribute ("instrument");
    for (auto* e : x->getChildWithTagNameIterator ("Item"))
    {
        Advice a;
        a.id = e->getStringAttribute ("id"); a.kind = e->getIntAttribute ("kind"); a.direction = e->getIntAttribute ("dir", 1);
        a.title = e->getStringAttribute ("title"); a.action = e->getStringAttribute ("action"); a.why = e->getStringAttribute ("why");
        a.unit = e->getStringAttribute ("unit", "dB"); a.lowHz = (float) e->getDoubleAttribute ("lo"); a.highHz = (float) e->getDoubleAttribute ("hi");
        a.focusHz = (float) e->getDoubleAttribute ("focus"); a.metric = e->getStringAttribute ("metric"); a.severity = (float) e->getDoubleAttribute ("sev");
        a.detected = (float) e->getDoubleAttribute ("det"); a.goal = (float) e->getDoubleAttribute ("goal"); a.value = (float) e->getDoubleAttribute ("val");
        a.status = e->getIntAttribute ("status"); a.checkMessage = e->getStringAttribute ("msg");
        items.push_back (a);
    }
}
