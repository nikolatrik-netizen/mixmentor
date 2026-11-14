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
}

//==============================================================================
GenrePicker::Card::Card (const juce::String& name, const juce::String& s)
    : juce::Button (name), summary (s)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GenrePicker::Card::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (isDown ? col::raisedHover : isHighlighted ? col::raised : col::card);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (isHighlighted ? col::accent.withAlpha (0.7f) : col::border);
    g.drawRoundedRectangle (r, 12.0f, isHighlighted ? 1.5f : 1.0f);

    auto inner = r.reduced (20.0f, 18.0f);

    // Accent stripe
    g.setGradientFill (juce::ColourGradient (col::accent, inner.getX(), 0.0f, col::accent2, inner.getX() + 40.0f, 0.0f, false));
    g.fillRoundedRectangle (inner.removeFromTop (3.0f).withWidth (40.0f), 1.5f);
    inner.removeFromTop (14.0f);

    g.setColour (col::text);
    g.setFont (font (18.0f, true));
    g.drawText (getButtonText(), inner.removeFromTop (24.0f), juce::Justification::centredLeft, false);
    inner.removeFromTop (6.0f);

    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.append (summary, font (12.5f), col::textDim);
    s.setLineSpacing (2.0f);
    juce::TextLayout layout;
    layout.createLayout (s, inner.getWidth());
    layout.draw (g, inner);
}

GenrePicker::GenrePicker()
{
    for (auto& genre : mm::KnowledgeBase::get().genres())
    {
        auto* c = cards.add (new Card (genre.name, genre.summary));
        const int index = cards.size() - 1;
        c->onClick = [this, index] { if (onPick) onPick (index); };
        addAndMakeVisible (c);
    }
}

void GenrePicker::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (col::bg);
    g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.10f), b.getCentreX(), b.getHeight() * 0.25f,
                                             col::bg.withAlpha (0.0f), b.getCentreX(), b.getHeight() * 0.85f, true));
    g.fillAll();

    auto area = b.withSizeKeepingCentre (b.getWidth(), 120.0f).translated (0.0f, -150.0f);
    g.setColour (col::accent);
    g.setFont (juce::Font (font (11.0f, true)).withExtraKerningFactor (0.2f));
    g.drawText ("MIX MENTOR", area.removeFromTop (20.0f), juce::Justification::centred, false);

    g.setColour (col::text);
    g.setFont (font (30.0f, true));
    g.drawText ("What are you making today?", area.removeFromTop (44.0f), juce::Justification::centred, false);

    g.setColour (col::textDim);
    g.setFont (font (14.0f));
    g.drawText ("Every tip, frequency zone and level target adapts to your genre. You can change it any time.",
                area.removeFromTop (24.0f), juce::Justification::centred, false);
}

void GenrePicker::resized()
{
    const int n = juce::jmax (1, cards.size());
    const int cardW = 250, cardH = 150, gap = 16;
    const int totalW = n * cardW + (n - 1) * gap;
    auto row = juce::Rectangle<int> (totalW, cardH).withCentre (getLocalBounds().getCentre().translated (0, 30));
    for (auto* c : cards)
    {
        c->setBounds (row.removeFromLeft (cardW));
        row.removeFromLeft (gap);
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
    auto inner = getLocalBounds().toFloat().reduced (4.0f);
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
    drawCard (g, getLocalBounds().toFloat(), col::card, 10.0f);

    for (int i = 0; i < options.size(); ++i)
    {
        const auto r = segment (i);
        if (i == selected)
        {
            g.setColour (col::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::accent.withAlpha (0.45f));
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        }
        else if (i == hovered)
        {
            g.setColour (col::raised);
            g.fillRoundedRectangle (r, 7.0f);
        }

        g.setColour (i == selected ? col::text : i == hovered ? col::text.withAlpha (0.85f) : col::textDim);
        g.setFont (font (13.0f, true));
        g.drawText (options[i], r, juce::Justification::centred, false);
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
GainPanel::GainPanel()
{
    addAndMakeVisible (meter);
}

void GainPanel::setState (float holdDb, float targetMin, float targetMax, bool hasGuide)
{
    hold = holdDb; minDb = targetMin; maxDb = targetMax; guide = hasGuide;
    meter.setTargetRange (targetMin, targetMax);
    repaint();
}

void GainPanel::resized()
{
    meter.setBounds (getLocalBounds().reduced (12, 14).removeFromRight (44));
}

void GainPanel::paint (juce::Graphics& g)
{
    drawCard (g, getLocalBounds().toFloat());
    auto area = getLocalBounds().toFloat().reduced (16.0f, 14.0f);
    area.removeFromRight (52.0f);

    drawCaption (g, "Gain staging", area.removeFromTop (14.0f));
    area.removeFromTop (12.0f);

    juce::String chip, message;
    juce::Colour c = col::textDim;
    const bool hasSignal = hold > -60.0f;

    if (! guide)            { chip = "-"; }
    else if (! hasSignal)   { chip = "No signal"; message = "Press play to measure the level."; }
    else if (hold > -1.0f)  { chip = "Clipping risk"; c = col::bad;
                              message = "Turn the clip gain or the plugin before this down."; }
    else if (hold > maxDb)  { chip = "Too hot"; c = col::bad;
                              message = "Turn it down about " + juce::String (hold - maxDb, 1) + " dB."; }
    else if (hold >= minDb) { chip = "Good level"; c = col::good;
                              message = "Plenty of headroom for processing."; }
    else                    { chip = "Low"; c = col::accent;
                              message = juce::String (minDb - hold, 1) + " dB under target. Fine if it should sit back in the mix."; }

    // Big readout
    g.setColour (hasSignal ? c : col::textFaint);
    g.setFont (font (34.0f, true));
    g.drawText (hasSignal ? juce::String (hold, 1) : juce::String ("--"), area.removeFromTop (40.0f),
                juce::Justification::centredLeft, false);
    g.setColour (col::textFaint);
    g.setFont (font (11.5f));
    g.drawText ("dB peak", area.removeFromTop (16.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (14.0f);

    // Target
    drawCaption (g, "Target", area.removeFromTop (14.0f));
    g.setColour (col::text);
    g.setFont (font (14.0f, true));
    g.drawText (juce::String ((int) minDb) + " to " + juce::String ((int) maxDb) + " dB",
                area.removeFromTop (22.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (12.0f);

    // Status chip
    g.setFont (font (12.0f, true));
    const float chipW = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), chip) + 26.0f;
    const auto chipRect = area.removeFromTop (24.0f).withWidth (chipW);
    g.setColour (c.withAlpha (0.14f));
    g.fillRoundedRectangle (chipRect, 12.0f);
    g.setColour (c);
    g.fillEllipse (chipRect.getX() + 9.0f, chipRect.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.drawText (chip, chipRect.withTrimmedLeft (19.0f), juce::Justification::centredLeft, false);
    area.removeFromTop (10.0f);

    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.append (message, font (12.5f), col::textDim);
    juce::TextLayout layout;
    layout.createLayout (s, area.getWidth());
    layout.draw (g, area);
}

//==============================================================================
void AdvicePanel::setItems (std::vector<Item> newItems, bool isLive)
{
    items = std::move (newItems);
    live = isLive;
    repaint();
}

void AdvicePanel::paint (juce::Graphics& g)
{
    drawCard (g, getLocalBounds().toFloat());
    auto area = getLocalBounds().toFloat().reduced (18.0f, 14.0f);

    auto top = area.removeFromTop (16.0f);
    drawCaption (g, "Live advice", top);
    if (live)
    {
        g.setFont (font (10.5f, true));
        const auto badge = top.removeFromRight (52.0f);
        g.setColour (col::good.withAlpha (0.14f));
        g.fillRoundedRectangle (badge, 8.0f);
        g.setColour (col::good);
        g.fillEllipse (badge.getX() + 8.0f, badge.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.drawText ("LIVE", badge.withTrimmedLeft (17.0f), juce::Justification::centredLeft, false);
    }
    area.removeFromTop (14.0f);

    for (auto& item : items)
    {
        if (area.getHeight() < 30.0f)
            break;

        juce::AttributedString text;
        text.setWordWrap (juce::AttributedString::byWord);
        text.append (item.title + "\n", font (14.0f, true), col::text);
        text.append (item.body, font (13.0f), col::textDim);
        text.setLineSpacing (2.0f);

        juce::TextLayout layout;
        layout.createLayout (text, area.getWidth() - 16.0f);
        const float h = juce::jmin (layout.getHeight(), area.getHeight());

        g.setColour (item.colour);
        g.fillRoundedRectangle (area.getX(), area.getY() + 2.0f, 3.0f, h - 2.0f, 1.5f);
        layout.draw (g, area.withTrimmedLeft (16.0f).withHeight (h));

        area.removeFromTop (h + 16.0f);
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
    genreBox.setTextWhenNothingSelected ("Choose genre...");
    genreBox.setTooltip ("Genre\nThe genre you're making. All advice adapts to it.");
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

    relistenButton.setTooltip ("Re-listen\nForget what the analyzer has heard so far and start again, e.g. after changing your EQ or moving to a new section.");
    relistenButton.onClick = [this] { spectrum.resetAnalysis(); updateAdvice(); };
    addAndMakeVisible (relistenButton);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (gainPanel);
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
    tipText.setLineSpacing (1.25f);
    tipText.setBorder (juce::BorderSize<int> (0));
    tipText.setIndents (0, 0);
    tipText.setMouseCursor (juce::MouseCursor::NormalCursor);
    addAndMakeVisible (tipText);

    picker.onPick = [this] (int g)
    {
        processor.genreIndex = g;
        picker.setVisible (false);
        refreshContent();
    };
    addChildComponent (picker);

    setResizable (true, true);
    setResizeLimits (960, 660, 1900, 1300);
    setSize (1120, 780);

    refreshContent();
    startTimerHz (30);
}

MixMentorEditor::~MixMentorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MixMentorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // Header bar
    g.setColour (col::header);
    g.fillRect (headerArea);
    g.setColour (col::border);
    g.fillRect (headerArea.withTop (headerArea.getBottom() - 1));

    // Logo: gradient tile with a little EQ curve
    auto h = headerArea.toFloat().reduced (20.0f, 0.0f);
    auto logo = h.removeFromLeft (30.0f).withSizeKeepingCentre (30.0f, 30.0f);
    g.setGradientFill (juce::ColourGradient (col::accent, logo.getX(), logo.getY(), col::accent2, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 8.0f);
    juce::Path wave;
    wave.startNewSubPath (logo.getX() + 6.0f, logo.getCentreY() + 4.0f);
    wave.cubicTo (logo.getX() + 11.0f, logo.getCentreY() + 4.0f, logo.getX() + 12.0f, logo.getY() + 8.0f,
                  logo.getCentreX(), logo.getY() + 8.0f);
    wave.cubicTo (logo.getCentreX() + 3.0f, logo.getY() + 8.0f, logo.getRight() - 11.0f, logo.getBottom() - 9.0f,
                  logo.getRight() - 6.0f, logo.getBottom() - 9.0f);
    g.setColour (juce::Colours::white);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    h.removeFromLeft (12.0f);
    auto titleArea = h.removeFromLeft (170.0f).withSizeKeepingCentre (170.0f, 34.0f);
    g.setColour (col::text);
    g.setFont (font (16.0f, true));
    g.drawText ("Mix Mentor", titleArea.removeFromTop (19.0f), juce::Justification::centredLeft, false);
    g.setColour (col::textFaint);
    g.setFont (font (11.0f));
    g.drawText ("learn while you mix", titleArea, juce::Justification::centredLeft, false);

    // Captions above the dropdowns
    drawCaption (g, "Genre", genreBox.getBounds().toFloat().withHeight (12.0f).translated (2.0f, -15.0f));
    drawCaption (g, "Instrument", instrumentBox.getBounds().toFloat().withHeight (12.0f).translated (2.0f, -15.0f));

    // Track name (right side of header)
    if (trackName.isNotEmpty())
    {
        auto r = headerArea.toFloat().withTrimmedRight (20.0f).removeFromRight (260.0f);
        drawCaption (g, "Track", r.withHeight (12.0f).withY (r.getCentreY() - 15.0f).withTrimmedLeft (0.0f));
        g.setColour (col::text);
        g.setFont (font (14.0f, true));
        g.drawText (trackName, r.withY (r.getCentreY() - 1.0f).withHeight (18.0f), juce::Justification::centredLeft, true);
    }

    // Analyzer card frame + header
    drawCard (g, analyzerCard.toFloat());
    auto ah = analyzerCard.toFloat().reduced (18.0f, 0.0f).removeFromTop (44.0f);
    drawCaption (g, "Spectrum", ah.removeFromLeft (80.0f));
    g.setColour (col::textDim);
    g.setFont (font (12.5f));
    g.drawText (summaryText, ah.withTrimmedRight (110.0f), juce::Justification::centredLeft, true);

    // Guide card
    drawCard (g, guideCard.toFloat());
    auto gh = guideCard.toFloat().reduced (18.0f, 14.0f);
    drawCaption (g, "Guide", gh.removeFromTop (16.0f));
    gh.removeFromTop (8.0f);
    g.setColour (col::text);
    g.setFont (font (16.0f, true));
    g.drawText (guideTitle, gh.removeFromTop (22.0f), juce::Justification::centredLeft, true);
}

void MixMentorEditor::resized()
{
    auto area = getLocalBounds();

    headerArea = area.removeFromTop (68);
    {
        auto h = headerArea.reduced (20, 0);
        h.removeFromLeft (30 + 12 + 170 + 24);
        const int comboY = headerArea.getY() + 26;
        genreBox.setBounds (h.getX(), comboY, 210, 32);
        instrumentBox.setBounds (h.getX() + 222, comboY, 190, 32);
    }

    area = area.reduced (20);

    // Top row: analyzer + gain
    auto top = area.removeFromTop ((int) (area.getHeight() * 0.56f));
    auto gainArea = top.removeFromRight (230);
    top.removeFromRight (14);
    analyzerCard = top;
    gainPanel.setBounds (gainArea);

    auto inner = analyzerCard.reduced (10, 0);
    auto bar = inner.removeFromTop (44);
    relistenButton.setBounds (bar.removeFromRight (96).withSizeKeepingCentre (96, 28).translated (-8, 0));
    spectrum.setBounds (inner.withTrimmedBottom (10));

    area.removeFromTop (14);
    processTabs.setBounds (area.removeFromTop (44));
    area.removeFromTop (14);

    // Bottom row: guide + advice
    auto adviceArea = area.removeFromRight ((int) (area.getWidth() * 0.46f));
    area.removeFromRight (14);
    advice.setBounds (adviceArea);
    guideCard = area;
    tipText.setBounds (guideCard.reduced (18, 14).withTrimmedTop (16 + 8 + 22 + 10));

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

    summaryText = g >= 0 ? kb.genres()[(size_t) g].summary : juce::String();

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
    guideTitle = process.name + " for " + kb.instruments()[(size_t) i].name + "  \xc2\xb7  " + kb.genres()[(size_t) g].name;

    auto it = guide->tips.find (process.id);
    tipText.setText (it != guide->tips.end() ? it->second : "No tip yet.", false);
    tipText.moveCaretToTop (false);

    updateGainStatus();
    updateAdvice();
    repaint();
}

void MixMentorEditor::updateGainStatus()
{
    const auto* guide = mm::KnowledgeBase::get().findGuide (processor.genreIndex.load(), processor.instrumentIndex.load());
    gainPanel.setState (holdDb,
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

    const auto genreName = kb.genres()[(size_t) g].name;
    const auto instrumentName = kb.instruments()[(size_t) i].name.toLowerCase();
    const juce::Colour grey = col::textFaint;

    if (! spectrum.hasReadings())
    {
        advice.setItems ({ { grey, "Listening...",
                             "Play the track (solo it for the most accurate advice). "
                             "Advice appears after about 2 seconds of sound and updates as you tweak your plugins." } }, false);
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
        values.set ("genre", genreName);
        values.set ("instrument", instrumentName);
        values.set ("dev", juce::String (dev, 1));
        values.set ("amt", juce::String (juce::jlimit (1, 6, (int) std::round (dev) + 1)));
        values.set ("hz", SpectrumView::formatHz (r.peakHz));
        values.set ("lo", SpectrumView::formatHz (zone.lowHz));
        values.set ("hi", SpectrumView::formatHz (zone.highHz));

        if (r.status > 0)
        {
            const auto tmpl = zone.tooMuch.isNotEmpty() ? zone.tooMuch
                : juce::String ("There's {dev} dB more {label} than typical for a {genre} {instrument}. "
                                "Cut about {amt} dB around {hz} with a medium-wide bell (Q 1-2), then compare with bypass.");
            problems.push_back ({ dev, { SpectrumView::colourForStatus (1),
                                         zone.label + ": too much (+" + juce::String (dev, 1) + " dB)",
                                         fillTemplate (tmpl, values) } });
        }
        else if (r.status < 0)
        {
            const auto tmpl = zone.tooLittle.isNotEmpty() ? zone.tooLittle
                : juce::String ("There's {dev} dB less {label} than typical for a {genre} {instrument}. "
                                "Boost about {amt} dB around {hz} with a wide bell (Q 0.7-1.5).");
            problems.push_back ({ dev, { SpectrumView::colourForStatus (-1),
                                         zone.label + ": too little (-" + juce::String (dev, 1) + " dB)",
                                         fillTemplate (tmpl, values) } });
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
                "Ringing frequency at " + SpectrumView::formatHz (r.resonanceHz),
                fillTemplate ("One narrow frequency sticks out about {res} dB in the {label} zone. "
                              "Cut 3-5 dB at {hz} with a narrow bell (Q 5-8). Tip: sweep a narrow boost "
                              "to hear it ring, then turn the boost into a cut.", values) } });
        }
    }

    std::sort (problems.begin(), problems.end(), [] (auto& a, auto& b) { return a.severity > b.severity; });

    std::vector<AdvicePanel::Item> items;
    for (auto& p : problems)
        items.push_back (p.item);

    if (problems.empty())
        items.push_back ({ SpectrumView::colourForStatus (0), "Sounding balanced for " + genreName,
                           "Every zone is inside the typical range for a " + genreName + " " + instrumentName
                               + ". Now judge it in the full mix: does it sit well next to the other elements?" });
    else if (! okZones.isEmpty())
        items.push_back ({ SpectrumView::colourForStatus (0), "Looks good: " + okZones.joinIntoString (", "),
                           "Leave these alone for now." });

    advice.setItems (std::move (items), true);
}

void MixMentorEditor::timerCallback()
{
    // Sync with changes made by the processor (state restore, track-name guessing).
    if (processor.genreIndex.load() != shownGenre || processor.instrumentIndex.load() != shownInstrument)
        refreshContent();

    if (auto name = processor.getTrackName(); name != trackName)
    {
        trackName = name;
        repaint (headerArea);
    }

    // Levels
    const float peakDb = juce::Decibels::gainToDecibels (processor.takePeak(), -100.0f);

    // Spectrum (zones are only judged while there's real signal, not silence between hits)
    const int n = processor.readAnalysisSamples (readBuffer.data(), (int) readBuffer.size());
    spectrum.pushSamples (readBuffer.data(), n);
    spectrum.updateSpectrum (processor.getCurrentSampleRate(), peakDb > -50.0f);

    const float rmsDb = juce::Decibels::gainToDecibels (processor.getLatestRms(), -100.0f);
    peakDisplayDb = juce::jmax (peakDb, peakDisplayDb - 1.0f);
    rmsDisplayDb  = 0.8f * rmsDisplayDb + 0.2f * rmsDb;

    if (peakDb >= holdDb)       { holdDb = peakDb; holdFrames = 60; }   // hold for ~2 s
    else if (holdFrames > 0)    { --holdFrames; }
    else                        { holdDb = juce::jmax (-100.0f, holdDb - 0.5f); }

    gainPanel.getMeter().setLevels (peakDisplayDb, holdDb, rmsDisplayDb);

    if (++frameCounter % 6 == 0)   // text ~5x per second so it's readable
        updateGainStatus();

    if (frameCounter % 15 == 0)    // advice twice a second, so it doesn't flicker
        updateAdvice();
}
