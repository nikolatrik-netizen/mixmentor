#include "PluginEditor.h"

using namespace theme;

namespace
{
    juce::String fillTemplate (juce::String t, const juce::StringPairArray& values)
    {
        for (auto& key : values.getAllKeys())
            t = t.replace ("{" + key + "}", values[key]);
        return t;
    }

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
void AdvicePanel::setItems (std::vector<Item> newItems, bool isLive, int issueCount)
{
    items = std::move (newItems);
    live = isLive;
    issues = issueCount;
    repaint();
}

void AdvicePanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), col::panel, 0.0f);
    auto area = getLocalBounds().toFloat().reduced (16.0f, 14.0f);

    // Header: title + issue count / live badge
    auto top = area.removeFromTop (18.0f);
    drawLabel (g, "problem frequencies", top);
    if (live)
    {
        const auto txt = issues == 0 ? juce::String ("all clear") : juce::String (issues) + (issues == 1 ? " issue" : " issues");
        const auto c = issues == 0 ? col::good : col::accent;
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

    for (auto& item : items)
    {
        if (area.getHeight() < 40.0f)
            break;

        const bool isIssue = item.freq.isNotEmpty();

        juce::AttributedString body;
        body.setWordWrap (juce::AttributedString::byWord);
        body.append (item.body, font (12.5f), col::textDim);
        body.setLineSpacing (1.5f);
        juce::TextLayout layout;
        layout.createLayout (body, area.getWidth() - 14.0f);

        const float headH = isIssue ? 40.0f : 20.0f;
        const float h = juce::jmin (headH + layout.getHeight() + 14.0f, area.getHeight());
        auto card = area.removeFromTop (h);
        area.removeFromTop (8.0f);

        if (isIssue)
        {
            g.setColour (col::box.withAlpha (0.6f));
            g.fillRoundedRectangle (card, 5.0f);
        }
        g.setColour (item.colour);
        g.fillRoundedRectangle (card.getX(), card.getY() + 6.0f, 3.0f, card.getHeight() - 12.0f, 1.5f);

        auto inner = card.reduced (14.0f, 7.0f).withTrimmedLeft (0.0f);
        if (isIssue)
        {
            // Big frequency read-out + amount chip, then the zone name
            auto row = inner.removeFromTop (22.0f);
            g.setColour (col::text);
            g.setFont (font (18.0f, true));
            g.drawText (item.freq, row, juce::Justification::centredLeft, false);

            g.setFont (font (12.0f, true));
            const float cw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), item.amount) + 14.0f;
            const auto chip = row.removeFromRight (cw).withSizeKeepingCentre (cw, 18.0f);
            g.setColour (item.colour.withAlpha (0.16f));
            g.fillRoundedRectangle (chip, 9.0f);
            g.setColour (item.colour);
            g.drawText (item.amount, chip, juce::Justification::centred, false);

            g.setColour (col::textDim);
            g.setFont (font (12.5f, true));
            g.drawText (item.title, inner.removeFromTop (16.0f), juce::Justification::centredLeft, true);
            inner.removeFromTop (4.0f);
        }
        else
        {
            g.setColour (col::text);
            g.setFont (font (13.5f, true));
            g.drawText (item.title, inner.removeFromTop (18.0f), juce::Justification::centredLeft, true);
            inner.removeFromTop (2.0f);
        }
        layout.draw (g, inner);
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

    relistenButton.getProperties().set ("flat", true);
    relistenButton.setTooltip ("Re-listen\nForget what the analyzer has heard so far and start again, e.g. after changing your EQ or moving to a new section.");
    relistenButton.onClick = [this] { spectrum.resetAnalysis(); updateAdvice(); };
    addAndMakeVisible (relistenButton);

    guideButton.getProperties().set ("flat", true);
    guideButton.setTooltip ("How to read the analyzer\n"
                            "White line: the average of the last few seconds.\n"
                            "Blue area: the live sound.\n"
                            "Dashed boxes: the target range for your genre and instrument.\n"
                            "Coloured bars: where each zone measures now (green ok, red too much, orange too little).\n\n"
                            "Hover a zone for its tip.\n\n"
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
    addAndMakeVisible (advice);

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
        tb.removeFromRight (4);
        relistenButton.setBounds (tb.removeFromRight (90));
    }

    auto upper = area.removeFromTop ((int) (area.getHeight() * 0.55f));
    advice.setBounds (upper.removeFromRight (320));
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
    updateAdvice();
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

void MixMentorEditor::updateAdvice()
{
    const auto& kb = mm::KnowledgeBase::get();
    const int g = processor.genreIndex.load(), i = processor.instrumentIndex.load();
    const auto* guide = kb.findGuide (g, i);

    if (guide == nullptr)
    {
        advice.setItems ({}, false);
        return;
    }

    const auto gName = kb.genres()[(size_t) g].name;
    const auto instrumentName = kb.instruments()[(size_t) i].name.toLowerCase();
    const juce::Colour grey = col::textFaint;

    if (! spectrum.hasReadings())
    {
        advice.setItems ({ { grey, "listening...",
                             "Play the track (solo it for the most accurate advice). Put Mix Mentor LAST on the track, "
                             "after your EQ and compressor, so it hears your changes. Advice appears after about 2 seconds.", {}, {} } }, false);
        return;
    }

    const auto& zones = spectrum.getZones();
    const auto& readings = spectrum.getReadings();
    if (zones.size() != readings.size())
        return;

    struct Ranked { float severity; AdvicePanel::Item item; };
    std::vector<Ranked> problems;
    juce::StringArray okZones;

    for (size_t z = 0; z < zones.size(); ++z)
    {
        const auto& zone = zones[z];
        const auto& r = readings[z];
        const float dev = std::abs (r.deviationDb);

        juce::StringPairArray values;
        values.set ("label", zone.label.toLowerCase());
        values.set ("genre", gName);
        values.set ("instrument", instrumentName);
        values.set ("dev", juce::String (dev, 1));
        values.set ("amt", juce::String (juce::jlimit (1, 6, (int) std::round (dev))));
        values.set ("hz", SpectrumView::formatHz (r.peakHz));
        values.set ("lo", SpectrumView::formatHz (zone.lowHz));
        values.set ("hi", SpectrumView::formatHz (zone.highHz));

        if (r.status > 0)
        {
            const auto tmpl = zone.tooMuch.isNotEmpty() ? zone.tooMuch
                : juce::String ("There's {dev} dB more {label} than typical for a {genre} {instrument}. "
                                "Cut about {amt} dB around {hz} with a medium-wide bell (Q 1-2), then give it a second to re-measure.");
            problems.push_back ({ dev, { SpectrumView::colourForStatus (1),
                                         zone.label.toLowerCase() + juce::String::fromUTF8 ("  \xe2\x80\x94  too much"),
                                         fillTemplate (tmpl, values),
                                         SpectrumView::formatHz (r.peakHz), "+" + juce::String (dev, 1) + " dB" } });
        }
        else if (r.status < 0)
        {
            const auto tmpl = zone.tooLittle.isNotEmpty() ? zone.tooLittle
                : juce::String ("There's {dev} dB less {label} than typical for a {genre} {instrument}. "
                                "Boost about {amt} dB around {hz} with a wide bell (Q 0.7-1.5), then give it a second to re-measure.");
            problems.push_back ({ dev, { SpectrumView::colourForStatus (-1),
                                         zone.label.toLowerCase() + juce::String::fromUTF8 ("  \xe2\x80\x94  too little"),
                                         fillTemplate (tmpl, values),
                                         SpectrumView::formatHz (r.peakHz), "-" + juce::String (dev, 1) + " dB" } });
        }
        else
        {
            okZones.add (zone.label);
        }

        if (r.resonanceHz > 0.0f)
        {
            values.set ("hz", SpectrumView::formatHz (r.resonanceHz));
            values.set ("res", juce::String (r.resonanceDb, 0));
            problems.push_back ({ r.resonanceDb - 6.0f, { SpectrumView::colourForStatus (1),
                "ringing resonance in " + zone.label.toLowerCase(),
                fillTemplate ("One narrow frequency sticks out about {res} dB in the {label} zone. "
                              "Cut 3-5 dB at {hz} with a narrow bell (Q 5-8). Tip: sweep a narrow boost "
                              "to hear it ring, then turn the boost into a cut.", values),
                SpectrumView::formatHz (r.resonanceHz), "+" + juce::String (juce::roundToInt (r.resonanceDb)) + " dB peak" } });
        }
    }

    std::sort (problems.begin(), problems.end(), [] (auto& a, auto& b) { return a.severity > b.severity; });

    std::vector<AdvicePanel::Item> items;
    for (auto& p : problems)
        items.push_back (p.item);

    if (problems.empty())
        items.push_back ({ SpectrumView::colourForStatus (0), "sounding balanced for " + gName.toLowerCase(),
                           "Every zone is inside the typical range for a " + gName + " " + instrumentName
                               + ". Now judge it in the full mix: does it sit well next to the other elements?", {}, {} });
    else if (! okZones.isEmpty())
        items.push_back ({ SpectrumView::colourForStatus (0), "looks good: " + okZones.joinIntoString (", ").toLowerCase(),
                           "Leave these alone for now.", {}, {} });

    advice.setItems (std::move (items), true, (int) problems.size());
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

    if (++frameCounter % 15 == 0)    // advice twice a second, so it doesn't flicker
        updateAdvice();
}
