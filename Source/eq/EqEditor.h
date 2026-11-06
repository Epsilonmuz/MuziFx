#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "EqProcessor.h"
#include "../shared/MuziWidgets.h"

namespace muzi::ui
{
const juce::Colour bandColours[8] = { juce::Colour (0xff6ea8ff), juce::Colour (0xff38d3c5), juce::Colour (0xff8be07a), juce::Colour (0xffe4d65a),
                                      juce::Colour (0xfff2a24b), juce::Colour (0xffef6b73), juce::Colour (0xffd17cf0), juce::Colour (0xff9b8cff) };
}

// ---------------------------------------------------------------- curve display + analyser
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (MuziEqProcessor&);
    void update();                                    // timer: spectra + curves
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    std::function<void (int)> onSelect;
    int selected = 4;
    float range = 12.0f;                              // +-dB shown
    int analyser = 2;                                 // 0 off, 1 pre, 2 pre+post
    float textScale = 1.0f;

private:
    static constexpr int kOrder = 12, kN = 1 << kOrder;
    MuziEqProcessor& proc;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kN, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ringIn = std::vector<float> (kN, 0.0f), ringOut = std::vector<float> (kN, 0.0f);
    int ringPos = 0;
    std::vector<float> work = std::vector<float> (kN * 2, 0.0f), scratch = std::vector<float> (muzi::ui::AudioTap::kSize, 0.0f);
    std::vector<float> specIn = std::vector<float> (kN / 2 + 1, -120.0f), specOut = std::vector<float> (kN / 2 + 1, -120.0f);
    std::vector<float> colIn, colOut, colMid, colSide;
    std::array<std::vector<float>, 8> colBand, colBandStatic;
    std::vector<double> colFreq;
    muzi::EqParams params;
    std::array<float, 8> dynGain {};
    bool anySide = false, anyMidOnly = false;
    int hover = -1, dragging = -1, hoverX = -1, hoverY = -1;
    juce::Point<float> dragStart; float dragF0 = 0, dragG0 = 0;

    juce::Rectangle<float> plot() const;
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float ySpec (float db) const;
    juce::Point<float> nodePos (int band) const;
    int nodeAt (juce::Point<float>) const;
    void computeSpectrum (std::vector<float>& ring, std::vector<float>& spec);
    void toColumns (const std::vector<float>& spec, std::vector<float>& cols);
    juce::RangedAudioParameter* prm (int band, const char* what) const;
    void setPrm (int band, const char* what, float value);
    void showMenu (int band);
    juce::Path curvePath (const std::vector<float>& cols, bool closeToZero) const;
};

// ---------------------------------------------------------------- band selector chip
class BandChip : public juce::Button
{
public:
    BandChip() : juce::Button ("band") {}
    int index = 0;
    bool bandOn = false, dynamic = false;
    juce::String typeName;
    float textScale = 1.0f;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// ---------------------------------------------------------------- editor
class MuziEqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MuziEqEditor (MuziEqProcessor&);
    ~MuziEqEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshForSnapshot();
    void selectBand (int b);

private:
    void timerCallback() override;
    void refreshPresetBox();
    void refreshBandControls();
    juce::Rectangle<int> sc (float x, float y, float w, float h) const;

    MuziEqProcessor& proc;
    muzi::ui::MuziLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    float s = 1.0f;
    int band = 4;

    EqDisplay display;
    std::array<BandChip, 8> chips;
    muzi::ui::Knob freq, gain, q, thr, drive, output;
    juce::TextButton onBtn { "ON" }, dynBtn { "DYNAMIC" }, colorBtn { "COLOR" }, propQBtn { "PROP Q" }, rangeBtn { "12 dB" }, anaBtn { "ANALYSER" };
    juce::TextButton chanBtn[3] { juce::TextButton ("STEREO"), juce::TextButton ("MID"), juce::TextButton ("SIDE") };
    juce::ComboBox typeBox, slopeBox, charBox, presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    muzi::ui::PowerButton bypassBtn;
    muzi::ui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt, dynAtt, colorAtt, propQAtt, bypassAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt, charAtt;
    std::unique_ptr<juce::ParameterAttachment> chanAtt;

    juce::Rectangle<int> header, stripBand, stripOut;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuziEqEditor)
};
