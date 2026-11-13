#include "PluginEditor.h"

namespace
{
    const juce::Colour bg       (0xff1d2027);
    const juce::Colour panel    (0xff262a33);
    const juce::Colour accent   (0xff7aa7ff);
    const juce::Colour textDim  (0xffa9b0bd);
}

//==============================================================================
GenrePicker::GenrePicker()
{
    const auto& genres = mm::KnowledgeBase::get().genres();
    for (size_t i = 0; i < genres.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (genres[i].name));
        b->setColour (juce::TextButton::buttonColourId, panel);
        b->setColour (juce::TextButton::buttonOnColourId, accent);
        b->onClick = [this, i] { if (onPick) onPick ((int) i); };
        addAndMakeVisible (b);
    }
}

void GenrePicker::paint (juce::Graphics& g)
{
    g.fillAll (bg.withAlpha (0.97f));
    auto area = getLocalBounds().withSizeKeepingCentre (520, 300);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("What genre are you making?", area.removeFromTop (40), juce::Justification::centred);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("Every tip, frequency zone and level target will be tailored to it.",
                area.removeFromTop (30), juce::Justification::centred);
}

void GenrePicker::resized()
{
    auto area = getLocalBounds().withSizeKeepingCentre (520, 300).withTrimmedTop (90);
    for (auto* b : buttons)
    {
        b->setBounds (area.removeFromTop (44).reduced (60, 0));
        area.removeFromTop (10);
    }
}

//==============================================================================
MixMentorEditor::MixMentorEditor (MixMentorProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    const auto& kb = mm::KnowledgeBase::get();

    titleLabel.setText ("MIX MENTOR", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accent);
    addAndMakeVisible (titleLabel);

    for (size_t i = 0; i < kb.genres().size(); ++i)
        genreBox.addItem (kb.genres()[i].name, (int) i + 1);
    genreBox.setTextWhenNothingSelected ("Choose genre...");
    genreBox.setTooltip ("The genre you're making. All advice adapts to it.");
    genreBox.onChange = [this]
    {
        processor.genreIndex = genreBox.getSelectedId() - 1;
        refreshContent();
    };
    addAndMakeVisible (genreBox);

    for (size_t i = 0; i < kb.instruments().size(); ++i)
        instrumentBox.addItem (kb.instruments()[i].name, (int) i + 1);
    instrumentBox.setTooltip ("What's on this track? (Guessed from the track name when your DAW provides it.)");
    instrumentBox.onChange = [this]
    {
        processor.instrumentIndex = instrumentBox.getSelectedId() - 1;
        processor.instrumentChosenByUser = true;
        refreshContent();
    };
    addAndMakeVisible (instrumentBox);

    trackLabel.setColour (juce::Label::textColourId, textDim);
    trackLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (trackLabel);

    summaryLabel.setColour (juce::Label::textColourId, textDim);
    summaryLabel.setFont (juce::FontOptions (13.0f, juce::Font::italic));
    addAndMakeVisible (summaryLabel);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (meter);

    gainStatus.setJustificationType (juce::Justification::topLeft);
    gainStatus.setFont (juce::FontOptions (12.5f));
    gainStatus.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (gainStatus);

    for (size_t i = 0; i < kb.processes().size(); ++i)
    {
        auto* b = processButtons.add (new juce::TextButton (kb.processes()[i].name));
        b->setRadioGroupId (1001);
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonColourId, panel);
        b->setColour (juce::TextButton::buttonOnColourId, accent.darker (0.3f));
        b->onClick = [this, i] { selectProcess ((int) i); };
        addAndMakeVisible (b);
    }

    tipHeader.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    tipHeader.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (tipHeader);

    tipText.setMultiLine (true, true);
    tipText.setReadOnly (true);
    tipText.setCaretVisible (false);
    tipText.setScrollbarsShown (true);
    tipText.setFont (juce::FontOptions (15.0f));
    tipText.setColour (juce::TextEditor::backgroundColourId, panel);
    tipText.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    tipText.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe6e9ef));
    tipText.setBorder (juce::BorderSize<int> (10));
    addAndMakeVisible (tipText);

    picker.onPick = [this] (int g)
    {
        processor.genreIndex = g;
        picker.setVisible (false);
        refreshContent();
    };
    addChildComponent (picker);

    processButtons[0]->setToggleState (true, juce::dontSendNotification);

    setResizable (true, true);
    setResizeLimits (820, 560, 1600, 1100);
    setSize (980, 640);

    refreshContent();
    startTimerHz (30);
}

MixMentorEditor::~MixMentorEditor()
{
    stopTimer();
}

void MixMentorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
}

void MixMentorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (34);
    titleLabel.setBounds (top.removeFromLeft (150));
    genreBox.setBounds (top.removeFromLeft (200).reduced (0, 3));
    top.removeFromLeft (10);
    instrumentBox.setBounds (top.removeFromLeft (180).reduced (0, 3));
    trackLabel.setBounds (top);

    summaryLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (4);

    auto middle = area.removeFromTop (juce::jmax (240, (int) (area.getHeight() * 0.52f)));
    auto right = middle.removeFromRight (170);
    middle.removeFromRight (10);
    spectrum.setBounds (middle);

    meter.setBounds (right.removeFromLeft (70));
    right.removeFromLeft (8);
    gainStatus.setBounds (right);

    area.removeFromTop (10);
    auto buttonsRow = area.removeFromTop (32);
    const int bw = buttonsRow.getWidth() / juce::jmax (1, processButtons.size());
    for (auto* b : processButtons)
        b->setBounds (buttonsRow.removeFromLeft (bw).reduced (2, 0));

    area.removeFromTop (8);
    tipHeader.setBounds (area.removeFromTop (24));
    tipText.setBounds (area);

    picker.setBounds (getLocalBounds());
}

void MixMentorEditor::selectProcess (int index)
{
    currentProcess = index;
    refreshContent();
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

    const auto* guide = kb.findGuide (g, i);

    if (g >= 0)
        summaryLabel.setText (kb.genres()[(size_t) g].summary, juce::dontSendNotification);

    if (guide == nullptr)
    {
        spectrum.setZones ({});
        tipHeader.setText ({}, juce::dontSendNotification);
        tipText.setText ("No guide for this combination yet.", false);
        return;
    }

    spectrum.setZones (guide->zones);
    meter.setTargetRange (guide->peakMinDb, guide->peakMaxDb);

    const auto& process = kb.processes()[(size_t) currentProcess];
    tipHeader.setText (kb.instruments()[(size_t) i].name + "   |   " + kb.genres()[(size_t) g].name
                           + "   |   " + process.name,
                       juce::dontSendNotification);

    auto it = guide->tips.find (process.id);
    tipText.setText (it != guide->tips.end() ? it->second : "No tip yet.", false);
    tipText.moveCaretToTop (false);

    updateGainStatus();
}

void MixMentorEditor::updateGainStatus()
{
    const auto* guide = mm::KnowledgeBase::get().findGuide (processor.genreIndex.load(), processor.instrumentIndex.load());
    if (guide == nullptr)
    {
        gainStatus.setText ({}, juce::dontSendNotification);
        return;
    }

    const auto target = juce::String ((int) guide->peakMinDb) + " to " + juce::String ((int) guide->peakMaxDb) + " dB";
    juce::String text = "GAIN STAGING\nTarget peaks: " + target + "\n\n";
    juce::Colour colour = textDim;

    if (holdDb < -60.0f)
    {
        text << "No signal yet.\nPress play.";
    }
    else
    {
        text << "Peak: " << juce::String (holdDb, 1) << " dB\n\n";

        if (holdDb > -1.0f)
        {
            text << "Clipping risk! Turn the clip gain or the plugin before this down.";
            colour = juce::Colour (0xffef5b5b);
        }
        else if (holdDb > guide->peakMaxDb)
        {
            text << "Too hot for this element. Turn it down about "
                 << juce::String (holdDb - guide->peakMaxDb, 1) << " dB.";
            colour = juce::Colour (0xffef5b5b);
        }
        else if (holdDb >= guide->peakMinDb)
        {
            text << "Good level. Plenty of headroom for processing.";
            colour = juce::Colour (0xff3ecf8e);
        }
        else
        {
            text << "About " << juce::String (guide->peakMinDb - holdDb, 1)
                 << " dB under target. Fine if it's meant to sit back in the mix.";
            colour = accent;
        }
    }

    gainStatus.setColour (juce::Label::textColourId, colour);
    gainStatus.setText (text, juce::dontSendNotification);
}

void MixMentorEditor::timerCallback()
{
    // Sync with changes made by the processor (state restore, track-name guessing).
    if (processor.genreIndex.load() != shownGenre || processor.instrumentIndex.load() != shownInstrument)
        refreshContent();

    if (auto name = processor.getTrackName(); name != lastTrackName)
    {
        lastTrackName = name;
        trackLabel.setText (name.isNotEmpty() ? "Track: " + name : juce::String(), juce::dontSendNotification);
    }

    // Spectrum
    const int n = processor.readAnalysisSamples (readBuffer.data(), (int) readBuffer.size());
    spectrum.pushSamples (readBuffer.data(), n);
    spectrum.updateSpectrum (processor.getCurrentSampleRate());

    // Levels
    const float peakDb = juce::Decibels::gainToDecibels (processor.takePeak(), -100.0f);
    const float rmsDb  = juce::Decibels::gainToDecibels (processor.getLatestRms(), -100.0f);

    peakDisplayDb = juce::jmax (peakDb, peakDisplayDb - 1.0f);
    rmsDisplayDb  = 0.8f * rmsDisplayDb + 0.2f * rmsDb;

    if (peakDb >= holdDb)       { holdDb = peakDb; holdFrames = 60; }   // hold for ~2 s
    else if (holdFrames > 0)    { --holdFrames; }
    else                        { holdDb = juce::jmax (-100.0f, holdDb - 0.5f); }

    meter.setLevels (peakDisplayDb, holdDb, rmsDisplayDb);

    if (++frameCounter % 6 == 0)  // update the text ~5x per second so it's readable
        updateGainStatus();
}
