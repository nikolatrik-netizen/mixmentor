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
    float blockPeak = 0.0f, sumSquares = 0.0f;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    const float channelScale = 1.0f / (float) numIn;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numIn; ++ch)
        {
            const float s = buffer.getSample (ch, i);
            blockPeak = juce::jmax (blockPeak, std::abs (s));
            sumSquares += s * s;
            mono += s;
        }
        mono *= channelScale;

        if (i < size1)               fifoBuffer[(size_t) (start1 + i)] = mono;
        else if (i < size1 + size2)  fifoBuffer[(size_t) (start2 + i - size1)] = mono;
    }
    fifo.finishedWrite (size1 + size2);   // if the GUI isn't reading, extra samples are dropped

    latestRms = std::sqrt (sumSquares / (float) (numSamples * numIn));

    float current = peakSinceLastRead.load();
    while (blockPeak > current && ! peakSinceLastRead.compare_exchange_weak (current, blockPeak)) {}
}

int MixMentorProcessor::readAnalysisSamples (float* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    fifo.prepareToRead (juce::jmin (maxSamples, fifo.getNumReady()), start1, size1, start2, size2);

    if (size1 > 0) std::copy_n (fifoBuffer.data() + start1, size1, dest);
    if (size2 > 0) std::copy_n (fifoBuffer.data() + start2, size2, dest + size1);

    fifo.finishedRead (size1 + size2);
    return size1 + size2;
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

    if (fixSession.valid)
    {
        auto* fx = xml.createNewChildElement ("FixSession");
        fx->setAttribute ("genre", fixSession.genreId);
        fx->setAttribute ("instrument", fixSession.instrumentId);
        fx->setAttribute ("good", fixSession.goodZones.joinIntoString ("|"));
        fx->setAttribute ("checks", fixSession.checks);
        for (auto& is : fixSession.issues)
        {
            auto* e = fx->createNewChildElement ("Issue");
            e->setAttribute ("zone", is.zone);
            e->setAttribute ("label", is.label);
            e->setAttribute ("lo", is.lowHz);
            e->setAttribute ("hi", is.highHz);
            e->setAttribute ("focus", is.focusHz);
            e->setAttribute ("action", is.action);
            e->setAttribute ("res", is.resonance);
            e->setAttribute ("bump", is.bump);
            e->setAttribute ("amount", is.amount);
            e->setAttribute ("baseline", is.baseline);
            e->setAttribute ("change", is.latestChange);
            e->setAttribute ("status", is.status);
        }
    }

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

        fixSession = {};
        if (auto* fx = xml->getChildByName ("FixSession"))
        {
            fixSession.valid = true;
            fixSession.genreId = fx->getStringAttribute ("genre");
            fixSession.instrumentId = fx->getStringAttribute ("instrument");
            fixSession.goodZones = juce::StringArray::fromTokens (fx->getStringAttribute ("good"), "|", "");
            fixSession.checks = fx->getIntAttribute ("checks");
            for (auto* e : fx->getChildWithTagNameIterator ("Issue"))
            {
                FixIssue is;
                is.zone = e->getIntAttribute ("zone");
                is.label = e->getStringAttribute ("label");
                is.lowHz = (float) e->getDoubleAttribute ("lo");
                is.highHz = (float) e->getDoubleAttribute ("hi");
                is.focusHz = (float) e->getDoubleAttribute ("focus");
                is.action = e->getIntAttribute ("action", 1);
                is.resonance = e->getBoolAttribute ("res");
                is.bump = e->getBoolAttribute ("bump");
                is.amount = (float) e->getDoubleAttribute ("amount", 3.0);
                is.baseline = (float) e->getDoubleAttribute ("baseline");
                is.latestChange = (float) e->getDoubleAttribute ("change");
                is.status = e->getIntAttribute ("status");
                fixSession.issues.push_back (is);
            }
        }

        workflowProgress.clear();
        for (auto* w : xml->getChildWithTagNameIterator ("Workflow"))
            workflowProgress[w->getStringAttribute ("genre")] = (juce::uint32) w->getStringAttribute ("done").getLargeIntValue();
    }
}

juce::AudioProcessorEditor* MixMentorProcessor::createEditor()
{
    return new MixMentorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixMentorProcessor();
}
