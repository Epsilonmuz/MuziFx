#pragma once
// Optional analog-style colour: 4x oversampled saturation, unity small-signal gain.
//   Tape: symmetric (odd harmonics), rounds peaks.
//   Tube: biased (even + odd harmonics), a warmer, thicker edge.
// Off by default; when on it adds the oversampler's latency (reported by the plugin).
#include <juce_dsp/juce_dsp.h>

namespace muzi
{
class ColorStage
{
public:
    void prepare (double fs, int maxBlock)
    {
        os.initProcessing ((size_t) maxBlock);
        os.reset();
        drive.reset (fs, 0.05);
        const double a = std::exp (-2.0 * juce::MathConstants<double>::pi * 6.0 / fs);   // 6 Hz DC blocker
        dcA = a;
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0;
    }
    void reset() { os.reset(); dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0; }
    int latency() const { return (int) os.getLatencyInSamples(); }

    void process (juce::AudioBuffer<float>& buf, float driveDb, int character)
    {
        drive.setTargetValue (juce::Decibels::decibelsToGain (driveDb));
        juce::dsp::AudioBlock<float> block (buf);
        auto up = os.processSamplesUp (block);
        const int n = (int) up.getNumSamples();
        const float b = character == 1 ? 0.35f : 0.0f;
        const float tb = std::tanh (b), norm = 1.0f / (1.0f - tb * tb);
        // the drive ramp advances at the base rate: one step every 4 oversampled samples
        float* l = up.getChannelPointer (0);
        float* r = up.getNumChannels() > 1 ? up.getChannelPointer (1) : nullptr;
        float g = drive.getCurrentValue();
        for (int i = 0; i < n; ++i)
        {
            if ((i & 3) == 0) g = drive.getNextValue();
            l[i] = (std::tanh (g * l[i] + b) - tb) * norm / g;
            if (r) r[i] = (std::tanh (g * r[i] + b) - tb) * norm / g;
        }
        os.processSamplesDown (block);
        if (character == 1)        // remove the DC the bias creates
            for (int c = 0; c < juce::jmin (2, buf.getNumChannels()); ++c)
            {
                auto* x = buf.getWritePointer (c);
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    const double y = x[i] - dcX[c] + dcA * dcY[c];
                    dcX[c] = x[i]; dcY[c] = y; x[i] = (float) y;
                }
            }
    }

private:
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> drive { 1.0f };
    double dcA = 0.999, dcX[2] {}, dcY[2] {};
};
} // namespace muzi
