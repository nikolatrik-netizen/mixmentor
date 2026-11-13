#include "SpectrumView.h"

SpectrumView::SpectrumView()
{
    setOpaque (true);
}

juce::Colour SpectrumView::colourForKind (const juce::String& kind)
{
    if (kind == "boost") return juce::Colour (0xff3ecf8e);   // green: usually worth enhancing
    if (kind == "cut")   return juce::Colour (0xffef5b5b);   // red: usually worth reducing
    return juce::Colour (0xfff2c14e);                        // yellow: watch / depends
}

void SpectrumView::pushSamples (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        history[(size_t) historyWritePos] = samples[i];
        historyWritePos = (historyWritePos + 1) % fftSize;
    }
}

void SpectrumView::updateSpectrum (double sampleRate)
{
    lastSampleRate = sampleRate > 0 ? sampleRate : 44100.0;

    // Unroll the circular history so the oldest sample comes first.
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = history[(size_t) ((historyWritePos + i) % fftSize)];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // A full-scale sine reads ~0 dB: Hann window halves amplitude, FFT scales by N/2.
    const float norm = 4.0f / (float) fftSize;

    for (size_t bin = 1; bin < smoothedDb.size(); ++bin)
    {
        const float hz = (float) bin * (float) lastSampleRate / (float) fftSize;
        // +3 dB/octave tilt around 1 kHz so a balanced mix looks roughly flat (like Pro-Q's analyzer).
        const float tilt = 3.0f * std::log2 (juce::jmax (hz, 1.0f) / 1000.0f);
        const float db = juce::Decibels::gainToDecibels (fftData[bin] * norm, minDb) + tilt;

        auto& s = smoothedDb[bin];
        s = db > s ? 0.5f * s + 0.5f * db     // rise quickly
                   : 0.88f * s + 0.12f * db;  // fall slowly
    }

    repaint();
}

void SpectrumView::setZones (std::vector<mm::Zone> newZones)
{
    zones = std::move (newZones);
    hoveredZone = -1;
    repaint();
}

float SpectrumView::xForFreq (float hz) const
{
    const float norm = std::log (hz / minHz) / std::log (maxHz / minHz);
    return norm * (float) getWidth();
}

float SpectrumView::freqForX (float x) const
{
    return minHz * std::pow (maxHz / minHz, x / (float) juce::jmax (1, getWidth()));
}

float SpectrumView::yForDb (float db) const
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), maxDb, minDb, 0.0f, (float) getHeight());
}

int SpectrumView::zoneIndexAt (float hz) const
{
    // If zones overlap, prefer the narrowest (most specific) one.
    int best = -1;
    float bestWidth = 1.0e9f;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        if (hz >= z.lowHz && hz <= z.highHz)
        {
            const float w = std::log (z.highHz / z.lowHz);
            if (w < bestWidth) { bestWidth = w; best = (int) i; }
        }
    }
    return best;
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff15171c));

    // Zones
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        const float x1 = xForFreq (z.lowHz), x2 = xForFreq (z.highHz);
        const auto colour = colourForKind (z.kind);
        const bool hovered = (int) i == hoveredZone;

        g.setColour (colour.withAlpha (hovered ? 0.28f : 0.13f));
        g.fillRect (juce::Rectangle<float> (x1, 0.0f, x2 - x1, bounds.getHeight()));
        g.setColour (colour.withAlpha (hovered ? 0.9f : 0.5f));
        g.drawVerticalLine ((int) x1, 0.0f, bounds.getHeight());
        g.drawVerticalLine ((int) x2, 0.0f, bounds.getHeight());

        // Stagger labels so neighbouring zones don't overlap.
        g.setColour (colour.withAlpha (0.95f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        const float labelY = 6.0f + (float) (i % 2) * 16.0f;
        g.drawFittedText (z.label, juce::Rectangle<int> ((int) x1 + 3, (int) labelY, juce::jmax (40, (int) (x2 - x1) + 60), 14),
                          juce::Justification::left, 1);
    }

    // Grid
    g.setFont (juce::FontOptions (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (hz);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, 0.0f, bounds.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        const auto text = hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz);
        g.drawText (text, (int) x + 2, getHeight() - 14, 40, 12, juce::Justification::left);
    }
    for (float db : { -18.0f, -36.0f, -54.0f, -72.0f })
    {
        const float y = yForDb (db);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) y, 0.0f, bounds.getWidth());
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawText (juce::String ((int) db) + " dB", getWidth() - 44, (int) y - 12, 40, 12, juce::Justification::right);
    }

    // Spectrum curve
    juce::Path curve;
    bool started = false;
    const float binHz = (float) lastSampleRate / (float) fftSize;

    for (int px = 0; px <= getWidth(); px += 2)
    {
        // Interpolate between bins at each pixel column for a smooth line.
        const float binPos = freqForX ((float) px) / binHz;
        const int b0 = juce::jlimit (1, (int) smoothedDb.size() - 2, (int) binPos);
        const float frac = juce::jlimit (0.0f, 1.0f, binPos - (float) b0);
        const float db = smoothedDb[(size_t) b0] + frac * (smoothedDb[(size_t) b0 + 1] - smoothedDb[(size_t) b0]);
        const float y = yForDb (db);

        if (! started) { curve.startNewSubPath ((float) px, y); started = true; }
        else           curve.lineTo ((float) px, y);
    }

    juce::Path fill (curve);
    fill.lineTo (bounds.getRight(), bounds.getBottom());
    fill.lineTo (0.0f, bounds.getBottom());
    fill.closeSubPath();
    g.setColour (juce::Colour (0xff7aa7ff).withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (juce::Colour (0xff9dbcff));
    g.strokePath (curve, juce::PathStrokeType (1.5f));

    if (zones.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Pick a genre and instrument to see frequency zones", getLocalBounds(), juce::Justification::centred);
    }
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const int z = zoneIndexAt (freqForX ((float) e.x));
    if (z != hoveredZone) { hoveredZone = z; repaint(); }
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    if (hoveredZone != -1) { hoveredZone = -1; repaint(); }
}

juce::String SpectrumView::getTooltip()
{
    const float hz = freqForX ((float) getMouseXYRelative().x);
    const int idx = zoneIndexAt (hz);

    auto fmt = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz"; };

    if (idx < 0)
        return fmt (hz);

    const auto& z = zones[(size_t) idx];
    return z.label + "  (" + fmt (z.lowHz) + " - " + fmt (z.highHz) + ")\n\n" + z.tip;
}
