#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "KnowledgeBase.h"

MixMentorProcessor::MixMentorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    mm::KnowledgeBase::get();   // load the knowledge base up front, off the audio thread
}

void MixMentorProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    fifo.reset();
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

    // Audio is passed through untouched; we only measure it.
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
