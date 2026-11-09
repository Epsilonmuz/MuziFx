#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Shared/epsilon_ui/EpsilonPalette.h"
#include "../../Shared/epsilon_ui/EpsilonHeader.h"

namespace muzi::ui
{
// ---- palette (one place to re-skin the whole plugin)
namespace col
{
    // Epsilon Sounds design system v1 - Muzi (premium) tier. Champagne = live / active only.
    inline const epsilon_ui::Palette brand = epsilon_ui::palette (epsilon_ui::Tier::Muzi);
    const juce::Colour bg0     = brand.bg;          // window
    const juce::Colour bg1     = brand.surface;     // panels
    const juce::Colour bg2     = brand.raised;      // raised
    const juce::Colour line    = brand.line;
    const juce::Colour text    = brand.ink;
    const juce::Colour quiet   = brand.inkMuted;
    const juce::Colour faint   = epsilon_ui::faint (brand);
    const juce::Colour accent  = brand.live;        // the cut
    const juce::Colour amber   = brand.meterHot;    // broad layer, range handles
    const juce::Colour red     = brand.meterClip;
    const juce::Colour trim    = brand.trim;        // brushed gold: wordmark and hallmark only
}

inline juce::Font font (float size, bool bold = false, float kerning = 0.0f)
{
    auto f = juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}

class MuziLookAndFeel : public juce::LookAndFeel_V4
{
public:
    float scale = 1.0f;

    MuziLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, col::bg2);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::quiet);
        setColour (juce::PopupMenu::backgroundColourId, col::bg2);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.18f));
        setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::TooltipWindow::backgroundColourId, col::bg2);
        setColour (juce::TooltipWindow::textColourId, col::text);
        setColour (juce::TooltipWindow::outlineColourId, col::line);
        setColour (juce::TextButton::buttonColourId, col::bg2);
        setColour (juce::TextButton::buttonOnColourId, col::accent.withAlpha (0.18f));
        setColour (juce::TextButton::textColourOffId, col::quiet);
        setColour (juce::TextButton::textColourOnId, col::accent);
    }

    // ---- knobs: dark body, value arc with soft glow, pointer
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f * scale);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const float trackW = juce::jmax (2.0f, r * 0.075f);
        const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
        const bool hot = s.isMouseOverOrDragging();

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - trackW, r - trackW, 0.0f, a0, a1, true);
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        if (std::abs (ang - from) > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, r - trackW, r - trackW, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (col::accent.withAlpha (hot ? 0.28f : 0.16f));
            g.strokePath (val, juce::PathStrokeType (trackW * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col::accent);
            g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float br = r * 0.70f;
        juce::ColourGradient body (col::bg2.brighter (0.18f), c.x, c.y - br, col::bg1.darker (0.2f), c.x, c.y + br, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (c.x - br + 1, c.y - br + 1, br * 2 - 2, br * 2 - 2, 1.0f);

        const auto p0 = c.getPointOnCircumference (br * 0.30f, ang);
        const auto p1 = c.getPointOnCircumference (br * 0.82f, ang);
        g.setColour (col::text);
        g.drawLine ({ p0, p1 }, juce::jmax (2.0f, r * 0.06f));
    }

    // ---- pill toggles (Delta) and segmented buttons (Mode)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float rad = r.getHeight() * 0.5f;
        const int edges = (int) b.getProperties().getWithDefault ("edges", 0);   // 1 = left end, 2 = right end, 3 = middle
        const bool left = edges == 0 || edges == 1, right = edges == 0 || edges == 2;
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad, rad, left, right, left, right);
        g.setColour (on ? col::accent.withAlpha (0.16f) : col::bg2.brighter (over ? 0.08f : 0.0f).darker (down ? 0.1f : 0.0f));
        g.fillPath (p);
        g.setColour (on ? col::accent.withAlpha (0.7f) : col::line);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (font (11.0f * scale, true, 0.08f));
        g.setColour (b.getToggleState() ? col::accent : col::quiet);
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::bg2);
        g.fillRoundedRectangle (r, 6.0f * scale);
        g.setColour (cb.isMouseOver (true) ? col::faint : col::line);
        g.drawRoundedRectangle (r, 6.0f * scale, 1.0f);
        const float ax = (float) w - 16.0f * scale, ay = (float) h * 0.5f;
        juce::Path arrow;
        arrow.addTriangle (ax - 4 * scale, ay - 2 * scale, ax + 4 * scale, ay - 2 * scale, ax, ay + 3 * scale);
        g.setColour (col::quiet);
        g.fillPath (arrow);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f * scale, true); }
    void positionComboBoxText (juce::ComboBox& cb, juce::Label& l) override
    {
        l.setBounds (cb.getLocalBounds().withTrimmedRight ((int) (24 * scale)).withTrimmedLeft ((int) (6 * scale)));
        l.setFont (getComboBoxFont (cb));
        l.setJustificationType (juce::Justification::centred);
    }
    juce::Font getPopupMenuFont() override { return font (14.0f * scale); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (11.0f * scale, true, 0.08f); }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const auto f = font (13.0f);
        const int w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (f, tip) + 20);
        juce::AttributedString s; s.append (tip, f);
        juce::TextLayout tl; tl.createLayout (s, (float) w - 16.0f);
        const int h = (int) tl.getHeight() + 14;
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                     pos.y > parent.getCentreY() ? pos.y - (h + 12) : pos.y + 24, w, h).constrainedWithin (parent);
    }
    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.setColour (col::bg2);
        g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 6.0f, 1.0f);
        juce::AttributedString s; s.append (text, font (13.0f), col::text);
        s.setJustification (juce::Justification::centredLeft);
        juce::TextLayout tl; tl.createLayout (s, (float) w - 16.0f);
        tl.draw (g, { 8.0f, 7.0f, (float) w - 16.0f, (float) h - 14.0f });
    }
};
} // namespace muzi::ui
