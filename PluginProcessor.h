#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <map>
#include <array>
#include "MelodyCore.h"
#include "TrackRegistry.h"
#include "Meter.h"
#include "LiveListener.h"
#include "Advisor.h"

/** One range the analysis asked you to adjust, and how your fix went. */
/** Passes audio through (with the trim) and taps it for analysis (spectrum + levels). */
class MixMentorProcessor : public juce::AudioProcessor
{
public:
    MixMentorProcessor();
    ~MixMentorProcessor() override;
    const int registryId;   // this instance in the shared TrackRegistry

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    juce::MidiBuffer melodyMidi;


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

    /** The editor turns this on while it's open: every block (stereo, after the trim) is streamed
        to it together with its song position. */
    std::atomic<bool> listening { false };
    /** GUI thread: takes the next streamed block. Returns its length (0 if none). */
    int readStreamBlock (float* left, float* right, int maxSamples, BlockInfo& info);
    /** GUI thread: latest audio (mono) for the live spectrum display. */
    int readDisplaySamples (float* dest, int maxSamples);
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
    Advisor advisor;                 // the suggestions (saved with the project)
    MelodyCore melody;

               // the melody tab (previews through this track)

    /** Ticked workflow steps (bit n = step n), per genre id. Message thread only. */
    std::map<juce::String, juce::uint32> workflowProgress;

private:
    static constexpr int fifoSize = 1 << 18;   // ~5 s of stereo audio, so a busy GUI doesn't lose any
    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoLeft = std::vector<float> ((size_t) fifoSize, 0.0f);
    std::vector<float> fifoRight = std::vector<float> ((size_t) fifoSize, 0.0f);
    std::atomic<int> droppedBlocks { 0 };
    juce::AbstractFifo displayFifo { 1 << 16 };
    std::vector<float> displayBuffer = std::vector<float> ((size_t) (1 << 16), 0.0f);
    struct StreamBlock { int numSamples = 0; BlockInfo info; };
    juce::AbstractFifo blockFifo { 4096 };
    std::array<StreamBlock, 4096> blockInfos {};


    std::atomic<float> peakSinceLastRead { 0.0f }, latestRms { 0.0f };
    std::atomic<double> currentSampleRate { 44100.0 };
    juce::SmoothedValue<float> trimGain { 1.0f };

    mutable juce::CriticalSection trackNameLock;
    juce::String trackName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMentorProcessor)
};
