#pragma once

#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>
#include "KnowledgeBase.h"

/** What the analysis measured inside one frequency zone. */
struct ZoneReading
{
    float levelDb = 0.0f;       // average level in the zone (display scale)
    float relativeDb = 0.0f;    // level compared with the track's overall average
    float deviationDb = 0.0f;   // >0 = this much over target, <0 = this much under, 0 = in range
    float peakHz = 0.0f;        // loudest frequency inside the zone
    float resonanceHz = 0.0f;   // narrow ringing peak, 0 if none
    float resonanceDb = 0.0f;   // how far that peak sticks out
    float bumpDb = 0.0f;        // how far the zone sticks out (+) or dips (-) compared with its neighbours
    float isolatedDb = 0.0f;    // zone level minus the track's level far away from the zone (an EQ cut here lowers this 1:1)
    int status = 0;             // +1 too much, -1 too little, 0 OK
};

/** The listening engine behind "analyse". No GUI: it can be tested on audio files.

    - Every sample is analysed (50 % overlapping Hann frames), timed in audio seconds, so it
      doesn't depend on how often the plugin window redraws.
    - High resolution (about 2.7 Hz per FFT bin) so the low end is measured properly.
    - Left and right are measured separately and their power added, so wide or out-of-phase
      parts don't cancel out the way they do in a mono sum.
    - Silence and quiet tails are left out (frames more than 20 dB below the loudest one),
      like the gating in a loudness meter.
    - Levels are read in 1/24-octave bands that average every FFT bin inside them. */
class Analyzer
{
public:
    void prepare (double sampleRate);
    double getSampleRate() const { return sampleRate; }

    void startCapture (float seconds);
    void reset();
    /** Message thread: feed audio (in order). Only used while capturing. */
    void push (const float* left, const float* right, int numSamples);
    /** Audio was lost (e.g. the window was frozen): drop the half-filled frame. */
    void markGap() { fill = 0; }

    bool isCapturing() const { return capturing; }
    float getProgress() const { return targetSeconds > 0 ? (float) juce::jmin (1.0, heardSeconds / targetSeconds) : 0.0f; }
    bool hasHeardSound() const { return heardSeconds > 0.0; }
    bool takeFinished() { const bool f = finished; finished = false; return f; }
    bool hasResult() const { return hasSnapshot; }

    /** Level (dB, display scale: a full-scale sine reads 0 dB) at a frequency, 1/24-octave smoothed. */
    float levelAt (float hz) const;
    /** 31 third-octave band levels from 20 Hz. */
    std::vector<float> thirdOctaveBands() const;

    /** Zone measurements for the fix list. displayTilt = the analyzer's +3 dB/oct display tilt. */
    std::vector<ZoneReading> readZones (const std::vector<mm::Zone>& zones, float& referenceDb) const;

    static float tiltDb (float hz) { return 3.0f * std::log2 (juce::jmax (hz, 1.0f) / 1000.0f); }

private:
    void processFrame();
    void finish();
    float findResonance (float lowHz, float highHz, float& resonanceDb) const;

    double sampleRate = 44100.0;
    int order = 14, N = 1 << 14, hop = 1 << 13;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, bufL, bufR, work;
    int fill = 0;

    bool capturing = false, finished = false, hasSnapshot = false;
    double heardSeconds = 0.0, targetSeconds = 4.0;

    std::vector<std::vector<float>> frameSpectra;   // power per bin, per frame
    std::vector<float> frameLoudnessDb;
    std::vector<double> power;                      // the result: average power per bin
    std::vector<float> gridHz, gridDb;              // 1/24-octave smoothed result
    float binHz() const { return (float) (sampleRate / N); }
};
