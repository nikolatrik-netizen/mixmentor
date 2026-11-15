#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <map>
#include <array>
#include "MelodyCore.h"
#include "TrackRegistry.h"
#include "Meter.h"

/** One range the analysis asked you to adjust, and how your fix went. */
struct FixIssue
{
    int zone = -1;                  // index of the frequency zone in the guide
    juce::String label;
    float lowHz = 0.0f, highHz = 0.0f, focusHz = 0.0f;
    int action = 1;                 // +1 = cut, -1 = boost
    bool resonance = false;         // a single ringing frequency rather than a range
    bool bump = false;              // measured as a bump/dip against its neighbours (else against the target range)
    float amount = 3.0f;            // suggested change in dB
    float baseline = 0.0f;          // what the analysis measured (relative level, or how far a resonance sticks out)
    float baselineLevel = 0.0f;     // the range's level compared with the rest of the track, far from it (for re-checks)
    float latestChange = 0.0f;      // dB moved in the right direction at the last re-check
    int status = 0;                 // 0 to do, 1 fixed, 2 improved, 3 no change heard, 4 got worse, 5 range cut but still ringing
    juce::String metric;            // non-empty = a dynamics / stereo / level issue (no zone): "crest", "swing", "lowwidth", ...
    float limit = 0.0f;             // for metric issues: the value that counts as fixed
    float fixedValue = 0.0f;        // what was measured when it was marked fixed (so a later analysis doesn't re-flag noise)

    juce::String key() const { return metric.isNotEmpty() ? "m:" + metric : (resonance ? "r:" : "z:") + label; }
};

/** The result of "analyse": a frozen list of things to fix. Saved with the project. */
struct FixSession
{
    bool valid = false;
    juce::String genreId, instrumentId;
    std::vector<FixIssue> issues;
    juce::StringArray goodZones;
    bool heardChange = true;        // did the last re-check hear any of the user's moves?
    float baseLoudness = -100.0f;   // loudness of the analysed loop
    float loopDiffLu = 0.0f;        // re-check loop minus analysed loop
    std::vector<float> baseBands;   // third-octave spectrum of the analysed loop (to notice a different part of the song)
    float contentDiffDb = 0.0f;     // how different the re-checked music was, away from the ranges being fixed
    bool contentMismatch = false;   // last re-check heard different music: results weren't updated
    int checks = 0;
};

/** Passes audio through (with the trim) and taps it for analysis (spectrum + levels). */
class MixMentorProcessor : public juce::AudioProcessor
{
public:
    MixMentorProcessor();
    ~MixMentorProcessor() override;
    const int registryId;   // this instance in the shared TrackRegistry

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    juce::MidiBuffer melodyMidi;
    juce::AbstractFifo meterFifo { 512 };
    std::array<MeterFrame, 512> meterBuffer {};

    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void updateTrackProperties (const TrackProperties& properties) override;

    //==============================================================================
    // Shared with the editor

    /** GUI thread: pulls up to maxSamples of stereo audio, in order. Returns how many were read. */
    int readAnalysisSamples (float* left, float* right, int maxSamples);
    /** Counts audio the GUI didn't pick up in time (the analysis then restarts its current frame). */
    int getDroppedCount() const { return droppedBlocks.load(); }

    /** GUI thread: highest peak (linear) since the last call, then resets it. */
    float takePeak() { return peakSinceLastRead.exchange (0.0f); }
    float getLatestRms() const { return latestRms.load(); }
    double getCurrentSampleRate() const { return currentSampleRate.load(); }

    juce::String getTrackName() const;

    /** Input trim in dB (the gain staging control). Saved with the project and automatable. */
    juce::AudioProcessorValueTreeState parameters;
    juce::RangedAudioParameter* trimParam = nullptr;
    std::atomic<float>* trimDb = nullptr;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<int>  genreIndex { -1 };              // -1 = user hasn't picked a genre yet
    std::atomic<int>  instrumentIndex { 0 };
    std::atomic<bool> instrumentChosenByUser { false };

    /** The current fix list (message thread only). */
    FixSession fixSession;
    MelodyCore melody;

    /** Message thread: takes the meter frames measured since the last call. */
    int readMeterFrames (MeterFrame* dest, int maxFrames);               // the melody tab (previews through this track)

    /** Ticked workflow steps (bit n = step n), per genre id. Message thread only. */
    std::map<juce::String, juce::uint32> workflowProgress;

private:
    static constexpr int fifoSize = 1 << 18;   // ~5 s of stereo audio, so a busy GUI doesn't lose any
    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoLeft = std::vector<float> ((size_t) fifoSize, 0.0f);
    std::vector<float> fifoRight = std::vector<float> ((size_t) fifoSize, 0.0f);
    std::atomic<int> droppedBlocks { 0 };
    LoudnessMeter meter;

    std::atomic<float> peakSinceLastRead { 0.0f }, latestRms { 0.0f };
    std::atomic<double> currentSampleRate { 44100.0 };
    juce::SmoothedValue<float> trimGain { 1.0f };

    mutable juce::CriticalSection trackNameLock;
    juce::String trackName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMentorProcessor)
};
