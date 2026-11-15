#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "KnowledgeBase.h"
#include "Analyzer.h"

/** What the fix list wants shown on a zone's label. */
struct ZoneMark
{
    int state = 0;          // 0 nothing, 1 cut to do, 2 boost to do, 3 fixed, 4 ok, 5 improved, 6 no change, 7 worse
    juce::String text;
};

/** Real-time spectrum analyzer with the genre's frequency zones. "Analyse" listens for a few
    seconds and freezes a snapshot (the white line), which the fix list is built from.
    Hovering a zone shows its tip as a tooltip. */
class SpectrumView : public juce::Component,
                     public juce::TooltipClient
{
public:
    SpectrumView();

    /** Feed every sample, in order (stereo). The display uses the latest ones, the analysis all of them. */
    void pushSamples (const float* left, const float* right, int numSamples);
    void updateSpectrum (double sampleRate, bool hasSignal);   // call from a GUI timer
    /** Some audio was lost before it reached the analyzer. */
    void audioGap() { analyzer.markGap(); }
    void setZones (std::vector<mm::Zone> newZones);

    /** Listen for this many seconds of playing audio, then freeze a snapshot. */
    void startCapture (float seconds);
    bool isCapturing() const { return analyzer.isCapturing(); }
    float getCaptureProgress() const { return analyzer.getProgress(); }
    /** Returns true once, when a capture has just finished. */
    bool takeCaptureResult() { const bool r = captureDone; captureDone = false; return r; }
    bool hasReadings() const { return analyzer.hasResult(); }
    /** 31 third-octave band levels (dB) of the frozen analysis, from 20 Hz. Empty before an analysis. */
    std::vector<float> getBandLevels() const;

    const std::vector<mm::Zone>& getZones() const { return zones; }
    const std::vector<ZoneReading>& getReadings() const { return readings; }
    float getReferenceDb() const { return referenceDb; }

    void setZoneMarks (std::vector<ZoneMark> marks);

    /** Forget the snapshot. */
    void resetAnalysis();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    juce::String getTooltip() override;

    static juce::Colour colourForKind (const juce::String& kind);
    static juce::Colour colourForStatus (int status);
    static juce::String formatHz (float hz);

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float minHz = 20.0f, maxHz = 20000.0f;
    static constexpr float minDb = -90.0f, maxDb = 0.0f;

    float xForFreq (float hz) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    int zoneIndexAt (float hz) const;

    static float tiltDb (float hz);
    float averagedDbAt (float hz) const;      // long-term spectrum, interpolated, tilted
    void computeReadings();

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> history = std::vector<float> ((size_t) fftSize, 0.0f);
    int historyWritePos = 0;
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> smoothedDb = std::vector<float> ((size_t) fftSize / 2, minDb);
    double lastSampleRate = 44100.0;
    bool captureDone = false;
    Analyzer analyzer;
    std::vector<ZoneMark> marks;

    std::vector<mm::Zone> zones;
    std::vector<ZoneReading> readings;
    float referenceDb = -60.0f;
    int hoveredZone = -1;
};
