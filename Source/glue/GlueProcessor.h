#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MuziGlueEngine.h"
#include "../shared/ColorStage.h"
#include "../shared/MuziWidgets.h"

class MuziGlueProcessor : public juce::AudioProcessor
{
public:
    MuziGlueProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Muzi Glue"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadFactoryPreset (int index);
    int currentPreset = 0;

    muzi::GlueParams readParams() const;
    static const float kRatios[5], kAttacks[6], kReleases[4];

    // GUI meters (max since last read)
    muzi::ui::PeakAtomic inPeak, outPeak, grPeak, detPeak;    // detPeak: detector level, linear

private:
    muzi::MuziGlueEngine engine;
    muzi::ColorStage color;
    bool colorWasOn = false;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> bypassMix;
    std::array<std::vector<float>, 2> dryDelay;
    int dryPos = 0;

    std::atomic<float> *threshold = nullptr, *ratio = nullptr, *knee = nullptr, *attack = nullptr, *release = nullptr,
                       *schpf = nullptr, *makeup = nullptr, *mix = nullptr, *colorOn = nullptr, *drive = nullptr,
                       *character = nullptr, *bypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuziGlueProcessor)
};
