// Minimal headless VST3 host for testing Muzi plugins exactly as a DAW loads it.
//   MuziHostTest <plugin.vst3> <signals_dir> <out_dir> [paramId=value ...] [--block N]
// Prints the reported latency, renders every .wav in signals_dir (32-bit float),
// and checks that saving/restoring state round-trips every parameter.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

static juce::AudioProcessorParameter* findParam (juce::AudioProcessor& p, const juce::String& id)
{
    for (auto* prm : p.getParameters())
        if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (prm))
            if (h->getParameterID() == id || prm->getName (64).equalsIgnoreCase (id))
                return prm;
    return nullptr;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 4) { std::puts ("usage: MuziHostTest plugin.vst3 signals_dir out_dir [id=value ...] [--block N] [--rate Hz]"); return 2; }

    const juce::File pluginFile { juce::String (argv[1]) }, inDir { juce::String (argv[2]) }, outDir { juce::String (argv[3]) };
    int block = 512;
    double fs = 48000.0;
    juce::StringPairArray sets;
    for (int i = 4; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--block" && i + 1 < argc) block = juce::String (argv[++i]).getIntValue();
        else if (a == "--rate" && i + 1 < argc) fs = juce::String (argv[++i]).getDoubleValue();
        else if (a.contains ("=")) sets.set (a.upToFirstOccurrenceOf ("=", false, false), a.fromFirstOccurrenceOf ("=", false, false));
    }

    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> types;
    vst3.findAllTypesForFile (types, pluginFile.getFullPathName());
    if (types.isEmpty()) { std::puts ("FAIL: no plugin found in file"); return 1; }

    juce::AudioPluginFormatManager fm;
    fm.addFormat (new juce::VST3PluginFormat());
    juce::String err;
    auto inst = fm.createPluginInstance (*types[0], fs, block, err);
    if (! inst) { std::printf ("FAIL: %s\n", err.toRawUTF8()); return 1; }
    std::printf ("loaded: %s, %d params\n", inst->getName().toRawUTF8(), inst->getParameters().size());

    for (auto& key : sets.getAllKeys())
    {
        auto* prm = findParam (*inst, key);
        if (! prm) { std::printf ("FAIL: no parameter '%s'\n", key.toRawUTF8()); return 1; }
        // values are given in real units; map through the parameter's own text parser
        const float norm = prm->getValueForText (sets[key]);
        prm->setValueNotifyingHost (norm);
        std::printf ("set %s = %s (%s)\n", key.toRawUTF8(), sets[key].toRawUTF8(), prm->getCurrentValueAsText().toRawUTF8());
    }

    inst->setPlayConfigDetails (2, 2, fs, block);
    inst->prepareToPlay (fs, block);
    juce::AudioBuffer<float> warm (2, block); warm.clear(); juce::MidiBuffer midi;
    inst->processBlock (warm, midi);          // lets a mode change apply before we read latency
    std::printf ("latency: %d samples\n", inst->getLatencySamples());

    // state round trip
    juce::MemoryBlock state;
    inst->getStateInformation (state);
    auto inst2 = fm.createPluginInstance (*types[0], fs, block, err);
    inst2->setStateInformation (state.getData(), (int) state.getSize());
    int mismatches = 0;
    for (int i = 0; i < inst->getParameters().size(); ++i)
        if (std::abs (inst->getParameters()[i]->getValue() - inst2->getParameters()[i]->getValue()) > 1e-6f) ++mismatches;
    std::printf ("state round trip: %s\n", mismatches == 0 ? "OK" : "MISMATCH");

    juce::AudioFormatManager afm; afm.registerBasicFormats();
    outDir.createDirectory();
    for (auto& f : inDir.findChildFiles (juce::File::findFiles, false, "*.wav"))
    {
        std::unique_ptr<juce::AudioFormatReader> rd (afm.createReaderFor (f));
        if (! rd) continue;
        const int n = (int) rd->lengthInSamples, ch = (int) rd->numChannels;
        juce::AudioBuffer<float> buf (2, n);
        rd->read (&buf, 0, n, 0, true, true);
        if (ch == 1) buf.copyFrom (1, 0, buf, 0, 0, n);
        inst->reset();
        for (int s = 0; s < n; s += block)
        {
            const int len = std::min (block, n - s);
            juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, s, len);
            inst->processBlock (view, midi);
        }
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int s = 0; s < n; ++s) finite &= std::isfinite (buf.getSample (c, s));
        auto out = outDir.getChildFile (f.getFileName());
        out.deleteFile();
        std::unique_ptr<juce::OutputStream> os (out.createOutputStream().release());
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> wr (wav.createWriterFor (os.get(), fs, 2, 32, {}, 0));
        if (wr) { os.release(); wr->writeFromAudioSampleBuffer (buf, 0, n); }
        std::printf ("rendered %s%s\n", f.getFileName().toRawUTF8(), finite ? "" : "  NON-FINITE OUTPUT");
    }
    inst->releaseResources();
    return mismatches == 0 ? 0 : 1;
}
