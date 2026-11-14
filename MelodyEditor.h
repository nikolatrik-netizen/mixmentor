#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MelodyProcessor.h"
#include "Theme.h"

/** Shows the original melody (grey) and the selected idea (amber) on a piano roll. */
class PianoRoll : public juce::Component
{
public:
    void setContent (const melody::Phrase& original, const melody::Phrase& idea, double lengthBeats,
                     const std::vector<int>& scalePitchClasses, int root);
    void setPlayhead (double beat, bool visible);
    void paint (juce::Graphics&) override;

private:
    melody::Phrase original, idea;
    double length = 4.0, playhead = 0.0;
    bool showPlayhead = false;
    std::vector<int> scale;
    int root = 0;
};

/** One clickable idea in the list. */
class IdeaCard : public juce::Button
{
public:
    IdeaCard() : juce::Button ({}) {}
    void setIdea (const juce::String& name, const juce::String& description, int noteCount);
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

private:
    juce::String title, description;
    int notes = 0;
};

/** Drag this into your DAW to drop the idea as a MIDI clip. */
class DragTile : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    std::function<juce::File()> createFile;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    bool dragging = false;
};

class MelodyEditor : public juce::AudioProcessorEditor,
                     public juce::DragAndDropContainer,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit MelodyEditor (MelodyProcessor&);
    ~MelodyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dropHover = false; repaint(); }

private:
    void timerCallback() override;
    void loadPhrase (const melody::Phrase& phrase);
    void regenerate();
    void selectIdea (int index);
    const melody::Phrase& currentPhrase() const;
    double currentLength() const;
    juce::File writeTempMidi();

    MelodyProcessor& processor;
    juce::SharedResourcePointer<theme::LookAndFeel> lookAndFeel;

    juce::ComboBox genreBox, barsBox;
    juce::TextButton captureButton { "capture" }, loadButton { "load .mid" }, newIdeasButton { "new ideas" },
                     playButton { "play" }, originalButton { "original" };
    PianoRoll roll;
    juce::OwnedArray<IdeaCard> cards;
    DragTile dragTile;
    juce::TooltipWindow tooltipWindow { this, 350 };
    std::unique_ptr<juce::FileChooser> chooser;

    melody::Analysis analysis;
    std::vector<melody::Variation> ideas;
    bool showOriginal = false, dropHover = false;
    juce::String statusText;

    juce::Rectangle<int> header, leftPanel, ideasPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyEditor)
};
