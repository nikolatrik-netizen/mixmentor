#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using namespace theme;

namespace
{

    constexpr int sidebarWidth = 252;
    constexpr int topBarHeight = 38;
}

//==============================================================================
GenrePicker::Card::Card (const juce::String& name, const juce::String& b, const juce::String& s)
    : juce::Button (name), bpm (b), summary (s)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GenrePicker::Card::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 6.0f);
    g.setColour (isDown ? col::box : isHighlighted ? col::box : col::panelLight);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (isHighlighted ? col::accent.withAlpha (0.7f) : col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto inner = r.reduced (16.0f, 12.0f);
    g.setColour (col::text);
    g.setFont (font (16.0f, true));
    g.drawText (getButtonText().toLowerCase(), inner.removeFromTop (20.0f), juce::Justification::centredLeft, true);
    g.setColour (col::textFaint);
    g.setFont (font (11.5f));
    g.drawText (bpm.toLowerCase(), inner.removeFromTop (17.0f), juce::Justification::centredLeft, true);
    inner.removeFromTop (6.0f);

    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.append (summary, font (12.0f), col::textDim);
    s.setLineSpacing (1.5f);
    juce::TextLayout layout;
    layout.createLayout (s, inner.getWidth());
    layout.draw (g, inner);
}

GenrePicker::GenrePicker()
{
    for (auto& genre : mm::KnowledgeBase::get().genres())
    {
        auto* c = cards.add (new Card (genre.name, genre.bpm, genre.summary));
        const int index = cards.size() - 1;
        c->onClick = [this, index] { if (onPick) onPick (index); };
        addAndMakeVisible (c);
    }
}

void GenrePicker::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::sideTop, 0.0f, 0.0f, col::sideBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    auto area = b.withHeight (130.0f).withY (40.0f);
    g.setColour (col::accent);
    g.setFont (juce::Font (font (11.0f, true)).withExtraKerningFactor (0.25f));
    g.drawText ("MIX MENTOR", area.withHeight (16.0f).translated (0.0f, -14.0f), juce::Justification::centred, false);
    g.setColour (col::text);
    g.setFont (fontLight (40.0f));
    g.drawText ("what are you making today?", area.removeFromTop (54.0f), juce::Justification::centred, false);
    g.setColour (col::textDim);
    g.setFont (font (14.0f));
    g.drawText ("every tip, frequency zone, level target and workflow step adapts to your genre. you can change it any time.",
                area.removeFromTop (24.0f), juce::Justification::centred, false);
}

void GenrePicker::resized()
{
    constexpr int cols = 4, cardW = 248, cardH = 132, gap = 14;
    const int rowsCount = (cards.size() + cols - 1) / cols;
    const int totalW = cols * cardW + (cols - 1) * gap;
    const int totalH = rowsCount * cardH + (rowsCount - 1) * gap;
    const int x0 = (getWidth() - totalW) / 2;
    const int y0 = juce::jmax (150, (getHeight() - totalH) / 2 + 50);

    for (int i = 0; i < cards.size(); ++i)
    {
        const int row = i / cols, col = i % cols;
        const int inRow = juce::jmin (cols, cards.size() - row * cols);
        const int rowOffset = (cols - inRow) * (cardW + gap) / 2;   // centre an incomplete last row
        cards[i]->setBounds (x0 + rowOffset + col * (cardW + gap), y0 + row * (cardH + gap), cardW, cardH);
    }
}

//==============================================================================
void SegmentedControl::setOptions (const juce::StringArray& names)
{
    options = names;
    repaint();
}

void SegmentedControl::setSelected (int index)
{
    selected = index;
    repaint();
}

juce::Rectangle<float> SegmentedControl::segment (int index) const
{
    auto inner = getLocalBounds().toFloat().reduced (3.0f);
    const float w = inner.getWidth() / (float) juce::jmax (1, options.size());
    return { inner.getX() + w * (float) index, inner.getY(), w, inner.getHeight() };
}

int SegmentedControl::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < options.size(); ++i)
        if (segment (i).contains (p))
            return i;
    return -1;
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawPanel (g, b, col::panel, 6.0f);

    for (int i = 0; i < options.size(); ++i)
    {
        const auto r = segment (i);
        if (i == selected)
        {
            g.setColour (col::box);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (col::accent);
            g.fillRoundedRectangle (r.withSizeKeepingCentre (juce::jmin (36.0f, r.getWidth() - 16.0f), 2.0f)
                                     .withY (r.getBottom() - 4.0f), 1.0f);
        }
        else if (i == hovered)
        {
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (r, 4.0f);
        }

        g.setColour (i == selected ? col::text : i == hovered ? col::text.withAlpha (0.85f) : col::textDim);
        g.setFont (font (13.5f, i == selected));
        g.drawText (options[i].toLowerCase(), r.withTrimmedBottom (2.0f), juce::Justification::centred, false);
    }
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.position);
    if (h != hovered) { hovered = h; repaint(); }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && i != selected)
    {
        selected = i;
        repaint();
        if (onChange) onChange (i);
    }
}

//==============================================================================
GainGauge::GainGauge (juce::AudioProcessorValueTreeState& params)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setDoubleClickReturnValue (true, 0.0);
    knob.setMouseDragSensitivity (220);
    knob.setTooltip ("Trim\nTurns this track's level up or down (+/-24 dB) after your processing, so it reaches the mix at the right level. "
                     "Drag up/down, double-click to reset to 0 dB. You can automate it in your DAW.");
    addAndMakeVisible (knob);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (params, "trim", knob);
    knob.onValueChange = [this] { repaint(); };

    autoButton.setTooltip ("Auto-match target\nSets the trim so this track's peaks land in the middle of the "
                           "target range for your genre and instrument. Play the loudest part first.");
    autoButton.onClick = [this] { if (onAutoMatch) onAutoMatch(); };
    addAndMakeVisible (autoButton);
}

void GainGauge::setState (float peakDb, float holdDb, float targetMin, float targetMax, bool hasGuide)
{
    peak = peakDb; hold = holdDb; minDb = targetMin; maxDb = targetMax; guide = hasGuide;
    autoButton.setEnabled (hasGuide && holdDb > -60.0f);
    repaint();
}

void GainGauge::resized()
{
    knob.setBounds (juce::Rectangle<int> (96, 96).withCentre (centre().toInt()));
    autoButton.setBounds (getLocalBounds().withY (190).withHeight (28).reduced (8, 0));
}

void GainGauge::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    constexpr float floorDb = -48.0f;
    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    constexpr float endAngle   =  juce::MathConstants<float>::pi * 0.75f;
    const auto angleFor = [&] (float db)
    {
        return juce::jmap (juce::jlimit (floorDb, 0.0f, db), floorDb, 0.0f, startAngle, endAngle);
    };

    const auto c = centre();
    const float ringR = 60.0f;

    // Target range (thin outer arc)
    juce::Path target;
    target.addCentredArc (c.x, c.y, ringR + 8.0f, ringR + 8.0f, 0.0f, angleFor (minDb), angleFor (maxDb), true);
    g.setColour (col::good);
    g.strokePath (target, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Level ring: track + live peak + hold tick
    juce::Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::box);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto levelColour = hold > maxDb ? col::bad : hold >= minDb ? col::good : col::text;
    if (peak > floorDb)
    {
        juce::Path level;
        level.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angleFor (peak), true);
        g.setColour (levelColour.withAlpha (0.9f));
        g.strokePath (level, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (hold > floorDb)
    {
        const float a = angleFor (hold);
        g.setColour (col::text);
        g.drawLine ({ c.getPointOnCircumference (ringR - 4.0f, a), c.getPointOnCircumference (ringR + 4.0f, a) }, 2.0f);
    }

    // Read-outs: peak | trim
    auto row = b.withY (152.0f).withHeight (28.0f).reduced (8.0f, 0.0f);
    const auto peakBox = row.removeFromLeft (row.getWidth() * 0.5f - 4.0f);
    row.removeFromLeft (8.0f);
    const bool hasSignal = hold > -60.0f;
    g.setColour (col::textFaint);
    g.setFont (font (10.5f));
    g.drawText ("peak", peakBox.translated (0.0f, -14.0f).withHeight (12.0f), juce::Justification::centred, false);
    g.drawText ("trim", row.translated (0.0f, -14.0f).withHeight (12.0f), juce::Justification::centred, false);
    drawValueBox (g, peakBox, hasSignal ? juce::String (hold, 1) + " dB" : juce::String ("--"), hasSignal ? levelColour : col::textFaint);
    const float trim = (float) knob.getValue();
    drawValueBox (g, row, (trim > 0.0f ? "+" : "") + juce::String (trim, 1) + " dB",
                  std::abs (trim) > 0.05f ? col::accent : col::text);

    // Verdict
    auto area = b.withTrimmedTop (228.0f);
    juce::String verdict, message;
    juce::Colour colour = col::textFaint;
    if (! guide)            { verdict = "-"; }
    else if (! hasSignal)   { verdict = "no signal"; message = "press play to measure the level."; }
    else if (hold > -1.0f)  { verdict = "clipping risk"; colour = col::bad; message = "turn the trim down, or press auto-match."; }
    else if (hold > maxDb)  { verdict = "too hot"; colour = col::bad; message = "turn the trim down about " + juce::String (hold - maxDb, 1) + " dB."; }
    else if (hold >= minDb) { verdict = "good level"; colour = col::good; message = "plenty of headroom for processing."; }
    else                    { verdict = "low"; colour = col::warn; message = juce::String (minDb - hold, 1) + " dB under target. turn the trim up, or leave it if it should sit back."; }

    g.setFont (font (13.5f, true));
    const float vw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), verdict) + 16.0f;
    auto vrc = area.removeFromTop (18.0f).withSizeKeepingCentre (vw, 18.0f);
    g.setColour (colour);
    g.fillEllipse (vrc.getX(), vrc.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (col::text);
    g.drawText (verdict, vrc.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (4.0f);

    g.setColour (col::textDim);
    g.setFont (font (12.0f));
    g.drawFittedText (message, area.removeFromTop (32.0f).toNearestInt(), juce::Justification::centredTop, 2, 1.0f);
    g.setColour (col::textFaint);
    g.setFont (font (11.5f));
    g.drawText ("target " + juce::String ((int) minDb) + " to " + juce::String ((int) maxDb) + " dB peak",
                area.removeFromTop (16.0f), juce::Justification::centred, false);
}

//==============================================================================
FixPanel::FixPanel()
{
    analyseButton.setTooltip ("Analyse\nLoop the loudest 4-8 bars of this track, then press this. "
                              "Mix Mentor listens for 4 seconds and makes a list of ranges to fix.");
    analyseButton.onClick = [this] { if (onAnalyse) onAnalyse(); };
    addAndMakeVisible (analyseButton);

    startOverButton.setTooltip ("Start over\nClear the list and analyse from scratch.");
    startOverButton.onClick = [this] { if (onStartOver) onStartOver(); };
    addChildComponent (startOverButton);
}

void FixPanel::setContent (int newMode, float newProgress, std::vector<Row> newRows, const juce::String& good,
                           const juce::String& warning, bool canRecheck)
{
    mode = newMode; progress = newProgress; rows = std::move (newRows);
    goodText = good; warningText = warning;
    analyseButton.setButtonText (canRecheck ? "re-check" : mode == 2 ? "analyse again" : "analyse");
    analyseButton.setEnabled (mode != 1);
    startOverButton.setVisible (mode == 2);
    resized();
    repaint();
}

void FixPanel::resized()
{
    auto b = getLocalBounds().reduced (16, 14).removeFromBottom (34);
    if (startOverButton.isVisible())
    {
        startOverButton.setBounds (b.removeFromRight (100));
        b.removeFromRight (8);
    }
    analyseButton.setBounds (b);
}

void FixPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), col::panel, 0.0f);
    auto area = getLocalBounds().toFloat().reduced (16.0f, 14.0f);
    area.removeFromBottom (44.0f);   // buttons

    // Header + count badge
    auto top = area.removeFromTop (18.0f);
    drawLabel (g, "fix list", top);
    int toDo = 0, fixedCount = 0;
    for (auto& r : rows) { if (r.state == 3) ++fixedCount; else ++toDo; }
    if (mode == 2)
    {
        const auto txt = rows.empty() ? juce::String ("all good") : toDo == 0 ? juce::String ("all fixed") : juce::String (toDo) + " to fix";
        const auto c = toDo == 0 ? col::good : col::accent;
        g.setFont (font (12.0f, true));
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), txt) + 22.0f;
        const auto badge = top.removeFromRight (w).withSizeKeepingCentre (w, 18.0f);
        g.setColour (c.withAlpha (0.15f));
        g.fillRoundedRectangle (badge, 9.0f);
        g.setColour (c);
        g.fillEllipse (badge.getX() + 7.0f, badge.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.drawText (txt, badge.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);
    }
    area.removeFromTop (12.0f);

    auto wrapped = [&] (const juce::String& text, float size, juce::Colour c, juce::Rectangle<float> r, bool draw = true)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        s.append (text, font (size), c);
        s.setLineSpacing (1.5f);
        juce::TextLayout tl;
        tl.createLayout (s, r.getWidth());
        if (draw) tl.draw (g, r.withHeight (juce::jmin (tl.getHeight(), r.getHeight())));
        return tl.getHeight();
    };

    if (mode == 0)
    {
        wrapped ("How it works:\n\n"
                 "1.  Put Mix Mentor LAST on the track, after your EQ and compressor.\n"
                 "2.  Loop the loudest 4-8 bars and press play.\n"
                 "3.  Press analyse. It listens for 4 seconds and lists the ranges to cut or boost.\n"
                 "4.  Make your EQ moves, then press re-check. Each range is marked fixed once it really changed.",
                 13.0f, col::textDim, area);
        return;
    }
    if (mode == 1)
    {
        g.setColour (col::text);
        g.setFont (font (15.0f, true));
        g.drawText ("listening...", area.removeFromTop (22.0f), juce::Justification::centredLeft, false);
        area.removeFromTop (8.0f);
        auto bar = area.removeFromTop (6.0f);
        g.setColour (col::box);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (col::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3.0f);
        area.removeFromTop (12.0f);
        wrapped ("Keep the loop playing. Only time with sound counts, so silence just pauses it.", 12.5f, col::textDim, area);
        return;
    }

    if (warningText.isNotEmpty())
    {
        const float h = wrapped (warningText, 12.5f, col::bad, area.reduced (10.0f, 0.0f), false) + 16.0f;
        auto box = area.removeFromTop (h);
        g.setColour (col::bad.withAlpha (0.12f));
        g.fillRoundedRectangle (box, 5.0f);
        wrapped (warningText, 12.5f, col::bad.brighter (0.3f), box.reduced (10.0f, 8.0f));
        area.removeFromTop (8.0f);
    }

    // Open items first, then fixed ones (compact)
    for (int pass = 0; pass < 2; ++pass)
    {
        for (auto& r : rows)
        {
            const bool isFixed = r.state == 3;
            if ((pass == 0) == isFixed || area.getHeight() < 30.0f)
                continue;

            juce::Colour c = r.state == 1 || r.state == 7 ? col::bad : r.state == 2 ? col::warn
                           : r.state == 3 ? col::good : r.state == 5 ? col::accent : col::textDim;

            if (isFixed)
            {
                auto line = area.removeFromTop (22.0f);
                area.removeFromTop (4.0f);
                g.setColour (col::good);
                g.fillEllipse (line.getX(), line.getCentreY() - 4.0f, 8.0f, 8.0f);
                g.setFont (font (13.0f, true));
                g.setColour (col::text.withAlpha (0.8f));
                g.drawText (r.range + "   " + r.title, line.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);
                continue;
            }

            const float bodyH = wrapped (r.body, 12.5f, col::textDim, area.reduced (14.0f, 0.0f), false);
            const float h = juce::jmin (area.getHeight(), 22.0f + 18.0f + bodyH + 16.0f);
            auto card = area.removeFromTop (h);
            area.removeFromTop (8.0f);

            g.setColour (col::box.withAlpha (0.6f));
            g.fillRoundedRectangle (card, 5.0f);
            g.setColour (c);
            g.fillRoundedRectangle (card.getX(), card.getY() + 6.0f, 3.0f, card.getHeight() - 12.0f, 1.5f);

            auto inner = card.reduced (14.0f, 7.0f);
            auto row = inner.removeFromTop (22.0f);
            g.setColour (col::text);
            g.setFont (font (17.0f, true));
            g.drawText (r.range, row, juce::Justification::centredLeft, false);
            if (r.chip.isNotEmpty())
            {
                g.setFont (font (12.0f, true));
                const float cw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), r.chip) + 16.0f;
                const auto chip = row.removeFromRight (cw).withSizeKeepingCentre (cw, 20.0f);
                g.setColour (c.withAlpha (0.16f));
                g.fillRoundedRectangle (chip, 10.0f);
                g.setColour (c);
                g.drawText (r.chip, chip, juce::Justification::centred, false);
            }
            g.setColour (c == col::textDim ? col::textDim : c.brighter (0.2f));
            g.setFont (font (12.5f, true));
            g.drawText (r.title, inner.removeFromTop (18.0f), juce::Justification::centredLeft, true);
            wrapped (r.body, 12.5f, col::textDim, inner);
        }
        if (pass == 0 && fixedCount > 0 && area.getHeight() > 30.0f)
        {
            area.removeFromTop (4.0f);
            drawLabel (g, "fixed", area.removeFromTop (18.0f), col::textFaint);
            area.removeFromTop (4.0f);
        }
    }

    if (goodText.isNotEmpty() && area.getHeight() > 30.0f)
    {
        area.removeFromTop (6.0f);
        wrapped ("Looks good: " + goodText, 12.5f, col::textFaint, area);
    }
}

//==============================================================================
void WorkflowList::setSteps (const std::vector<mm::WorkflowStep>& s, juce::uint32 doneBits)
{
    steps = s;
    done = doneBits;
    setWidthAndLayout (getWidth());
}

void WorkflowList::setWidthAndLayout (int width)
{
    rows.clear();
    float y = 0.0f;
    const float w = (float) juce::jmax (100, width);
    juce::String phase;

    for (size_t i = 0; i < steps.size(); ++i)
    {
        const auto& st = steps[i];
        if (st.phase != phase)
        {
            phase = st.phase;
            Row header;
            header.phaseHeader = true;
            header.phase = phase;
            header.bounds = { 0.0f, y + (i == 0 ? 0.0f : 10.0f), w, 22.0f };
            y = header.bounds.getBottom() + 4.0f;
            rows.push_back (std::move (header));
        }

        const bool isDone = (done >> i) & 1u;
        juce::AttributedString text;
        text.setWordWrap (juce::AttributedString::byWord);
        text.append (juce::String ((int) i + 1) + ".  " + st.title + "\n",
                     font (13.5f, true), isDone ? col::textFaint : col::text);
        text.append (st.text, font (12.5f), isDone ? col::textFaint : col::textDim);
        text.setLineSpacing (1.5f);

        Row row;
        row.step = (int) i;
        row.layout.createLayout (text, w - 44.0f);
        row.bounds = { 0.0f, y, w, row.layout.getHeight() + 14.0f };
        y = row.bounds.getBottom() + 2.0f;
        rows.push_back (std::move (row));
    }

    setSize ((int) w, (int) y + 8);
    repaint();
}

void WorkflowList::paint (juce::Graphics& g)
{
    for (size_t r = 0; r < rows.size(); ++r)
    {
        auto& row = rows[r];
        if (row.phaseHeader)
        {
            drawLabel (g, row.phase, row.bounds, col::textFaint);
            g.setColour (col::border.withAlpha (0.5f));
            g.fillRect (row.bounds.getX() + 70.0f, row.bounds.getCentreY(), row.bounds.getWidth() - 70.0f, 1.0f);
            continue;
        }

        const bool isDone = (done >> row.step) & 1u;
        if ((int) r == hovered)
        {
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (row.bounds, 4.0f);
        }

        // Check circle
        const auto box = juce::Rectangle<float> (8.0f, row.bounds.getY() + 8.0f, 18.0f, 18.0f);
        if (isDone)
        {
            g.setColour (col::good);
            g.fillEllipse (box);
            juce::Path tick;
            tick.startNewSubPath (box.getX() + 5.0f, box.getCentreY());
            tick.lineTo (box.getX() + 8.0f, box.getCentreY() + 3.5f);
            tick.lineTo (box.getRight() - 4.5f, box.getY() + 5.5f);
            g.setColour (juce::Colours::white);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (col::boxHover);
            g.fillEllipse (box);
            g.setColour (col::border);
            g.drawEllipse (box.reduced (0.5f), 1.2f);
        }

        row.layout.draw (g, row.bounds.withTrimmedLeft (36.0f).withTrimmedTop (7.0f));
    }
}

void WorkflowList::mouseDown (const juce::MouseEvent& e)
{
    for (auto& row : rows)
        if (! row.phaseHeader && row.bounds.contains (e.position))
        {
            if (onToggle) onToggle (row.step);
            return;
        }
}

void WorkflowList::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t r = 0; r < rows.size(); ++r)
        if (! rows[r].phaseHeader && rows[r].bounds.contains (e.position))
            h = (int) r;
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void WorkflowList::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

//==============================================================================
namespace
{
    /** Fills a read-only TextEditor, turning ALL-CAPS lines into small bold headings. */
    void setGuideText (juce::TextEditor& editor, const juce::String& text)
    {
        editor.clear();
        auto lines = juce::StringArray::fromLines (text);
        for (int i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            const bool heading = line.length() > 2 && line == line.toUpperCase() && line.containsAnyOf ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
            editor.setFont (heading ? font (12.0f, true) : font (14.5f));
            editor.setColour (juce::TextEditor::textColourId, heading ? col::textDim : col::text);
            editor.insertTextAtCaret (line + (i < lines.size() - 1 ? "\n" : ""));
        }
        editor.moveCaretToTop (false);
    }
}

//==============================================================================
MixMentorEditor::MixMentorEditor (MixMentorProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel.get());
    tooltipWindow.setOpaque (false);

    const auto& kb = mm::KnowledgeBase::get();

    for (size_t i = 0; i < kb.genres().size(); ++i)
        genreBox.addItem (kb.genres()[i].name, (int) i + 1);
    genreBox.setTextWhenNothingSelected ("choose genre...");
    genreBox.setTooltip ("Genre\nThe genre you're making. Tips, zones, level targets and the workflow all adapt to it.");
    genreBox.onChange = [this]
    {
        processor.genreIndex = genreBox.getSelectedId() - 1;
        refreshContent();
    };
    addAndMakeVisible (genreBox);

    for (size_t i = 0; i < kb.instruments().size(); ++i)
        instrumentBox.addItem (kb.instruments()[i].name, (int) i + 1);
    instrumentBox.setTooltip ("Instrument\nWhat's on this track? Guessed from the track name when your DAW provides it.");
    instrumentBox.onChange = [this]
    {
        processor.instrumentIndex = instrumentBox.getSelectedId() - 1;
        processor.instrumentChosenByUser = true;
        refreshContent();
    };
    addAndMakeVisible (instrumentBox);

    guideButton.getProperties().set ("flat", true);
    guideButton.setTooltip ("How to read the analyzer\n"
                            "White line: the average of the last few seconds.\n"
                            "Blue area: the live sound.\n"
                            "Dashed boxes: the target range for your genre and instrument.\n"
                            "Coloured bars: where each zone measures now (green ok, red too much, orange too little).\n\n"
                            "Press analyse in the fix list to get a list of ranges to cut or boost. Hover a zone for its tip.\n\n"
                            "Put Mix Mentor last on the track (after your EQ and compressor), "
                            "otherwise it can't hear your changes.");
    addAndMakeVisible (guideButton);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (gauge);
    gauge.onAutoMatch = [this]
    {
        const auto* guide = mm::KnowledgeBase::get().findGuide (processor.genreIndex.load(), processor.instrumentIndex.load());
        if (guide == nullptr || holdDb < -60.0f)
            return;
        // The meter reads after the trim, so add the difference between target centre and current peak.
        const float targetCentre = 0.5f * (guide->peakMinDb + guide->peakMaxDb);
        const float newTrim = juce::jlimit (-24.0f, 24.0f, processor.trimDb->load() + (targetCentre - holdDb));
        processor.trimParam->beginChangeGesture();
        processor.trimParam->setValueNotifyingHost (processor.trimParam->convertTo0to1 (newTrim));
        processor.trimParam->endChangeGesture();
        holdDb = -100.0f;   // re-measure from scratch with the new trim
        holdFrames = 0;
    };
    addAndMakeVisible (fixPanel);
    fixPanel.onAnalyse = [this]
    {
        bool anyOpen = false;
        for (auto& is : processor.fixSession.issues) anyOpen = anyOpen || is.status != 1;
        startCapture (processor.fixSession.valid && fixListMatchesSettings() && anyOpen ? 2 : 1);
    };
    fixPanel.onStartOver = [this]
    {
        processor.fixSession = {};
        spectrum.resetAnalysis();
        refreshFixPanel();
    };

    juce::StringArray processNames;
    for (auto& proc : kb.processes())
        processNames.add (proc.name);
    processTabs.setOptions (processNames);
    processTabs.onChange = [this] (int index) { currentProcess = index; refreshContent(); };
    addAndMakeVisible (processTabs);

    tipText.setMultiLine (true, true);
    tipText.setReadOnly (true);
    tipText.setCaretVisible (false);
    tipText.setScrollbarsShown (true);
    tipText.setFont (font (14.5f));
    tipText.setLineSpacing (1.3f);
    tipText.setBorder (juce::BorderSize<int> (0));
    tipText.setIndents (0, 0);
    tipText.setMouseCursor (juce::MouseCursor::NormalCursor);
    addAndMakeVisible (tipText);

    workflow.onToggle = [this] (int step)
    {
        const auto& genres = mm::KnowledgeBase::get().genres();
        const int g = processor.genreIndex.load();
        if (! juce::isPositiveAndBelow (g, (int) genres.size()))
            return;
        processor.workflowProgress[genres[(size_t) g].id] ^= (1u << step);
        workflow.setSteps (genres[(size_t) g].workflow, workflowBits());
        repaint (guidePanel);
    };
    workflowView.setViewedComponent (&workflow, false);
    workflowView.setScrollBarsShown (true, false);
    workflowView.setScrollBarThickness (8);
    addChildComponent (workflowView);

    picker.onPick = [this] (int g)
    {
        processor.genreIndex = g;
        picker.setVisible (false);
        refreshContent();
    };
    addChildComponent (picker);

    setResizable (true, true);
    setResizeLimits (1200, 720, 2000, 1300);
    setSize (1320, 800);

    refreshContent();
    startTimerHz (30);
}

MixMentorEditor::~MixMentorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

bool MixMentorEditor::isWorkflowTab() const
{
    const auto& procs = mm::KnowledgeBase::get().processes();
    return juce::isPositiveAndBelow (currentProcess, (int) procs.size()) && procs[(size_t) currentProcess].id == "workflow";
}

bool MixMentorEditor::isTheoryTab() const
{
    const auto& procs = mm::KnowledgeBase::get().processes();
    return juce::isPositiveAndBelow (currentProcess, (int) procs.size()) && procs[(size_t) currentProcess].id == "theory";
}

juce::uint32 MixMentorEditor::workflowBits() const
{
    const auto& genres = mm::KnowledgeBase::get().genres();
    const int g = processor.genreIndex.load();
    if (! juce::isPositiveAndBelow (g, (int) genres.size()))
        return 0;
    auto it = processor.workflowProgress.find (genres[(size_t) g].id);
    return it != processor.workflowProgress.end() ? it->second : 0u;
}

void MixMentorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::window);

    // Sidebar
    g.setGradientFill (juce::ColourGradient (col::sideTop, 0.0f, 0.0f, col::sideBottom, 0.0f, (float) getHeight(), false));
    g.fillRect (sidebar);
    g.setColour (col::border.withAlpha (0.7f));
    g.fillRect (sidebar.getRight() - 1, 0, 1, getHeight());

    auto s = sidebar.toFloat().reduced (22.0f, 0.0f);
    drawLogoMark (g, juce::Rectangle<float> (s.getX(), 22.0f, 52.0f, 52.0f));
    g.setColour (col::text);
    g.setFont (fontLight (29.0f));
    g.drawText ("mix mentor", s.withTrimmedLeft (64.0f).withY (22.0f).withHeight (34.0f), juce::Justification::centredLeft, false);
    const float wordW = (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (fontLight (29.0f)), "mix mentor");
    g.setColour (col::accent);
    g.fillEllipse (s.getX() + 64.0f + wordW + 5.0f, 44.0f, 6.0f, 6.0f);
    g.setColour (col::textFaint);
    g.setFont (font (11.0f));
    g.drawText ("learn while you mix   v0.6", s.withTrimmedLeft (65.0f).withY (56.0f).withHeight (16.0f), juce::Justification::centredLeft, false);

    g.setColour (col::border.withAlpha (0.6f));
    g.fillRect (s.getX(), 94.0f, s.getWidth(), 1.0f);

    drawLabel (g, "genre", genreBox.getBounds().toFloat().translated (0.0f, -22.0f).withHeight (18.0f));
    drawLabel (g, "instrument", instrumentBox.getBounds().toFloat().translated (0.0f, -22.0f).withHeight (18.0f));

    // Tempo read-out
    drawLabel (g, "tempo", s.withY (234.0f).withHeight (18.0f));
    {
        const auto box = s.withY (254.0f).withHeight (42.0f);
        g.setColour (col::box);
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (col::border.withAlpha (0.6f));
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (col::text);
        g.setFont (font (12.5f));
        g.drawFittedText (bpmText.isNotEmpty() ? bpmText.toLowerCase() : juce::String ("-"),
                          box.reduced (8.0f, 3.0f).toNearestInt(), juce::Justification::centred, 2, 1.0f);
    }

    g.setColour (col::border.withAlpha (0.6f));
    g.fillRect (s.getX(), 310.0f, s.getWidth(), 1.0f);
    drawLabel (g, "gain staging", s.withY (320.0f).withHeight (18.0f));

    // Key elements from the genre guide, as small chips
    if (! elements.isEmpty())
    {
        auto e = elementsArea.toFloat();
        g.setColour (col::border.withAlpha (0.6f));
        g.fillRect (e.getX(), e.getY(), e.getWidth(), 1.0f);
        drawLabel (g, "key elements", e.removeFromTop (32.0f).withTrimmedTop (10.0f));

        g.setFont (font (11.5f));
        float x = e.getX(), y = e.getY();
        for (auto& el : elements)
        {
            const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), el) + 16.0f;
            if (x + w > e.getRight()) { x = e.getX(); y += 26.0f; }
            if (y + 21.0f > e.getBottom()) break;
            const juce::Rectangle<float> chip (x, y, w, 21.0f);
            g.setColour (col::box.withAlpha (0.8f));
            g.fillRoundedRectangle (chip, 10.5f);
            g.setColour (col::border.withAlpha (0.5f));
            g.drawRoundedRectangle (chip.reduced (0.5f), 10.5f, 1.0f);
            g.setColour (col::textDim);
            g.drawText (el, chip, juce::Justification::centred, false);
            x += w + 6.0f;
        }
    }

    // Top bar over the analyzer
    g.setColour (col::topBar);
    g.fillRect (topBar);
    g.setColour (col::border);
    g.fillRect (topBar.getX(), topBar.getBottom() - 1, topBar.getWidth(), 1);
    auto tb = topBar.toFloat().reduced (16.0f, 0.0f);
    g.setColour (col::accent);
    g.setFont (font (14.0f, true));
    g.drawText (genreName.toLowerCase(), tb.removeFromLeft (180.0f), juce::Justification::centredLeft, true);
    g.setColour (col::textDim);
    g.setFont (font (12.5f));
    g.drawText (summaryText.toLowerCase(), tb.withTrimmedRight (150.0f), juce::Justification::centredLeft, true);

    // Guide panel
    drawPanel (g, guidePanel.toFloat());
    auto gp = guidePanel.toFloat().reduced (18.0f, 14.0f);
    auto header = gp.removeFromTop (18.0f);
    drawLabel (g, isWorkflowTab() ? "workflow" : isTheoryTab() ? "theory" : "guide", header);

    if (isWorkflowTab())
    {
        const auto& genres = mm::KnowledgeBase::get().genres();
        const int gi = processor.genreIndex.load();
        const int total = juce::isPositiveAndBelow (gi, (int) genres.size()) ? (int) genres[(size_t) gi].workflow.size() : 0;
        int doneCount = 0;
        for (int i = 0; i < total; ++i)
            doneCount += (int) ((workflowBits() >> i) & 1u);

        auto pr = header.removeFromRight (200.0f);
        g.setColour (col::textDim);
        g.setFont (font (12.5f));
        g.drawText (juce::String (doneCount) + " of " + juce::String (total) + " done", pr.removeFromLeft (90.0f),
                    juce::Justification::centredRight, false);
        auto bar = pr.withSizeKeepingCentre (pr.getWidth() - 14.0f, 5.0f).withX (pr.getX() + 12.0f);
        g.setColour (col::border.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 2.5f);
        if (total > 0 && doneCount > 0)
        {
            g.setColour (col::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) doneCount / (float) total), 2.5f);
        }
    }

    gp.removeFromTop (8.0f);
    g.setColour (col::text);
    g.setFont (font (17.0f, true));
    g.drawText (guideTitle.toLowerCase(), gp.removeFromTop (24.0f), juce::Justification::centredLeft, true);
}

void MixMentorEditor::resized()
{
    auto area = getLocalBounds();
    sidebar = area.removeFromLeft (sidebarWidth);

    auto s = sidebar.reduced (22, 0);
    genreBox.setBounds (s.getX(), 128, s.getWidth(), 30);
    instrumentBox.setBounds (s.getX(), 188, s.getWidth(), 30);
    gauge.setBounds (s.getX(), 344, s.getWidth(), 300);
    elementsArea = juce::Rectangle<int> (s.getX(), 652, s.getWidth(), getHeight() - 652 - 10);

    topBar = area.removeFromTop (topBarHeight);
    {
        auto tb = topBar.reduced (8, 5);
        guideButton.setBounds (tb.removeFromRight (30));
    }

    auto upper = area.removeFromTop ((int) (area.getHeight() * 0.55f));
    fixPanel.setBounds (upper.removeFromRight (330));
    spectrum.setBounds (upper);

    area = area.reduced (16, 14);
    processTabs.setBounds (area.removeFromTop (36));
    area.removeFromTop (12);
    guidePanel = area;

    auto content = guidePanel.reduced (18, 14).withTrimmedTop (18 + 8 + 24 + 10);
    content = content.withWidth (juce::jmin (content.getWidth(), 880));   // keep lines readable
    tipText.setBounds (content);
    workflowView.setBounds (content.withTrimmedRight (-10));
    workflow.setWidthAndLayout (content.getWidth() - 4);

    picker.setBounds (getLocalBounds());
}

void MixMentorEditor::refreshContent()
{
    const auto& kb = mm::KnowledgeBase::get();
    const int g = processor.genreIndex.load();
    const int i = processor.instrumentIndex.load();
    shownGenre = g;
    shownInstrument = i;

    picker.setVisible (g < 0);
    genreBox.setSelectedId (g + 1, juce::dontSendNotification);
    instrumentBox.setSelectedId (i + 1, juce::dontSendNotification);
    processTabs.setSelected (currentProcess);

    const bool validGenre = juce::isPositiveAndBelow (g, (int) kb.genres().size());
    if (validGenre)
    {
        const auto& genre = kb.genres()[(size_t) g];
        summaryText = genre.summary;
        bpmText = genre.bpm;
        genreName = genre.name;
        elements = genre.elements;
    }

    const bool wf = isWorkflowTab();
    tipText.setVisible (! wf);
    workflowView.setVisible (wf);

    const auto* guide = kb.findGuide (g, i);
    if (guide == nullptr)
    {
        spectrum.setZones ({});
        guideTitle = {};
        tipText.setText ("No guide for this combination yet.", false);
        repaint();
        return;
    }

    spectrum.setZones (guide->zones);

    const auto& process = kb.processes()[(size_t) currentProcess];
    const auto& genre = kb.genres()[(size_t) g];
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");

    if (wf)
    {
        guideTitle = juce::String ((int) genre.workflow.size()) + "-step workflow" + dot + genre.name;
        workflow.setSteps (genre.workflow, workflowBits());
    }
    else if (isTheoryTab())
    {
        guideTitle = "music theory" + dot + genre.name;
        setGuideText (tipText, genre.theoryText);
    }
    else
    {
        guideTitle = process.name + " for " + kb.instruments()[(size_t) i].name + dot + genre.name;
        auto it = guide->tips.find (process.id);
        setGuideText (tipText, it != guide->tips.end() ? it->second : juce::String ("No tip yet."));
    }

    updateGainStatus();
    refreshFixPanel();
    repaint();
}

void MixMentorEditor::updateGainStatus()
{
    const auto* guide = mm::KnowledgeBase::get().findGuide (processor.genreIndex.load(), processor.instrumentIndex.load());
    gauge.setState (peakDisplayDb, holdDb,
                    guide != nullptr ? guide->peakMinDb : -12.0f,
                    guide != nullptr ? guide->peakMaxDb : -6.0f,
                    guide != nullptr);
}

bool MixMentorEditor::fixListMatchesSettings() const
{
    const auto& kb = mm::KnowledgeBase::get();
    const int g = processor.genreIndex.load(), i = processor.instrumentIndex.load();
    if (! juce::isPositiveAndBelow (g, (int) kb.genres().size()) || ! juce::isPositiveAndBelow (i, (int) kb.instruments().size()))
        return false;
    return processor.fixSession.genreId == kb.genres()[(size_t) g].id
        && processor.fixSession.instrumentId == kb.instruments()[(size_t) i].id;
}

void MixMentorEditor::startCapture (int kind)
{
    captureKind = kind;
    spectrum.startCapture (4.0f);
    refreshFixPanel();
}

void MixMentorEditor::captureFinished()
{
    if (captureKind == 2) recheckFixList();
    else                  buildFixList();
    captureKind = 0;
    refreshFixPanel();
}

void MixMentorEditor::buildFixList()
{
    const auto& kb = mm::KnowledgeBase::get();
    const int g = processor.genreIndex.load(), i = processor.instrumentIndex.load();
    const auto* guide = kb.findGuide (g, i);
    const auto& zones = spectrum.getZones();
    const auto& readings = spectrum.getReadings();
    if (guide == nullptr || zones.size() != readings.size())
        return;

    FixSession fs;
    fs.valid = true;
    fs.genreId = kb.genres()[(size_t) g].id;
    fs.instrumentId = kb.instruments()[(size_t) i].id;

    struct Candidate { float severity; FixIssue issue; };
    std::vector<Candidate> found;
    constexpr float tolerance = 3.0f;   // only clear problems make the list

    for (size_t z = 0; z < zones.size(); ++z)
    {
        const auto& zone = zones[z];
        const auto& r = readings[z];
        bool flagged = false;

        // Too much: a clear build-up compared with the neighbouring frequencies. Boost zones (like a
        // kick's thump) are supposed to stick out, so they're only flagged when it's extreme.
        const float bumpLimit = zone.kind == "boost" ? 9.0f : zone.kind == "watch" ? 3.5f : 2.5f;
        const bool tooMuch = r.bumpDb > bumpLimit
                          || (r.relativeDb > zone.targetMaxDb + 6.0f && r.bumpDb > 1.0f);
        // Too little: a clear hole, or far below the genre's typical level.
        const bool tooLittle = ! tooMuch && ((zone.kind == "boost" && r.bumpDb < -5.0f)
                                             || r.relativeDb < zone.targetMinDb - tolerance);

        if (tooMuch || tooLittle)
        {
            FixIssue is;
            is.zone = (int) z;
            is.label = zone.label;
            is.lowHz = zone.lowHz; is.highHz = zone.highHz; is.focusHz = r.peakHz;
            is.action = tooMuch ? 1 : -1;
            is.bump = tooMuch ? true : (zone.kind == "boost" && r.bumpDb < -5.0f);
            const float dev = is.bump ? std::abs (r.bumpDb) : std::abs (r.relativeDb - zone.targetMinDb);
            // An EQ bell only partly covers a zone, so suggest a bit more than the measured bump.
            is.amount = juce::jlimit (2.0f, 6.0f, std::round (is.bump ? dev + 1.0f : dev * 0.8f));
            is.baseline = is.bump ? r.bumpDb : r.relativeDb;
            found.push_back ({ dev, is });
            flagged = true;
        }

        if (r.resonanceHz > 0.0f && r.resonanceDb >= 12.0f)
        {
            FixIssue is;
            is.zone = (int) z;
            is.label = zone.label;
            is.resonance = true;
            is.focusHz = r.resonanceHz;
            is.lowHz = r.resonanceHz * 0.94f; is.highHz = r.resonanceHz * 1.06f;
            is.action = 1;
            is.amount = r.resonanceDb >= 16.0f ? 5.0f : 3.0f;
            is.baseline = r.resonanceDb;
            found.push_back ({ r.resonanceDb - 8.0f, is });
            flagged = true;
        }

        if (! flagged)
            fs.goodZones.add (zone.label.toLowerCase());
    }

    std::sort (found.begin(), found.end(), [] (auto& a, auto& b) { return a.severity > b.severity; });
    for (size_t k = 0; k < found.size() && k < 4; ++k)
        fs.issues.push_back (found[k].issue);

    processor.fixSession = fs;
}

void MixMentorEditor::recheckFixList()
{
    auto& fs = processor.fixSession;
    const auto& zones = spectrum.getZones();
    const auto& readings = spectrum.getReadings();
    if (zones.size() != readings.size())
        return;

    for (auto& is : fs.issues)
    {
        if (is.status == 1 || ! juce::isPositiveAndBelow (is.zone, (int) readings.size()))
            continue;   // fixed stays fixed

        const auto& zone = zones[(size_t) is.zone];
        const auto& r = readings[(size_t) is.zone];

        if (is.resonance)
        {
            // How much less the ringing frequency sticks out than before (0 if it's gone).
            const bool stillThere = r.resonanceHz > 0.0f && std::abs (r.resonanceHz - is.focusHz) < is.focusHz * 0.08f;
            const float now = stillThere ? r.resonanceDb : 0.0f;
            const float change = is.baseline - now;
            is.latestChange = change;
            is.status = (! stillThere || change >= 3.0f) ? 1 : change >= 1.0f ? 2 : change <= -2.0f ? 4 : 3;
            continue;
        }

        const float now = is.bump ? r.bumpDb : r.relativeDb;
        const float change = (is.baseline - now) * (float) is.action;   // > 0 = moved the right way
        const bool inRange = is.bump ? (is.action > 0 ? now <= 1.5f : now >= -1.5f)
                                     : (is.action > 0 ? now <= zone.targetMaxDb + 1.0f : now >= zone.targetMinDb - 1.0f);
        const float neededChange = is.bump ? juce::jmax (1.5f, std::abs (is.baseline) * 0.6f)
                                           : juce::jmax (2.0f, is.amount * 0.6f);
        is.latestChange = change;
        if (inRange || change >= neededChange)                       is.status = 1;   // fixed
        else if (change >= 0.8f)                                     is.status = 2;   // improved
        else if (change <= -1.5f)                                    is.status = 4;   // worse
        else                                                         is.status = 3;   // no change heard
    }
    ++fs.checks;
}

void MixMentorEditor::refreshFixPanel()
{
    const auto& fs = processor.fixSession;
    std::vector<ZoneMark> zoneMarks (spectrum.getZones().size());

    if (spectrum.isCapturing())
    {
        fixPanel.setContent (1, spectrum.getCaptureProgress(), {}, {}, {}, fs.valid && fixListMatchesSettings());
        spectrum.setZoneMarks (zoneMarks);
        return;
    }

    if (! fs.valid || ! fixListMatchesSettings())
    {
        fixPanel.setContent (0, 0.0f, {}, {}, {}, false);
        spectrum.setZoneMarks (zoneMarks);
        return;
    }

    auto fmtRange = [] (const FixIssue& is)
    {
        if (is.resonance) return SpectrumView::formatHz (is.focusHz);
        auto lo = SpectrumView::formatHz (is.lowHz), hi = SpectrumView::formatHz (is.highHz);
        if (lo.endsWith (" Hz") && hi.endsWith (" Hz")) lo = lo.dropLastCharacters (3);
        if (lo.endsWith (" kHz") && hi.endsWith (" kHz")) lo = lo.dropLastCharacters (4);
        return lo + "-" + hi;
    };

    const auto& kb = mm::KnowledgeBase::get();
    const auto* guide = kb.findGuide (processor.genreIndex.load(), processor.instrumentIndex.load());

    std::vector<FixPanel::Row> rows;
    int noChange = 0, open = 0;
    for (auto& is : fs.issues)
    {
        FixPanel::Row row;
        row.range = fmtRange (is);
        const auto verb = is.action > 0 ? juce::String ("cut") : juce::String ("boost");
        const auto amt = juce::String (juce::roundToInt (is.amount));
        const auto label = is.resonance ? juce::String ("ringing in ") + is.label.toLowerCase() : is.label.toLowerCase();
        const auto moved = juce::String (std::abs (is.latestChange), 1);

        // What to do: specific instructions for special zones, otherwise a plain EQ move.
        juce::String how;
        const mm::Zone* zone = (guide != nullptr && juce::isPositiveAndBelow (is.zone, (int) guide->zones.size()))
                                   ? &guide->zones[(size_t) is.zone] : nullptr;
        if (is.resonance)
            how = "Cut " + amt + " dB at " + SpectrumView::formatHz (is.focusHz) + " with a narrow bell (Q 5-8).";
        else if (zone != nullptr && (zone->label.equalsIgnoreCase ("rumble") || zone->label.equalsIgnoreCase ("low cut")))
            how = "Add a high-pass filter and raise it to around " + SpectrumView::formatHz (is.highHz) + ".";
        else if (zone != nullptr && zone->label.equalsIgnoreCase ("sibilance"))
            how = "Use a de-esser set to around " + SpectrumView::formatHz (is.focusHz) + ".";
        else if (is.action < 0 && zone != nullptr && zone->tooLittle.containsIgnoreCase ("saturation"))
            how = "Add saturation, or boost about " + amt + " dB between " + fmtRange (is) + ".";
        else
            how = juce::String (is.action > 0 ? "Cut" : "Boost") + " about " + amt + " dB between " + fmtRange (is)
                + " (centre ~" + SpectrumView::formatHz (is.focusHz) + ") with a wide bell.";

        switch (is.status)
        {
            case 1:
                row.state = 3;
                row.title = label + (fs.checks > 0 ? "  (moved " + moved + " dB)" : juce::String());
                break;
            case 2:
                row.state = 5; ++open;
                row.chip = verb + " a bit more";
                row.title = label + ": better, moved " + moved + " dB";
                row.body = "Going the right way. Push a little further, then re-check.";
                break;
            case 3:
                row.state = 6; ++open; ++noChange;
                row.chip = verb + " ~" + amt + " dB";
                row.title = label + ": no change heard";
                row.body = "I didn't hear a change here. Is your EQ band between " + fmtRange (is)
                         + "? Is Mix Mentor after your EQ? " + how;
                break;
            case 4:
                row.state = 7; ++open;
                row.chip = verb + " ~" + amt + " dB";
                row.title = label + ": went the wrong way (" + moved + " dB)";
                row.body = how;
                break;
            default:
                row.state = is.action > 0 ? 1 : 2; ++open;
                row.chip = verb + " ~" + amt + " dB";
                row.title = label + (is.action > 0 ? ": too much" : ": too little");
                row.body = how + " Then press re-check.";
                break;
        }
        rows.push_back (row);

        // Mark the zone on the analyzer (open issues win over fixed ones in the same zone)
        if (juce::isPositiveAndBelow (is.zone, (int) zoneMarks.size()))
        {
            auto& m = zoneMarks[(size_t) is.zone];
            if (m.state == 0 || m.state == 3)
            {
                m.state = row.state;
                m.text = row.state == 3 ? "fixed" : row.state == 6 ? "no change" : row.state == 7 ? "worse"
                       : row.state == 5 ? "a bit more" : verb + " ~" + amt + " dB";
            }
        }
    }

    const auto& zones = spectrum.getZones();
    for (size_t z = 0; z < zones.size(); ++z)
        if (zoneMarks[z].state == 0 && fs.goodZones.contains (zones[z].label.toLowerCase()))
            zoneMarks[z] = { 4, "ok" };

    juce::String warning;
    if (fs.checks > 0 && open > 0 && noChange == open)
        warning = "I didn't hear any of your changes. Mix Mentor must be the LAST plugin on this track, "
                  "after your EQ, and the same part of the song should be playing.";

    juce::String good = fs.goodZones.joinIntoString (", ") + ".";
    if (fs.issues.empty())
        good = "every range is inside the typical range for this genre and instrument (" + fs.goodZones.joinIntoString (", ") + ").";

    fixPanel.setContent (2, 0.0f, rows, good, warning, open > 0);
    spectrum.setZoneMarks (zoneMarks);
}

void MixMentorEditor::timerCallback()
{
    // Sync with changes made by the processor (state restore, track-name guessing).
    if (processor.genreIndex.load() != shownGenre || processor.instrumentIndex.load() != shownInstrument)
        refreshContent();

    // Levels
    const float peakDb = juce::Decibels::gainToDecibels (processor.takePeak(), -100.0f);

    // Spectrum (zones are only judged while there's real signal, not silence between hits)
    const int n = processor.readAnalysisSamples (readBuffer.data(), (int) readBuffer.size());
    spectrum.pushSamples (readBuffer.data(), n);
    spectrum.updateSpectrum (processor.getCurrentSampleRate(), peakDb > -50.0f);

    peakDisplayDb = juce::jmax (peakDb, peakDisplayDb - 1.0f);
    if (peakDb >= holdDb)       { holdDb = peakDb; holdFrames = 60; }   // hold for ~2 s
    else if (holdFrames > 0)    { --holdFrames; }
    else                        { holdDb = juce::jmax (-100.0f, holdDb - 0.5f); }

    updateGainStatus();

    if (spectrum.takeCaptureResult())
        captureFinished();
    else if (spectrum.isCapturing() && ++frameCounter % 5 == 0)
        refreshFixPanel();   // progress bar
}
