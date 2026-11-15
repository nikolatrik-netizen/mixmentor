#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "Analyzer.h"
#include "Meter.h"

/** Song position that came with a block of audio (from the audio thread). */
struct BlockInfo
{
    bool hasPosition = false;      // the host reports a song position (false in the standalone app)
    bool playing = true;
    double ppq = 0.0, bpm = 120.0;
    bool looping = false;
    double loopStart = 0.0, loopEnd = 0.0;
    double barLength = 4.0;        // in quarter notes
};

/** Listens continuously and splits what it hears into "passes" over the same bars.

    With a song position (any DAW): it listens to the bars of your loop (up to 8 bars), or to
    4 bars at a time if you're not looping. Every pass gives:
      - a long-term spectrum (Analyzer) and dynamics/stereo readings (LoudnessMeter),
      - the energy in 60 bands (1/6 octave) for every beat.
    Because the per-beat energies of the previous pass are kept, each beat of the current pass
    can be compared with the very same beat (same notes, same drums) of the previous one. That
    shows the effect of a change after one beat, without the music itself getting in the way.

    Without a song position it falls back to 4-second passes (no beat-by-beat comparison). */
class LiveListener
{
public:
    static constexpr int numBands = 60;               // 1/6 octave from 20 Hz
    using Bands = std::array<float, numBands>;        // energy per band (linear power)

    struct Pass
    {
        std::vector<Bands> beats;                      // per beat (or per 0.5 s in time mode)
        std::vector<int> beatFrames;                   // FFT frames that landed in each beat
        std::shared_ptr<Analyzer> spectrum;            // long-term spectrum of the pass
        DynamicsReading dynamics;
        bool sameBarsAsBefore = false;                 // repeats the previous pass's bars (comparable beat by beat)
    };

    void prepare (double sampleRate);
    /** Forget the bars and all passes (start over / settings changed). */
    void reset();
    /** Some audio was lost: drop the pass in progress. */
    void markGap() { inPass = false; current.reset(); analyzer.reset(); lastPpq = -1.0e9; }
    /** Feed audio in order, with the song position of its first sample. */
    void push (const float* left, const float* right, int numSamples, const BlockInfo& info);

    // Status
    bool isMusical() const          { return musical; }
    bool isLooping() const          { return looping; }
    double getRangeStart() const    { return rangeStart; }
    double getRangeEnd() const      { return rangeEnd; }
    double getBarLength() const     { return barLength; }
    int getPassCount() const        { return passCount; }
    int getCurrentBeat() const      { return currentBeat; }
    int getBeatsPerPass() const     { return (int) std::round (musical ? rangeEnd - rangeStart : 8.0); }
    float getPassProgress() const;
    bool isListeningNow() const     { return inPass; }

    /** The last finished pass (nullptr before the first one). */
    const Pass* lastPass() const     { return previous.get(); }
    const Pass* currentPass() const  { return current.get(); }

    /** Called (on the same thread) when a beat / a whole pass has finished. */
    std::function<void()> onBeat, onPassComplete;

    // ---- measurements on band energies -------------------------------------------------
    static float bandHz (int band) { return 20.0f * std::pow (2.0f, (float) band / 6.0f); }

    /** Level of a frequency range against everything far away from it (dB), over the given beats.
        An EQ move on the range changes this 1:1, the music outside the range doesn't. */
    static float rangeVsRest (const std::vector<Bands>& beats, int fromBeat, int toBeat, float lowHz, float highHz);
    /** Level of a narrow band (a resonance) against its surroundings (+-1/2 octave), over beats. */
    static float peakVsSurroundings (const std::vector<Bands>& beats, int fromBeat, int toBeat, float hz);

private:
    void startPass();
    void finishPass (bool complete);
    void addSample (float l, float r, double ppq);
    void analyseFrame();

    double sampleRate = 44100.0;
    bool musical = false, looping = false, inPass = false;
    double rangeStart = -1.0, rangeEnd = -1.0, barLength = 4.0, lastPpq = -1.0e9;
    double loopStartSeen = -1.0, loopEndSeen = -1.0;
    int passCount = 0, currentBeat = 0;
    double timeInPass = 0.0;                     // seconds (time mode)
    double coveredBeats = 0.0;

    std::unique_ptr<Pass> current, previous;
    std::unique_ptr<Analyzer> analyzer;
    LoudnessMeter meter;
    std::vector<MeterFrame> frames;

    // per-beat band analysis: 8192-point frames, hop 2048
    static constexpr int fftOrder = 13, fftSize = 1 << fftOrder, hopSize = 2048;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, ringL, ringR, work;
    std::vector<double> ringPpq;
    int ringFill = 0, sinceHop = 0;
    std::vector<int> bandOfBin;
};
