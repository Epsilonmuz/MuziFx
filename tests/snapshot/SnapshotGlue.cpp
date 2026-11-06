// Renders the Muzi Glue editor to PNG after feeding it audio:
//   MuziSnapshotGlue <in.wav> <out.png> [width] [preset] [threshold dB]
#include "GlueProcessor.h"
#include "GlueEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    MuziGlueProcessor proc;
    if (argc > 4) proc.loadFactoryPreset (juce::String (argv[4]).getIntValue());
    if (argc > 5) if (auto* p = proc.apvts.getParameter ("threshold")) p->setValueNotifyingHost (p->convertTo0to1 (juce::String (argv[5]).getFloatValue()));
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    juce::AudioFormatManager afm; afm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (afm.createReaderFor (juce::File (juce::String (argv[1]))));
    const juce::int64 start = argc > 6 ? (juce::int64) (juce::String (argv[6]).getDoubleValue() * 44100) : 0;
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples - start, 44100 * 6);
    juce::AudioBuffer<float> buf (2, n); rd->read (&buf, 0, n, start, true, true);
    juce::MidiBuffer midi;
    std::unique_ptr<MuziGlueEditor> ed (static_cast<MuziGlueEditor*> (proc.createEditor()));
    if (argc > 3) ed->setSize (juce::String (argv[3]).getIntValue(), juce::String (argv[3]).getIntValue() * 560 / 960);
    const int hop = 44100 / 30;                   // one GUI frame per 1/30 s of audio, like a running editor
    for (int s = 0; s + hop <= n; s += hop)
    {
        for (int k = s; k + 490 <= s + hop; k += 490) { juce::AudioBuffer<float> v (buf.getArrayOfWritePointers(), 2, k, 490); proc.processBlock (v, midi); }
        ed->refreshForSnapshot (1);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out { juce::String (argv[2]) }; out.deleteFile();
    juce::FileOutputStream os (out); juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
