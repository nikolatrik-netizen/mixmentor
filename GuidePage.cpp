#include "GuidePage.h"
#include "Theme.h"
#include <regex>

using namespace theme;

namespace
{
    // Numbers with units (the key information): "25-30 Hz", "3-5 kHz", "2-4 dB", "10 ms", "4:1", "54-58%"
    const std::regex numberPattern (R"((\+|-)?\d+(?:[.,]\d+)?(?:\s?(?:-|to)\s?\d+(?:[.,]\d+)?)?\s?(?:kHz|Hz|ms|dB|LUFS|BPM|bpm|%|:1|s\b|x\b))");

    /** Text with every number+unit highlighted. */
    juce::AttributedString highlighted (const juce::String& text, float size, juce::Colour normal, bool bold = false)
    {
        juce::AttributedString s;
        s.setLineSpacing (4.0f);
        const std::string str = text.toStdString();
        std::sregex_iterator it (str.begin(), str.end(), numberPattern), end;
        size_t pos = 0;
        for (; it != end; ++it)
        {
            const auto m = *it;
            if ((size_t) m.position() > pos)
                s.append (juce::String::fromUTF8 (str.substr (pos, (size_t) m.position() - pos).c_str()), font (size, bold), normal);
            s.append (juce::String::fromUTF8 (m.str().c_str()), font (size, true), col::accent);
            pos = (size_t) (m.position() + m.length());
        }
        if (pos < str.size())
            s.append (juce::String::fromUTF8 (str.substr (pos).c_str()), font (size, bold), normal);
        return s;
    }

    float layoutHeight (const juce::AttributedString& s, float width)
    {
        juce::TextLayout tl;
        tl.createLayout (s, width);
        return tl.getHeight();
    }

    float draw (juce::Graphics* g, const juce::AttributedString& s, juce::Rectangle<float> area)
    {
        juce::TextLayout tl;
        tl.createLayout (s, area.getWidth());
        if (g != nullptr) tl.draw (*g, area.withHeight (tl.getHeight() + 2.0f));
        return tl.getHeight();
    }

    juce::StringArray extractNumbers (const juce::String& text, int maxCount)
    {
        juce::StringArray out;
        const std::string str = text.toStdString();
        for (std::sregex_iterator it (str.begin(), str.end(), numberPattern), end; it != end && out.size() < maxCount; ++it)
        {
            auto n = juce::String::fromUTF8 (it->str().c_str()).trim();
            if (! n.containsAnyOf ("0123456789")) continue;
            out.addIfNotAlreadyThere (n);
        }
        return out;
    }

    juce::String titleCase (juce::String s)
    {
        s = s.toLowerCase();
        return s.substring (0, 1).toUpperCase() + s.substring (1);
    }
}

//==============================================================================
GuidePage::GuidePage()
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

void GuidePage::showTips (const juce::String& processName, const juce::String& instrumentName, const juce::String& genreName,
                          const juce::String& tipText, const std::vector<mm::Trick>& tricks, const juce::StringArray& mistakes)
{
    content.theory = false;
    content.title = processName + " for " + instrumentName.toLowerCase();
    content.subtitle = genreName;
    content.tips = juce::StringArray::fromTokens (tipText.replace ("\r", ""), "\n", "");
    content.tips.trim();
    content.tips.removeEmptyStrings();
    content.tricks = tricks;
    content.mistakes = mistakes;
    juce::String all = tipText;
    for (auto& t : tricks) all << "\n" << t.text;
    content.keyNumbers = extractNumbers (all, 8);
    content.sections.clear();
    resized();
    viewport.setViewPosition (0, 0);
}

void GuidePage::showTheory (const juce::String& genreName, const juce::String& bpm, const juce::String& theoryText)
{
    content.theory = true;
    content.title = "Music theory";
    content.subtitle = genreName + (bpm.isNotEmpty() ? "  |  " + bpm : juce::String());
    content.sections.clear();
    for (auto& line : juce::StringArray::fromLines (theoryText))
    {
        const auto l = line.trim();
        if (l.isEmpty()) continue;
        const bool heading = l.length() > 2 && l == l.toUpperCase() && l.containsAnyOf ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (heading || content.sections.empty())
            content.sections.push_back ({ heading ? titleCase (l) : juce::String ("Overview"), {} });
        if (! heading)
            content.sections.back().lines.add (l);
    }
    content.keyNumbers.clear();
    content.tips.clear();
    content.tricks.clear();
    content.mistakes.clear();
    resized();
    viewport.setViewPosition (0, 0);
}

void GuidePage::resized()
{
    viewport.setBounds (getLocalBounds());
    const int w = juce::jmax (300, viewport.getMaximumVisibleWidth());
    content.setSize (w, juce::jmax (getHeight(), content.layout (w)));
    content.repaint();
}

//==============================================================================
int GuidePage::Content::layout (int width) { return (int) paintOrMeasure (nullptr, (float) width) + 24; }
void GuidePage::Content::paint (juce::Graphics& g) { paintOrMeasure (&g, (float) getWidth()); }

float GuidePage::Content::paintOrMeasure (juce::Graphics* g, float width)
{
    const float margin = 24.0f;
    float y = 20.0f;
    const float w = width - 2.0f * margin;

    // ---- header ----
    if (g != nullptr)
    {
        g->setColour (col::accent);
        g->setFont (font (13.0f, true));
        g->drawText (subtitle.toUpperCase(), juce::Rectangle<float> (margin, y, w, 18.0f), juce::Justification::centredLeft, true);
        g->setColour (col::text);
        g->setFont (fontLight (32.0f));
        g->drawText (title, juce::Rectangle<float> (margin, y + 20.0f, w, 42.0f), juce::Justification::centredLeft, true);
    }
    y += 70.0f;

    // key numbers as chips
    if (! keyNumbers.isEmpty())
    {
        if (g != nullptr)
        {
            g->setColour (col::textDim);
            g->setFont (font (12.0f, true));
            g->drawText ("KEY NUMBERS", juce::Rectangle<float> (margin, y, 120.0f, 28.0f), juce::Justification::centredLeft, false);
        }
        float x = margin + 110.0f;
        for (auto& n : keyNumbers)
        {
            const float cw = (float) juce::GlyphArrangement::getStringWidthInt (font (15.0f, true), n) + 26.0f;
            if (x + cw > margin + w) { x = margin + 110.0f; y += 36.0f; }
            if (g != nullptr)
            {
                const juce::Rectangle<float> chip (x, y, cw, 28.0f);
                g->setColour (col::accent.withAlpha (0.13f));
                g->fillRoundedRectangle (chip, 14.0f);
                g->setColour (col::accent.withAlpha (0.5f));
                g->drawRoundedRectangle (chip.reduced (0.5f), 14.0f, 1.0f);
                g->setColour (col::accent);
                g->setFont (font (15.0f, true));
                g->drawText (n, chip, juce::Justification::centred, false);
            }
            x += cw + 8.0f;
        }
        y += 44.0f;
    }

    if (! theory)
    {
        // ---- two columns: your instrument | genre tricks + mistakes ----
        const float colGap = 22.0f;
        const float leftW = w * 0.56f, rightW = w - leftW - colGap;
        const float rightX = margin + leftW + colGap;
        float yl = y, yr = y;

        auto sectionLabel = [&] (const juce::String& t, float x, float& yy, float cw)
        {
            if (g != nullptr)
            {
                g->setColour (col::textDim);
                g->setFont (font (12.5f, true));
                g->drawText (t.toUpperCase(), juce::Rectangle<float> (x, yy, cw, 20.0f), juce::Justification::centredLeft, false);
            }
            yy += 28.0f;
        };

        sectionLabel ("what to do", margin, yl, leftW);
        for (int i = 0; i < tips.size(); ++i)
        {
            auto s = highlighted (tips[i], 16.0f, col::text);
            const float pad = 18.0f, numW = 44.0f;
            const float th = layoutHeight (s, leftW - 2 * pad - numW);
            const float h = juce::jmax (56.0f, th + 2 * pad);
            const juce::Rectangle<float> card (margin, yl, leftW, h);
            if (g != nullptr)
            {
                g->setColour (col::panelLight);
                g->fillRoundedRectangle (card, 10.0f);
                g->setColour (col::border.withAlpha (0.6f));
                g->drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);
                auto badge = juce::Rectangle<float> (card.getX() + pad, card.getY() + pad - 2.0f, 28.0f, 28.0f);
                g->setColour (col::accent.withAlpha (0.18f));
                g->fillEllipse (badge);
                g->setColour (col::accent);
                g->setFont (font (14.0f, true));
                g->drawText (juce::String (i + 1), badge, juce::Justification::centred, false);
                draw (g, s, card.reduced (pad).withTrimmedLeft (numW));
            }
            yl += h + 12.0f;
        }

        if (! tricks.empty())
        {
            sectionLabel ("pro tricks for " + subtitle, rightX, yr, rightW);
            for (auto& t : tricks)
            {
                auto body = highlighted (t.text, 14.5f, col::textDim);
                const float pad = 16.0f;
                const float th = layoutHeight (body, rightW - 2 * pad - 6.0f);
                const float h = pad + 22.0f + 6.0f + th + pad;
                const juce::Rectangle<float> card (rightX, yr, rightW, h);
                if (g != nullptr)
                {
                    g->setColour (col::accent.withAlpha (0.07f));
                    g->fillRoundedRectangle (card, 10.0f);
                    g->setColour (col::accent.withAlpha (0.35f));
                    g->drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);
                    g->setColour (col::accent);
                    g->fillRoundedRectangle (juce::Rectangle<float> (card.getX() + 8.0f, card.getY() + 14.0f, 3.0f, h - 28.0f), 1.5f);
                    auto in = card.reduced (pad).withTrimmedLeft (6.0f);
                    g->setColour (col::text);
                    g->setFont (font (16.0f, true));
                    g->drawText (t.title, in.removeFromTop (22.0f), juce::Justification::centredLeft, true);
                    in.removeFromTop (6.0f);
                    draw (g, body, in);
                }
                yr += h + 12.0f;
            }
        }

        if (! mistakes.isEmpty())
        {
            yr += 6.0f;
            sectionLabel ("common mistakes", rightX, yr, rightW);
            const float pad = 16.0f;
            float inner = 0.0f;
            std::vector<juce::AttributedString> rows;
            for (auto& m : mistakes)
            {
                rows.push_back (highlighted (m, 14.5f, col::textDim));
                inner += layoutHeight (rows.back(), rightW - 2 * pad - 22.0f) + 10.0f;
            }
            const juce::Rectangle<float> card (rightX, yr, rightW, inner + 2 * pad - 10.0f);
            if (g != nullptr)
            {
                g->setColour (col::bad.withAlpha (0.07f));
                g->fillRoundedRectangle (card, 10.0f);
                g->setColour (col::bad.withAlpha (0.3f));
                g->drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);
                float yy = card.getY() + pad;
                for (auto& r : rows)
                {
                    g->setColour (col::bad);
                    g->fillEllipse (card.getX() + pad, yy + 7.0f, 7.0f, 7.0f);
                    yy += draw (g, r, juce::Rectangle<float> (card.getX() + pad + 22.0f, yy, rightW - 2 * pad - 22.0f, 400.0f)) + 10.0f;
                }
            }
            yr += card.getHeight() + 12.0f;
        }
        return juce::jmax (yl, yr);
    }

    // ---- theory: section cards in two columns ----
    const float colGap = 18.0f;
    const float colW = (w - colGap) * 0.5f;
    float colY[2] = { y, y };
    for (auto& sec : sections)
    {
        const int c = colY[0] <= colY[1] ? 0 : 1;
        const float x = margin + c * (colW + colGap);
        const float pad = 18.0f;
        std::vector<juce::AttributedString> rows;
        float inner = 0.0f;
        for (auto& line : sec.lines)
        {
            const bool bullet = line.startsWith (juce::String::fromUTF8 ("\xe2\x80\xa2")) || line.startsWith ("-");
            auto text = bullet ? line.substring (1).trim() : line;
            rows.push_back (highlighted (text, 15.0f, bullet ? col::text : col::textDim));
            inner += layoutHeight (rows.back(), colW - 2 * pad - (bullet ? 18.0f : 0.0f)) + 9.0f;
        }
        const float h = pad + 24.0f + 8.0f + inner + pad - 9.0f;
        const juce::Rectangle<float> card (x, colY[c], colW, h);
        if (g != nullptr)
        {
            g->setColour (col::panelLight);
            g->fillRoundedRectangle (card, 10.0f);
            g->setColour (col::border.withAlpha (0.6f));
            g->drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);
            auto in = card.reduced (pad);
            g->setColour (col::accent);
            g->setFont (font (17.0f, true));
            g->drawText (sec.heading, in.removeFromTop (24.0f), juce::Justification::centredLeft, true);
            in.removeFromTop (8.0f);
            for (size_t i = 0; i < rows.size(); ++i)
            {
                const auto& line = sec.lines[(int) i];
                const bool bullet = line.startsWith (juce::String::fromUTF8 ("\xe2\x80\xa2")) || line.startsWith ("-");
                auto area = in;
                if (bullet)
                {
                    g->setColour (col::accent.withAlpha (0.8f));
                    g->fillEllipse (area.getX() + 2.0f, area.getY() + 8.0f, 6.0f, 6.0f);
                    area.removeFromLeft (18.0f);
                }
                in.removeFromTop (draw (g, rows[i], area) + 9.0f);
            }
        }
        colY[c] += h + 14.0f;
    }
    return juce::jmax (colY[0], colY[1]);
}
