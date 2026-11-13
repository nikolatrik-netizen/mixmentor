#include "LevelMeter.h"

void LevelMeter::setLevels (float peakDb, float holdDb, float rmsDb)
{
    peak = peakDb; hold = holdDb; rms = rmsDb;
    repaint();
}

void LevelMeter::setTargetRange (float minDb, float maxDb)
{
    targetMin = minDb; targetMax = maxDb;
    repaint();
}

float LevelMeter::yForDb (float db, juce::Rectangle<float> area) const
{
    return juce::jmap (juce::jlimit (floorDb, 0.0f, db), 0.0f, floorDb, area.getY(), area.getBottom());
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (0.0f, 7.0f);
    auto scaleArea = area.removeFromLeft (30.0f);
    auto bar = area.reduced (4.0f, 0.0f);

    g.setColour (juce::Colour (0xff15171c));
    g.fillRoundedRectangle (bar, 3.0f);

    // Target range
    const float yMax = yForDb (targetMax, bar), yMin = yForDb (targetMin, bar);
    g.setColour (juce::Colour (0xff3ecf8e).withAlpha (0.18f));
    g.fillRect (bar.withY (yMax).withBottom (yMin));
    g.setColour (juce::Colour (0xff3ecf8e).withAlpha (0.7f));
    g.drawHorizontalLine ((int) yMax, bar.getX(), bar.getRight());
    g.drawHorizontalLine ((int) yMin, bar.getX(), bar.getRight());

    // RMS (wide, dim) and peak (narrow, bright)
    const auto colourFor = [this] (float db)
    {
        if (db > targetMax) return juce::Colour (0xffef5b5b);
        if (db >= targetMin) return juce::Colour (0xff3ecf8e);
        return juce::Colour (0xff7aa7ff);
    };

    const float yRms = yForDb (rms, bar);
    g.setColour (colourFor (rms).withAlpha (0.35f));
    g.fillRect (bar.withTop (yRms));

    const float yPeak = yForDb (peak, bar);
    g.setColour (colourFor (peak));
    g.fillRect (bar.reduced (bar.getWidth() * 0.3f, 0.0f).withTop (yPeak));

    // Peak hold line
    if (hold > floorDb)
    {
        g.setColour (juce::Colours::white);
        g.drawHorizontalLine ((int) yForDb (hold, bar), bar.getX(), bar.getRight());
    }

    // Scale
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    for (float db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f })
        g.drawText (juce::String ((int) db), scaleArea.withY (yForDb (db, bar) - 6.0f).withHeight (12.0f).toNearestInt(),
                    juce::Justification::centredRight);
}
