#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <map>

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
    float latestChange = 0.0f;      // dB moved in the right direction at the last re-check
    int status = 0;                 // 0 to do, 1 fixed, 2 improved, 3 no change heard, 4 got worse
};

/** The result of "analyse": a frozen list of things to fix. Saved with the project. */
struct FixSession
{
    bool valid = false;
    juce::String genreId, instrumentId;
    std::vector<FixIssue> issues;
    juce::StringArray goodZones;
    int checks = 0;
};

/** Passes audio through (with the trim) and taps it for analysis (spectrum + levels). */
class MixMentorProcessor : public juce::AudioProcessor
{
public:
    MixMentorProcessor();
    ~MixMentorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
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
