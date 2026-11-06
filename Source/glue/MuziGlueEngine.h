#pragma once
// Muzi Glue engine: feed-forward bus compressor after Giannoulis, Massberg & Reiss,
// "Digital Dynamic Range Compressor Design - A Tutorial and Analysis" (JAES 2012):
// stereo-linked level detection, soft-knee gain computer in the log domain,
// smooth branching peak detector on the gain reduction, and a program-dependent
// auto release (a fast and a slow detector; the slower one only builds up during
// sustained compression). No JUCE dependency, no allocation after prepare().
#include "../shared/MatchedFilters.h"
#include <atomic>
#include <cmath>

namespace muzi
{
struct GlueParams
{
    float thresholdDb = -10.0f;
    float ratio = 2.0f;
    float kneeDb = 6.0f;
    float attackMs = 10.0f;
    float releaseMs = 300.0f;
    bool autoRelease = true;
    float scHpfHz = 20.0f;          // <= 20 Hz: off
    float makeupDb = 0.0f;
    float mix = 1.0f;               // 0..1, parallel compression
};

class MuziGlueEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        grDb = grFast = grSlow = 0.0;
        hp[0].reset(); hp[1].reset(); hp2[0].reset(); hp2[1].reset();
        lastHpf = -1;
        smoothMakeup = -1000;
        smoothMix = -1;
    }
    void reset() { prepare (fs); }

    // static curve: gain reduction (dB, >= 0) for an input level (dB)
    static double reductionDb (double x, double T, double R, double W)
    {
        double y;
        if (2.0 * (x - T) < -W) y = x;
        else if (2.0 * std::abs (x - T) <= W) { const double d = x - T + W * 0.5; y = x + (1.0 / R - 1.0) * d * d / (2.0 * W); }
        else y = T + (x - T) / R;
        return x - y;
    }

    void process (float* L, float* R, int n, const GlueParams& p)
    {
        if (std::abs (p.scHpfHz - lastHpf) > 0.0f)
        {
            lastHpf = p.scHpfHz;
            hpOn = p.scHpfHz > 20.5f;
            if (hpOn) { hpC = dsp::highPass2 (p.scHpfHz, 0.5412, fs); hpC2 = dsp::highPass2 (p.scHpfHz, 1.3066, fs); }   // 24 dB/oct Butterworth
        }
        const double aA = coef (p.attackMs);
        const double aR = coef (p.releaseMs);
        const double aRf = coef (std::min (60.0, (double) p.releaseMs)), aRs = coef (1000.0);
        const double aAs = coef (std::max (300.0, 10.0 * p.attackMs));
        const double W = std::max (0.0, (double) p.kneeDb), T = p.thresholdDb, Rt = std::max (1.0, (double) p.ratio);
        const double mkTarget = std::pow (10.0, p.makeupDb / 20.0);
        if (smoothMakeup < -999) smoothMakeup = mkTarget;
        if (smoothMix < 0) smoothMix = p.mix;
        const double pa = coef (20.0);

        for (int s = 0; s < n; ++s)
        {
            double l = L[s], r = R[s], dl = l, dr = r;
            if (hpOn) { dl = hp2[0].process (hpC2, hp[0].process (hpC, l)); dr = hp2[1].process (hpC2, hp[1].process (hpC, r)); }
            const double lvl = std::max (std::abs (dl), std::abs (dr));               // stereo-linked peak
            const double xg = 20.0 * std::log10 (lvl + 1e-12);
            const double xl = reductionDb (xg, T, Rt, W);

            if (p.autoRelease)
            {
                grFast = xl > grFast ? aA * grFast + (1 - aA) * xl : aRf * grFast + (1 - aRf) * xl;
                grSlow = xl > grSlow ? aAs * grSlow + (1 - aAs) * xl : aRs * grSlow + (1 - aRs) * xl;
                grDb = std::max (grFast, grSlow);
            }
            else
            {
                grDb = xl > grDb ? aA * grDb + (1 - aA) * xl : aR * grDb + (1 - aR) * xl;
                grFast = grSlow = grDb;
            }
            smoothMakeup = pa * smoothMakeup + (1 - pa) * mkTarget;
            smoothMix = pa * smoothMix + (1 - pa) * p.mix;
            const double g = std::pow (10.0, -grDb / 20.0);
            const double wet = smoothMix * g * smoothMakeup, dry = 1.0 - smoothMix;
            L[s] = (float) (l * (dry + wet));
            R[s] = (float) (r * (dry + wet));
            peakGr = std::max (peakGr, grDb);
            peakIn = std::max (peakIn, xg);
        }
        meterGr.store ((float) peakGr);
        meterInDb.store ((float) peakIn);
        peakGr = 0; peakIn = -200;
    }

    std::atomic<float> meterGr { 0.0f };      // highest gain reduction in the last block (dB)
    std::atomic<float> meterInDb { -200.0f }; // highest detector level in the last block (dBFS)

private:
    double fs = 48000, grDb = 0, grFast = 0, grSlow = 0, smoothMakeup = -1000, smoothMix = -1;
    double peakGr = 0, peakIn = -200;
    float lastHpf = -1; bool hpOn = false;
    dsp::Biquad hpC, hpC2;
    dsp::BiquadState hp[2], hp2[2];
    double coef (double ms) const { return std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * fs)); }
};
} // namespace muzi
