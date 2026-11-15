#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <map>
#include <array>
#include "MelodyCore.h"
#include "TrackRegistry.h"

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

/** Dynamics, loudness and stereo measured over the same 4 seconds as the spectrum. */
struct DynamicsReading
{
    bool valid = false;
    float peakDb = -100.0f;         // highest sample peak
    float rmsDb = -100.0f;          // average level while playing
    float crestDb = 0.0f;           // peak minus average: punch vs. density
    float loudnessLufs = -100.0f;   // K-weighted loudness of the analysed loop (like short-term LUFS)
    float swingDb = 0.0f;           // how much the level jumps around between loud and quiet moments
    float correlation = 1.0f;       // +1 mono, 0 very wide, below 0 = phase problems
    float lowCorrelation = 1.0f;    // the same, below ~120 Hz
    float sideDb = -100.0f;         // side vs. mid energy (width)
    float lowSideDb = -100.0f;      // the same, below ~120 Hz
    bool stereo = false;            // the track has two different channels
    int clippedSamples = 0;
};

/** One 50 ms slice of meter data, from the audio thread to the editor. */
struct MeterFrame
{
    float msK = 0, peak = 0, ms = 0;
    float ll = 0, rr = 0, lr = 0, lowLL = 0, lowRR = 0, lowLR = 0;
    int n = 0, clips = 0;
};

/** The result of "analyse": a frozen list of things to fix. Saved with the project. */
struct FixSession
{
    bool valid = false;
    juce::String genreId, instrumentId;
    std::vector<FixIssue> issues;
    juce::StringArray goodZones;
    bool heardChange = true;        // did the last re-check hear any of the user's moves?
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
    MeterFrame currentFrame;
    int frameLength = 2205;
    juce::IIRFilter kShelf[2], kHighPass[2], lowA[2], lowB[2];
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

    /** GUI thread: pulls up to maxSamples of the latest mono audio. Returns how many were read. */
    int readAnalysisSamples (float* dest, int maxSamples);

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
    static constexpr int fifoSize = 1 << 15;
    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoBuffer = std::vector<float> ((size_t) fifoSize, 0.0f);

    std::atomic<float> peakSinceLastRead { 0.0f }, latestRms { 0.0f };
    std::atomic<double> currentSampleRate { 44100.0 };
    juce::SmoothedValue<float> trimGain { 1.0f };

    mutable juce::CriticalSection trackNameLock;
    juce::String trackName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMentorProcessor)
};
