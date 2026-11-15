#include "Analyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

void Analyzer::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    // About 2.7-2.9 Hz per bin at any sample rate.
    order = sampleRate <= 50000.0 ? 14 : sampleRate <= 100000.0 ? 15 : 16;
    N = 1 << order;
    hop = N / 2;
    fft = std::make_unique<juce::dsp::FFT> (order);

    window.resize ((size_t) N);
    for (int i = 0; i < N; ++i)   // periodic Hann: 50 % overlap weights every sample equally
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) N);

    bufL.assign ((size_t) N, 0.0f);
    bufR.assign ((size_t) N, 0.0f);
    work.assign ((size_t) N * 2, 0.0f);
    reset();
}

void Analyzer::reset()
{
    capturing = finished = hasSnapshot = false;
    heardSeconds = 0.0;
    fill = 0;
    frameSpectra.clear();
    frameLoudnessDb.clear();
    power.clear();
    gridHz.clear();
    gridDb.clear();
}

void Analyzer::startCapture (float seconds)
{
    if (fft == nullptr) prepare (sampleRate);
    reset();
    targetSeconds = seconds;
    capturing = true;
}

void Analyzer::push (const float* left, const float* right, int numSamples)
{
    if (! capturing || fft == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        bufL[(size_t) fill] = left[i];
        bufR[(size_t) fill] = right != nullptr ? right[i] : left[i];
        if (++fill == N)
        {
            processFrame();
            if (! capturing) return;
            // keep the second half: 50 % overlap
            std::copy (bufL.begin() + hop, bufL.end(), bufL.begin());
            std::copy (bufR.begin() + hop, bufR.end(), bufR.begin());
            fill = N - hop;
        }
    }
}

void Analyzer::processFrame()
{
    // Level of the new part of the frame (what this frame adds), for gating.
    double ms = 0.0;
    for (int i = N - hop; i < N; ++i)
        ms += 0.5 * ((double) bufL[(size_t) i] * bufL[(size_t) i] + (double) bufR[(size_t) i] * bufR[(size_t) i]);
    ms /= hop;
    const float frameDb = ms > 1.0e-12 ? (float) (10.0 * std::log10 (ms)) : -120.0f;
    if (frameDb < -65.0f)
        return;   // silence: doesn't count and isn't analysed

    std::vector<float> spec ((size_t) N / 2, 0.0f);
    // Amplitude scale so a full-scale sine reads 0 dB at its peak bin (Hann halves the amplitude).
    const float norm = 4.0f / (float) N;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& src = ch == 0 ? bufL : bufR;
        for (int i = 0; i < N; ++i) work[(size_t) i] = src[(size_t) i] * window[(size_t) i];
        std::fill (work.begin() + N, work.end(), 0.0f);
        fft->performRealOnlyForwardTransform (work.data(), true);
        for (int b = 0; b < N / 2; ++b)
        {
            const float re = work[(size_t) (2 * b)] * norm, im = work[(size_t) (2 * b + 1)] * norm;
            spec[(size_t) b] += 0.5f * (re * re + im * im);   // average of the two channels' power
        }
    }

    frameSpectra.push_back (std::move (spec));
    frameLoudnessDb.push_back (frameDb);
    heardSeconds += hop / sampleRate;

    if (heardSeconds >= targetSeconds)
        finish();
}

void Analyzer::finish()
{
    capturing = false;
    if (frameSpectra.empty())
        return;

    // Leave out quiet frames (tails, gaps): more than 20 dB below the loudest frame.
    const float loudest = *std::max_element (frameLoudnessDb.begin(), frameLoudnessDb.end());
    power.assign ((size_t) N / 2, 0.0);
    int used = 0;
    for (size_t f = 0; f < frameSpectra.size(); ++f)
    {
        if (frameLoudnessDb[f] < loudest - 20.0f) continue;
        for (size_t b = 0; b < power.size(); ++b) power[b] += frameSpectra[f][b];
        ++used;
    }
    for (auto& p : power) p /= juce::jmax (1, used);

    // 1/24-octave bands from 10 Hz to Nyquist: every bin inside a band is averaged.
    gridHz.clear(); gridDb.clear();
    const float nyq = (float) sampleRate * 0.5f;
    for (float f = 10.0f; f < nyq * 0.98f; f *= std::pow (2.0f, 1.0f / 24.0f))
    {
        const float lo = f * std::pow (2.0f, -1.0f / 48.0f), hi = f * std::pow (2.0f, 1.0f / 48.0f);
        const int b0 = (int) std::ceil (lo / binHz()), b1 = (int) std::floor (hi / binHz());
        double p = 0.0;
        if (b1 >= b0 && b0 >= 1)
        {
            for (int b = b0; b <= b1 && b < (int) power.size(); ++b) p += power[(size_t) b];
            p /= (b1 - b0 + 1);
        }
        else
        {
            const float pos = f / binHz();
            const int i0 = juce::jlimit (1, (int) power.size() - 2, (int) pos);
            const double frac = juce::jlimit (0.0, 1.0, (double) pos - i0);
            p = power[(size_t) i0] * (1.0 - frac) + power[(size_t) i0 + 1] * frac;
        }
        gridHz.push_back (f);
        gridDb.push_back ((float) (10.0 * std::log10 (p + 1.0e-20)));
    }

    hasSnapshot = true;
    finished = true;
}

float Analyzer::levelAt (float hz) const
{
    if (gridHz.empty()) return -200.0f;
    const float pos = (float) (std::log2 (juce::jmax (hz, 10.0f) / 10.0f) * 24.0);
    const int i0 = juce::jlimit (0, (int) gridDb.size() - 1, (int) pos);
    const int i1 = juce::jmin ((int) gridDb.size() - 1, i0 + 1);
    const float frac = juce::jlimit (0.0f, 1.0f, pos - (float) i0);
    return gridDb[(size_t) i0] * (1.0f - frac) + gridDb[(size_t) i1] * frac;
}

std::vector<float> Analyzer::thirdOctaveBands() const
{
    std::vector<float> out;
    if (! hasSnapshot) return out;
    for (int k = 0; k < 31; ++k)
    {
        const float centre = 20.0f * std::pow (2.0f, (float) k / 3.0f);
        double acc = 0.0; int n = 0;
        for (size_t i = 0; i < gridHz.size(); ++i)
            if (gridHz[i] >= centre * std::pow (2.0f, -1.0f / 6.0f) && gridHz[i] < centre * std::pow (2.0f, 1.0f / 6.0f))
            { acc += std::pow (10.0, gridDb[i] / 10.0); ++n; }
        out.push_back (n > 0 ? (float) (10.0 * std::log10 (acc / n + 1.0e-20)) : -200.0f);
    }
    return out;
}

float Analyzer::findResonance (float lowHz, float highHz, float& resonanceDb) const
{
    // A resonance is a narrow peak that rings at the same frequency all the time, sticking out
    // of the surrounding third-octave. Musical notes are not resonances: their harmonics form a
    // series (f, 2f, 3f...) and move when the notes change, so those are skipped.
    resonanceDb = 0.0f;
    const int b0 = juce::jmax (2, (int) std::ceil (lowHz / binHz()));
    const int b1 = juce::jmin ((int) power.size() - 3, (int) std::floor (highHz / binHz()));
    if (b1 - b0 < 8) return 0.0f;

    auto dbOf = [] (double p) { return (float) (10.0 * std::log10 (p + 1.0e-20)); };

    // Local reference: median of the bins within +-1/6 octave (excluding the peak itself).
    auto localMedian = [&] (const std::vector<float>& spec, int bin)
    {
        const int lo = juce::jmax (1, (int) (bin * 0.891f)), hi = juce::jmin ((int) spec.size() - 1, (int) (bin * 1.122f));
        std::vector<float> v;
        for (int b = lo; b <= hi; ++b) if (std::abs (b - bin) > 2) v.push_back (spec[(size_t) b]);
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    };

    // Strongest local maximum in the zone, measured against its surroundings.
    std::vector<float> avg (power.begin(), power.end());
    float bestRise = 0.0f; int bestBin = -1;
    for (int b = b0; b <= b1; ++b)
    {
        if (avg[(size_t) b] < avg[(size_t) b - 1] || avg[(size_t) b] < avg[(size_t) b + 1]) continue;
        const float rise = dbOf (avg[(size_t) b]) - dbOf (localMedian (avg, b));
        if (rise > bestRise) { bestRise = rise; bestBin = b; }
    }
    if (bestBin < 0 || bestRise < 10.0f) return 0.0f;
    const float peakHz = bestBin * binHz();

    // Part of a harmonic series? Look for a fundamental f0 = peak / k (k = 1..6) with at least
    // three other strong partials at multiples of f0.
    for (int k = 1; k <= 6; ++k)
    {
        const float f0 = peakHz / (float) k;
        if (f0 < 30.0f) break;
        int partials = 0;
        for (int m = 1; m <= 8; ++m)
        {
            if (m == k) continue;
            const int b = (int) std::round (f0 * m / binHz());
            if (b < 3 || b >= (int) avg.size() - 3) continue;
            float best = 0.0f;
            for (int d = -2; d <= 2; ++d) best = juce::jmax (best, avg[(size_t) (b + d)]);
            if (dbOf (best) - dbOf (localMedian (avg, b)) >= 8.0f) ++partials;
        }
        if (partials >= 3) return 0.0f;   // it's a note's harmonic, not a resonance
    }

    // It must ring in (nearly) every frame, not just in a few loud notes.
    int present = 0, counted = 0;
    for (size_t f = 0; f < frameSpectra.size(); ++f)
    {
        const auto& s = frameSpectra[f];
        float best = 0.0f;
        for (int d = -1; d <= 1; ++d) best = juce::jmax (best, s[(size_t) (bestBin + d)]);
        ++counted;
        if (dbOf (best) - dbOf (localMedian (s, bestBin)) >= 6.0f) ++present;
    }
    if (counted == 0 || present < counted * 0.75f) return 0.0f;

    resonanceDb = bestRise;
    return peakHz;
}

std::vector<ZoneReading> Analyzer::readZones (const std::vector<mm::Zone>& zones, float& referenceDb) const
{
    std::vector<ZoneReading> readings (zones.size());
    if (! hasSnapshot)
        return readings;

    // Tonal balance is judged with 1/6-octave smoothing (power average over +-1/12 octave), the
    // resolution tonal-balance tools use: single harmonics don't count as build-ups. Resonances
    // are found separately at full resolution. Display scale: +3 dB/oct tilt, so a balanced mix
    // reads roughly flat.
    auto tilted = [this] (float hz)
    {
        double acc = 0.0;
        for (int j = -2; j <= 2; ++j)
            acc += std::pow (10.0, levelAt (hz * std::pow (2.0f, (float) j / 24.0f)) / 10.0);
        return (float) (10.0 * std::log10 (acc / 5.0 + 1.0e-20)) + tiltDb (hz);
    };

    // The track's overall level (30 Hz - 16 kHz) is the reference every zone is compared with.
    // Near-silent parts of the spectrum are clamped to a floor 45 dB below the loudest part,
    // so they don't drag the reference down.
    constexpr int refPoints = 96;
    float spectrumMax = -400.0f;
    std::vector<float> refLevels;
    for (int i = 0; i < refPoints; ++i)
    {
        const float db = tilted (30.0f * std::pow (16000.0f / 30.0f, (float) i / (float) (refPoints - 1)));
        refLevels.push_back (db);
        spectrumMax = juce::jmax (spectrumMax, db);
    }
    const float floorDb = spectrumMax - 45.0f;
    double sum = 0.0;
    for (float db : refLevels) sum += juce::jmax (floorDb, db);
    referenceDb = (float) (sum / refPoints);

    // Common 1/24-octave grid (display scale, floored)
    constexpr int perOctave = 24;
    const float gridLo = 20.0f, gridHi = juce::jmin (20000.0f, (float) sampleRate * 0.49f);
    const int gridN = (int) (std::log2 (gridHi / gridLo) * perOctave) + 1;
    std::vector<float> gHz ((size_t) gridN), level ((size_t) gridN);
    for (int k = 0; k < gridN; ++k)
    {
        gHz[(size_t) k] = gridLo * std::pow (2.0f, (float) k / perOctave);
        level[(size_t) k] = juce::jmax (floorDb, tilted (gHz[(size_t) k]));
    }
    auto meanBetween = [&] (float lo, float hi, int& count)
    {
        double acc = 0.0; count = 0;
        for (int k = 0; k < gridN; ++k)
            if (gHz[(size_t) k] >= lo && gHz[(size_t) k] <= hi) { acc += level[(size_t) k]; ++count; }
        return count > 0 ? (float) (acc / count) : 0.0f;
    };

    for (size_t z = 0; z < zones.size(); ++z)
    {
        const auto& zone = zones[z];
        auto& r = readings[z];

        // Average level and loudest point in the zone
        int n = 0;
        r.levelDb = meanBetween (zone.lowHz, zone.highHz, n);
        float peakDb = -1000.0f;
        for (int k = 0; k < gridN; ++k)
            if (gHz[(size_t) k] >= zone.lowHz && gHz[(size_t) k] <= zone.highHz && level[(size_t) k] > peakDb)
            { peakDb = level[(size_t) k]; r.peakHz = gHz[(size_t) k]; }
        r.relativeDb = r.levelDb - referenceDb;

        // Bump: the zone against a straight line joining the half-octave just below and just above it.
        {
            const float halfOct = std::sqrt (2.0f);
            int nb = 0, na = 0;
            const float below = meanBetween (zone.lowHz / halfOct, zone.lowHz * 0.97f, nb);
            const float above = meanBetween (zone.highHz * 1.03f, juce::jmin (gridHi, zone.highHz * halfOct), na);
            const float xb = std::log2 (zone.lowHz / std::pow (halfOct, 0.5f));
            const float xa = std::log2 (zone.highHz * std::pow (halfOct, 0.5f));
            double bumpSum = 0.0; int bumpCount = 0;
            for (int k = 0; k < gridN; ++k)
            {
                const float f = gHz[(size_t) k];
                if (f < zone.lowHz || f > zone.highHz) continue;
                float line;
                if (nb > 0 && na > 0)  line = below + (above - below) * (std::log2 (f) - xb) / (xa - xb);
                else if (nb > 0)       line = below;
                else if (na > 0)       line = above;
                else                   line = level[(size_t) k];
                bumpSum += level[(size_t) k] - line;
                ++bumpCount;
            }
            r.bumpDb = bumpCount > 0 ? (float) (bumpSum / bumpCount) : 0.0f;
        }

        // The zone against everything at least an octave away (an EQ move on the zone shows up 1:1 here).
        {
            double acc = 0.0; int count = 0;
            for (int k = 0; k < gridN; ++k)
            {
                const float f = gHz[(size_t) k];
                if (f < 30.0f || f > 16000.0f || (f > zone.lowHz * 0.5f && f < zone.highHz * 2.0f)) continue;
                acc += level[(size_t) k]; ++count;
            }
            r.isolatedDb = count > 0 ? r.levelDb - (float) (acc / count) : r.relativeDb;
        }

        if (r.relativeDb > zone.targetMaxDb)        { r.status = 1;  r.deviationDb = r.relativeDb - zone.targetMaxDb; }
        else if (r.relativeDb < zone.targetMinDb)   { r.status = -1; r.deviationDb = r.relativeDb - zone.targetMinDb; }

        // Resonances: not in boost zones (a peak there is usually wanted) and not in the deep lows.
        if (zone.kind != "boost" && zone.highHz >= 120.0f)
        {
            float rise = 0.0f;
            const float hz = findResonance (juce::jmax (zone.lowHz, 100.0f), zone.highHz, rise);
            if (hz > 0.0f) { r.resonanceHz = hz; r.resonanceDb = rise; }
        }
    }
    return readings;
}
