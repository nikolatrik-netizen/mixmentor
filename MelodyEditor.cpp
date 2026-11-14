#include "MelodyEditor.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "KnowledgeBase.h"

using namespace theme;

//==============================================================================
void PianoRoll::setContent (const melody::Phrase& o, const melody::Phrase& i, double len,
                            const std::vector<int>& sc, int r)
{
    original = o; idea = i; length = juce::jmax (1.0, len); scale = sc; root = r;
    repaint();
}

void PianoRoll::setPlayhead (double beat, bool visible)
{
    if (std::abs (beat - playhead) > 1.0e-3 || visible != showPlayhead)
    {
        playhead = beat; showPlayhead = visible;
        repaint();
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::analyzerTop);
    g.fillRoundedRectangle (b, 6.0f);

    if (original.empty() && idea.empty())
    {
        g.setColour (col::textFaint);
        g.setFont (font (14.0f));
        g.drawFittedText ("capture a melody or drop a .mid file to start", getLocalBounds().reduced (20), juce::Justification::centred, 2);
        return;
    }

    // Pitch range covering both phrases (with a little room)
    int lo = 127, hi = 0;
    for (auto* p : { &original, &idea })
        for (auto& n : *p) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    lo -= 3; hi += 3;
    if (hi - lo < 18) { const int mid = (hi + lo) / 2; lo = mid - 9; hi = mid + 9; }

    const auto area = b.reduced (0.0f, 6.0f).withTrimmedLeft (34.0f);
    const float rowH = area.getHeight() / (float) (hi - lo + 1);
    auto yFor = [&] (int pitch) { return area.getBottom() - (float) (pitch - lo + 1) * rowH; };
    auto xFor = [&] (double beat) { return area.getX() + (float) (beat / length) * area.getWidth(); };

    // Rows: black keys darker, notes of the detected scale tinted, root marked
    for (int p = lo; p <= hi; ++p)
    {
        const int pc = ((p % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const bool inScale = std::find (scale.begin(), scale.end(), pc) != scale.end();
        const juce::Rectangle<float> row (area.getX(), yFor (p), area.getWidth(), rowH);
        if (inScale)     { g.setColour (col::accent.withAlpha (pc == root ? 0.07f : 0.03f)); g.fillRect (row); }
        else if (black)  { g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRect (row); }

        if (pc == 0 || pc == root)
        {
            g.setColour (pc == root ? col::accent.withAlpha (0.8f) : col::textFaint);
            g.setFont (font (10.0f));
            g.drawText (melody::noteName (p), juce::Rectangle<float> (2.0f, row.getY() - 2.0f, 30.0f, rowH + 4.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // Beat and bar lines
    for (int beat = 0; beat <= (int) length; ++beat)
    {
        g.setColour (juce::Colours::white.withAlpha (beat % 4 == 0 ? 0.12f : 0.04f));
        g.fillRect (xFor (beat), area.getY(), 1.0f, area.getHeight());
    }

    auto drawNotes = [&] (const melody::Phrase& p, juce::Colour c, bool outline)
    {
        for (auto& n : p)
        {
            auto r = juce::Rectangle<float> (xFor (n.start), yFor (n.pitch),
                                             juce::jmax (3.0f, xFor (n.start + n.length) - xFor (n.start) - 1.0f), rowH)
                         .reduced (0.0f, rowH > 6.0f ? 1.0f : 0.0f);
            if (outline)
            {
                g.setColour (c);
                g.drawRoundedRectangle (r, 2.0f, 1.0f);
            }
            else
            {
                g.setColour (c.withAlpha (0.35f + 0.6f * n.velocity));
                g.fillRoundedRectangle (r, 2.0f);
            }
        }
    };
    drawNotes (original, col::textFaint, ! idea.empty());   // original as outlines behind the idea
    drawNotes (idea, col::accent, false);

    if (showPlayhead)
    {
        g.setColour (col::text.withAlpha (0.8f));
        g.fillRect (xFor (playhead), area.getY(), 1.5f, area.getHeight());
    }
}

//==============================================================================
void IdeaCard::setIdea (const juce::String& name, const juce::String& desc, int noteCount)
{
    title = name; description = desc; notes = noteCount;
    repaint();
}

void IdeaCard::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (on ? col::box : isHighlighted || isDown ? col::panelLight.brighter (0.03f) : col::panelLight);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? col::accent : isHighlighted ? col::border.brighter (0.3f) : col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 1.5f : 1.0f);

    auto inner = r.reduced (12.0f, 9.0f);
    auto top = inner.removeFromTop (18.0f);
    g.setColour (col::textFaint);
    g.setFont (font (11.0f));
    g.drawText (juce::String (notes) + " notes", top.removeFromRight (60.0f), juce::Justification::centredRight, false);
    g.setColour (on ? col::accent : col::text);
    g.setFont (font (14.0f, true));
    g.drawText (title, top, juce::Justification::centredLeft, true);
    inner.removeFromTop (3.0f);
    g.setColour (col::textDim);
    g.setFont (font (12.0f));
    g.drawFittedText (description, inner.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
}

//==============================================================================
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool hover = isMouseOver();
    g.setColour (hover ? col::accent.withAlpha (0.18f) : col::accent.withAlpha (0.1f));
    g.fillRoundedRectangle (r, 5.0f);
    juce::Path outline, dashed;
    outline.addRoundedRectangle (r, 5.0f);
    const float dashes[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (col::accent.withAlpha (hover ? 0.9f : 0.6f));
    g.fillPath (dashed);

    g.setColour (col::accent);
    g.setFont (font (13.0f, true));
    g.drawText (dragging ? "dropping..." : "drag midi to your DAW", getLocalBounds(), juce::Justification::centred, false);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5 || ! createFile)
        return;

    const auto file = createFile();
    if (! file.existsAsFile())
        return;

    dragging = true;
    repaint();
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
        [safe = juce::Component::SafePointer<DragTile> (this)]
        {
            if (safe != nullptr) { safe->dragging = false; safe->repaint(); }
        });
}

//==============================================================================
MelodyEditor::MelodyEditor (MelodyProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel.get());
    tooltipWindow.setOpaque (false);

    const auto& kb = mm::KnowledgeBase::get();
    for (size_t i = 0; i < kb.genres().size(); ++i)
        genreBox.addItem (kb.genres()[i].name, (int) i + 1);
    const int gi = kb.indexOfGenre (processor.genreId);
    genreBox.setSelectedId (gi >= 0 ? gi + 1 : 1, juce::dontSendNotification);
    if (gi < 0) processor.genreId = kb.genres().front().id;
    genreBox.setTooltip ("Genre\nSteers the ideas: groove, swing, rhythm style and how jazzy the colour notes are.");
    genreBox.onChange = [this]
    {
        processor.genreId = mm::KnowledgeBase::get().genres()[(size_t) genreBox.getSelectedId() - 1].id;
        regenerate();
    };
    addAndMakeVisible (genreBox);

    for (int bars : { 1, 2, 4, 8 })
        barsBox.addItem (juce::String (bars) + (bars == 1 ? " bar" : " bars"), bars);
    barsBox.setSelectedId (2, juce::dontSendNotification);
    barsBox.setTooltip ("Capture length\nHow many bars to record, starting at the bar of the first note you play.");
    addAndMakeVisible (barsBox);

    captureButton.setTooltip ("Capture\nPress, then play your melody (or press play in your DAW on a MIDI clip on this track). "
                              "Recording starts at your first note and stops after the chosen number of bars.");
    captureButton.onClick = [this]
    {
        if (processor.isCapturing()) processor.stopCapture();
        else
        {
            processor.setPreviewPlaying (false);
            processor.startCapture (barsBox.getSelectedId());
        }
    };
    addAndMakeVisible (captureButton);

    loadButton.setTooltip ("Load a MIDI file\nOr simply drag a .mid file onto this window.");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a melody", juce::File(), "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadPhrase (melody::readMidiFile (fc.getResult()));
                              });
    };
    addAndMakeVisible (loadButton);

    newIdeasButton.setTooltip ("New ideas\nRoll the dice again: same techniques, different notes.");
    newIdeasButton.onClick = [this] { ++processor.seed; regenerate(); };
    addAndMakeVisible (newIdeasButton);

    playButton.setTooltip ("Play\nLoops the selected idea through the built-in preview sound (and the MIDI output).");
    playButton.onClick = [this] { processor.setPreviewPlaying (! processor.isPreviewPlaying()); };
    addAndMakeVisible (playButton);

    originalButton.setTooltip ("Original\nToggle between your melody and the selected idea.");
    originalButton.setClickingTogglesState (true);
    originalButton.onClick = [this] { showOriginal = originalButton.getToggleState(); selectIdea (processor.selectedVariation); };
    addAndMakeVisible (originalButton);

    addAndMakeVisible (roll);

    dragTile.createFile = [this] { return writeTempMidi(); };
    dragTile.setTooltip ("Drag to your DAW\nDrag this onto a track in Studio One (or any DAW) to drop the idea as a MIDI clip.");
    addAndMakeVisible (dragTile);

    for (int i = 0; i < 9; ++i)
    {
        auto* c = cards.add (new IdeaCard());
        c->setClickingTogglesState (false);
        c->onClick = [this, i] { showOriginal = false; originalButton.setToggleState (false, juce::dontSendNotification); selectIdea (i); };
        addAndMakeVisible (c);
    }

    setSize (1120, 700);
    loadPhrase (processor.original);
    startTimerHz (30);
}

MelodyEditor::~MelodyEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

const melody::Phrase& MelodyEditor::currentPhrase() const
{
    if (showOriginal || ideas.empty() || ! juce::isPositiveAndBelow (processor.selectedVariation, (int) ideas.size()))
        return processor.original;
    return ideas[(size_t) processor.selectedVariation].notes;
}

double MelodyEditor::currentLength() const
{
    double end = analysis.lengthBeats;
    for (auto& n : currentPhrase()) end = juce::jmax (end, n.start + n.length);
    return std::ceil (end / 4.0 - 0.01) * 4.0;
}

void MelodyEditor::loadPhrase (const melody::Phrase& phrase)
{
    processor.original = phrase;
    analysis = melody::analyse (phrase);
    regenerate();
}

void MelodyEditor::regenerate()
{
    const auto feel = melody::feelForGenre (processor.genreId);
    ideas = melody::generate (processor.original, analysis, feel, processor.seed);

    for (int i = 0; i < cards.size(); ++i)
    {
        const bool has = i < (int) ideas.size();
        cards[i]->setVisible (has);
        if (has)
            cards[i]->setIdea (ideas[(size_t) i].name, ideas[(size_t) i].description, (int) ideas[(size_t) i].notes.size());
    }

    if (processor.original.empty())
        statusText = "no melody yet";
    else
    {
        const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
        statusText = analysis.keyName + dot + juce::String ((int) (analysis.lengthBeats / 4.0)) + " bars" + dot
                   + juce::String ((int) processor.original.size()) + " notes" + dot
                   + melody::noteName (analysis.lowest) + "-" + melody::noteName (analysis.highest);
    }

    selectIdea (juce::jlimit (0, juce::jmax (0, (int) ideas.size() - 1), processor.selectedVariation));
    repaint();
}

void MelodyEditor::selectIdea (int index)
{
    processor.selectedVariation = index;
    for (int i = 0; i < cards.size(); ++i)
        cards[i]->setToggleState (! showOriginal && i == index, juce::dontSendNotification);

    const auto& shown = currentPhrase();
    roll.setContent (processor.original, showOriginal ? melody::Phrase() : shown, currentLength(), analysis.scale, analysis.root);
    processor.setPreview (shown, currentLength());
}

juce::File MelodyEditor::writeTempMidi()
{
    const auto& phrase = currentPhrase();
    if (phrase.empty())
        return {};

    const auto name = showOriginal || ideas.empty() ? juce::String ("original")
                                                    : ideas[(size_t) processor.selectedVariation].name;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MixMentorMelody");
    dir.createDirectory();
    auto file = dir.getChildFile ("Mix Mentor - " + name.replaceCharacters ("&/\\:", "+---") + ".mid");
    melody::writeMidiFile (phrase, file, processor.getBpm());
    return file;
}

bool MelodyEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void MelodyEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
        {
            loadPhrase (melody::readMidiFile (juce::File (f)));
            break;
        }
    repaint();
}

void MelodyEditor::timerCallback()
{
    melody::Phrase captured;
    const bool finished = processor.pollCapture (captured);
    if (finished)
        loadPhrase (captured);
    else if (processor.isCapturing())
        roll.setContent (captured, {}, barsBox.getSelectedId() * 4.0, analysis.scale, analysis.root);

    captureButton.setButtonText (processor.isCapturing() ? "stop capture" : "capture");
    captureButton.setToggleState (processor.isCapturing(), juce::dontSendNotification);
    playButton.setButtonText (processor.isPreviewPlaying() ? "stop" : "play");
    roll.setPlayhead (processor.getPreviewBeat(), processor.isPreviewPlaying());
}

//==============================================================================
void MelodyEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::window);

    // Header
    g.setColour (col::topBar);
    g.fillRect (header);
    g.setColour (col::border);
    g.fillRect (header.getX(), header.getBottom() - 1, header.getWidth(), 1);
    auto h = header.toFloat().reduced (20.0f, 0.0f);
    drawLogoMark (g, h.withWidth (42.0f).withSizeKeepingCentre (42.0f, 42.0f), true);
    h.removeFromLeft (54.0f);
    g.setColour (col::text);
    g.setFont (fontLight (28.0f));
    g.drawText ("mix mentor", h.withWidth (150.0f), juce::Justification::centredLeft, false);
    const float wordW = (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (fontLight (28.0f)), "mix mentor");
    g.setColour (col::accent);
    g.setFont (font (28.0f, false));
    g.drawText ("melody", h.withTrimmedLeft (wordW + 10.0f).withWidth (110.0f), juce::Justification::centredLeft, false);
    g.setColour (col::textDim);
    g.setFont (font (13.0f));
    g.drawText (statusText, h.withTrimmedLeft (280.0f).withTrimmedRight (260.0f), juce::Justification::centredLeft, true);
    drawLabel (g, "genre", genreBox.getBounds().toFloat().translated (-58.0f, 0.0f).withWidth (50.0f), col::textDim,
               juce::Justification::centredRight);

    // Left panel
    g.setColour (col::sideTop);
    g.fillRect (leftPanel);
    g.setColour (col::border);
    g.fillRect (leftPanel.getRight() - 1, leftPanel.getY(), 1, leftPanel.getHeight());
    auto l = leftPanel.toFloat().reduced (20.0f, 18.0f);
    drawLabel (g, "input", l.removeFromTop (18.0f));

    // Drop zone
    auto drop = juce::Rectangle<float> (l.getX(), (float) loadButton.getBottom() + 14.0f, l.getWidth(), 70.0f);
    juce::Path outline, dashed;
    outline.addRoundedRectangle (drop, 5.0f);
    const float dashes[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (dropHover ? col::accent : col::border.brighter (0.2f));
    g.fillPath (dashed);
    g.setColour (dropHover ? col::accent : col::textFaint);
    g.setFont (font (12.5f));
    g.drawFittedText ("or drop a .mid file\nanywhere on this window", drop.toNearestInt(), juce::Justification::centred, 2);

    // How-to
    auto how = juce::Rectangle<float> (l.getX(), drop.getBottom() + 22.0f, l.getWidth(), l.getBottom() - drop.getBottom() - 22.0f);
    drawLabel (g, "how it works", how.removeFromTop (18.0f));
    how.removeFromTop (6.0f);
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.setLineSpacing (3.0f);
    s.append ("1.  Put this plugin on an instrument track.\n"
              "2.  Press capture and play your melody (or play a MIDI clip on this track).\n"
              "3.  Pick an idea below and press play to hear it.\n"
              "4.  Drag it into your DAW as a MIDI clip.\n\n"
              "The original stays as grey outlines, so you can see what changed.",
              font (12.5f), col::textDim);
    juce::TextLayout tl;
    tl.createLayout (s, how.getWidth());
    tl.draw (g, how);

    // Ideas label
    drawLabel (g, "ideas", ideasPanel.toFloat().withHeight (18.0f).translated (0.0f, -24.0f));
}

void MelodyEditor::resized()
{
    auto area = getLocalBounds();
    header = area.removeFromTop (64);
    genreBox.setBounds (header.withTrimmedRight (22).removeFromRight (200).withSizeKeepingCentre (200, 30));

    leftPanel = area.removeFromLeft (250);
    auto l = leftPanel.reduced (20, 18).withTrimmedTop (26);
    auto row = l.removeFromTop (34);
    captureButton.setBounds (row.removeFromLeft (row.getWidth() - 96));
    row.removeFromLeft (8);
    barsBox.setBounds (row);
    l.removeFromTop (10);
    loadButton.setBounds (l.removeFromTop (32));

    area = area.reduced (20, 16);
    auto transport = area.removeFromTop (34);
    playButton.setBounds (transport.removeFromLeft (90));
    transport.removeFromLeft (8);
    originalButton.setBounds (transport.removeFromLeft (100));
    transport.removeFromLeft (8);
    newIdeasButton.setBounds (transport.removeFromLeft (110));
    dragTile.setBounds (transport.removeFromRight (220));
    area.removeFromTop (12);

    roll.setBounds (area.removeFromTop ((int) (area.getHeight() * 0.48f)));
    area.removeFromTop (36);
    ideasPanel = area;

    const int cols = 3, rows = 3, gap = 10;
    const int cw = (area.getWidth() - (cols - 1) * gap) / cols;
    const int ch = (area.getHeight() - (rows - 1) * gap) / rows;
    for (int i = 0; i < cards.size(); ++i)
        cards[i]->setBounds (area.getX() + (i % cols) * (cw + gap), area.getY() + (i / cols) * (ch + gap), cw, ch);
}
