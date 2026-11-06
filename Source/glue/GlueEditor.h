#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GlueProcessor.h"
#include "../shared/MuziWidgets.h"

// Gain-reduction needle meter (VU-style arc)
class GrNeedle : public juce::Component
{
public:
    void push (float grDb);
    void paint (juce::Graphics&) override;
    float textScale = 1.0f;
private:
    float shown = 0.0f, hold = 0.0f; int holdTicks = 0;
};

// Scrolling history: input level, output level and gain reduction
class GlueHistory : public juce::Component
{
public:
    void push (float inDb, float outDb, float grDb);
    void paint (juce::Graphics&) override;
    float textScale = 1.0f;
private:
    static constexpr int kLen = 150;                  // 5 s at 30 Hz
    std::array<float, kLen> in {}, out {}, gr {};
    int pos = 0;
};

// Static transfer curve with the live operating point
class TransferCurve : public juce::Component
{
public:
    explicit TransferCurve (MuziGlueProcessor& p) : proc (p) {}
    void setLevel (float detDb, float grDb) { det = detDb; gr = grDb; repaint(); }
    void paint (juce::Graphics&) override;
    float textScale = 1.0f;
private:
    MuziGlueProcessor& proc;
    float det = -100.0f, gr = 0.0f;
};

class MuziGlueEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MuziGlueEditor (MuziGlueProcessor&);
    ~MuziGlueEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshForSnapshot (int frames);

private:
    void timerCallback() override;
    void refreshPresetBox();
    juce::Rectangle<int> sc (float x, float y, float w, float h) const;

    MuziGlueProcessor& proc;
    muzi::ui::MuziLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    float s = 1.0f;

    GrNeedle needle;
    GlueHistory history;
    TransferCurve transfer;
    muzi::ui::Knob threshold, ratio, attack, release, knee, schpf, makeup, mix, drive;
    muzi::ui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    juce::TextButton colorBtn { "COLOR" };
    juce::ComboBox charBox, presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    muzi::ui::PowerButton bypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> colorAtt, bypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> charAtt;
    juce::Rectangle<int> panelMeter, panelHist, panelCurve, panelKnobs, panelColor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuziGlueEditor)
};
