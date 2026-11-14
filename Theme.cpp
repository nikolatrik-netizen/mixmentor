#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include "BinaryData.h"

namespace theme
{
    namespace
    {
        juce::Typeface::Ptr loadFace (const void* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }

        juce::Typeface::Ptr lightFace()    { static auto f = loadFace (BinaryData::InterLight_ttf,    BinaryData::InterLight_ttfSize);    return f; }
        juce::Typeface::Ptr regularFace()  { static auto f = loadFace (BinaryData::InterRegular_ttf,  BinaryData::InterRegular_ttfSize);  return f; }
        juce::Typeface::Ptr semiboldFace() { static auto f = loadFace (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize); return f; }
    }

    juce::FontOptions font (float height, bool semibold)
    {
        return juce::FontOptions (semibold ? semiboldFace() : regularFace()).withHeight (height);
    }

    juce::FontOptions fontLight (float height)
    {
        return juce::FontOptions (lightFace()).withHeight (height);
    }

    void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> area, bool withNote)
    {
        // Designed on a 120 x 120 grid, scaled to the area.
        const float sz = juce::jmin (area.getWidth(), area.getHeight());
        const auto box = area.withSizeKeepingCentre (sz, sz);
        const float k = sz / 120.0f;
        auto pt = [&] (float x, float y) { return juce::Point<float> (box.getX() + x * k, box.getY() + y * k); };
        const bool small = sz < 40.0f;   // thicker strokes at small sizes so it stays readable

        // Tile
        g.setColour (juce::Colour (0xff17191d));
        g.fillRoundedRectangle (box.reduced (2.0f * k), 30.0f * k);
        g.setColour (juce::Colour (0xff2c3036));
        g.drawRoundedRectangle (box.reduced (2.0f * k), 30.0f * k, juce::jmax (1.0f, 2.0f * k));

        // The M: two EQ bells
        juce::Path m;
        m.startNewSubPath (pt (22, 88));
        m.cubicTo (pt (33, 88), pt (35, 33), pt (44, 33));
        m.cubicTo (pt (53, 33), pt (54, 64), pt (60, 64));
        m.cubicTo (pt (66, 64), pt (67, 33), pt (76, 33));
        m.cubicTo (pt (85, 33), pt (87, 88), pt (98, 88));
        g.setColour (col::text);
        g.strokePath (m, juce::PathStrokeType ((small ? 11.0f : 8.0f) * k, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

        if (! withNote)
        {
            // Amber band node on the first peak
            const float r = (small ? 12.0f : 9.0f) * k;
            g.setColour (juce::Colour (0xff17191d));
            g.fillEllipse (juce::Rectangle<float> (2.0f * (r + 4.0f * k), 2.0f * (r + 4.0f * k)).withCentre (pt (44, 33)));
            g.setColour (col::accent);
            g.fillEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (pt (44, 33)));
        }
        else
        {
            // Amber eighth note sitting on the first peak
            juce::Path head;
            head.addEllipse (-9.0f, -7.0f, 18.0f, 14.0f);
            head.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (-20.0f))
                                     .scaled (k).translated (pt (40, 36)));
            g.setColour (juce::Colour (0xff17191d));
            g.strokePath (head, juce::PathStrokeType (3.0f * k));
            g.setColour (col::accent);
            g.fillPath (head);

            juce::Path stem;
            stem.startNewSubPath (pt (48, 34));
            stem.lineTo (pt (48, 12));
            stem.quadraticTo (pt (55, 15), pt (60, 24));
            g.strokePath (stem, juce::PathStrokeType (4.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill, float radius)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (area, radius);
        g.setColour (juce::Colours::white.withAlpha (0.035f));                // faint top highlight
        g.drawHorizontalLine ((int) area.getY() + 1, area.getX() + radius, area.getRight() - radius);
        g.setColour (col::border.withAlpha (0.8f));
        g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);
    }

    void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                    juce::Colour colour, juce::Justification just)
    {
        g.setColour (colour);
        g.setFont (font (12.5f));
        g.drawText (text.toLowerCase(), area, just, false);
    }

    void drawValueBox (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour textColour)
    {
        g.setColour (col::box);
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (col::border.withAlpha (0.6f));
        g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (textColour);
        g.setFont (font (13.5f));
        g.drawText (text, area, juce::Justification::centred, false);
    }

    //==============================================================================
    LookAndFeel::LookAndFeel()
    {
        using namespace juce;

        setColourScheme ({ col::window, col::panel, col::box, col::border, col::text,
                           col::accent, Colours::white, col::box, col::text });

        setColour (ResizableWindow::backgroundColourId, col::window);

        setColour (ComboBox::backgroundColourId, col::box);
        setColour (ComboBox::outlineColourId, col::border);
        setColour (ComboBox::textColourId, col::text);
        setColour (ComboBox::arrowColourId, col::textDim);
        setColour (ComboBox::focusedOutlineColourId, col::accent);

        setColour (PopupMenu::backgroundColourId, col::boxHover);
        setColour (PopupMenu::textColourId, col::text);
        setColour (PopupMenu::highlightedBackgroundColourId, col::boxHover);
        setColour (PopupMenu::highlightedTextColourId, col::text);

        setColour (TextButton::buttonColourId, col::box);
        setColour (TextButton::textColourOffId, col::text);
        setColour (TextButton::textColourOnId, col::text);

        setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        setColour (TextEditor::textColourId, col::text);
        setColour (TextEditor::highlightColourId, col::accent.withAlpha (0.2f));

        setColour (Label::textColourId, col::text);
        setColour (ScrollBar::thumbColourId, col::textFaint);

        setColour (TooltipWindow::backgroundColourId, col::boxHover);
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

        g.setColour (hover ? col::boxHover : col::box);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::border.withAlpha (hover ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        const float cx = (float) width - 15.0f, cy = (float) height * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (col::textDim);
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return font (14.0f);
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (10, 0, box.getWidth() - 32, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    //==============================================================================
    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        g.fillAll (col::boxHover);
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
            g.setColour (col::border.withAlpha (0.6f));
            g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        auto r = area.toFloat().reduced (4.0f, 1.0f);
        if (isHighlighted && isActive)
        {
            g.setColour (col::accent.withAlpha (0.14f));
            g.fillRoundedRectangle (r, 3.0f);
        }

        if (isTicked)
        {
            g.setColour (col::accent);
            g.fillEllipse (r.getX() + 10.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
        }

        g.setColour (isActive ? col::text : col::textFaint);
        g.setFont (font (14.0f, isTicked));
        g.drawText (text, r.withTrimmedLeft (26.0f), juce::Justification::centredLeft, true);
    }

    void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int,
                                                 int& idealWidth, int& idealHeight)
    {
        if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
        idealHeight = 30;
        idealWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (font (14.0f, true)), text) + 48;
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return font (14.0f);
    }

    //==============================================================================
    // Buttons with the "flat" property are text-only (used on the dark top bar).
    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool isHighlighted, bool isDown)
    {
        if (b.getProperties()["flat"])
        {
            if (isHighlighted || isDown)
            {
                g.setColour (juce::Colours::white.withAlpha (isDown ? 0.08f : 0.04f));
                g.fillRoundedRectangle (b.getLocalBounds().toFloat(), 3.0f);
            }
            return;
        }

        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (isDown ? col::box : isHighlighted ? col::boxHover : col::box);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (isHighlighted ? col::accent.withAlpha (0.6f) : col::border);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
    {
        const bool flat = b.getProperties()["flat"];
        g.setColour (flat ? (b.isMouseOver() ? col::accent : col::textDim) : col::text);
        g.setFont (flat ? font (14.0f) : font (13.0f));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int)
    {
        return font (13.0f);
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto c = bounds.getCentre();
        const bool hover = slider.isMouseOverOrDragging();

        // Body: soft shadow, gradient and a thin rim
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.translated (0.0f, 3.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f47), c.x, c.y - r,
                                                 juce::Colour (0xff1d2025), c.x, c.y + r, false));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (hover ? col::accent.withAlpha (0.5f) : juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.5f), 1.0f);

        // Pointer
        const float a = startAngle + pos * (endAngle - startAngle);
        g.setColour (col::accent);
        g.drawLine ({ c.getPointOnCircumference (r * 0.45f, a), c.getPointOnCircumference (r * 0.85f, a) }, 3.0f);
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
        // First line is the title; the rest is body text.
        const auto title = text.upToFirstOccurrenceOf ("\n", false, false);
        const auto body  = text.fromFirstOccurrenceOf ("\n", false, false).trim();

        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        s.append (title, font (13.5f, true), col::text);
        if (body.isNotEmpty())
            s.append ("\n\n" + body, font (13.0f), col::textDim);
        s.setLineSpacing (1.5f);

        juce::TextLayout layout;
        layout.createLayout (s, maxWidth);
        return layout;
    }

    juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
    {
        constexpr float maxW = 300.0f;
        const auto layout = layoutTooltip (tipText, maxW);
        const int w = (int) std::ceil (juce::jmin (maxW, layout.getWidth())) + 28;
        const int h = (int) std::ceil (layout.getHeight()) + 24;

        // Always open to the right of the mouse, vertically centred on it.
        juce::Rectangle<int> r (screenPos.x + 18, screenPos.y - h / 2, w, h);

        // Only flip to the left if there's genuinely no room on the right.
        if (r.getRight() > parentArea.getRight())
            r.setX (screenPos.x - 18 - w);

        return r.constrainedWithin (parentArea);
    }

    void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (col::boxHover);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (col::accent.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

        layoutTooltip (text, (float) width - 28.0f).draw (g, r.reduced (14.0f, 12.0f));
    }
}
