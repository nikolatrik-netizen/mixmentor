#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "KnowledgeBase.h"

/** Real-time spectrum analyzer with the genre's frequency zones drawn on top.
    Hovering a zone shows its tip as a tooltip. */
class SpectrumView : public juce::Component,
                     public juce::TooltipClient
{
public:
    SpectrumView();

    void pushSamples (const float* samples, int numSamples);
    void updateSpectrum (double sampleRate);   // call from a GUI timer
    void setZones (std::vector<mm::Zone> newZones);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    juce::String getTooltip() override;

    static juce::Colour colourForKind (const juce::String& kind);

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr float minHz = 20.0f, maxHz = 20000.0f;
    static constexpr float minDb = -90.0f, maxDb = 0.0f;

    float xForFreq (float hz) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    int zoneIndexAt (float hz) const;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> history = std::vector<float> ((size_t) fftSize, 0.0f);
    int historyWritePos = 0;
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> smoothedDb = std::vector<float> ((size_t) fftSize / 2, minDb);
    double lastSampleRate = 44100.0;

    std::vector<mm::Zone> zones;
    int hoveredZone = -1;
};
