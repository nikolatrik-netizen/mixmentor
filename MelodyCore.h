#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MelodyEngine.h"
#include <atomic>
#include <map>

/** The melody idea generator's audio side: captures incoming MIDI, previews ideas through a
    small built-in synth, and keeps the melody. Used by Mix Mentor (melody tab) and by the
    separate Mix Mentor Melody instrument. */
class MelodyCore
{
public:
    MelodyCore();

    void prepare (double sampleRate);
    /** Adds the preview sound (and any incoming notes) to the buffer; preview notes are added to midi. */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, juce::AudioPlayHead* playHead);

    // Capture (message thread starts/stops it, audio thread records)
    void startCapture (int bars);
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }
    /** Message thread: collects what has been captured so far. Returns true when capture just finished. */
    bool pollCapture (melody::Phrase& out);

    // Preview playback
    void setPreview (const melody::Phrase& phrase, double lengthBeats);
    void setPreviewPlaying (bool shouldPlay);
    bool isPreviewPlaying() const { return previewOn.load(); }
    double getPreviewBeat() const { return previewBeat.load(); }
    double getBpm() const { return bpm.load(); }

    void writeState (juce::XmlElement& xml) const;
    void readState (const juce::XmlElement& xml);

    // State shared with the editor (message thread)
    juce::String genreId;
    melody::Phrase original;
    int selectedVariation = 0;
    int keyRoot = -1;     // 0-11, -1 = detect from the melody
    int scaleType = -1;   // melody::ScaleType, -1 = detect
    juce::int64 seed = 1;

private:
    struct CapturedEvent { bool on; int pitch; float velocity; double beat; };

    double currentBeat (int sampleOffset) const;

    juce::Synthesiser synth;
    std::atomic<double> sampleRateAtomic { 44100.0 }, bpm { 120.0 };

    // capture
    std::atomic<bool> capturing { false }, captureFinished { false };
    std::atomic<int> captureBars { 2 };
    std::atomic<double> captureStartBeat { -1.0 };
    double internalBeat = 0.0;          // free-running clock when the host isn't playing
    bool hostPlaying = false;
    double hostBeatAtBlockStart = 0.0;
    juce::AbstractFifo captureFifo { 4096 };
    std::vector<CapturedEvent> captureBuffer = std::vector<CapturedEvent> (4096);
    std::map<int, melody::Note> openNotes;   // message thread: notes waiting for their note-off
    melody::Phrase captured;                 // message thread

    // preview
    juce::SpinLock previewLock;
    melody::Phrase previewPhrase;
    double previewLength = 4.0;
    std::atomic<bool> previewOn { false };
    std::atomic<double> previewBeat { 0.0 };
    bool previewWasOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyCore)
};
