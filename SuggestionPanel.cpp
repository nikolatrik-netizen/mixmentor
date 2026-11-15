#include "SuggestionPanel.h"
#include "Theme.h"
#include <cmath>

using namespace theme;

namespace
{
    constexpr float pad = 14.0f, gap = 10.0f;

    float textHeight (const juce::String& text, const juce::Font& f, float width, float spacing = 3.0f)
    {
        if (text.isEmpty() || width < 10.0f) return 0.0f;
        juce::AttributedString s;
        s.append (text, f, col::text);
        s.setLineSpacing (spacing);
        juce::TextLayout tl;
        tl.createLayout (s, width);
        return tl.getHeight();
    }

    float drawText (juce::Graphics& g, const juce::String& text, const juce::Font& f, juce::Colour c, juce::Rectangle<float> area, float spacing = 3.0f)
    {
        if (text.isEmpty()) return 0.0f;
        juce::AttributedString s;
        s.append (text, f, c);
        s.setLineSpacing (spacing);
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
        if (a.kind == Advice::kRange)     return "eq";
        if (a.metric == "phase" || a.metric == "lowwidth" || a.metric == "width") return "stereo";
        if (a.metric == "clip" || a.metric == "truepeak" || a.metric.startsWith ("loud")) return "level";
        return "dynamics";
    }

    juce::String fmt (float v, const juce::String& unit, bool sign = false)
    {
        const bool corr = unit.isEmpty();
        return (sign && v > 0 ? "+" : "") + juce::String (v, corr ? 2 : 1) + (corr ? juce::String() : " " + unit);
    }

    bool reachedGoal (const Advice& a) { return a.toGo() <= (a.unit.isEmpty() ? 0.03f : 0.5f); }

    juce::Font titleFont()  { return font (16.5f, true); }
    juce::Font actionFont() { return font (15.0f); }
    juce::Font bodyFont()   { return font (14.0f); }
    juce::Font smallFont()  { return font (13.0f); }
    juce::Font labelFont()  { return font (11.5f, true); }
}

//==============================================================================
void BandNumbers::setReadings (std::vector<Advisor::RegionReading> r, bool hasTarget, const juce::String& targetName,
                               const juce::String& standardLabel)
{
    stdLabel = standardLabel;
    readings = std::move (r);
    target = hasTarget;
    name = targetName;
    repaint();
}

void BandNumbers::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 6.0f);
    auto caption = area.removeFromTop (20.0f);
    g.setFont (font (12.5f));
    g.setColour (col::textDim);
    g.drawText (target ? "big = how far you are from " + name + " (dB, green = in range)   |   you / " + stdLabel + " = level vs the track's average"
                       : juce::String ("big = how much each range sticks out (+) or dips (-), dB   |   you / typical = level vs the track's average"),
                caption, juce::Justification::centredLeft, true);
    area.removeFromTop (4.0f);

    const int n = 8;
    const float w = (area.getWidth() - (n - 1) * 6.0f) / n;
    for (int i = 0; i < n; ++i)
    {
        auto box = juce::Rectangle<float> (area.getX() + i * (w + 6.0f), area.getY(), w, area.getHeight());
        juce::Colour c = col::textDim, fill = col::box;
        juce::String value = "-";
        juce::String rangeText;
        if (i < (int) readings.size())
        {
            const auto& r = readings[(size_t) i];
            const float v = r.value;
            value = (v > 0 ? "+" : "") + juce::String (v, 1);
            auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 0 : 1) + "k" : juce::String ((int) f); };
            rangeText = hz (r.lowHz) + "-" + hz (r.highHz);
            if (r.noContent)
            {
                value = "-";
                rangeText = "no audio here";
                c = col::textFaint;
            }
            else if (target)
            {
                if (v > r.tolerance)       { c = col::bad;  fill = col::bad.withAlpha (0.14f); }
                else if (v < -r.tolerance) { c = col::warn; fill = col::warn.withAlpha (0.14f); }
                else                       { c = col::good; fill = col::good.withAlpha (0.12f); }
            }
            else
            {
                c = col::text;
                if (r.issue > 0)      { c = col::bad;  fill = col::bad.withAlpha (0.14f); }
                else if (r.issue < 0) { c = col::warn; fill = col::warn.withAlpha (0.14f); }
            }
            if (target && r.issue != 0 && fill == col::good.withAlpha (0.12f))
                fill = (r.issue > 0 ? col::bad : col::warn).withAlpha (0.10f);
        }
        g.setColour (fill);
        g.fillRoundedRectangle (box, 6.0f);
        auto in = box.reduced (4.0f, 6.0f);
        g.setColour (col::textDim);
        g.setFont (font (12.5f));
        g.drawText (i < (int) readings.size() ? readings[(size_t) i].name : juce::String(), in.removeFromTop (16.0f), juce::Justification::centred, false);
        g.setColour (c);
        g.setFont (font (20.0f, true));
        g.drawText (value, in.removeFromTop (25.0f), juce::Justification::centred, false);

        // your level, then the standard (both vs the track's average)
        auto row1 = in.removeFromTop (16.0f), row2 = in.removeFromTop (16.0f);
        if (i < (int) readings.size() && ! readings[(size_t) i].noContent)
        {
            const auto& r = readings[(size_t) i];
            auto num = [] (float v) { return (v >= 0.05f ? "+" : "") + juce::String (v, 1); };
            auto line = [&] (juce::Rectangle<float> rr, const juce::String& label, float v, juce::Colour lc)
            {
                juce::AttributedString a;
                a.setJustification (juce::Justification::centred);
                a.append (label + " ", font (11.5f), col::textFaint);
                a.append (num (v), font (12.5f, true), lc);
                a.draw (g, rr);
            };
            line (row1, "you", r.mine, col::text);
            if (r.hasStandard) line (row2, stdLabel, r.standard, col::accent);
        }
        g.setColour (col::textFaint);
        g.setFont (font (11.5f));
        g.drawText (rangeText, in.removeFromTop (15.0f), juce::Justification::centred, false);
    }
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
    // keep a valid selection: default to the first open item
    bool selectedExists = false;
    for (auto& a : items) selectedExists = selectedExists || (a.id == content.selected && a.status == Advice::open);
    if (! selectedExists && ! (content.selected.isEmpty() && content.userCollapsed))
    {
        content.selected = {};
        for (auto& a : items) if (a.status == Advice::open) { content.selected = a.id; break; }
    }
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
    auto buttons = header.removeFromRight (220).withSizeKeepingCentre (220, 30);
    startOverButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromRight (8);
    rescanButton.setBounds (buttons);
    r.removeFromTop (8);
    viewport.setBounds (r);
    const int w = juce::jmax (100, viewport.getMaximumVisibleWidth());
    content.setSize (w, juce::jmax (viewport.getHeight(), content.layout (w)));
}

void SuggestionPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    auto header = r.removeFromTop (58.0f).withTrimmedRight (230.0f);

    auto titleRow = header.removeFromTop (28.0f);
    g.setColour (col::text);
    g.setFont (font (19.0f, true));
    g.drawText ("what to do", titleRow, juce::Justification::centredLeft, false);
    const juce::String t = openItems > 0 ? juce::String (openItems) + " to do" + (doneItems > 0 ? " · " + juce::String (doneItems) + " done" : juce::String())
                                         : (doneItems > 0 ? juce::String ("all done") : juce::String());
    if (t.isNotEmpty())
    {
        g.setFont (font (12.5f, true));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t) + 22.0f;
        auto badge = juce::Rectangle<float> (titleRow.getX() + 108.0f, titleRow.getCentreY() - 11.0f, tw, 22.0f);
        const auto c = openItems > 0 ? col::accent : col::good;
        g.setColour (c.withAlpha (0.15f));
        g.fillRoundedRectangle (badge, 11.0f);
        g.setColour (c);
        g.drawText (t, badge, juce::Justification::centred, false);
    }

    auto st = header.removeFromTop (24.0f);
    g.setColour (listening ? col::good : col::textFaint);
    g.fillEllipse (st.getX(), st.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (col::textDim);
    g.setFont (font (13.0f));
    g.drawText (status, st.withTrimmedLeft (16.0f), juce::Justification::centredLeft, true);

    auto bar = juce::Rectangle<float> (r.getX(), r.getY() - 6.0f, r.getWidth(), 3.0f);
    g.setColour (col::border.withAlpha (0.5f));
    g.fillRoundedRectangle (bar, 1.5f);
    if (listening && progress > 0.0f)
    {
        g.setColour (col::good.withAlpha (0.8f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 1.5f);
    }
}

//==============================================================================
float SuggestionPanel::Content::detailHeight (const Advice& a, float width) const
{
    const float inner = width - 2.0f * pad;
    float h = 12.0f;
    h += 20.0f;   // "how to do it"
    for (int i = 0; i < a.steps.size(); ++i) h += textHeight (a.steps[i], bodyFont(), inner - 28.0f) + 7.0f;
    if (a.listenFor.isNotEmpty()) h += 10.0f + 20.0f + textHeight (a.listenFor, bodyFont(), inner);
    if (a.caution.isNotEmpty())   h += 10.0f + 20.0f + textHeight (a.caution, bodyFont(), inner);
    if (a.why.isNotEmpty())       h += 10.0f + 20.0f + textHeight (a.why, bodyFont(), inner);
    return h;
}

float SuggestionPanel::Content::cardHeight (const Advice& a, float width) const
{
    const float inner = width - 2.0f * pad;
    float h = pad + 22.0f + 4.0f + textHeight (a.action, actionFont(), inner) + 12.0f + 10.0f + 10.0f + 22.0f;
    if (a.checkMessage.isNotEmpty() && ! a.checking) h += 18.0f;
    if (a.id == selected) h += detailHeight (a, width);
    return h + pad;
}

int SuggestionPanel::Content::layout (int width)
{
    const float w = (float) width - 4.0f;
    if (items.empty() && infos.empty())
        return (int) (40.0f + textHeight (emptyText, bodyFont(), w - 20.0f));
    float h = 2.0f;
    for (auto& a : items) if (a.status != Advice::done) h += cardHeight (a, w) + gap;
    for (auto& i : infos) h += pad * 2 + 22.0f + 4.0f + textHeight (i.action, actionFont(), w - 2 * pad) + 8.0f + textHeight (i.why, bodyFont(), w - 2 * pad) + gap;
    bool anyDone = false;
    for (auto& a : items) if (a.status == Advice::done) { anyDone = true; h += 42.0f + 6.0f; }
    if (anyDone) h += 28.0f;
    if (! good.isEmpty()) h += 14.0f + textHeight ("Looks good: " + good.joinIntoString (", "), bodyFont(), w);
    return (int) h + 20;
}

void SuggestionPanel::Content::paint (juce::Graphics& g)
{
    hits.clear();
    const float w = (float) getWidth() - 4.0f;
    float y = 2.0f;

    if (items.empty() && infos.empty())
    {
        drawText (g, emptyText, bodyFont(), col::textDim, juce::Rectangle<float> (10.0f, 20.0f, w - 20.0f, 600.0f));
        return;
    }

    for (auto& a : items)
    {
        if (a.status == Advice::done) continue;
        const bool open = a.id == selected;
        const float h = cardHeight (a, w);
        auto card = juce::Rectangle<float> (0.0f, y, w, h);
        const auto c = colourFor (a);
        g.setColour (open ? col::panelLight.brighter (0.04f) : col::panelLight);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (c.withAlpha (open ? 0.9f : 0.55f));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, open ? 1.6f : 1.0f);
        hits.push_back ({ card, a.id, 2 });

        auto in = card.reduced (pad);

        // title + tag
        auto titleRow = in.removeFromTop (22.0f);
        g.setFont (labelFont());
        const auto tag = tagFor (a);
        const float tagW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), tag) + 16.0f;
        auto tagR = titleRow.removeFromRight (tagW).withSizeKeepingCentre (tagW, 19.0f);
        g.setColour (c.withAlpha (0.16f));
        g.fillRoundedRectangle (tagR, 9.5f);
        g.setColour (c);
        g.drawText (tag, tagR, juce::Justification::centred, false);
        g.setColour (col::text);
        g.setFont (titleFont());
        g.drawText (a.title, titleRow.withTrimmedRight (8.0f), juce::Justification::centredLeft, true);

        // the move, with its numbers
        in.removeFromTop (4.0f);
        const float ah = drawText (g, a.action, actionFont(), col::accent, in);
        in.removeFromTop (ah + 12.0f);

        // meter: green = goal zone, marker = where you are now (moves live)
        const bool corr = a.unit.isEmpty();
        const float zone = corr ? 0.3f : 3.0f;
        const float start = juce::jmax (corr ? 0.3f : 3.0f, (float) a.direction * (a.detected - a.goal) + (corr ? 0.1f : 1.0f));
        const float now = a.toGo();
        auto meter = in.removeFromTop (10.0f);
        g.setColour (col::box);
        g.fillRoundedRectangle (meter, 5.0f);
        auto xFor = [&] (float v) { return meter.getX() + meter.getWidth() * juce::jlimit (0.0f, 1.0f, (v + zone) / (start + zone)); };
        const auto greenZone = juce::Rectangle<float> (meter.getX(), meter.getY(), xFor (0.0f) - meter.getX(), meter.getHeight());
        g.setColour (col::good.withAlpha (0.35f));
        g.fillRoundedRectangle (greenZone, 5.0f);
        const float mx = xFor (now);
        g.setColour (reachedGoal (a) ? col::good : c);
        g.fillRoundedRectangle (juce::Rectangle<float> (mx - 2.5f, meter.getY() - 4.0f, 5.0f, meter.getHeight() + 8.0f), 2.0f);
        in.removeFromTop (10.0f);

        // caption + Done box
        auto row = in.removeFromTop (22.0f);
        auto boxArea = row.removeFromRight (150.0f);
        auto box = boxArea.removeFromLeft (20.0f).withSizeKeepingCentre (18.0f, 18.0f);
        g.setColour (a.checking ? col::accent : col::box);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (a.checking ? col::accent : col::border.brighter (0.3f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.2f);
        if (a.checking) drawTick (g, box.reduced (3.0f), col::window, 2.0f);
        g.setColour (col::text);
        g.setFont (font (13.5f, true));
        g.drawText (a.checking ? "checking..." : "Done, check it", boxArea.withTrimmedLeft (8.0f), juce::Justification::centredLeft, false);
        hits.insert (hits.begin(), { box.getUnion (boxArea).expanded (3.0f), a.id, 0 });

        g.setFont (smallFont());
        juce::String cap;
        if (reachedGoal (a)) cap = "in range now";
        else if (a.kind == Advice::kMetric)
            cap = "now " + fmt (a.value, a.unit) + "  ·  goal " + (a.direction > 0 ? "below " : "above ") + fmt (a.goal, a.unit);
        else
            cap = "now " + fmt (now, a.unit) + (a.direction > 0 ? " too much" : " too little") + "  ·  goal: in the green";
        if (a.liveThisPass && a.kind != Advice::kMetric) cap << "  ·  live";
        g.setColour (reachedGoal (a) ? col::good : col::textDim);
        g.drawText (cap, row, juce::Justification::centredLeft, true);

        if (a.checkMessage.isNotEmpty() && ! a.checking)
        {
            auto msg = in.removeFromTop (18.0f);
            g.setFont (smallFont());
            g.setColour (a.checkMessage.startsWith ("Not") ? col::bad.brighter (0.2f) : col::good);
            g.drawText (a.checkMessage, msg, juce::Justification::centredLeft, true);
        }

        // In-depth advice for the selected issue
        if (open)
        {
            in.removeFromTop (12.0f);
            g.setColour (col::border.withAlpha (0.7f));
            g.fillRect (in.getX(), in.getY() - 6.0f, in.getWidth(), 1.0f);
            auto label = [&] (const juce::String& t, juce::Colour lc)
            {
                g.setColour (lc);
                g.setFont (labelFont());
                g.drawText (t.toUpperCase(), in.removeFromTop (20.0f), juce::Justification::centredLeft, false);
            };
            label ("how to do it", col::textDim);
            for (int i = 0; i < a.steps.size(); ++i)
            {
                auto stepArea = in;
                auto num = stepArea.removeFromLeft (28.0f).withHeight (20.0f);
                g.setColour (col::accent.withAlpha (0.2f));
                g.fillEllipse (num.withSizeKeepingCentre (20.0f, 20.0f).translated (-4.0f, 0.0f));
                g.setColour (col::accent);
                g.setFont (font (12.0f, true));
                g.drawText (juce::String (i + 1), num.withSizeKeepingCentre (20.0f, 20.0f).translated (-4.0f, 0.0f), juce::Justification::centred, false);
                const float sh = drawText (g, a.steps[i], bodyFont(), col::text, stepArea.withTrimmedTop (1.0f));
                in.removeFromTop (sh + 7.0f);
            }
            if (a.listenFor.isNotEmpty())
            {
                in.removeFromTop (10.0f);
                label ("listen for", col::good);
                in.removeFromTop (drawText (g, a.listenFor, bodyFont(), col::text, in));
            }
            if (a.caution.isNotEmpty())
            {
                in.removeFromTop (10.0f);
                label ("watch out", col::accent);
                in.removeFromTop (drawText (g, a.caution, bodyFont(), col::textDim, in));
            }
            if (a.why.isNotEmpty())
            {
                in.removeFromTop (10.0f);
                label ("why", col::textDim);
                in.removeFromTop (drawText (g, a.why, bodyFont(), col::textDim, in));
            }
        }
        else
        {
            g.setColour (col::textFaint);
            g.setFont (font (11.5f));
        }
        y += h + gap;
    }

    for (auto& i : infos)
    {
        const float inner = w - 2 * pad;
        const float h = pad * 2 + 22.0f + 4.0f + textHeight (i.action, actionFont(), inner) + 8.0f + textHeight (i.why, bodyFont(), inner);
        auto card = juce::Rectangle<float> (0.0f, y, w, h);
        g.setColour (col::panelLight);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (col::warn.withAlpha (0.55f));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.0f);
        auto in = card.reduced (pad);
        auto titleRow = in.removeFromTop (22.0f);
        g.setFont (labelFont());
        const float tagW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), i.tag) + 16.0f;
        auto tagR = titleRow.removeFromRight (tagW).withSizeKeepingCentre (tagW, 19.0f);
        g.setColour (col::warn.withAlpha (0.16f));
        g.fillRoundedRectangle (tagR, 9.5f);
        g.setColour (col::warn);
        g.drawText (i.tag, tagR, juce::Justification::centred, false);
        g.setColour (col::text);
        g.setFont (titleFont());
        g.drawText (i.title, titleRow, juce::Justification::centredLeft, true);
        in.removeFromTop (4.0f);
        const float ah = drawText (g, i.action, actionFont(), col::accent, in);
        in.removeFromTop (ah + 8.0f);
        drawText (g, i.why, bodyFont(), col::textDim, in);
        y += h + gap;
    }

    bool header = false;
    for (auto& a : items)
    {
        if (a.status != Advice::done) continue;
        if (! header)
        {
            g.setColour (col::textFaint);
            g.setFont (labelFont());
            g.drawText ("DONE", juce::Rectangle<float> (2.0f, y + 4.0f, w, 18.0f), juce::Justification::centredLeft, false);
            y += 28.0f;
            header = true;
        }
        auto row = juce::Rectangle<float> (0.0f, y, w, 42.0f);
        g.setColour (col::good.withAlpha (0.08f));
        g.fillRoundedRectangle (row, 8.0f);
        drawTick (g, row.withWidth (42.0f).reduced (12.0f), col::good, 2.4f);
        g.setColour (col::text);
        g.setFont (font (14.5f, true));
        g.drawText (a.title, row.withTrimmedLeft (44.0f).withTrimmedRight (200.0f), juce::Justification::centredLeft, true);
        auto undo = row.removeFromRight (60.0f);
        g.setFont (smallFont());
        g.setColour (col::textDim);
        g.drawText ("undo", undo, juce::Justification::centred, false);
        g.setColour (col::good);
        g.drawText (a.checkMessage.isNotEmpty() ? a.checkMessage : juce::String ("fixed"), row.removeFromRight (190.0f),
                    juce::Justification::centredRight, true);
        hits.insert (hits.begin(), { undo, a.id, 1 });
        y += 42.0f + 6.0f;
    }

    if (! good.isEmpty())
    {
        y += 8.0f;
        drawText (g, "Looks good: " + good.joinIntoString (", ") + ".", bodyFont(), col::textFaint, juce::Rectangle<float> (2.0f, y, w, 200.0f));
    }
}

void SuggestionPanel::Content::mouseDown (const juce::MouseEvent& e)
{
    for (auto& h : hits)
        if (h.area.contains (e.position))
        {
            if (h.what == 0)      { if (owner.onCheck) owner.onCheck (h.id); }
            else if (h.what == 1) { if (owner.onReopen) owner.onReopen (h.id); }
            else
            {
                selected = selected == h.id ? juce::String() : h.id;
                userCollapsed = selected.isEmpty();
                setSize (getWidth(), juce::jmax (owner.viewport.getHeight(), layout (getWidth())));
                repaint();
                if (owner.onSelect) owner.onSelect (selected);
            }
            return;
        }
}

void SuggestionPanel::Content::mouseMove (const juce::MouseEvent& e)
{
    bool over = false;
    for (auto& h : hits) over = over || h.area.contains (e.position);
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
