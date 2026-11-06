#include "GlueProcessor.h"
#include "GlueEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

const float MuziGlueProcessor::kRatios[5] = { 1.5f, 2.0f, 3.0f, 4.0f, 10.0f };
const float MuziGlueProcessor::kAttacks[6] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
const float MuziGlueProcessor::kReleases[4] = { 100.0f, 300.0f, 600.0f, 1200.0f };

namespace
{
struct Preset { const char* name; float thr; int ratio, attack, release; float knee, hpf, makeup, mix; };
// Thresholds are starting points: set Threshold for the amount of gain reduction you want.
const Preset kPresets[] = {
    { "Default",                     -10, 1, 4, 4,  6,  20, 0,  100 },
    { "Mix Bus - Classic Glue",      -14, 1, 5, 4,  3,  60, 2,  100 },
    { "Mix Bus - Gentle",            -12, 0, 5, 4,  9,  90, 1,  100 },
    { "Master - 1 dB of Glue",        -8, 0, 5, 4,  6,  90, 0.5f, 100 },
    { "Drum Bus - Punch",            -18, 3, 5, 0,  3,  20, 3,  100 },
    { "Drum Bus - Parallel Smash",   -30, 4, 0, 0,  0,  20, 10, 35 },
    { "Vocal Bus - Smooth",          -18, 2, 3, 1,  6, 120, 3,  100 },
    { "Pump",                        -20, 4, 0, 1,  0,  20, 4,  100 },
};
}

int MuziGlueProcessor::getNumPrograms() { return (int) std::size (kPresets); }
const juce::String MuziGlueProcessor::getProgramName (int i) { return kPresets[juce::jlimit (0, getNumPrograms() - 1, i)].name; }

void MuziGlueProcessor::loadFactoryPreset (int index)
{
    currentPreset = juce::jlimit (0, getNumPrograms() - 1, index);
    const auto& p = kPresets[currentPreset];
    auto set = [this] (const char* id, float v) { if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set ("threshold", p.thr); set ("ratio", (float) p.ratio); set ("attack", (float) p.attack); set ("release", (float) p.release);
    set ("knee", p.knee); set ("schpf", p.hpf); set ("makeup", p.makeup); set ("mix", p.mix);
    apvts.state.setProperty ("preset", currentPreset, nullptr);
}

APVTS::ParameterLayout MuziGlueProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto db = AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1); });
    auto pct = AudioParameterFloatAttributes().withLabel ("%");
    auto hz = AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? String ("Off") : String (roundToInt (v)) + " Hz"; })
                .withValueFromStringFunction ([] (const String& t) { return t.containsIgnoreCase ("off") ? 20.0f : t.getFloatValue(); });
    auto logRange = [] (float lo, float hi)
    {
        return NormalisableRange<float> (lo, hi,
            [] (float a, float b, float t) { return a * std::pow (b / a, t); },
            [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
            [] (float a, float b, float v) { return jlimit (a, b, v); });
    };
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold", NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -10.0f, db));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "ratio", 1 }, "Ratio", StringArray { "1.5:1", "2:1", "3:1", "4:1", "10:1" }, 1));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "attack", 1 }, "Attack", StringArray { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 4));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "release", 1 }, "Release", StringArray { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 4));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "knee", 1 }, "Knee", NormalisableRange<float> (0.0f, 12.0f, 0.1f), 6.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "schpf", 1 }, "SC HPF", logRange (20.0f, 500.0f), 20.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "makeup", 1 }, "Makeup", NormalisableRange<float> (-6.0f, 18.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "color", 1 }, "Color", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f, db));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "character", 1 }, "Character", StringArray { "Tape", "Tube" }, 0));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

MuziGlueProcessor::MuziGlueProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MuziGlue", createLayout())
{
    threshold = apvts.getRawParameterValue ("threshold");
    ratio = apvts.getRawParameterValue ("ratio");
    knee = apvts.getRawParameterValue ("knee");
    attack = apvts.getRawParameterValue ("attack");
    release = apvts.getRawParameterValue ("release");
    schpf = apvts.getRawParameterValue ("schpf");
    makeup = apvts.getRawParameterValue ("makeup");
    mix = apvts.getRawParameterValue ("mix");
    colorOn = apvts.getRawParameterValue ("color");
    drive = apvts.getRawParameterValue ("drive");
    character = apvts.getRawParameterValue ("character");
    bypass = apvts.getRawParameterValue ("bypass");
    bypassParam = apvts.getParameter ("bypass");
}

bool MuziGlueProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

muzi::GlueParams MuziGlueProcessor::readParams() const
{
    muzi::GlueParams p;
    p.thresholdDb = threshold->load();
    p.ratio = kRatios[juce::jlimit (0, 4, (int) ratio->load())];
    p.kneeDb = knee->load();
    p.attackMs = kAttacks[juce::jlimit (0, 5, (int) attack->load())];
    const int r = (int) release->load();
    p.autoRelease = r >= 4;
    p.releaseMs = p.autoRelease ? 300.0f : kReleases[juce::jlimit (0, 3, r)];
    p.scHpfHz = schpf->load();
    p.makeupDb = makeup->load();
    p.mix = mix->load() * 0.01f;
    return p;
}

void MuziGlueProcessor::prepareToPlay (double sr, int block)
{
    engine.prepare (sr);
    color.prepare (sr, block);
    bypassMix.reset (sr, 0.02);
    bypassMix.setCurrentAndTargetValue (bypass->load() > 0.5f ? 1.0f : 0.0f);
    dry.setSize (2, block * 2 + 64);
    for (auto& d : dryDelay) d.assign (256, 0.0f);
    dryPos = 0;
    colorWasOn = colorOn->load() > 0.5f;
    setLatencySamples (colorWasOn ? color.latency() : 0);
}

void MuziGlueProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    if (n == 0) return;
    inPeak.put (buf.getMagnitude (0, n));

    const bool colorNow = colorOn->load() > 0.5f;
    if (colorNow != colorWasOn) { colorWasOn = colorNow; color.reset(); setLatencySamples (colorNow ? color.latency() : 0); }

    bypassMix.setTargetValue (bypass->load() > 0.5f ? 1.0f : 0.0f);
    const bool fading = bypassMix.isSmoothing() || bypassMix.getCurrentValue() > 0.0f;
    if (fading && dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    {
        const int lat = colorNow ? color.latency() : 0, size = (int) dryDelay[0].size();
        int pos = dryPos;
        for (int s = 0; s < n; ++s)
        {
            for (int c = 0; c < juce::jmin (2, nch); ++c)
            {
                auto& d = dryDelay[(size_t) c];
                d[(size_t) pos] = buf.getSample (c, s);
                if (fading) dry.setSample (c, s, d[(size_t) ((pos - lat + size) % size)]);
            }
            pos = (pos + 1) % size;
        }
        dryPos = pos;
    }

    float* L = buf.getWritePointer (0);
    float* R = nch > 1 ? buf.getWritePointer (1) : buf.getWritePointer (0);
    engine.process (L, R, n, readParams());
    grPeak.put (engine.meterGr.load());
    detPeak.put (juce::Decibels::decibelsToGain (engine.meterInDb.load(), -200.0f));

    if (colorNow) color.process (buf, drive->load(), (int) character->load());

    if (fading)
        for (int s = 0; s < n; ++s)
        {
            const float m = bypassMix.getNextValue();
            for (int c = 0; c < nch; ++c) { float* x = buf.getWritePointer (c); x[s] += m * (dry.getSample (c, s) - x[s]); }
        }
    outPeak.put (buf.getMagnitude (0, n));
}

void MuziGlueProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}
void MuziGlueProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = (int) apvts.state.getProperty ("preset", 0);
        }
}

juce::AudioProcessorEditor* MuziGlueProcessor::createEditor() { return new MuziGlueEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MuziGlueProcessor(); }
