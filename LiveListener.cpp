#include "LiveListener.h"
#include <algorithm>
#include <cmath>

void LiveListener::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);
    ringL.assign ((size_t) fftSize, 0.0f);
    ringR.assign ((size_t) fftSize, 0.0f);
    ringPpq.assign ((size_t) fftSize, 0.0);
    work.assign ((size_t) fftSize * 2, 0.0f);
    bandOfBin.assign ((size_t) fftSize / 2, -1);
    for (int b = 1; b < fftSize / 2; ++b)
    {
        const double f = b * sampleRate / fftSize;
        if (f < 17.8 || f > 22400.0) continue;
        bandOfBin[(size_t) b] = juce::jlimit (0, numBands - 1, (int) std::round (6.0 * std::log2 (f / 20.0)));
    }
    meter.prepare (sampleRate);
    reset();
}

void LiveListener::reset()
{
    rangeStart = rangeEnd = -1.0;
    loopStartSeen = loopEndSeen = -1.0;
    lastPpq = -1.0e9;
    passCount = currentBeat = 0;
    inPass = false;
    current.reset();
    previous.reset();
    analyzer.reset();
    frames.clear();
    ringFill = sinceHop = 0;
    timeInPass = coveredBeats = 0.0;
}

float LiveListener::getPassProgress() const
{
    if (! inPass) return 0.0f;
    if (musical) return (float) juce::jlimit (0.0, 1.0, coveredBeats / juce::jmax (0.25, rangeEnd - rangeStart));
    return (float) juce::jlimit (0.0, 1.0, timeInPass / 4.0);
}

void LiveListener::startPass()
{
    current = std::make_unique<Pass>();
    current->beats.assign ((size_t) juce::jmax (1, getBeatsPerPass()), Bands {});
    current->beatFrames.assign (current->beats.size(), 0);
    current->sameBarsAsBefore = previous != nullptr && musical && looping;
    analyzer = std::make_unique<Analyzer>();
    analyzer->prepare (sampleRate);
    analyzer->startCapture (1.0e6f);
    meter.startNewCapture();
    frames.clear();
    ringFill = sinceHop = 0;
    coveredBeats = timeInPass = 0.0;
    currentBeat = 0;
    inPass = true;
}

void LiveListener::finishPass (bool complete)
{
    inPass = false;
    if (current == nullptr) return;
    if (complete && analyzer != nullptr)
    {
        analyzer->finishNow();
        if (analyzer->hasResult())
        {
            current->spectrum = std::shared_ptr<Analyzer> (analyzer.release());
            current->dynamics = LoudnessMeter::compute (frames);
            previous = std::move (current);
            ++passCount;
            if (onPassComplete) onPassComplete();
        }
    }
    current.reset();
    analyzer.reset();
}

void LiveListener::analyseFrame()
{
    // Energy per 1/6-octave band of this frame (both channels' power averaged)
    Bands e {};
    std::array<int, numBands> counts {};
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& src = ch == 0 ? ringL : ringR;
        for (int i = 0; i < fftSize; ++i) work[(size_t) i] = src[(size_t) i] * window[(size_t) i];
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        fft->performRealOnlyForwardTransform (work.data(), true);
        for (int b = 1; b < fftSize / 2; ++b)
        {
            const int band = bandOfBin[(size_t) b];
            if (band < 0) continue;
            const float re = work[(size_t) (2 * b)], im = work[(size_t) (2 * b + 1)];
            e[(size_t) band] += 0.5f * (re * re + im * im);
            if (ch == 0) ++counts[(size_t) band];
        }
    }
    for (int k = 0; k < numBands; ++k)
        if (counts[(size_t) k] > 0) e[(size_t) k] /= (float) counts[(size_t) k];

    // Which beat does this frame belong to? (its centre)
    int beat;
    if (musical) beat = (int) std::floor (ringPpq[(size_t) fftSize / 2] - rangeStart);
    else         beat = (int) std::floor ((timeInPass - (double) fftSize / 2 / sampleRate) / 0.5);
    if (current == nullptr || beat < 0 || beat >= (int) current->beats.size())
        return;

    auto& avg = current->beats[(size_t) beat];
    auto& n = current->beatFrames[(size_t) beat];
    ++n;
    for (int k = 0; k < numBands; ++k)
        avg[(size_t) k] += (e[(size_t) k] - avg[(size_t) k]) / (float) n;

    if (beat > currentBeat)
    {
        currentBeat = beat;
        if (onBeat) onBeat();
    }
}

void LiveListener::addSample (float l, float r, double ppq)
{
    ringL[(size_t) ringFill] = l;
    ringR[(size_t) ringFill] = r;
    ringPpq[(size_t) ringFill] = ppq;
    ++ringFill;
    if (ringFill == fftSize)
    {
        analyseFrame();
        std::copy (ringL.begin() + hopSize, ringL.end(), ringL.begin());
        std::copy (ringR.begin() + hopSize, ringR.end(), ringR.begin());
        std::copy (ringPpq.begin() + hopSize, ringPpq.end(), ringPpq.begin());
        ringFill = fftSize - hopSize;
    }
}

void LiveListener::push (const float* L, const float* R, int numSamples, const BlockInfo& info)
{
    const bool nowMusical = info.hasPosition;
    if (nowMusical != musical)
    {
        reset();
        musical = nowMusical;
    }
    if (musical && ! info.playing)
        return;   // stopped: pause (the pass continues when playback continues in place)

    // ---- choose the bars (musical) ----
    if (musical)
    {
        barLength = juce::jmax (0.25, info.barLength);
        const bool loopUsable = info.looping && info.loopEnd - info.loopStart >= barLength * 0.99;
        const bool loopChanged = loopUsable && (std::abs (info.loopStart - loopStartSeen) > 1.0e-3
                                                || std::abs (info.loopEnd - loopEndSeen) > 1.0e-3);
        if (loopUsable != looping || loopChanged || rangeStart < 0.0)
        {
            const bool hadRange = rangeStart >= 0.0;
            looping = loopUsable;
            if (looping)
            {
                rangeStart = info.loopStart;
                rangeEnd = juce::jmin (info.loopEnd, info.loopStart + 8.0 * barLength);
                loopStartSeen = info.loopStart;
                loopEndSeen = info.loopEnd;
            }
            else
            {
                rangeStart = (std::floor (info.ppq / barLength + 1.0e-9) + 1.0) * barLength;   // next bar
                rangeEnd = rangeStart + 4.0 * barLength;
            }
            if (hadRange) { current.reset(); analyzer.reset(); inPass = false; previous.reset(); }
        }
    }

    const double beatsPerSample = info.bpm / (60.0 * sampleRate);
    const double tol = juce::jmax (0.1, 2.0 * numSamples * beatsPerSample);
    int segStart = -1;
    auto flush = [&] (int endIndex)
    {
        if (segStart >= 0 && endIndex > segStart && analyzer != nullptr)
        {
            analyzer->push (L + segStart, R + segStart, endIndex - segStart);
            meter.process (L + segStart, R + segStart, endIndex - segStart, [this] (const MeterFrame& f)
            {
                if (f.n > 0 && f.ms / (float) f.n > 1.0e-7f) frames.push_back (f);
            });
        }
        segStart = -1;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        if (! musical)
        {
            // time mode: 4 s passes of audible audio
            const bool audible = std::abs (L[i]) > 1.0e-4f || std::abs (R[i]) > 1.0e-4f;
            if (! audible) { flush (i); continue; }
            if (! inPass) startPass();
            if (segStart < 0) segStart = i;
            timeInPass += 1.0 / sampleRate;
            addSample (L[i], R[i], 0.0);
            if (timeInPass >= 4.0) { flush (i + 1); finishPass (true); }
            continue;
        }

        double ppq = info.ppq + i * beatsPerSample;
        if (info.looping && info.loopEnd > info.loopStart && ppq >= info.loopEnd)
            ppq -= (info.loopEnd - info.loopStart);   // the host wrapped inside this block

        const bool inside = ppq >= rangeStart - 1.0e-9 && ppq < rangeEnd;
        const bool jumpedBack = ppq < lastPpq - 0.05;
        const bool jumpedAhead = ppq > lastPpq + tol && lastPpq > -1.0e8;
        lastPpq = ppq;

        if (inPass && (jumpedBack || jumpedAhead || ! inside))
        {
            flush (i);
            const double length = rangeEnd - rangeStart;
            const bool complete = coveredBeats >= 0.9 * length;
            finishPass (complete);
            if (! looping && ! inside && ppq >= rangeEnd - 1.0e-9)
            {
                // not looping: move on to the next 4 bars
                rangeStart = std::floor (ppq / barLength + 1.0e-9) * barLength;
                if (rangeStart < ppq - tol) rangeStart += barLength;
                rangeEnd = rangeStart + 4.0 * barLength;
                previous.reset();
            }
        }

        if (! inPass)
        {
            const bool atStart = ppq >= rangeStart - 1.0e-9 && ppq < rangeStart + tol;
            if (! atStart) continue;   // wait for the first beat of the bars
            startPass();
        }
        if (segStart < 0) segStart = i;
        coveredBeats += beatsPerSample;
        addSample (L[i], R[i], ppq);
    }
    flush (numSamples);
}

float LiveListener::rangeVsRest (const std::vector<Bands>& beats, int fromBeat, int toBeat, float lowHz, float highHz)
{
    double inE = 0.0, restE = 0.0;
    int inN = 0, restN = 0;
    // make sure narrow ranges still cover at least the nearest band
    const int k0 = (int) std::round (6.0 * std::log2 (juce::jmax (lowHz, 20.0f) / 20.0));
    const int k1 = juce::jmax (k0, (int) std::round (6.0 * std::log2 (juce::jmax (highHz, 20.0f) / 20.0)));
    for (int b = juce::jmax (0, fromBeat); b < juce::jmin ((int) beats.size(), toBeat); ++b)
        for (int k = 0; k < numBands; ++k)
        {
            const float f = bandHz (k);
            const double e = beats[(size_t) b][(size_t) k];
            if (k >= k0 && k <= k1)                                           { inE += e; ++inN; }
            else if (f >= 30.0f && f <= 16000.0f && (f < lowHz * 0.5f || f > highHz * 2.0f)) { restE += e; ++restN; }
        }
    if (inN == 0 || restN == 0 || inE <= 0.0 || restE <= 0.0) return 0.0f;
    return (float) (10.0 * std::log10 ((inE / inN) / (restE / restN)));
}

float LiveListener::peakVsSurroundings (const std::vector<Bands>& beats, int fromBeat, int toBeat, float hz)
{
    const int k = juce::jlimit (3, numBands - 4, (int) std::round (6.0 * std::log2 (juce::jmax (hz, 20.0f) / 20.0)));
    double peak = 0.0, around = 0.0;
    for (int b = juce::jmax (0, fromBeat); b < juce::jmin ((int) beats.size(), toBeat); ++b)
    {
        const auto& x = beats[(size_t) b];
        peak += x[(size_t) k];
        around += 0.25 * (x[(size_t) (k - 3)] + x[(size_t) (k - 2)] + x[(size_t) (k + 2)] + x[(size_t) (k + 3)]);
    }
    if (peak <= 0.0 || around <= 0.0) return 0.0f;
    return (float) (10.0 * std::log10 (peak / around));
}
