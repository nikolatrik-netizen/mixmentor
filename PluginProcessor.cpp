#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "PluginEditor.h"
#include "KnowledgeBase.h"

MixMentorProcessor::MixMentorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      registryId (TrackRegistry::get().newId()),
      parameters (*this, nullptr, "PARAMS", createParameterLayout())
{
    trimParam = parameters.getParameter ("trim");
    trimDb = parameters.getRawParameterValue ("trim");

    mm::KnowledgeBase::get();   // load the knowledge base up front, off the audio thread
}

juce::AudioProcessorValueTreeState::ParameterLayout MixMentorProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "trim", 1 }, "Trim",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; })));
    return layout;
}

void MixMentorProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    fifo.reset();
    trimGain.reset (sampleRate, 0.05);   // 50 ms ramp so trim changes never click
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (trimDb->load()));
    melody.prepare (sampleRate);

    melodyMidi.ensureSize (2048);
}

bool MixMentorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MixMentorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numIn == 0 || numSamples == 0)
        return;

    // Apply the trim (gain staging). At 0 dB the audio passes through untouched.
    trimGain.setTargetValue (juce::Decibels::decibelsToGain (trimDb->load()));
    if (trimGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = trimGain.getNextValue();
            for (int ch = 0; ch < numIn; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
        }
    }
    else if (std::abs (trimGain.getTargetValue() - 1.0f) > 1.0e-6f)
    {
        buffer.applyGain (0, numSamples, trimGain.getTargetValue());
    }

    // Measure after the trim, so the meter shows what the next plugin receives.
    const float* L = buffer.getReadPointer (0);
    const float* R = buffer.getReadPointer (numIn > 1 ? 1 : 0);
    float blockPeak = 0.0f, sumSquares = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        blockPeak = juce::jmax (blockPeak, std::abs (L[i]), std::abs (R[i]));
        sumSquares += 0.5f * (L[i] * L[i] + R[i] * R[i]);
    }
    latestRms = std::sqrt (sumSquares / (float) numSamples);
    float current = peakSinceLastRead.load();
    while (blockPeak > current && ! peakSinceLastRead.compare_exchange_weak (current, blockPeak)) {}

    // Live display: everything (mono)
    if (displayFifo.getFreeSpace() >= numSamples)
    {
        int s1, n1, s2, n2;
        displayFifo.prepareToWrite (numSamples, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) displayBuffer[(size_t) (s1 + i)] = 0.5f * (L[i] + R[i]);
        for (int i = 0; i < n2; ++i) displayBuffer[(size_t) (s2 + i)] = 0.5f * (L[n1 + i] + R[n1 + i]);
        displayFifo.finishedWrite (n1 + n2);
    }

    // Stream to the editor's listener: the samples, then the block's song position.
    if (listening.load())
    {
        BlockInfo info;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition(); pos.hasValue())
            {
                const auto ppq = pos->getPpqPosition();
                const auto bpm = pos->getBpm();
                if (ppq.hasValue() && bpm.hasValue() && *bpm > 1.0)
                {
                    info.hasPosition = true;
                    info.playing = pos->getIsPlaying();
                    info.ppq = *ppq;
                    info.bpm = *bpm;
                    info.looping = pos->getIsLooping();
                    if (auto loop = pos->getLoopPoints(); loop.hasValue())
                    { info.loopStart = loop->ppqStart; info.loopEnd = loop->ppqEnd; }
                    if (auto sig = pos->getTimeSignature(); sig.hasValue() && sig->denominator > 0)
                        info.barLength = 4.0 * sig->numerator / sig->denominator;
                }
            }

        if (fifo.getFreeSpace() >= numSamples && blockFifo.getFreeSpace() >= 1)
        {
            int start1, size1, start2, size2;
            fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
            std::copy_n (L, size1, fifoLeft.data() + start1);
            std::copy_n (R, size1, fifoRight.data() + start1);
            if (size2 > 0)
            {
                std::copy_n (L + size1, size2, fifoLeft.data() + start2);
                std::copy_n (R + size1, size2, fifoRight.data() + start2);
            }
            fifo.finishedWrite (size1 + size2);
            int b1, m1, b2, m2;
            blockFifo.prepareToWrite (1, b1, m1, b2, m2);
            blockInfos[(size_t) b1] = { numSamples, info };
            blockFifo.finishedWrite (1);
        }
        else
        {
            droppedBlocks.fetch_add (1);   // the editor isn't reading fast enough
        }
    }

    // Melody tab preview: added after the analysis, so it never affects the mix advice.
    melodyMidi.clear();
    melody.process (buffer, melodyMidi, getPlayHead());
}

int MixMentorProcessor::readDisplaySamples (float* dest, int maxSamples)
{
    int s1, n1, s2, n2;
    displayFifo.prepareToRead (juce::jmin (maxSamples, displayFifo.getNumReady()), s1, n1, s2, n2);
    std::copy_n (displayBuffer.data() + s1, n1, dest);
    if (n2 > 0) std::copy_n (displayBuffer.data() + s2, n2, dest + n1);
    displayFifo.finishedRead (n1 + n2);
    return n1 + n2;
}

int MixMentorProcessor::readStreamBlock (float* left, float* right, int maxSamples, BlockInfo& info)
{
    if (blockFifo.getNumReady() < 1) return 0;
    int b1, m1, b2, m2;
    blockFifo.prepareToRead (1, b1, m1, b2, m2);
    const auto block = blockInfos[(size_t) b1];
    blockFifo.finishedRead (1);
    info = block.info;
    const int n = juce::jmin (block.numSamples, maxSamples, fifo.getNumReady());
    int start1, size1, start2, size2;
    fifo.prepareToRead (block.numSamples, start1, size1, start2, size2);
    if (size1 > 0) { std::copy_n (fifoLeft.data() + start1, juce::jmin (size1, n), left);  std::copy_n (fifoRight.data() + start1, juce::jmin (size1, n), right); }
    if (size2 > 0 && n > size1) { std::copy_n (fifoLeft.data() + start2, n - size1, left + size1); std::copy_n (fifoRight.data() + start2, n - size1, right + size1); }
    fifo.finishedRead (size1 + size2);
    return n;
}

void MixMentorProcessor::updateTrackProperties (const TrackProperties& properties)
{
    // Some DAWs tell the plugin the name of the track it is on (VST3/AU/CLAP feature).
    // If the user hasn't picked an instrument themselves, guess it from that name.
    const auto name = properties.name.value_or (juce::String());
    {
        const juce::ScopedLock sl (trackNameLock);
        trackName = name;
    }

    if (! instrumentChosenByUser.load())
    {
        const int guess = mm::KnowledgeBase::get().guessInstrumentFromTrackName (name);
        if (guess >= 0)
            instrumentIndex = guess;
    }
}

juce::String MixMentorProcessor::getTrackName() const
{
    const juce::ScopedLock sl (trackNameLock);
    return trackName;
}

void MixMentorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto& kb = mm::KnowledgeBase::get();
    juce::XmlElement xml ("MixMentor");

    const int g = genreIndex.load(), i = instrumentIndex.load();
    if (juce::isPositiveAndBelow (g, (int) kb.genres().size()))
        xml.setAttribute ("genre", kb.genres()[(size_t) g].id);
    if (juce::isPositiveAndBelow (i, (int) kb.instruments().size()))
        xml.setAttribute ("instrument", kb.instruments()[(size_t) i].id);
    xml.setAttribute ("instrumentChosenByUser", instrumentChosenByUser.load());

    if (auto paramsXml = parameters.copyState().createXml())
        xml.addChildElement (paramsXml.release());

    xml.setAttribute ("advisor", advisor.toXmlString());

    melody.writeState (*xml.createNewChildElement ("Melody"));

    for (auto& [genreId, bits] : workflowProgress)
    {
        auto* w = xml.createNewChildElement ("Workflow");
        w->setAttribute ("genre", genreId);
        w->setAttribute ("done", juce::String ((juce::int64) bits));
    }

    copyXmlToBinary (xml, destData);
}

void MixMentorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName ("MixMentor"))
    {
        const auto& kb = mm::KnowledgeBase::get();
        genreIndex = kb.indexOfGenre (xml->getStringAttribute ("genre"));
        instrumentIndex = juce::jmax (0, kb.indexOfInstrument (xml->getStringAttribute ("instrument")));
        instrumentChosenByUser = xml->getBoolAttribute ("instrumentChosenByUser");

        if (auto* paramsXml = xml->getChildByName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*paramsXml));

        if (auto* m = xml->getChildByName ("Melody"))
            melody.readState (*m);

        advisor.fromXmlString (xml->getStringAttribute ("advisor"));

        workflowProgress.clear();
        for (auto* w : xml->getChildWithTagNameIterator ("Workflow"))
            workflowProgress[w->getStringAttribute ("genre")] = (juce::uint32) w->getStringAttribute ("done").getLargeIntValue();
    }
}

juce::AudioProcessorEditor* MixMentorProcessor::createEditor()
{
    return new MixMentorEditor (*this);
}

MixMentorProcessor::~MixMentorProcessor()
{
    TrackRegistry::get().remove (registryId);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixMentorProcessor();
}

