#include "SpectrumView.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "Theme.h"

SpectrumView::SpectrumView()
{
    setOpaque (false);
}

juce::Colour SpectrumView::colourForKind (const juce::String& kind)
{
    if (kind == "boost") return theme::col::good;   // usually worth enhancing
    if (kind == "cut")   return theme::col::bad;    // usually worth reducing
    return theme::col::warn;                        // watch / depends
}

juce::Colour SpectrumView::colourForStatus (int status)
{
    if (status > 0) return theme::col::bad;    // too much
    if (status < 0) return theme::col::warn;   // too little
    return theme::col::good;                   // OK
}

juce::String SpectrumView::formatHz (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, 1) + " kHz";
    const int rounded = hz < 100.0f ? (int) std::round (hz) : 5 * (int) std::round (hz / 5.0f);
    return juce::String (rounded) + " Hz";
}

void SpectrumView::pushDisplay (const float* mono, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        history[(size_t) historyWritePos] = mono[i];
        historyWritePos = (historyWritePos + 1) % fftSize;
    }
}

void SpectrumView::pushCapture (const float* left, const float* right, int numSamples)
{
    analyzer.push (left, right, numSamples);
    if (analyzer.takeFinished())
    {
        computeReadings();
        captureDone = true;
    }
}

bool SpectrumView::finishCapture()
{
    analyzer.finishNow();
    if (analyzer.takeFinished() && analyzer.hasResult())
    {
        computeReadings();
        captureDone = true;
        return true;
    }
    return false;
}

void SpectrumView::resetAnalysis()
{
    analyzer.reset();
    captureDone = false;
    readings.clear();
    repaint();
}

void SpectrumView::startCapture (float seconds)
{
    if (std::abs (analyzer.getSampleRate() - lastSampleRate) > 1.0)
        analyzer.prepare (lastSampleRate);
    analyzer.startCapture (seconds);
    captureDone = false;
    repaint();
}

void SpectrumView::setZoneMarks (std::vector<ZoneMark> newMarks)
{
    marks = std::move (newMarks);
    repaint();
}

float SpectrumView::tiltDb (float hz)
{
    // +3 dB/octave around 1 kHz so a balanced mix looks roughly flat (like Pro-Q's analyzer).
    return 3.0f * std::log2 (juce::jmax (hz, 1.0f) / 1000.0f);
}

void SpectrumView::updateSpectrum (double sampleRate, bool hasSignal)
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

    juce::ignoreUnused (hasSignal);
    for (size_t bin = 1; bin < smoothedDb.size(); ++bin)
    {
        const float hz = (float) bin * (float) lastSampleRate / (float) fftSize;
        const float mag = fftData[bin] * norm;
        const float db = juce::Decibels::gainToDecibels (mag, minDb) + tiltDb (hz);

        auto& sm = smoothedDb[bin];
        sm = db > sm ? 0.5f * sm + 0.5f * db     // rise quickly
                     : 0.88f * sm + 0.12f * db;  // fall slowly
    }
    repaint();
}

float SpectrumView::averagedDbAt (float hz) const
{
    return analyzer.levelAt (hz) + tiltDb (hz);
}

void SpectrumView::computeReadings()
{
    readings = analyzer.readZones (zones, referenceDb);
}

void SpectrumView::setZones (std::vector<mm::Zone> newZones)
{
    zones = std::move (newZones);
    hoveredZone = -1;
    computeReadings();
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
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();
    const auto white = juce::Colours::white;

    // Background: soft blue-grey gradient
    g.setGradientFill (juce::ColourGradient (col::analyzerTop, 0.0f, 0.0f, col::analyzerBot, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Grid
    for (float hz : { 30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
                      3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f })
    {
        g.setColour (white.withAlpha (0.025f));
        g.drawVerticalLine ((int) xForFreq (hz), 0.0f, bounds.getHeight());
    }
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        g.setColour (white.withAlpha (0.06f));
        g.drawVerticalLine ((int) xForFreq (hz), 0.0f, bounds.getHeight());
    }
    for (float db : { -18.0f, -36.0f, -54.0f, -72.0f })
    {
        g.setColour (white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) yForDb (db), 0.0f, bounds.getWidth());
    }

    // Zone bands: soft colour tints with thin edges
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        const float x1 = xForFreq (z.lowHz), x2 = xForFreq (z.highHz);
        const auto c = colourForKind (z.kind);
        const bool hovered = (int) i == hoveredZone;

        g.setGradientFill (juce::ColourGradient (c.withAlpha (hovered ? 0.16f : 0.08f), 0.0f, 0.0f,
                                                 c.withAlpha (0.01f), 0.0f, bounds.getBottom(), false));
        g.fillRect (juce::Rectangle<float> (x1, 0.0f, x2 - x1, bounds.getHeight()));
        g.setColour (c.withAlpha (hovered ? 0.7f : 0.25f));
        g.fillRect (x1, 0.0f, 1.0f, bounds.getHeight());
        g.fillRect (x2 - 1.0f, 0.0f, 1.0f, bounds.getHeight());
    }

    // Live spectrum: darker blue area, like a filled analyzer
    juce::Path curve;
    const float binHz = (float) lastSampleRate / (float) fftSize;
    for (int px = 0; px <= getWidth(); px += 2)
    {
        const float binPos = freqForX ((float) px) / binHz;
        const int b0 = juce::jlimit (1, (int) smoothedDb.size() - 2, (int) binPos);
        const float frac = juce::jlimit (0.0f, 1.0f, binPos - (float) b0);
        const float db = smoothedDb[(size_t) b0] + frac * (smoothedDb[(size_t) b0 + 1] - smoothedDb[(size_t) b0]);
        const float y = yForDb (db);
        if (px == 0) curve.startNewSubPath ((float) px, y);
        else         curve.lineTo ((float) px, y);
    }
    juce::Path fill (curve);
    fill.lineTo (bounds.getRight(), bounds.getBottom());
    fill.lineTo (0.0f, bounds.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (col::liveFill.brighter (0.3f).withAlpha (0.75f), 0.0f, bounds.getHeight() * 0.3f,
                                             col::liveFill.withAlpha (0.25f), 0.0f, bounds.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col::liveFill.brighter (0.9f).withAlpha (0.6f));
    g.strokePath (curve, juce::PathStrokeType (1.0f));

    // The analysed snapshot (white): frozen once "analyse" / "re-check" has finished
    if (analyzer.hasResult())
    {
        juce::Path avg;
        for (int px = 0; px <= getWidth(); px += 3)
        {
            const float y = yForDb (averagedDbAt (freqForX ((float) px)));
            if (px == 0) avg.startNewSubPath ((float) px, y);
            else         avg.lineTo ((float) px, y);
        }
        g.setColour (white.withAlpha (0.08f));
        g.strokePath (avg, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (white.withAlpha (0.95f));
        g.strokePath (avg, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Labels
    g.setFont (font (11.5f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (hz);
        const auto text = hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz);
        g.setColour (col::textFaint);
        g.drawText (text, (int) x - 20, getHeight() - 20, 40, 14, juce::Justification::centred);
    }
    for (float db : { -18.0f, -36.0f, -54.0f, -72.0f })
    {
        g.setColour (col::textFaint);
        g.drawText (juce::String ((int) db), getWidth() - 36, (int) yForDb (db) - 15, 30, 13, juce::Justification::right);
    }

    // Per-zone: the fix list's verdict (frozen until the next analyse / re-check)
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        const float x1 = xForFreq (z.lowHz), x2 = xForFreq (z.highHz);
        const float w = x2 - x1;
        const auto kindColour = colourForKind (z.kind);
        const ZoneMark mark = i < marks.size() ? marks[i] : ZoneMark();

        juce::Colour markColour = kindColour;
        switch (mark.state)
        {
            case 1: case 7: markColour = col::bad; break;    // cut to do / got worse
            case 2:         markColour = col::warn; break;   // boost to do
            case 3: case 4: markColour = col::good; break;   // fixed / ok
            case 5:         markColour = col::accent; break; // improved, a bit more
            case 6:         markColour = col::textDim; break;// no change heard
            default: break;
        }

        // Highlight the ranges still to fix, with the target range from the analysis
        if ((mark.state == 1 || mark.state == 2 || mark.state >= 5) && analyzer.hasResult())
        {
            g.setColour (markColour.withAlpha (0.10f));
            g.fillRect (juce::Rectangle<float> (x1, 0.0f, w, bounds.getHeight()));

            const float yTop = yForDb (referenceDb + juce::jmin (z.targetMaxDb, 30.0f));
            const float yBottom = yForDb (referenceDb + z.targetMinDb);
            const auto box = juce::Rectangle<float> (x1 + 3.0f, yTop, w - 6.0f, juce::jmax (1.0f, yBottom - yTop));
            juce::Path outline, dashed;
            outline.addRoundedRectangle (box, 3.0f);
            const float dashes[] = { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (white.withAlpha (0.45f));
            g.fillPath (dashed);
        }

        // Label pill
        const auto statusText = mark.text;
        g.setFont (font (12.0f));
        const float labelW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), z.label.toLowerCase());
        juce::Font statusFont (font (12.0f, true));
        const float statusW = statusText.isEmpty() ? 0.0f
                            : (float) juce::GlyphArrangement::getStringWidthInt (statusFont, statusText) + 8.0f;
        const float pillW = 24.0f + labelW + statusW + 8.0f;
        const float pillY = 10.0f + (float) (i % 2) * 28.0f;
        auto pill = juce::Rectangle<float> (juce::jlimit (4.0f, bounds.getRight() - pillW - 4.0f, x1 + 4.0f), pillY, pillW, 21.0f);

        g.setColour (col::box.withAlpha (0.95f));
        g.fillRoundedRectangle (pill, 10.5f);
        g.setColour ((mark.state != 0 ? markColour : kindColour).withAlpha (0.4f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 10.5f, 1.0f);

        g.setColour (kindColour);
        g.fillEllipse (pill.getX() + 8.0f, pill.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setColour (col::text);
        g.setFont (font (12.0f));
        g.drawText (z.label.toLowerCase(), pill.withTrimmedLeft (23.0f).withWidth (labelW + 2.0f),
                    juce::Justification::centredLeft, false);

        if (statusText.isNotEmpty())
        {
            g.setFont (statusFont);
            g.setColour (markColour);
            g.drawText (statusText, pill.withTrimmedLeft (23.0f + labelW + 6.0f).withWidth (statusW),
                        juce::Justification::centredLeft, false);
        }
    }

    // Status / legend
    if (zones.empty())
    {
        g.setColour (col::textDim);
        g.setFont (font (14.0f));
        g.drawText ("pick a genre and instrument to see frequency zones", getLocalBounds(), juce::Justification::centred);
    }
    else if (analyzer.isCapturing())
    {
        const auto text = captureStatus.isNotEmpty() ? captureStatus
                        : ! analyzer.hasHeardSound() ? juce::String ("waiting for sound: press play in your DAW")
                                                     : "listening...  " + juce::String ((int) (getCaptureProgress() * 100.0f)) + "%";
        g.setFont (font (13.5f));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 32.0f;
        const auto r = juce::Rectangle<float> (tw, 32.0f).withCentre (bounds.getCentre().translated (0.0f, 30.0f));
        g.setColour (col::box);
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (col::accent.withAlpha (0.25f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * getCaptureProgress()), 16.0f);
        g.setColour (col::text);
        g.drawText (text, r, juce::Justification::centred, false);
    }
    else
    {
        struct Key { juce::String t; int style; };
        const Key keys[] = { { "analysed", 0 }, { "live", 1 }, { "target", 2 } };
        g.setFont (font (11.5f));
        float x = 12.0f;
        const float y = bounds.getBottom() - 40.0f;
        for (auto& k : keys)
        {
            if (k.style == 0 && ! analyzer.hasResult()) continue;
            if (k.style == 0)      { g.setColour (white); g.fillRoundedRectangle (x, y + 5.0f, 14.0f, 2.0f, 1.0f); }
            else if (k.style == 1) { g.setColour (col::liveFill.brighter (0.3f)); g.fillRoundedRectangle (x, y + 1.0f, 14.0f, 10.0f, 2.0f); }
            else                   { g.setColour (white.withAlpha (0.8f)); g.drawRoundedRectangle (x, y + 1.5f, 14.0f, 9.0f, 2.0f, 1.0f); }
            g.setColour (col::textDim);
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), k.t);
            g.drawText (k.t, juce::Rectangle<float> (x + 19.0f, y, tw + 4.0f, 12.0f), juce::Justification::centredLeft, false);
            x += 19.0f + tw + 16.0f;
        }
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

    if (idx < 0)
        return formatHz (hz);

    const auto& z = zones[(size_t) idx];
    juce::String text = z.label + "  (" + formatHz (z.lowHz) + " - " + formatHz (z.highHz) + ")\n\n" + z.tip;

    if (juce::isPositiveAndBelow (idx, (int) marks.size()) && marks[(size_t) idx].text.isNotEmpty())
        text << "\n\nFix list: " << marks[(size_t) idx].text;
    return text;
}

std::vector<float> SpectrumView::getBandLevels() const
{
    return analyzer.thirdOctaveBands();
}
