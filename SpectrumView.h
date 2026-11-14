#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "KnowledgeBase.h"

/** What the analyzer currently measures inside one frequency zone. */
struct ZoneReading
{
    float levelDb = 0.0f;       // average level in the zone (display scale)
    float relativeDb = 0.0f;    // level compared with the track's overall average
    float deviationDb = 0.0f;   // >0 = this much over target, <0 = this much under, 0 = in range
    float peakHz = 0.0f;        // loudest frequency inside the zone
    float resonanceHz = 0.0f;   // narrow ringing peak, 0 if none
    float resonanceDb = 0.0f;   // how far that peak sticks out
    int status = 0;             // +1 too much, -1 too little, 0 OK
};

/** Real-time spectrum analyzer. Draws the genre's frequency zones and, while audio plays,
    measures each zone against its target range. Hovering a zone shows its tip as a tooltip. */
class SpectrumView : public juce::Component,
                     public juce::TooltipClient
{
public:
    SpectrumView();

    void pushSamples (const float* samples, int numSamples);
    void updateSpectrum (double sampleRate, bool hasSignal);   // call from a GUI timer
    void setZones (std::vector<mm::Zone> newZones);

    /** True once enough audio has been heard to judge the zones. */
    bool hasReadings() const { return signalFrames >= framesNeeded; }
    float getListeningProgress() const { return juce::jmin (1.0f, (float) signalFrames / (float) framesNeeded); }
    const std::vector<mm::Zone>& getZones() const { return zones; }
    const std::vector<ZoneReading>& getReadings() const { return readings; }
    float getReferenceDb() const { return referenceDb; }

    /** Forget the averaged spectrum, e.g. after switching to a different sound. */
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
    static constexpr int framesNeeded = 45;   // ~1.5 s of audio at 30 fps

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
    std::vector<double> averagedPower = std::vector<double> ((size_t) fftSize / 2, 0.0);
    double lastSampleRate = 44100.0;
    int signalFrames = 0;

    std::vector<mm::Zone> zones;
    std::vector<ZoneReading> readings;
    float referenceDb = -60.0f;
    int hoveredZone = -1;
};
