#include "Meter.h"
#include <algorithm>
#include <cmath>

void LoudnessMeter::prepare (double sampleRate)
{
    frameLength = juce::jmax (256, (int) (sampleRate * 0.05));
    for (int ch = 0; ch < 2; ++ch)
    {
        // K-weighting, ITU-R BS.1770: +4 dB high shelf at ~1.68 kHz, then a ~38 Hz high-pass.
        kShelf[ch].setCoefficients (juce::IIRCoefficients::makeHighShelf (sampleRate, 1681.974450955533, 0.7071752369554196,
                                                                         juce::Decibels::decibelsToGain (3.999843853973347)));
        kHighPass[ch].setCoefficients (juce::IIRCoefficients::makeHighPass (sampleRate, 38.13547087602444, 0.5003270373238773));
        lowA[ch].setCoefficients (juce::IIRCoefficients::makeLowPass (sampleRate, 120.0, 0.7071));
        lowB[ch].setCoefficients (juce::IIRCoefficients::makeLowPass (sampleRate, 120.0, 0.7071));
        kShelf[ch].reset(); kHighPass[ch].reset(); lowA[ch].reset(); lowB[ch].reset();
    }

    // True peak: 4x oversampling with a 48-tap windowed-sinc interpolator (as in BS.1770 annex 2).
    // Above 96 kHz the signal is already oversampled enough.
    const int total = tapsPerPhase * phases;
    for (int p = 0; p < phases; ++p)
        for (int t = 0; t < tapsPerPhase; ++t)
        {
            const int k = t * phases + p;                       // index in the full filter
            const double x = (k - (total - 1) / 2.0) / phases;   // in input samples
            const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
            const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (k + 0.5) / total);
            coeffs[(size_t) p][(size_t) t] = (float) (sinc * w);
        }
    // Normalise every phase to unity gain at DC.
    for (auto& ph : coeffs)
    {
        float s = 0.0f;
        for (float c : ph) s += c;
        for (auto& c : ph) c /= s;
    }
    for (auto& h : hist) h.fill (0.0f);
    histPos = { 0, 0 };
    current = {};
}

float LoudnessMeter::truePeak (int ch, float x)
{
    auto& h = hist[(size_t) ch];
    auto& pos = histPos[(size_t) ch];
    // Write twice so the last 12 samples are always contiguous.
    h[(size_t) pos] = x;
    h[(size_t) (pos + tapsPerPhase)] = x;
    pos = (pos + 1) % tapsPerPhase;

    float peak = std::abs (x);
    for (int p = 0; p < phases; ++p)
    {
        float acc = 0.0f;
        for (int t = 0; t < tapsPerPhase; ++t)
            acc += coeffs[(size_t) p][(size_t) t] * h[(size_t) (pos + tapsPerPhase - 1 - t)];
        peak = juce::jmax (peak, std::abs (acc));
    }
    return peak;
}

DynamicsReading LoudnessMeter::compute (const std::vector<MeterFrame>& frames)
{
    DynamicsReading d;
    if (frames.size() < 10)
        return d;

    auto toDb = [] (double power) { return power > 1.0e-12 ? (float) (10.0 * std::log10 (power)) : -120.0f; };

    // Loudness: 400 ms blocks every 100 ms, absolute gate -70 LUFS, relative gate -10 LU (BS.1770-4).
    std::vector<double> blocks;
    for (size_t start = 0; start + 8 <= frames.size(); start += 2)
    {
        double e = 0.0; long long n = 0;
        for (size_t k = start; k < start + 8; ++k) { e += frames[k].msK; n += frames[k].n; }
        blocks.push_back (e / (double) juce::jmax (1LL, n));
    }
    auto lufsOf = [] (double e) { return -0.691 + 10.0 * std::log10 (e + 1.0e-20); };
    std::vector<double> gated;
    for (double e : blocks) if (lufsOf (e) > -70.0) gated.push_back (e);
    if (! gated.empty())
    {
        double mean = 0.0;
        for (double e : gated) mean += e;
        mean /= (double) gated.size();
        const double relGate = lufsOf (mean) - 10.0;
        double sum = 0.0; int n = 0;
        for (double e : gated) if (lufsOf (e) > relGate) { sum += e; ++n; }
        d.loudnessLufs = n > 0 ? (float) lufsOf (sum / n) : (float) lufsOf (mean);
    }

    // Peak, RMS, stereo
    double ms = 0, ll = 0, rr = 0, lr = 0, lowLL = 0, lowRR = 0, lowLR = 0;
    long long n = 0;
    float peak = 0.0f;
    std::vector<float> frameDb;
    for (auto& f : frames)
    {
        ms += f.ms; ll += f.ll; rr += f.rr; lr += f.lr;
        lowLL += f.lowLL; lowRR += f.lowRR; lowLR += f.lowLR;
        n += f.n; peak = juce::jmax (peak, f.peak); d.clippedSamples += f.clips;
        frameDb.push_back (toDb (f.ms / juce::jmax (1, f.n)));
    }
    d.peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    d.rmsDb = toDb (ms / (double) n);
    d.crestDb = d.peakDb - d.rmsDb;

    // Level swings: loud vs. quiet moments while playing (frames far below the loudest are tails/gaps).
    std::sort (frameDb.begin(), frameDb.end());
    const float top = frameDb.back();
    std::vector<float> body;
    for (float x : frameDb) if (x > top - 24.0f) body.push_back (x);
    if (body.size() >= 8)
        d.swingDb = body[(size_t) (body.size() * 0.9)] - body[(size_t) (body.size() * 0.2)];

    const double denom = std::sqrt (ll * rr), lowDenom = std::sqrt (lowLL * lowRR);
    d.stereo = std::abs (ll - rr) > 1.0e-6 * (ll + rr) || std::abs (lr - denom) > 1.0e-4 * denom;
    d.correlation = denom > 1.0e-12 ? (float) (lr / denom) : 1.0f;
    d.lowCorrelation = lowDenom > 1.0e-12 ? (float) (lowLR / lowDenom) : 1.0f;
    d.sideDb = toDb ((ll + rr - 2 * lr) / 4.0) - toDb ((ll + rr + 2 * lr) / 4.0);
    // Only judge low-end width if there is real low end (more than ~5 % of the energy).
    d.lowSideDb = (lowLL + lowRR) > 0.05 * (ll + rr)
                      ? toDb ((lowLL + lowRR - 2 * lowLR) / 4.0) - toDb ((lowLL + lowRR + 2 * lowLR) / 4.0)
                      : -100.0f;
    d.valid = true;
    return d;
}
