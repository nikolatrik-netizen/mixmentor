#include "LevelMeter.h"
#include "Theme.h"

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
    using namespace theme;

    auto area = getLocalBounds().toFloat().reduced (0.0f, 7.0f);
    auto scaleArea = area.removeFromLeft (24.0f);
    area.removeFromLeft (4.0f);
    auto bar = area.withSizeKeepingCentre (juce::jmin (14.0f, area.getWidth()), area.getHeight());

    // Track
    g.setColour (col::analyzerTop);
    g.fillRoundedRectangle (bar, 4.0f);

    // Target range
    const float yMax = yForDb (targetMax, bar), yMin = yForDb (targetMin, bar);
    g.setColour (col::good.withAlpha (0.12f));
    g.fillRect (bar.withY (yMax).withBottom (yMin).expanded (3.0f, 0.0f));
    g.setColour (col::good.withAlpha (0.7f));
    g.fillRect (bar.getX() - 3.0f, yMax, bar.getWidth() + 6.0f, 1.0f);
    g.fillRect (bar.getX() - 3.0f, yMin, bar.getWidth() + 6.0f, 1.0f);

    const auto colourFor = [this] (float db)
    {
        if (db > targetMax) return col::bad;
        if (db >= targetMin) return col::good;
        return col::accent;
    };

    // RMS (wide, translucent) and peak (narrow, bright with gradient)
    const auto inner = bar.reduced (2.0f);
    const float yRms = yForDb (rms, inner);
    g.setColour (colourFor (rms).withAlpha (0.28f));
    g.fillRoundedRectangle (inner.withTop (yRms), 2.5f);

    const float yPeak = yForDb (peak, inner);
    const auto c = colourFor (peak);
    const auto peakBar = inner.reduced (inner.getWidth() * 0.25f, 0.0f).withTop (yPeak);
    g.setGradientFill (juce::ColourGradient (c, 0.0f, peakBar.getY(), c.withAlpha (0.5f), 0.0f, peakBar.getBottom(), false));
    g.fillRoundedRectangle (peakBar, 2.0f);

    // Peak hold
    if (hold > floorDb)
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (inner.getX(), yForDb (hold, inner) - 0.5f, inner.getWidth(), 1.5f);
    }

    // Scale
    g.setFont (font (9.5f));
    g.setColour (col::textFaint);
    for (float db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f })
        g.drawText (juce::String ((int) db), scaleArea.withY (yForDb (db, bar) - 6.0f).withHeight (12.0f).toNearestInt(),
                    juce::Justification::centredRight);
}
