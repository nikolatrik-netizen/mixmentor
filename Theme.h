#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Colours, fonts and the custom look of every control. Dark graphite with a warm amber accent. */
namespace theme
{
    namespace col
    {
        const juce::Colour window      (0xff0f1114);   // deep graphite
        const juce::Colour sideTop     (0xff181a1e);
        const juce::Colour sideBottom  (0xff121417);
        const juce::Colour panel       (0xff191c20);
        const juce::Colour panelLight  (0xff1f2328);
        const juce::Colour box         (0xff24282e);   // value boxes, dropdowns
        const juce::Colour boxHover    (0xff2c3138);
        const juce::Colour border      (0xff30353d);
        const juce::Colour topBar      (0xff16181c);

        const juce::Colour analyzerTop (0xff111317);
        const juce::Colour analyzerBot (0xff161a1f);
        const juce::Colour liveFill    (0xff2b4656);   // muted teal

        const juce::Colour text        (0xffe8e6e3);
        const juce::Colour textDim     (0xff9b9fa6);
        const juce::Colour textFaint   (0xff636871);
        const juce::Colour knob        (0xff2a2e34);

        const juce::Colour accent      (0xfff2a03d);   // warm amber
        const juce::Colour good        (0xff4cc38a);
        const juce::Colour bad         (0xfff0625d);
        const juce::Colour warn        (0xff5aa9e6);   // "too little" = cool blue
    }

    juce::FontOptions font (float height, bool semibold = false);
    juce::FontOptions fontLight (float height);

    /** Soft rounded panel. */
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area, juce::Colour fill = col::panel, float radius = 6.0f);

    /** Small lowercase section label, e.g. "live advice". */
    void drawLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                    juce::Colour colour = col::textDim, juce::Justification just = juce::Justification::centredLeft);

    /** Light rounded value box with centred text, like a hardware read-out. */
    void drawValueBox (juce::Graphics&, juce::Rectangle<float> area, const juce::String& text,
                       juce::Colour textColour = col::text);

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
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                               float startAngle, float endAngle, juce::Slider&) override;

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
