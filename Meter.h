#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

/** One 50 ms slice of meter data, from the audio thread to the editor. */
struct MeterFrame
{
    float msK = 0, peak = 0, ms = 0;          // K-weighted energy (sum of channels), true peak, plain energy (channel average)
    float ll = 0, rr = 0, lr = 0, lowLL = 0, lowRR = 0, lowLR = 0;
    int n = 0, clips = 0;
};

/** Dynamics, loudness and stereo measured over the analysed loop. */
struct DynamicsReading
{
    bool valid = false;
    float peakDb = -100.0f;         // true peak (4x oversampled), dBTP
    float rmsDb = -100.0f;          // average level while playing
    float crestDb = 0.0f;           // peak minus average: punch vs. density
    float loudnessLufs = -100.0f;   // integrated loudness of the loop (ITU BS.1770 with gating)
    float swingDb = 0.0f;           // how much the level jumps around between loud and quiet moments
    float correlation = 1.0f;       // +1 mono, 0 very wide, below 0 = phase problems
    float lowCorrelation = 1.0f;    // the same, below ~120 Hz
    float sideDb = -100.0f;         // side vs. mid energy (width)
    float lowSideDb = -100.0f;      // the same, below ~120 Hz
    bool stereo = false;            // the track has two different channels
    int clippedSamples = 0;
};

/** Measures on the audio thread: ITU BS.1770 K-weighting, 4x oversampled true peak,
    stereo correlation (full band and below 120 Hz). Produces 50 ms MeterFrames. */
class LoudnessMeter
{
public:
    void prepare (double sampleRate);
    /** Measures a block; calls emit (const MeterFrame&) every 50 ms. */
    template <typename Emit>
    void process (const float* L, const float* R, int numSamples, Emit&& emit)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float l = L[i], r = R[i];
            const float kl = kHighPass[0].processSingleSampleRaw (kShelf[0].processSingleSampleRaw (l));
            const float kr = kHighPass[1].processSingleSampleRaw (kShelf[1].processSingleSampleRaw (r));
            const float lowL = lowB[0].processSingleSampleRaw (lowA[0].processSingleSampleRaw (l));
            const float lowR = lowB[1].processSingleSampleRaw (lowA[1].processSingleSampleRaw (r));
            auto& f = current;
            f.msK += kl * kl + kr * kr;
            f.ms += 0.5f * (l * l + r * r);
            f.peak = juce::jmax (f.peak, truePeak (0, l), truePeak (1, r));
            if (std::abs (l) >= 0.999f || std::abs (r) >= 0.999f) ++f.clips;
            f.ll += l * l; f.rr += r * r; f.lr += l * r;
            f.lowLL += lowL * lowL; f.lowRR += lowR * lowR; f.lowLR += lowL * lowR;
            if (++f.n >= frameLength) { emit (f); f = {}; }
        }
    }

    /** Drops a half-filled frame (a new capture starts). */
    void startNewCapture() { current = {}; }

    /** Turns the frames of an analysed loop into readings. */
    static DynamicsReading compute (const std::vector<MeterFrame>& frames);

private:
    float truePeak (int ch, float x);

    static constexpr int tapsPerPhase = 12, phases = 4;
    std::array<std::array<float, tapsPerPhase>, phases> coeffs {};
    std::array<std::array<float, tapsPerPhase * 2>, 2> hist {};
    std::array<int, 2> histPos {};

    juce::IIRFilter kShelf[2], kHighPass[2], lowA[2], lowB[2];
    MeterFrame current;
    int frameLength = 2205;
};
