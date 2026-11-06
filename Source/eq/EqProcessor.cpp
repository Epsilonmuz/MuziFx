#include "EqProcessor.h"
#include "EqEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace muzi;

namespace
{
const juce::StringArray kTypeNames { "Bell", "Low Shelf", "High Shelf", "Tilt", "Low Cut", "High Cut", "Notch" };
const juce::StringArray kSlopeNames { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
const juce::StringArray kChanNames { "Stereo", "Mid", "Side" };

struct BandDefault { int type; float freq, q; };
const BandDefault kDefaults[8] = { { LowCut, 30, 0.71f }, { LowShelf, 90, 0.71f }, { Bell, 220, 1.0f }, { Bell, 600, 1.0f },
                                   { Bell, 1600, 1.0f }, { Bell, 4500, 1.0f }, { HighShelf, 10000, 0.71f }, { HighCut, 18000, 0.71f } };

// --- factory presets: only the bands listed are switched on, the rest return to their defaults
struct PB { int band, type; float freq, gain, q; int slope = 1, chan = Stereo; bool dyn = false; float thr = -20; };
struct Preset { const char* name; std::vector<PB> bands; };
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p {
        { "Flat", {} },
        { "Master - Tame Forward Mids", { { 0, LowCut, 22, 0, 0.71f, 1 }, { 4, Bell, 1500, -2.0f, 0.7f } } },
        { "Master - Dynamic Mid Control", { { 0, LowCut, 22, 0, 0.71f, 1 }, { 4, Bell, 1600, -3.0f, 0.8f, 1, Stereo, true, -24 } } },
        { "Master - Air & Weight", { { 0, LowCut, 20, 0, 0.71f, 1 }, { 1, LowShelf, 60, 1.0f, 0.71f }, { 6, HighShelf, 12000, 1.5f, 0.71f } } },
        { "Mix Bus - Smile", { { 1, LowShelf, 100, 1.5f, 0.71f }, { 3, Bell, 400, -1.0f, 0.8f }, { 6, HighShelf, 10000, 2.0f, 0.71f } } },
        { "Vocal - Clarity", { { 0, LowCut, 90, 0, 0.71f, 2 }, { 2, Bell, 300, -2.0f, 1.2f }, { 4, Bell, 3000, 2.0f, 0.9f },
                               { 5, Bell, 6500, -4.0f, 4.0f, 1, Stereo, true, -30 }, { 6, HighShelf, 12000, 2.5f, 0.71f } } },
        { "808 - Clean Up", { { 0, LowCut, 25, 0, 0.71f, 3 }, { 2, Bell, 250, -3.0f, 1.4f }, { 7, HighCut, 8000, 0, 0.71f, 1 } } },
        { "Kick - Punch", { { 0, LowCut, 30, 0, 0.71f, 2 }, { 1, Bell, 60, 3.0f, 1.2f }, { 3, Bell, 350, -4.0f, 1.5f }, { 5, Bell, 4000, 3.0f, 1.2f } } },
        { "Side - Mono Bass, Wider Top", { { 0, LowCut, 120, 0, 0.71f, 1, Side }, { 6, HighShelf, 8000, 2.5f, 0.71f, 1, Side } } },
        { "Tilt - Brighter", { { 3, Tilt, 1000, 3.0f, 0.71f } } },
    };
    return p;
}
}

int MuziEqProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String MuziEqProcessor::getProgramName (int i) { return presets()[(size_t) juce::jlimit (0, getNumPrograms() - 1, i)].name; }

void MuziEqProcessor::loadFactoryPreset (int index)
{
    currentPreset = juce::jlimit (0, getNumPrograms() - 1, index);
    auto set = [this] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    for (int b = 0; b < kBands; ++b)
    {
        const auto& d = kDefaults[b];
        set (bandId (b, "on"), 0); set (bandId (b, "type"), (float) d.type); set (bandId (b, "freq"), d.freq);
        set (bandId (b, "gain"), 0); set (bandId (b, "q"), d.q); set (bandId (b, "slope"), 1);
        set (bandId (b, "chan"), 0); set (bandId (b, "dyn"), 0); set (bandId (b, "thr"), -20);
    }
    for (const auto& pb : presets()[(size_t) currentPreset].bands)
    {
        set (bandId (pb.band, "type"), (float) pb.type); set (bandId (pb.band, "freq"), pb.freq);
        set (bandId (pb.band, "gain"), pb.gain); set (bandId (pb.band, "q"), pb.q); set (bandId (pb.band, "slope"), (float) pb.slope);
        set (bandId (pb.band, "chan"), (float) pb.chan); set (bandId (pb.band, "dyn"), pb.dyn ? 1.0f : 0.0f); set (bandId (pb.band, "thr"), pb.thr);
        set (bandId (pb.band, "on"), 1);
    }
    apvts.state.setProperty ("preset", currentPreset, nullptr);
}

APVTS::ParameterLayout MuziEqProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto db = AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1); });
    auto hz = AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; })
                .withValueFromStringFunction ([] (const String& t) { auto v = t.getFloatValue(); return t.containsIgnoreCase ("k") ? v * 1000.0f : v; });
    auto qa = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10 ? 2 : 1); });
    auto logRange = [] (float lo, float hi)
    {
        return NormalisableRange<float> (lo, hi,
            [] (float a, float b, float t) { return a * std::pow (b / a, t); },
            [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
            [] (float a, float b, float v) { return jlimit (a, b, v); });
    };
    for (int b = 0; b < kBands; ++b)
    {
        const auto& d = kDefaults[b];
        const String n = "Band " + String (b + 1) + " ";
        l.add (std::make_unique<AudioParameterBool>  (ParameterID { bandId (b, "on"), 1 }, n + "On", false));
        l.add (std::make_unique<AudioParameterChoice>(ParameterID { bandId (b, "type"), 1 }, n + "Type", kTypeNames, d.type));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "freq"), 1 }, n + "Freq", logRange (20.0f, 20000.0f), d.freq, hz));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "gain"), 1 }, n + "Gain", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, db));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "q"), 1 }, n + "Q", logRange (0.1f, 18.0f), d.q, qa));
        l.add (std::make_unique<AudioParameterChoice>(ParameterID { bandId (b, "slope"), 1 }, n + "Slope", kSlopeNames, 1));
        l.add (std::make_unique<AudioParameterChoice>(ParameterID { bandId (b, "chan"), 1 }, n + "Channel", kChanNames, 0));
        l.add (std::make_unique<AudioParameterBool>  (ParameterID { bandId (b, "dyn"), 1 }, n + "Dynamic", false));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "thr"), 1 }, n + "Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -20.0f, db));
    }
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "propq", 1 }, "Proportional Q", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, db));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "color", 1 }, "Color", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 6.0f, db));
    l.add (std::make_unique<AudioParameterChoice>(ParameterID { "character", 1 }, "Character", StringArray { "Tape", "Tube" }, 0));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

MuziEqProcessor::MuziEqProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MuziEQ", createLayout())
{
    for (int b = 0; b < kBands; ++b)
    {
        auto get = [&] (const char* w) { return apvts.getRawParameterValue (bandId (b, w)); };
        bp[(size_t) b] = { get ("on"), get ("type"), get ("freq"), get ("gain"), get ("q"), get ("slope"), get ("chan"), get ("dyn"), get ("thr") };
    }
    propQ = apvts.getRawParameterValue ("propq");
    output = apvts.getRawParameterValue ("output");
    colorOn = apvts.getRawParameterValue ("color");
    drive = apvts.getRawParameterValue ("drive");
    character = apvts.getRawParameterValue ("character");
    bypass = apvts.getRawParameterValue ("bypass");
    bypassParam = apvts.getParameter ("bypass");
}

bool MuziEqProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void MuziEqProcessor::prepareToPlay (double sr, int block)
{
    currentRate = sr;
    engine.prepare (sr);
    color.prepare (sr, block);
    outGain.reset (sr, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));
    bypassMix.reset (sr, 0.02);
    bypassMix.setCurrentAndTargetValue (bypass->load() > 0.5f ? 1.0f : 0.0f);
    dryForBypass.setSize (2, block * 2 + 64);
    for (auto& d : dryDelay) d.assign (256, 0.0f);
    dryPos = 0;
    colorWasOn = colorOn->load() > 0.5f;
    setLatencySamples (colorWasOn ? color.latency() : 0);
}

EqParams MuziEqProcessor::readParams() const
{
    EqParams p;
    for (int b = 0; b < kBands; ++b)
    {
        const auto& s = bp[(size_t) b];
        auto& d = p.band[(size_t) b];
        d.on = s.on->load() > 0.5f;
        d.type = (int) s.type->load();
        d.freq = s.freq->load(); d.gainDb = s.gain->load(); d.q = s.q->load();
        d.slope = (int) s.slope->load(); d.channel = (int) s.chan->load();
        d.dynamic = s.dyn->load() > 0.5f; d.thresholdDb = s.thr->load();
    }
    p.proportionalQ = propQ->load() > 0.5f;
    return p;
}

void MuziEqProcessor::tap (muzi::ui::AudioTap& t, muzi::ui::PeakAtomic& pk, const juce::AudioBuffer<float>& b)
{
    const int n = b.getNumSamples();
    float mono[2048];
    float peak = 0.0f;
    for (int s0 = 0; s0 < n; s0 += 2048)
    {
        const int len = juce::jmin (2048, n - s0);
        for (int i = 0; i < len; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < b.getNumChannels(); ++c) { const float x = b.getSample (c, s0 + i); v += x; peak = juce::jmax (peak, std::abs (x)); }
            mono[i] = v / (float) b.getNumChannels();
        }
        t.push (mono, len);
    }
    pk.put (peak);
}

void MuziEqProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    if (n == 0) return;
    tap (preTap, inPeak, buf);

    const bool colorNow = colorOn->load() > 0.5f;
    if (colorNow != colorWasOn)
    {
        colorWasOn = colorNow;
        color.reset();
        setLatencySamples (colorNow ? color.latency() : 0);
    }

    bypassMix.setTargetValue (bypass->load() > 0.5f ? 1.0f : 0.0f);
    const bool fading = bypassMix.isSmoothing() || bypassMix.getCurrentValue() > 0.0f;
    if (fading)
    {
        if (dryForBypass.getNumSamples() < n) dryForBypass.setSize (2, n, false, false, true);
        for (int c = 0; c < nch; ++c) dryForBypass.copyFrom (c, 0, buf, c, 0, n);
    }
    // the dry path always runs through a delay equal to the reported latency
    {
        const int lat = colorNow ? color.latency() : 0, size = (int) dryDelay[0].size();
        int pos = dryPos;
        for (int s = 0; s < n; ++s)
        {
            for (int c = 0; c < juce::jmin (2, nch); ++c)
            {
                auto& d = dryDelay[(size_t) c];
                d[(size_t) pos] = buf.getSample (c, s);
                if (fading) dryForBypass.setSample (c, s, d[(size_t) ((pos - lat + size) % size)]);
            }
            pos = (pos + 1) % size;
        }
        dryPos = pos;
    }

    float* L = buf.getWritePointer (0);
    float* R = nch > 1 ? buf.getWritePointer (1) : buf.getWritePointer (0);
    engine.process (L, R, n, readParams());

    if (colorNow) color.process (buf, drive->load(), (int) character->load());

    outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));
    for (int s = 0; s < n; ++s)
    {
        const float g = outGain.getNextValue();
        for (int c = 0; c < nch; ++c) buf.getWritePointer (c)[s] *= g;
    }

    if (fading)
    {
        // bypass crossfade (20 ms), dry path latency-aligned
        for (int s = 0; s < n; ++s)
        {
            const float m = bypassMix.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                float* x = buf.getWritePointer (c);
                x[s] = x[s] + m * (dryForBypass.getSample (c, s) - x[s]);
            }
        }
    }
    tap (postTap, outPeak, buf);
}

void MuziEqProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}
void MuziEqProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = (int) apvts.state.getProperty ("preset", 0);
        }
}

juce::AudioProcessorEditor* MuziEqProcessor::createEditor() { return new MuziEqEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MuziEqProcessor(); }
