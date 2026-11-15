// Runs Mix Mentor's analysis code on a WAV file and prints what it measured (JSON).
#include "Analyzer.h"
#include "Meter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
int main (int argc, char** argv)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (argv[1])));
    if (! rd) { std::cerr << "cannot read\n"; return 1; }
    const double sr = rd->sampleRate; const int len = (int) rd->lengthInSamples;
    juce::AudioBuffer<float> buf ((int) rd->numChannels, len);
    rd->read (&buf, 0, len, 0, true, true);
    const float* L = buf.getReadPointer (0); const float* R = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
    const float seconds = argc > 2 ? (float) atof (argv[2]) : 4.0f;

    Analyzer an; an.prepare (sr); an.startCapture (seconds);
    LoudnessMeter m; m.prepare (sr);
    std::vector<MeterFrame> frames;
    for (int pos = 0; pos < len; pos += 512)   // like a DAW: blocks of 512
    {
        const int n = juce::jmin (512, len - pos);
        m.process (L + pos, R + pos, n, [&] (const MeterFrame& f) { if (f.ms / (float) f.n > 1.0e-7f) frames.push_back (f); });
        an.push (L + pos, R + pos, n);
    }
    auto d = LoudnessMeter::compute (frames);

    std::vector<mm::Zone> zones;
    auto addZone = [&] (const char* label, float lo, float hi, const char* kind) { mm::Zone z; z.label = label; z.lowHz = lo; z.highHz = hi; z.kind = kind; z.targetMinDb = -30; z.targetMaxDb = 30; zones.push_back (z); };
    addZone ("sub", 40, 70, "boost"); addZone ("low", 100, 180, "watch"); addZone ("box", 200, 400, "cut");
    addZone ("mid", 800, 1600, "watch"); addZone ("pres", 2000, 4000, "cut"); addZone ("air", 8000, 14000, "boost");
    float ref = 0; auto rs = an.readZones (zones, ref);

    std::cout << "{\"lufs\":" << d.loudnessLufs << ",\"tp\":" << d.peakDb << ",\"rms\":" << d.rmsDb << ",\"crest\":" << d.crestDb
              << ",\"corr\":" << d.correlation << ",\"lowcorr\":" << d.lowCorrelation << ",\"side\":" << d.sideDb
              << ",\"swing\":" << d.swingDb << ",\"heard\":" << an.hasResult() << ",\"zones\":{";
    for (size_t z = 0; z < zones.size(); ++z)
        std::cout << (z ? "," : "") << "\"" << zones[z].label << "\":{\"level\":" << rs[z].levelDb << ",\"bump\":" << rs[z].bumpDb
                  << ",\"iso\":" << rs[z].isolatedDb << ",\"resHz\":" << rs[z].resonanceHz << ",\"resDb\":" << rs[z].resonanceDb << "}";
    std::cout << "},\"bands\":[";
    auto b = an.thirdOctaveBands();
    for (size_t k = 0; k < b.size(); ++k) std::cout << (k ? "," : "") << b[k];
    std::cout << "]}\n";
}
