#include "MelodyCore.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace
{
    /** A soft, plucky preview sound: a sine plus a little triangle, with an ADSR. */
    struct PreviewSound : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    struct PreviewVoice : public juce::SynthesiserVoice
    {
        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PreviewSound*> (s) != nullptr; }

        void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
        {
            phase = 0.0;
            delta = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (note) / getSampleRate();
            level = 0.18f * (0.3f + 0.7f * velocity);
            env.setSampleRate (getSampleRate());
            env.setParameters ({ 0.004f, 0.35f, 0.45f, 0.25f });
            env.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff) env.noteOff();
            else { env.reset(); clearCurrentNote(); }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive()) return;
            for (int i = 0; i < num; ++i)
            {
                const float e = env.getNextSample();
                const double p = phase / juce::MathConstants<double>::twoPi;
                const float tri = (float) (4.0 * std::abs (p - std::floor (p + 0.5)) - 1.0);
                const float s = (float) (std::sin (phase) * 0.8 + tri * 0.2) * level * e;
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.addSample (ch, start + i, s);
                phase += delta;
                if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
                if (! env.isActive()) { clearCurrentNote(); break; }
            }
        }

        double phase = 0.0, delta = 0.0;
        float level = 0.0f;
        juce::ADSR env;
    };
}

MelodyCore::MelodyCore()
{
    for (int i = 0; i < 12; ++i)
        synth.addVoice (new PreviewVoice());
    synth.addSound (new PreviewSound());
}

void MelodyCore::prepare (double sampleRate)
{
    sampleRateAtomic = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

double MelodyCore::currentBeat (int sampleOffset) const
{
    const double beatsPerSample = bpm.load() / 60.0 / sampleRateAtomic.load();
    return (hostPlaying ? hostBeatAtBlockStart : internalBeat) + sampleOffset * beatsPerSample;
}

void MelodyCore::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, juce::AudioPlayHead* playHead)
{
    const int numSamples = buffer.getNumSamples();

    // Tempo and position from the host (falls back to a free-running clock at 120 BPM).
    hostPlaying = false;
    if (auto* ph = playHead)
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                {
                    hostPlaying = true;
                    hostBeatAtBlockStart = *ppq;
                }
        }
    }
    const double beatsPerSample = bpm.load() / 60.0 / sampleRateAtomic.load();

    // ---- capture incoming notes
    if (capturing.load())
    {
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (! (msg.isNoteOn() || msg.isNoteOff())) continue;
            const double beat = currentBeat (meta.samplePosition);

            if (captureStartBeat.load() < 0.0)
            {
                if (! msg.isNoteOn()) continue;
                captureStartBeat = std::floor (beat / 4.0) * 4.0;   // start at the bar of the first note
            }

            int s1, n1, s2, n2;
            captureFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0)
            {
                captureBuffer[(size_t) s1] = { msg.isNoteOn(), msg.getNoteNumber(), msg.getFloatVelocity(), beat - captureStartBeat.load() };
                captureFifo.finishedWrite (1);
            }
        }

        if (captureStartBeat.load() >= 0.0 && currentBeat (numSamples) - captureStartBeat.load() >= captureBars.load() * 4.0)
        {
            capturing = false;
            captureFinished = true;
        }
    }

    // ---- preview playback (loops the selected phrase)
    juce::MidiBuffer previewMidi;
    const bool on = previewOn.load();
    if (on)
    {
        const juce::SpinLock::ScopedTryLockType lock (previewLock);
        if (lock.isLocked() && previewLength > 0.0)
        {
            const double b0 = previewBeat.load();
            const double b1 = b0 + numSamples * beatsPerSample;

            for (auto& n : previewPhrase)
            {
                auto schedule = [&] (double eventBeat, bool noteOn)
                {
                    // Find occurrences of eventBeat (mod loop length) inside [b0, b1).
                    double t = eventBeat + std::floor (b0 / previewLength) * previewLength;
                    for (int k = 0; k < 2; ++k, t += previewLength)
                        if (t >= b0 && t < b1)
                        {
                            const int pos = juce::jlimit (0, numSamples - 1, (int) ((t - b0) / beatsPerSample));
                            previewMidi.addEvent (noteOn ? juce::MidiMessage::noteOn (1, n.pitch, n.velocity)
                                                         : juce::MidiMessage::noteOff (1, n.pitch), pos);
                        }
                };
                schedule (n.start, true);
                schedule (std::fmod (n.start + n.length - 0.001, previewLength), false);
            }
            previewBeat = std::fmod (b1, previewLength);
        }
        previewWasOn = true;
    }
    else if (previewWasOn)
    {
        for (int ch = 1; ch <= 16; ++ch)
            previewMidi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
        synth.allNotesOff (0, true);
        previewWasOn = false;
        previewBeat = 0.0;
    }

    // Play incoming notes plus the preview through the built-in synth, and send both to the MIDI output.
    midi.addEvents (previewMidi, 0, numSamples, 0);
    synth.renderNextBlock (buffer, midi, 0, numSamples);

    if (! hostPlaying)
        internalBeat += numSamples * beatsPerSample;
}

//==============================================================================
void MelodyCore::startCapture (int bars)
{
    int s1, n1, s2, n2;
    captureFifo.prepareToRead (captureFifo.getNumReady(), s1, n1, s2, n2);
    captureFifo.finishedRead (n1 + n2);
    captured.clear();
    openNotes.clear();
    captureBars = bars;
    captureStartBeat = -1.0;
    captureFinished = false;
    capturing = true;
}

void MelodyCore::stopCapture()
{
    if (capturing.exchange (false))
        captureFinished = true;
}

bool MelodyCore::pollCapture (melody::Phrase& out)
{
    int s1, n1, s2, n2;
    captureFifo.prepareToRead (captureFifo.getNumReady(), s1, n1, s2, n2);
    auto handle = [this] (const CapturedEvent& e)
    {
        if (e.on)
        {
            melody::Note n;
            n.start = e.beat;
            n.pitch = e.pitch;
            n.velocity = e.velocity;
            openNotes[e.pitch] = n;
        }
        else if (auto it = openNotes.find (e.pitch); it != openNotes.end())
        {
            it->second.length = juce::jmax (1.0 / 32.0, e.beat - it->second.start);
            captured.push_back (it->second);
            openNotes.erase (it);
        }
    };
    for (int i = 0; i < n1; ++i) handle (captureBuffer[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) handle (captureBuffer[(size_t) (s2 + i)]);
    captureFifo.finishedRead (n1 + n2);

    if (captureFinished.exchange (false))
    {
        for (auto& [pitch, n] : openNotes)   // notes still held at the end of capture
        {
            n.length = juce::jmax (0.25, captureBars.load() * 4.0 - n.start);
            captured.push_back (n);
        }
        openNotes.clear();
        std::sort (captured.begin(), captured.end(), [] (auto& a, auto& b) { return a.start < b.start; });
        out = captured;
        return true;
    }
    out = captured;
    return false;
}

void MelodyCore::setPreview (const melody::Phrase& phrase, double lengthBeats)
{
    const juce::SpinLock::ScopedLockType lock (previewLock);
    previewPhrase = phrase;
    previewLength = juce::jmax (1.0, lengthBeats);
}

void MelodyCore::setPreviewPlaying (bool shouldPlay)
{
    if (shouldPlay && ! previewOn.load())
        previewBeat = 0.0;
    previewOn = shouldPlay;
}

//==============================================================================
void MelodyCore::writeState (juce::XmlElement& xml) const
{
    xml.setAttribute ("genre", genreId);
    xml.setAttribute ("variation", selectedVariation);
    xml.setAttribute ("seed", juce::String (seed));
    for (auto& n : original)
    {
        auto* e = xml.createNewChildElement ("Note");
        e->setAttribute ("s", n.start);
        e->setAttribute ("l", n.length);
        e->setAttribute ("p", n.pitch);
        e->setAttribute ("v", n.velocity);
    }
}

void MelodyCore::readState (const juce::XmlElement& xml)
{
    genreId = xml.getStringAttribute ("genre");
    selectedVariation = xml.getIntAttribute ("variation");
    seed = xml.getStringAttribute ("seed", "1").getLargeIntValue();
    original.clear();
    for (auto* e : xml.getChildWithTagNameIterator ("Note"))
        original.push_back ({ e->getDoubleAttribute ("s"), e->getDoubleAttribute ("l"),
                              e->getIntAttribute ("p"), (float) e->getDoubleAttribute ("v") });
}
