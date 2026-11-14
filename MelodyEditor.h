#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MelodyProcessor.h"
#include "MelodyPanel.h"
#include "Theme.h"

/** Window of the separate Mix Mentor Melody instrument: just the melody panel. */
class MelodyEditor : public juce::AudioProcessorEditor
{
public:
    explicit MelodyEditor (MelodyProcessor& p)
        : AudioProcessorEditor (&p), panel (p.core, false)
    {
        setLookAndFeel (&lookAndFeel.get());
        tooltipWindow.setOpaque (false);
        addAndMakeVisible (panel);
        setSize (1120, 700);
    }
    ~MelodyEditor() override { setLookAndFeel (nullptr); }
    void resized() override { panel.setBounds (getLocalBounds()); }

private:
    juce::SharedResourcePointer<theme::LookAndFeel> lookAndFeel;
    MelodyPanel panel;
    juce::TooltipWindow tooltipWindow { this, 350 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyEditor)
};
