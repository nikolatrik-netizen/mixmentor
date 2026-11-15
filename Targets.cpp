#include "Targets.h"
#include "Analyzer.h"
#include "Meter.h"
#include <algorithm>
#include <cmath>

std::vector<float> TargetCurve::grid()
{
    std::vector<float> g;
    for (float f = 20.0f; f <= 20000.0f * 1.001f; f *= std::pow (2.0f, 1.0f / 6.0f))
        g.push_back (f);
    return g;
}

static float interp (const std::vector<float>& xs, const std::vector<float>& ys, float x)
{
    if (xs.empty()) return 0.0f;
    if (x <= xs.front()) return ys.front();
    if (x >= xs.back()) return ys.back();
    const auto it = std::upper_bound (xs.begin(), xs.end(), x);
    const size_t i = (size_t) (it - xs.begin());
    const float t = std::log2 (x / xs[i - 1]) / std::log2 (xs[i] / xs[i - 1]);
    return ys[i - 1] + (ys[i] - ys[i - 1]) * t;
}

float TargetCurve::levelAt (float f) const     { return interp (hz, db, f); }
float TargetCurve::toleranceAt (float f) const { return interp (hz, tolerance, f); }

TargetCurve TargetCurve::forGenre (const juce::String& id, const juce::String& genreName)
{
    // Per-genre numbers. slope = log-log magnitude slope (Pestana et al. Table 2), converted to
    // dB/octave of power per FFT bin: 20*log10(2^slope) = 6.02 * slope.
    struct G { float slope, extraHighDbOct, lowCutHz, lowBoostDb, tol; };
    G g { -0.943f, -2.0f, 60.0f, 0.0f, 3.0f };                              // pop
    if (id == "hiphop")                                    g = { -0.814f, -1.0f, 35.0f, 3.0f, 3.5f };
    else if (id == "house" || id == "techno" || id == "trance" || id == "dnb"
             || id == "dubstep" || id == "futurebass" || id == "ukg") g = { -0.746f, -1.5f, 38.0f, 2.0f, 3.5f };
    else if (id == "lofi")                                 g = { -1.033f, -3.5f, 50.0f, 1.0f, 3.5f };   // R&B/soul-like, softer top
    else if (id == "ambient")                              g = { -1.03f, -2.5f, 45.0f, 0.0f, 4.0f };

    TargetCurve t;
    t.valid = true;
    t.name = genreName + " (genre average)";
    const float dbPerOct = 6.02f * g.slope;
    for (float f : grid())
    {
        const float oct = std::log2 (f / 1000.0f);
        float v = dbPerOct * oct;
        if (f > 4000.0f) v += g.extraHighDbOct * std::log2 (f / 4000.0f);          // steeper top
        if (f < 100.0f)                                                              // low end
        {
            // flatten below 100 Hz (a "bump" of extra low end for bass-heavy genres) ...
            v = dbPerOct * std::log2 (100.0f / 1000.0f) + g.lowBoostDb * std::min (1.0f, std::log2 (100.0f / f));
            // ... then the low cut: 12 dB/octave below it
            if (f < g.lowCutHz) v -= 12.0f * std::log2 (g.lowCutHz / f);
        }
        t.hz.push_back (f);
        t.db.push_back (v);
        // the extremes vary more between songs
        const float edge = (f < 60.0f || f > 12000.0f) ? 1.5f : 0.0f;
        t.tolerance.push_back (g.tol + edge);
    }
    return t;
}

TargetCurve TargetCurve::fromAudioFile (const juce::File& file)
{
    TargetCurve t;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (file));
    if (rd == nullptr || rd->lengthInSamples < (juce::int64) (rd->sampleRate * 2.0))
        return t;

    const double sr = rd->sampleRate;
    const int maxLen = (int) juce::jmin (rd->lengthInSamples, (juce::int64) (sr * 240.0));
    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, rd->numChannels), maxLen);
    rd->read (&buf, 0, maxLen, 0, true, true);
    const float* L = buf.getReadPointer (0);
    const float* R = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);

    // Use the loudest ~90 s (choruses/drops are what you compare against), in 1 s chunks.
    const int chunk = (int) sr;
    std::vector<std::pair<double, int>> loud;
    for (int s = 0; s + chunk <= maxLen; s += chunk)
    {
        double e = 0.0;
        for (int i = s; i < s + chunk; ++i) e += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]);
        loud.push_back ({ e, s });
    }
    std::sort (loud.begin(), loud.end(), [] (auto& a, auto& b) { return a.first > b.first; });
    const size_t keep = juce::jmin (loud.size(), (size_t) 90);
    std::vector<int> starts;
    for (size_t k = 0; k < keep; ++k) starts.push_back (loud[k].second);
    std::sort (starts.begin(), starts.end());

    Analyzer an;
    an.prepare (sr);
    an.startCapture (1.0e6f);
    LoudnessMeter meter;
    meter.prepare (sr);
    std::vector<MeterFrame> frames;
    for (int s : starts)
    {
        an.markGap();
        an.push (L + s, R + s, chunk);
        meter.process (L + s, R + s, chunk, [&frames] (const MeterFrame& f) { frames.push_back (f); });
    }
    an.finishNow();
    if (! an.hasResult())
        return t;

    const auto dyn = LoudnessMeter::compute (frames);
    t.valid = true;
    t.fromReference = true;
    t.name = file.getFileNameWithoutExtension();
    t.loudnessLufs = dyn.loudnessLufs;
    t.crestDb = dyn.crestDb;
    t.correlation = dyn.correlation;
    for (float f : grid())
    {
        // 1/6-octave smoothed (power average over +-1/12 octave)
        double acc = 0.0;
        for (int j = -2; j <= 2; ++j) acc += std::pow (10.0, an.levelAt (f * std::pow (2.0f, (float) j / 24.0f)) / 10.0);
        t.hz.push_back (f);
        t.db.push_back ((float) (10.0 * std::log10 (acc / 5.0 + 1.0e-20)));
        t.tolerance.push_back ((f < 50.0f || f > 14000.0f) ? 3.5f : 2.0f);
    }
    return t;
}

juce::String TargetCurve::toString() const
{
    if (! valid) return {};
    juce::StringArray parts;
    parts.add (name.replaceCharacter ('|', '/'));
    parts.add (fromReference ? "1" : "0");
    parts.add (juce::String (loudnessLufs, 2));
    parts.add (juce::String (crestDb, 2));
    parts.add (juce::String (correlation, 3));
    juce::StringArray d, tol;
    for (size_t i = 0; i < db.size(); ++i) { d.add (juce::String (db[i], 2)); tol.add (juce::String (tolerance[i], 2)); }
    parts.add (d.joinIntoString (" "));
    parts.add (tol.joinIntoString (" "));
    return parts.joinIntoString ("|");
}

TargetCurve TargetCurve::fromString (const juce::String& s)
{
    TargetCurve t;
    auto parts = juce::StringArray::fromTokens (s, "|", "");
    if (parts.size() < 7) return t;
    const auto g = grid();
    auto d = juce::StringArray::fromTokens (parts[5], " ", "");
    auto tol = juce::StringArray::fromTokens (parts[6], " ", "");
    if ((size_t) d.size() != g.size() || (size_t) tol.size() != g.size()) return t;
    t.name = parts[0];
    t.fromReference = parts[1] == "1";
    t.loudnessLufs = parts[2].getFloatValue();
    t.crestDb = parts[3].getFloatValue();
    t.correlation = parts[4].getFloatValue();
    t.hz = g;
    for (int i = 0; i < d.size(); ++i) { t.db.push_back (d[i].getFloatValue()); t.tolerance.push_back (tol[i].getFloatValue()); }
    t.valid = true;
    return t;
}
