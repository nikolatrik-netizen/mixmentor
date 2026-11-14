#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MelodyEngine.h"
#include <atomic>
#include <map>

/** Mix Mentor Melody: captures the MIDI played into it, and previews generated variations
    through a small built-in synth (also sent to the MIDI output). */
class MelodyProcessor : public juce::AudioProcessor
{
public:
    MelodyProcessor();
    ~MelodyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
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

    // State shared with the editor (message thread)
    juce::String genreId;
    melody::Phrase original;
    int selectedVariation = 0;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyProcessor)
};
