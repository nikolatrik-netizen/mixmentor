#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Vertical peak/RMS meter with the genre's target peak range shaded in. */
class LevelMeter : public juce::Component
{
public:
    void setLevels (float peakDb, float holdDb, float rmsDb);
    void setTargetRange (float minDb, float maxDb);
    void paint (juce::Graphics&) override;

private:
    float yForDb (float db, juce::Rectangle<float> area) const;

    float peak = -100.0f, hold = -100.0f, rms = -100.0f;
    float targetMin = -12.0f, targetMax = -6.0f;
    static constexpr float floorDb = -48.0f;
};
