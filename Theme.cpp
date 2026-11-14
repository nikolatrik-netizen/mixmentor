#include "Theme.h"
#include "BinaryData.h"

namespace theme
{
    namespace
    {
        juce::Typeface::Ptr regularFace()
        {
            static auto face = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,
                                                                        (size_t) BinaryData::InterRegular_ttfSize);
            return face;
        }

        juce::Typeface::Ptr semiboldFace()
        {
            static auto face = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf,
                                                                        (size_t) BinaryData::InterSemiBold_ttfSize);
            return face;
        }
    }

    juce::FontOptions font (float height, bool semibold)
    {
        return juce::FontOptions (semibold ? semiboldFace() : regularFace()).withHeight (height);
    }

    void drawCard (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill, float radius)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (area, radius);
        g.setColour (col::border);
        g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);
    }

    void drawCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour colour)
    {
        g.setColour (colour);
        g.setFont (juce::Font (font (11.0f, true)).withExtraKerningFactor (0.12f));
        g.drawText (text.toUpperCase(), area, juce::Justification::centredLeft, false);
    }

    //==============================================================================
    LookAndFeel::LookAndFeel()
    {
        using namespace juce;

        setColourScheme ({ col::bg, col::card, col::raised, col::border, col::text,
                           col::accent, col::text, col::raised, col::text });

        setColour (ResizableWindow::backgroundColourId, col::bg);

        setColour (ComboBox::backgroundColourId, col::raised);
        setColour (ComboBox::outlineColourId, col::border);
        setColour (ComboBox::textColourId, col::text);
        setColour (ComboBox::arrowColourId, col::textDim);
        setColour (ComboBox::focusedOutlineColourId, col::accent);

        setColour (PopupMenu::backgroundColourId, col::raised);
        setColour (PopupMenu::textColourId, col::text);
        setColour (PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.16f));
        setColour (PopupMenu::highlightedTextColourId, col::text);

        setColour (TextButton::buttonColourId, col::raised);
        setColour (TextButton::buttonOnColourId, col::accent.withAlpha (0.2f));
        setColour (TextButton::textColourOffId, col::text);
        setColour (TextButton::textColourOnId, col::text);

        setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        setColour (TextEditor::textColourId, col::text.withAlpha (0.88f));
        setColour (TextEditor::highlightColourId, col::accent.withAlpha (0.3f));

        setColour (Label::textColourId, col::text);
        setColour (ScrollBar::thumbColourId, col::textFaint);

        setColour (TooltipWindow::backgroundColourId, col::raised);
        setColour (TooltipWindow::textColourId, col::text);
        setColour (TooltipWindow::outlineColourId, col::border);
    }

    juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
    {
        return f.isBold() ? semiboldFace() : regularFace();
    }

    //==============================================================================
    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int, int, int, int, juce::ComboBox& box)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        const bool hover = box.isMouseOver (true) || isButtonDown;

        g.setColour (hover ? col::raisedHover : col::raised);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (box.hasKeyboardFocus (false) ? col::accent.withAlpha (0.6f) : col::border);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);

        // Chevron
        const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (hover ? col::text : col::textDim);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return font (14.0f, true);
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (12, 0, box.getWidth() - 36, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    //==============================================================================
    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        g.fillAll (col::raised);
        g.setColour (col::border);
        g.drawRect (0, 0, width, height, 1);
    }

    void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                         bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool,
                                         const juce::String& text, const juce::String&,
                                         const juce::Drawable*, const juce::Colour*)
    {
        if (isSeparator)
        {
            g.setColour (col::border);
            g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        auto r = area.toFloat().reduced (4.0f, 1.0f);
        if (isHighlighted && isActive)
        {
            g.setColour (col::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (r, 5.0f);
        }

        if (isTicked)
        {
            g.setColour (col::accent);
            g.fillEllipse (r.getX() + 10.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
        }

        g.setColour (isActive ? (isTicked ? col::text : col::text.withAlpha (0.85f)) : col::textFaint);
        g.setFont (font (14.0f, isTicked));
        g.drawText (text, r.withTrimmedLeft (26.0f), juce::Justification::centredLeft, true);
    }

    void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int,
                                                 int& idealWidth, int& idealHeight)
    {
        if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
        idealHeight = 32;
        idealWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (font (14.0f, true)), text) + 48;
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return font (14.0f);
    }

    //==============================================================================
    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool isHighlighted, bool isDown)
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        g.setColour (on ? col::accent.withAlpha (0.2f)
                        : isDown ? col::raisedHover.brighter (0.05f)
                        : isHighlighted ? col::raisedHover : col::raised);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (on ? col::accent.withAlpha (0.5f) : col::border);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int)
    {
        return font (13.0f, true);
    }

    void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                     bool isVertical, int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown)
    {
        juce::Rectangle<float> thumb = isVertical
            ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) width - 4.0f, (float) thumbSize)
            : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) height - 4.0f);
        g.setColour (col::textFaint.withAlpha (isMouseOver || isMouseDown ? 0.8f : 0.45f));
        g.fillRoundedRectangle (thumb, 3.0f);
    }

    //==============================================================================
    juce::TextLayout LookAndFeel::layoutTooltip (const juce::String& text, float maxWidth)
    {
        // First line is the title (bold); the rest is body text.
        const auto title = text.upToFirstOccurrenceOf ("\n", false, false);
        const auto body  = text.fromFirstOccurrenceOf ("\n", false, false).trim();

        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        s.append (title, font (13.0f, true), col::text);
        if (body.isNotEmpty())
            s.append ("\n\n" + body, font (12.5f), col::text.withAlpha (0.8f));

        juce::TextLayout layout;
        layout.createLayout (s, maxWidth);
        return layout;
    }

    juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
    {
        const auto layout = layoutTooltip (tipText, 300.0f);
        const int w = (int) std::ceil (juce::jmin (300.0f, layout.getWidth())) + 24;
        const int h = (int) std::ceil (layout.getHeight()) + 20;

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8)  : screenPos.y + 12,
                                     w, h).constrainedWithin (parentArea);
    }

    void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (col::raised);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::border.brighter (0.2f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        layoutTooltip (text, (float) width - 24.0f).draw (g, r.reduced (12.0f, 10.0f));
    }
}
