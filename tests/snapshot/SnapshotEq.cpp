// Renders the Muzi EQ editor to PNG after feeding it audio:
//   MuziSnapshotEQ <in.wav> <out.png> [width] [preset] [selectedBand]
#include "EqProcessor.h"
#include "EqEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    MuziEqProcessor proc;
    if (argc > 4) proc.loadFactoryPreset (juce::String (argv[4]).getIntValue());
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioFormatManager afm; afm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (afm.createReaderFor (juce::File (juce::String (argv[1]))));
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, 48000 * 4);
    juce::AudioBuffer<float> buf (2, n); rd->read (&buf, 0, n, 0, true, true);
    juce::MidiBuffer midi;
    std::unique_ptr<MuziEqEditor> ed (static_cast<MuziEqEditor*> (proc.createEditor()));
    if (argc > 3) ed->setSize (juce::String (argv[3]).getIntValue(), juce::String (argv[3]).getIntValue() * 650 / 1000);
    if (argc > 5) ed->selectBand (juce::String (argv[5]).getIntValue());
    for (int s = 0; s + 512 <= n; s += 512) { juce::AudioBuffer<float> v (buf.getArrayOfWritePointers(), 2, s, 512); proc.processBlock (v, midi); if (s % 4096 == 0) ed->refreshForSnapshot(); }
    ed->refreshForSnapshot();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out { juce::String (argv[2]) }; out.deleteFile();
    juce::FileOutputStream os (out); juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
