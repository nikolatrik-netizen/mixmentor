#include "SuggestionPanel.h"
#include "Theme.h"
#include <cmath>

using namespace theme;

namespace
{
    constexpr float pad = 16.0f, gap = 12.0f;

    float textHeight (const juce::String& text, const juce::Font& f, float width)
    {
        if (text.isEmpty()) return 0.0f;
        juce::AttributedString s;
        s.append (text, f, col::text);
        s.setLineSpacing (3.0f);
        juce::TextLayout tl;
        tl.createLayout (s, width);
        return tl.getHeight();
    }

    float drawText (juce::Graphics& g, const juce::String& text, const juce::Font& f, juce::Colour c, juce::Rectangle<float> area)
    {
        if (text.isEmpty()) return 0.0f;
        juce::AttributedString s;
        s.append (text, f, c);
        s.setLineSpacing (3.0f);
        juce::TextLayout tl;
        tl.createLayout (s, area.getWidth());
        tl.draw (g, area.withHeight (tl.getHeight() + 2.0f));
        return tl.getHeight();
    }

    void drawTick (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thickness = 2.0f)
    {
        juce::Path p;
        p.startNewSubPath (r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.55f);
        p.lineTo (r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.78f);
        p.lineTo (r.getX() + r.getWidth() * 0.84f, r.getY() + r.getHeight() * 0.24f);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Colour colourFor (const Advice& a)
    {
        if (a.status == Advice::done) return col::good;
        if (a.kind == Advice::kMetric) return col::accent;
        return a.direction > 0 ? col::bad : col::warn;
    }

    juce::String tagFor (const Advice& a)
    {
        if (a.kind == Advice::kResonance) return "resonance";
        if (a.kind == Advice::kRange)     return "tone";
        if (a.metric == "phase" || a.metric == "lowwidth" || a.metric == "width") return "stereo";
        if (a.metric == "clip" || a.metric == "truepeak" || a.metric.startsWith ("loud")) return "level";
        return "dynamics";
    }

    juce::String num (float v, const juce::String& unit)
    {
        const bool corr = unit.isEmpty();
        return juce::String (v, corr ? 2 : 1) + (corr ? juce::String() : " " + unit);
    }

    const juce::Font titleFont()  { return font (17.5f, true); }
    const juce::Font actionFont() { return font (15.5f, true); }
    const juce::Font bodyFont()   { return font (14.0f); }
    const juce::Font smallFont()  { return font (13.0f); }
}

//==============================================================================
SuggestionPanel::SuggestionPanel()
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    rescanButton.setTooltip ("Scan again\nLook for new problems on the next loop. Things you've fixed stay fixed.");
    rescanButton.onClick = [this] { if (onRescan) onRescan(); };
    addAndMakeVisible (rescanButton);
    startOverButton.setTooltip ("Start over\nForget the list and listen from scratch.");
    startOverButton.onClick = [this] { if (onStartOver) onStartOver(); };
    addAndMakeVisible (startOverButton);
}

void SuggestionPanel::setState (const std::vector<Advice>& items, const juce::StringArray& goodNotes, const std::vector<Info>& infos,
                                const juce::String& newStatus, float passProgress, bool listeningNow, const juce::String& emptyText,
                                const juce::String& newTargetName)
{
    content.items = items;
    content.infos = infos;
    content.good = goodNotes;
    content.emptyText = emptyText;
    status = newStatus;
    progress = passProgress;
    listening = listeningNow;
    targetName = newTargetName;
    openItems = doneItems = 0;
    for (auto& a : items) (a.status == Advice::done ? doneItems : openItems)++;
    const int w = juce::jmax (100, viewport.getMaximumVisibleWidth());
    content.setSize (w, juce::jmax (viewport.getHeight(), content.layout (w)));
    content.repaint();
    repaint();
}

void SuggestionPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    auto header = r.removeFromTop (58);
    auto buttons = header.removeFromRight (230).withSizeKeepingCentre (230, 30);
    startOverButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromRight (8);
    rescanButton.setBounds (buttons);
    r.removeFromTop (6);
    viewport.setBounds (r);
    const int w = juce::jmax (100, viewport.getMaximumVisibleWidth());
    content.setSize (w, juce::jmax (viewport.getHeight(), content.layout (w)));
}

void SuggestionPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    auto header = r.removeFromTop (58.0f).withTrimmedRight (240.0f);

    g.setColour (col::text);
    g.setFont (font (19.0f, true));
    g.drawText ("what to do", header.removeFromTop (28.0f), juce::Justification::centredLeft, false);

    // count badge
    {
        const juce::String t = openItems > 0 ? juce::String (openItems) + " to do" + (doneItems > 0 ? ", " + juce::String (doneItems) + " done" : juce::String())
                                             : (doneItems > 0 ? juce::String ("all done") : juce::String());
        if (t.isNotEmpty())
        {
            g.setFont (font (12.5f, true));
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + 22.0f;
            auto badge = juce::Rectangle<float> (header.getX() + 112.0f, header.getY() - 25.0f, tw, 22.0f);
            const auto c = openItems > 0 ? col::accent : col::good;
            g.setColour (c.withAlpha (0.15f));
            g.fillRoundedRectangle (badge, 11.0f);
            g.setColour (c);
            g.drawText (t, badge, juce::Justification::centred, false);
        }
    }

    // listening status + progress through the loop
    auto st = header.removeFromTop (24.0f);
    g.setColour (listening ? col::good : col::textFaint);
    g.fillEllipse (st.getX(), st.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (col::textDim);
    g.setFont (font (13.0f));
    g.drawText (status, st.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);
    auto bar = juce::Rectangle<float> (r.getX(), r.getY() - 4.0f, r.getWidth(), 3.0f);
    g.setColour (col::border.withAlpha (0.5f));
    g.fillRoundedRectangle (bar, 1.5f);
    if (listening && progress > 0.0f)
    {
        g.setColour (col::good.withAlpha (0.8f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 1.5f);
    }
}

//==============================================================================
float SuggestionPanel::Content::cardHeight (const Advice& a, float width) const
{
    const float inner = width - 2.0f * pad - 6.0f;
    if (a.status == Advice::done) return 44.0f;
    float h = pad;
    h += 24.0f;                                          // title
    h += textHeight (a.action, actionFont(), inner) + 8.0f;
    h += 10.0f + 20.0f;                                  // meter + its caption
    h += 8.0f + textHeight (a.why, bodyFont(), inner);
    h += 14.0f + 24.0f;                                  // check row
    if (a.checkMessage.isNotEmpty()) h += 20.0f;
    return h + pad;
}

int SuggestionPanel::Content::layout (int width)
{
    float h = 4.0f;
    const float w = (float) width - 4.0f;
    if (items.empty() && infos.empty())
        return (int) (h + 40.0f + textHeight (emptyText, bodyFont(), w - 20.0f));
    for (auto& a : items) if (a.status != Advice::done) h += cardHeight (a, w) + gap;
    for (auto& i : infos) h += pad * 2 + 24.0f + textHeight (i.action, actionFont(), w - 2 * pad - 6) + 8.0f + textHeight (i.why, bodyFont(), w - 2 * pad - 6) + gap;
    bool anyDone = false;
    for (auto& a : items) if (a.status == Advice::done) { anyDone = true; h += 44.0f + 6.0f; }
    if (anyDone) h += 30.0f;
    if (! good.isEmpty()) h += 16.0f + textHeight ("Looks good: " + good.joinIntoString (", "), bodyFont(), w);
    return (int) h + 20;
}

void SuggestionPanel::Content::paint (juce::Graphics& g)
{
    hits.clear();
    const float w = (float) getWidth() - 4.0f;
    float y = 4.0f;

    if (items.empty() && infos.empty())
    {
        drawText (g, emptyText, bodyFont(), col::textDim, juce::Rectangle<float> (10.0f, y + 20.0f, w - 20.0f, 400.0f));
        return;
    }

    // Open items: full cards
    for (auto& a : items)
    {
        if (a.status == Advice::done) continue;
        const float h = cardHeight (a, w);
        auto card = juce::Rectangle<float> (0.0f, y, w, h);
        const auto c = colourFor (a);
        g.setColour (col::panelLight);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (col::border.withAlpha (0.6f));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (c);
        g.fillRoundedRectangle (card.withWidth (5.0f).reduced (0.0f, 6.0f).translated (6.0f, 0.0f), 2.5f);

        auto in = card.reduced (pad, pad).withTrimmedLeft (8.0f);

        // title + tag
        auto titleRow = in.removeFromTop (24.0f);
        g.setFont (smallFont());
        const auto tag = tagFor (a);
        const float tagW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), tag) + 18.0f;
        auto tagR = titleRow.removeFromRight (tagW).withSizeKeepingCentre (tagW, 20.0f);
        g.setColour (col::box);
        g.fillRoundedRectangle (tagR, 10.0f);
        g.setColour (col::textDim);
        g.drawText (tag, tagR, juce::Justification::centred, false);
        g.setColour (col::text);
        g.setFont (titleFont());
        g.drawText (a.title, titleRow, juce::Justification::centredLeft, true);

        // action
        in.removeFromTop (2.0f);
        const float ah = drawText (g, a.action, actionFont(), col::accent, in);
        in.removeFromTop (ah + 8.0f);

        // meter: how far along the way to the goal (moves live while you turn the knob)
        auto meter = in.removeFromTop (10.0f);
        const bool reached = a.toGo() <= (a.unit.isEmpty() ? 0.03f : 0.5f);
        g.setColour (col::box);
        g.fillRoundedRectangle (meter, 5.0f);
        const float p = reached ? 1.0f : a.progress();
        if (p > 0.0f)
        {
            g.setColour ((reached ? col::good : c).withAlpha (0.9f));
            g.fillRoundedRectangle (meter.withWidth (juce::jmax (10.0f, meter.getWidth() * p)), 5.0f);
        }
        g.setColour (col::good);
        g.fillRect (meter.getRight() - 3.0f, meter.getY() - 3.0f, 3.0f, meter.getHeight() + 6.0f);   // the goal

        auto caption = in.removeFromTop (20.0f);
        g.setFont (smallFont());
        juce::String capText;
        if (reached) capText = "In range now";
        else if (a.kind == Advice::kMetric)
            capText = "Now " + num (a.value, a.unit) + "   goal " + juce::String (a.direction > 0 ? "below " : "above ") + num (a.goal, a.unit);
        else
            capText = juce::String (a.toGo(), 1) + " dB to go";
        capText << (a.kind != Advice::kMetric ? (a.liveThisPass ? "   live, beat by beat" : "   updates every loop") : "   updates every loop");
        g.setColour (reached ? col::good : col::textDim);
        g.drawText (capText, caption.withTrimmedTop (4.0f), juce::Justification::centredLeft, true);

        // why
        in.removeFromTop (8.0f);
        const float wh = drawText (g, a.why, bodyFont(), col::textDim, in);
        in.removeFromTop (wh + 14.0f);

        // "Done, check it"
        auto row = in.removeFromTop (24.0f);
        auto box = row.removeFromLeft (20.0f).withSizeKeepingCentre (20.0f, 20.0f);
        g.setColour (a.checking ? col::accent : col::box);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (a.checking ? col::accent : col::border.brighter (0.3f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.2f);
        if (a.checking) drawTick (g, box.reduced (3.0f), col::window, 2.2f);
        g.setColour (col::text);
        g.setFont (font (14.0f, true));
        const juce::String label = a.checking ? "Checking on the next loop..." : "Done, check it";
        const float lw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 10.0f;
        g.drawText (label, row.withTrimmedLeft (10.0f), juce::Justification::centredLeft, false);
        hits.push_back ({ box.getUnion (row.withWidth (lw + 10.0f)).expanded (4.0f), a.id, false });

        if (a.checkMessage.isNotEmpty() && ! a.checking)
        {
            auto msg = in.removeFromTop (20.0f);
            g.setFont (smallFont());
            g.setColour (a.checkMessage.startsWith ("Not") ? col::bad.brighter (0.2f) : col::good);
            g.drawText (a.checkMessage, msg.withTrimmedTop (4.0f), juce::Justification::centredLeft, true);
        }
        y += h + gap;
    }

    // Between-tracks notes
    for (auto& i : infos)
    {
        const float inner = w - 2 * pad - 6;
        const float h = pad * 2 + 24.0f + textHeight (i.action, actionFont(), inner) + 8.0f + textHeight (i.why, bodyFont(), inner);
        auto card = juce::Rectangle<float> (0.0f, y, w, h);
        g.setColour (col::panelLight);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (col::warn);
        g.fillRoundedRectangle (card.withWidth (5.0f).reduced (0.0f, 6.0f).translated (6.0f, 0.0f), 2.5f);
        auto in = card.reduced (pad, pad).withTrimmedLeft (8.0f);
        auto titleRow = in.removeFromTop (24.0f);
        g.setFont (smallFont());
        const float tagW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), i.tag) + 18.0f;
        auto tagR = titleRow.removeFromRight (tagW).withSizeKeepingCentre (tagW, 20.0f);
        g.setColour (col::box);
        g.fillRoundedRectangle (tagR, 10.0f);
        g.setColour (col::textDim);
        g.drawText (i.tag, tagR, juce::Justification::centred, false);
        g.setColour (col::text);
        g.setFont (titleFont());
        g.drawText (i.title, titleRow, juce::Justification::centredLeft, true);
        in.removeFromTop (2.0f);
        const float ah = drawText (g, i.action, actionFont(), col::accent, in);
        in.removeFromTop (ah + 8.0f);
        drawText (g, i.why, bodyFont(), col::textDim, in);
        y += h + gap;
    }

    // Done items: compact
    bool header = false;
    for (auto& a : items)
    {
        if (a.status != Advice::done) continue;
        if (! header)
        {
            g.setColour (col::textFaint);
            g.setFont (font (12.5f, true));
            g.drawText ("DONE", juce::Rectangle<float> (2.0f, y + 6.0f, w, 18.0f), juce::Justification::centredLeft, false);
            y += 30.0f;
            header = true;
        }
        auto row = juce::Rectangle<float> (0.0f, y, w, 44.0f);
        g.setColour (col::good.withAlpha (0.07f));
        g.fillRoundedRectangle (row, 8.0f);
        drawTick (g, row.withWidth (44.0f).reduced (13.0f), col::good, 2.4f);
        g.setColour (col::text);
        g.setFont (font (15.0f, true));
        g.drawText (a.title, row.withTrimmedLeft (46.0f).withTrimmedRight (170.0f), juce::Justification::centredLeft, true);
        g.setFont (smallFont());
        g.setColour (col::textDim);
        auto undo = row.removeFromRight (64.0f);
        g.drawText ("undo", undo, juce::Justification::centred, false);
        g.setColour (col::good);
        g.drawText (a.checkMessage.isNotEmpty() ? a.checkMessage : juce::String ("fixed"), row.removeFromRight (190.0f),
                    juce::Justification::centredRight, true);
        hits.push_back ({ undo, a.id, true });
        y += 44.0f + 6.0f;
    }

    if (! good.isEmpty())
    {
        y += 10.0f;
        drawText (g, "Looks good: " + good.joinIntoString (", ") + ".", bodyFont(), col::textFaint,
                  juce::Rectangle<float> (2.0f, y, w, 200.0f));
    }
}

void SuggestionPanel::Content::mouseDown (const juce::MouseEvent& e)
{
    for (auto& h : hits)
        if (h.area.contains (e.position))
        {
            if (h.reopen) { if (owner.onReopen) owner.onReopen (h.id); }
            else if (owner.onCheck) owner.onCheck (h.id);
            return;
        }
}

void SuggestionPanel::Content::mouseMove (const juce::MouseEvent& e)
{
    bool over = false;
    for (auto& h : hits) over = over || h.area.contains (e.position);
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
