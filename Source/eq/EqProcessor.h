#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MuziEqEngine.h"
#include "../shared/ColorStage.h"
#include "../shared/MuziWidgets.h"

class MuziEqProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kBands = muzi::EqParams::kBands;
    MuziEqProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Muzi EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String bandId (int band, const char* what) { return "b" + juce::String (band + 1) + "_" + what; }

    void loadFactoryPreset (int index);
    int currentPreset = 0;

    // GUI access
    muzi::EqParams readParams() const;          // current parameter values (any thread)
    const muzi::MuziEqEngine& getEngine() const { return engine; }
    double getCurrentSampleRate() const { return currentRate; }
    muzi::ui::AudioTap preTap, postTap;
    muzi::ui::PeakAtomic inPeak, outPeak;

private:
    muzi::MuziEqEngine engine;
    muzi::ColorStage color;
    double currentRate = 48000.0;
    bool colorWasOn = false;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain;
    juce::AudioBuffer<float> dryForBypass;
    juce::SmoothedValue<float> bypassMix;
    std::array<std::vector<float>, 2> dryDelay;     // keeps the bypassed signal aligned with the colour latency
    int dryPos = 0;

    struct BandPtrs { std::atomic<float> *on, *type, *freq, *gain, *q, *slope, *chan, *dyn, *thr; };
    std::array<BandPtrs, kBands> bp {};
    std::atomic<float> *propQ = nullptr, *output = nullptr, *colorOn = nullptr, *drive = nullptr, *character = nullptr, *bypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    void tap (muzi::ui::AudioTap&, muzi::ui::PeakAtomic&, const juce::AudioBuffer<float>&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuziEqProcessor)
};
