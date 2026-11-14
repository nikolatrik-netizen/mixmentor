#include "MelodyProcessor.h"
#include "MelodyEditor.h"

MelodyProcessor::MelodyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MelodyProcessor::prepareToPlay (double sampleRate, int)
{
    core.prepare (sampleRate);
}

bool MelodyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MelodyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    core.process (buffer, midi, getPlayHead());
}

void MelodyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("MixMentorMelody");
    core.writeState (xml);
    copyXmlToBinary (xml, dest);
}

void MelodyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName ("MixMentorMelody"))
        core.readState (*xml);
}

juce::AudioProcessorEditor* MelodyProcessor::createEditor()
{
    return new MelodyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MelodyProcessor();
}
