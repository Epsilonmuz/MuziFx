#pragma once
// Widgets shared by the Muzi plugins: knob, power (bypass) button, level meter, audio tap.
#include <juce_audio_processors/juce_audio_processors.h>
#include "MuziLookAndFeel.h"

namespace muzi::ui
{
// Single-producer / single-consumer audio tap (audio thread writes, GUI reads). Never blocks.
struct AudioTap
{
    static constexpr int kSize = 1 << 15;
    juce::AbstractFifo fifo { kSize };
    std::vector<float> data = std::vector<float> (kSize, 0.0f);
    void push (const float* x, int n)
    {
        const auto s = fifo.write (n);
        if (s.blockSize1 > 0) std::copy (x, x + s.blockSize1, data.begin() + s.startIndex1);
        if (s.blockSize2 > 0) std::copy (x + s.blockSize1, x + s.blockSize1 + s.blockSize2, data.begin() + s.startIndex2);
    }
    int pull (float* dst, int maxN)
    {
        const auto s = fifo.read (juce::jmin (maxN, fifo.getNumReady()));
        if (s.blockSize1 > 0) std::copy (data.begin() + s.startIndex1, data.begin() + s.startIndex1 + s.blockSize1, dst);
        if (s.blockSize2 > 0) std::copy (data.begin() + s.startIndex2, data.begin() + s.startIndex2 + s.blockSize2, dst + s.blockSize1);
        return s.blockSize1 + s.blockSize2;
    }
};

// peak meter value that the GUI reads and resets
struct PeakAtomic
{
    std::atomic<float> v { 0.0f };
    void put (float x) { float o = v.load(); while (x > o && ! v.compare_exchange_weak (o, x)) {} }
    float take() { return v.exchange (0.0f); }
};

// Rotary knob: name above, live value below, double-click resets.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& st, const juce::String& paramId, const juce::String& n,
          const juce::String& tip, bool bipolar = false)
        : name (n.toUpperCase())
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        slider.setTooltip (tip);
        slider.getProperties().set ("bipolar", bipolar);
        addAndMakeVisible (slider);
        slider.onValueChange = [this] { repaint(); };
        setTitle (n);
        attach (st, paramId);
    }
    void attach (juce::AudioProcessorValueTreeState& st, const juce::String& id)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
        if (auto* p = st.getParameter (id))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            const auto unit = p->getLabel();
            slider.textFromValueFunction = [p, unit] (double v)
            {
                auto t = p->getText (p->convertTo0to1 ((float) v), 16);
                return unit.isEmpty() || t.endsWith (unit) ? t : t + " " + unit;
            };
        }
        repaint();
    }
    void setName2 (const juce::String& n) { name = n.toUpperCase(); repaint(); }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (nameH()); r.removeFromBottom (valH());
        const int side = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (side, side));
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (slider.isMouseOverOrDragging() ? col::text : col::quiet);
        g.setFont (font ((hero ? 12.0f : 10.5f) * textScale, true, 0.12f));
        g.drawText (name, getLocalBounds().removeFromTop (nameH()), juce::Justification::centred);
        g.setColour (isEnabled() ? col::text : col::faint);
        g.setFont (font ((hero ? 15.0f : 12.5f) * textScale, hero));
        g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromBottom (valH()), juce::Justification::centred);
    }
    float textScale = 1.0f;
    bool hero = false;
    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    int nameH() const { return juce::roundToInt ((hero ? 18.0f : 16.0f) * textScale); }
    int valH() const { return juce::roundToInt ((hero ? 22.0f : 18.0f) * textScale); }
};

class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("Bypass") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool bypassed = getToggleState();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (bypassed ? col::red.withAlpha (0.14f) : col::bg2.brighter (over ? 0.08f : 0.0f));
        g.fillEllipse (r);
        g.setColour (bypassed ? col::red.withAlpha (0.8f) : col::line);
        g.drawEllipse (r, 1.0f);
        const auto c = r.getCentre();
        const float rad = r.getWidth() * 0.24f;
        juce::Path icon;
        icon.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
        icon.startNewSubPath (c.x, c.y - rad * 1.25f);
        icon.lineTo (c.x, c.y - rad * 0.2f);
        g.setColour (bypassed ? col::red : col::accent);
        g.strokePath (icon, juce::PathStrokeType (r.getWidth() * 0.075f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& text) : label (text) {}
    void tick (float linearPeak)
    {
        const float db = juce::Decibels::gainToDecibels (linearPeak, -60.0f);
        shown = db > shown ? db : juce::jmax (db, shown - 1.2f);
        if (db >= hold) { hold = db; holdTicks = 45; }
        else if (--holdTicks <= 0) hold = juce::jmax (-60.0f, hold - 0.8f);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (16.0f * textScale);
        r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 10.0f * textScale), r.getHeight());
        g.setColour (col::faint);
        g.setFont (font (9.5f * textScale, true, 0.1f));
        g.drawText (label, lab.expanded (8.0f * textScale, 0), juce::Justification::centred);
        g.setColour (col::bg0);
        g.fillRoundedRectangle (r, 3.0f);
        auto toY = [&] (float db) { return r.getBottom() - (juce::jlimit (-60.0f, 3.0f, db) + 60.0f) / 63.0f * r.getHeight(); };
        juce::ColourGradient grad (col::accent, 0, r.getBottom(), col::red, 0, r.getY(), false);
        grad.addColour ((double) ((r.getBottom() - toY (-6.0f)) / r.getHeight()), col::amber);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r.withTop (toY (shown)), 3.0f);
        if (hold > -59.0f)
        {
            g.setColour (hold > -0.1f ? col::red : col::text.withAlpha (0.8f));
            g.fillRect (r.getX(), toY (hold) - 1.0f, r.getWidth(), 2.0f);
        }
    }
    float textScale = 1.0f;

private:
    juce::String label;
    float shown = -60.0f, hold = -60.0f;
    int holdTicks = 0;
};

// small helper: a label-less toggle bound to a bool parameter
inline std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
bindToggle (juce::AudioProcessorValueTreeState& st, const juce::String& id, juce::Button& b, const juce::String& tip)
{
    b.setClickingTogglesState (true);
    b.setTooltip (tip);
    return std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, id, b);
}

inline juce::String freqText (double f)
{
    return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
}
} // namespace muzi::ui
