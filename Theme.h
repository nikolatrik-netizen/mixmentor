#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Colours, fonts and the custom look of every control. */
namespace theme
{
    namespace col
    {
        const juce::Colour bg          (0xff0e1015);
        const juce::Colour header      (0xff12151b);
        const juce::Colour card        (0xff161a21);
        const juce::Colour raised      (0xff1d222b);
        const juce::Colour raisedHover (0xff252b36);
        const juce::Colour border      (0xff262c37);
        const juce::Colour analyzerTop (0xff0b0d12);
        const juce::Colour analyzerBot (0xff10131a);

        const juce::Colour text        (0xffe9ecf2);
        const juce::Colour textDim     (0xff9aa3b5);
        const juce::Colour textFaint   (0xff5d6576);

        const juce::Colour accent      (0xff8b93ff);   // soft periwinkle
        const juce::Colour accent2     (0xff5fd4f4);   // cyan, used in gradients

        const juce::Colour good        (0xff3ddc97);
        const juce::Colour bad         (0xffff6b6b);
        const juce::Colour warn        (0xffffc24b);
    }

    /** Inter, embedded in the plugin so it looks the same on every computer. */
    juce::FontOptions font (float height, bool semibold = false);

    /** Rounded panel with a hairline border. */
    void drawCard (juce::Graphics&, juce::Rectangle<float> area, juce::Colour fill = col::card, float radius = 10.0f);

    /** Small all-caps section title, e.g. "LIVE ADVICE". */
    void drawCaption (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                      juce::Colour colour = col::textFaint);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                                bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                const juce::String& text, const juce::String& shortcutKeyText,
                                const juce::Drawable* icon, const juce::Colour* textColour) override;
        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                        int& idealWidth, int& idealHeight) override;
        juce::Font getPopupMenuFont() override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool isHighlighted, bool isDown) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        int getDefaultScrollbarWidth() override { return 8; }
        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isVertical,
                            int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown) override;

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    private:
        static juce::TextLayout layoutTooltip (const juce::String& text, float maxWidth);
    };
}
