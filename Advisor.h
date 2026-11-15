#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "KnowledgeBase.h"
#include "LiveListener.h"
#include "Targets.h"

/** One thing to fix, with everything needed to show it and to measure it live. */
struct Advice
{
    enum Kind { kRange = 0, kResonance = 1, kMetric = 2 };
    enum Status { open = 0, done = 1 };

    juce::String id;                  // stable key (so a re-scan recognises the same problem)
    int kind = kRange;
    int direction = 1;                // +1 too much (cut / reduce), -1 too little (boost / increase)
    juce::String title;               // "Low-mids too loud"
    juce::String action;              // "EQ: cut 4 dB at 280 Hz, Q 1.2"
    juce::String why;                 // plain-language explanation
    juce::StringArray steps;          // in-depth: how to do it, step by step
    juce::String listenFor, caution;  // in-depth: what you should hear, what to watch out for
    juce::String unit = "dB";
    float lowHz = 0, highHz = 0, focusHz = 0;
    juce::String metric;              // for kind == kMetric: crestHi, swing, lowwidth, ...
    float severity = 0;

    // Measurement: "value" is what the change moves; "goal" is where it counts as fixed.
    float detected = 0;               // value when it was found
    float goal = 0;                   // fixed when direction * (value - goal) <= 0
    float value = 0;                  // latest value (live, beat by beat when possible)
    bool liveThisPass = false;        // value was updated from the current pass's beats

    // Workflow
    int status = open;
    bool checking = false;            // "Done, check it" ticked: waiting for the next full pass
    juce::String checkMessage;        // result of the last check
    bool goalReachedLastPass = false;

    float toGo() const { return (float) direction * (value - goal); }   // > 0: still to do
    float progress() const
    {
        const float total = (float) direction * (detected - goal);
        return total > 1.0e-3f ? juce::jlimit (0.0f, 1.0f, 1.0f - toGo() / total) : 1.0f;
    }
};

/** Finds problems, measures them live and checks fixes.

    Detection compares the last full pass against:
      - a target curve (genre average for the mix bus, or a loaded reference for anything),
      - build-ups / holes against the neighbouring frequencies in the instrument's key zones,
      - narrow resonances that ring all the time,
      - dynamics, loudness and stereo rules per instrument.
    Every problem is then measured with a value an EQ/compressor move changes directly, updated
    on every beat by comparing it with the same beat of the previous pass. */
class Advisor
{
public:
    std::vector<Advice> items;
    TargetCurve target;               // genre or reference (may be invalid for instruments without a reference)
    juce::String genreId, genreName, instrumentId, instrumentName;
    juce::StringArray goodNotes;      // "looks good" summary from the last scan
    int scans = 0;

    void setContext (const juce::String& genre, const juce::String& genreLabel,
                     const juce::String& instrument, const juce::String& instrumentLabel);
    /** Use a reference (or clear it: invalid curve -> genre average for the mix bus, none otherwise). */
    void setReference (const TargetCurve& reference);
    bool hasReference() const { return reference.valid; }
    const TargetCurve& getReference() const { return reference; }
    /** The curve the analyzer should draw as the target band (may be invalid). */
    const TargetCurve& activeTarget() const { return target; }

    /** Look for problems in the last full pass. Done items stay done unless clearly worse again. */
    void scan (const LiveListener::Pass& pass, const mm::Guide* guide);
    /** After every beat: update values from the current pass vs. the same beats of the previous one. */
    void updateLive (const LiveListener& listener);
    /** After every full pass: update values, finish checks. */
    void updateAfterPass (const LiveListener::Pass& pass);
    /** The user ticked "Done, check it". */
    void requestCheck (const juce::String& id);
    void reopen (const juce::String& id);
    void clear() { items.clear(); goodNotes.clear(); scans = 0; }

    int openCount() const;

    /** The numbers under the curve: each frequency region against the target (or, without a target,
        against the rest of the track), from the last loop plus the live change in the current one. */
    struct RegionReading { juce::String name; float lowHz, highHz, value, tolerance; bool vsTarget; int issue = 0; };
    std::vector<RegionReading> regionReadings (const LiveListener& listener) const;

    juce::String toXmlString() const;
    void fromXmlString (const juce::String&);

    /** The value an item measures in a pass (whole pass). */
    static float measure (const Advice& a, const LiveListener::Pass& pass, int fromBeat = 0, int toBeat = 1 << 20);

private:
    TargetCurve reference;
    void addTargetIssues (const LiveListener::Pass& pass, std::vector<Advice>& found);
    void addZoneIssues (const LiveListener::Pass& pass, const mm::Guide& guide, std::vector<Advice>& found);
    void addDynamicsIssues (const DynamicsReading& d, std::vector<Advice>& found);
    void describeRange (Advice& a, float excess, float tol) const;
    void addDetails (Advice& a) const;
};
