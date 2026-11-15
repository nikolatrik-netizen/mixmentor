#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

/** What "good" looks like for a sound: a level per 1/6-octave band (dB, same scale as
    Analyzer::levelAt) and how far a band may stray before it's worth fixing.

    Built-in genre targets follow the long-term spectra of commercial recordings measured by
    Pestana, Ma, Reiss et al. ("Spectral characteristics of popular commercial recordings
    1950-2010", AES 135th Convention, 2013, 772 songs):
      - a decay of about 5 dB per octave between 100 Hz and 4 kHz, with per-genre slopes
        (their log-log magnitude slope: pop -0.94, hip-hop -0.81, electronic -0.75, R&B/soul -1.03),
      - a steeper slope above 4 kHz (hip-hop is the brightest, then electronic),
      - a low cut around 60 Hz, with an extended low end for hip-hop and electronic.
    Individual songs vary around this, so the tolerance is a band, not a line (like the
    "typical variation" bands in tonal-balance meters).

    A reference (a song or sample you load) replaces the genre curve with its own measured
    spectrum and a tighter tolerance. */
struct TargetCurve
{
    bool valid = false;
    juce::String name;                 // "Hip-hop / trap (genre)" or the reference's file name
    bool fromReference = false;
    std::vector<float> hz, db;         // 1/6-octave points, 20 Hz - 20 kHz
    std::vector<float> tolerance;      // +-dB per point
    // Reference only: its dynamics/stereo, for comparisons
    float loudnessLufs = -100.0f, crestDb = 0.0f, correlation = 1.0f;

    /** Target level at a frequency (interpolated). */
    float levelAt (float f) const;
    float toleranceAt (float f) const;

    /** The 1/6-octave grid used everywhere. */
    static std::vector<float> grid();

    /** Built-in curve for a genre id (hiphop, pop, house, ...). */
    static TargetCurve forGenre (const juce::String& genreId, const juce::String& genreName);

    /** Measures an audio file (up to 90 s of its loudest part). Empty result if unreadable. */
    static TargetCurve fromAudioFile (const juce::File& file);

    juce::String toString() const;                       // for saving with the project
    static TargetCurve fromString (const juce::String&);
};
