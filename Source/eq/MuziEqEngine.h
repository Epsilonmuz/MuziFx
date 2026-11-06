#pragma once
// Muzi EQ engine: 8 analog-matched bands, proportional Q, M/S per band,
// dynamic bands. No JUCE dependency, no allocation after prepare().
#include "../shared/MatchedFilters.h"
#include <array>
#include <atomic>

namespace muzi
{
enum BandType { Bell = 0, LowShelf, HighShelf, Tilt, LowCut, HighCut, Notch, kNumTypes };
enum BandChannel { Stereo = 0, Mid, Side };

struct BandParams
{
    bool on = false;
    int type = Bell;
    float freq = 1000.0f, gainDb = 0.0f, q = 0.71f;
    int slope = 1;              // cuts: 0..5 -> 6, 12, 18, 24, 36, 48 dB/oct
    int channel = Stereo;
    bool dynamic = false;
    float thresholdDb = -20.0f;
};

struct EqParams
{
    static constexpr int kBands = 8;
    std::array<BandParams, kBands> band;
    bool proportionalQ = true;
};

inline int slopeOrder (int slopeIndex) { static const int o[] = { 1, 2, 3, 4, 6, 8 }; return o[std::clamp (slopeIndex, 0, 5)]; }
inline bool typeHasGain (int t) { return t == Bell || t == LowShelf || t == HighShelf || t == Tilt; }
inline bool typeCanBeDynamic (int t) { return t == Bell || t == LowShelf || t == HighShelf; }

// Proportional Q: Q is exact at +-6 dB; smaller moves are wider, bigger moves narrower.
inline double effectiveQ (int type, double q, double gainDb, bool proportional)
{
    if (type != Bell || ! proportional) return q;
    return std::clamp (q * std::pow (10.0, (std::abs (gainDb) - 6.0) / 40.0), 0.05, 60.0);
}

// Builds a band's sections. Returns the number of sections used (<= 4).
inline int designBand (int type, double f, double gainDb, double q, int slope, bool propQ, double fs, dsp::Biquad* out)
{
    using namespace dsp;
    f = std::clamp (f, 10.0, 0.49 * fs);
    switch (type)
    {
        case Bell:      out[0] = bell (f, gainDb, effectiveQ (type, q, gainDb, propQ), fs); return 1;
        case LowShelf:  out[0] = lowShelf (f, gainDb, q, fs); return 1;
        case HighShelf: out[0] = highShelf (f, gainDb, q, fs); return 1;
        case Tilt:      out[0] = tilt (f, gainDb, fs); return 1;
        case Notch:     out[0] = notch (f, q, fs); return 1;
        case LowCut:
        case HighCut:
        {
            const int n = slopeOrder (slope);
            int k = 0;
            if (n & 1) out[k++] = type == LowCut ? highPass1 (f, fs) : lowPass1 (f, fs);
            const int pairs = n / 2;
            for (int i = 1; i <= pairs; ++i)
            {
                const double theta = (n & 1) ? i * kPi / n : (2.0 * i - 1.0) * kPi / (2.0 * n);
                double qb = 1.0 / (2.0 * std::cos (theta));                               // Butterworth pole pair
                if (i == 1) qb *= q / 0.7071;           // resonance: user Q shapes the corner
                out[k++] = type == LowCut ? highPass2 (f, qb, fs) : lowPass2 (f, qb, fs);
            }
            return k;
        }
        default: out[0] = {}; return 1;
    }
}

class MuziEqEngine
{
public:
    static constexpr int kBands = EqParams::kBands, kMaxSec = 4, kCtl = 16;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const double ctlRate = fs / kCtl;
        smoothA = std::exp (-1.0 / (0.025 * ctlRate));      // 25 ms parameter glide
        fadeStep = 1.0 / (0.020 * ctlRate);                  // 20 ms on/off and type crossfades
        detAtt = std::exp (-1.0 / (0.003 * fs));
        detRel = std::exp (-1.0 / (0.100 * fs));
        dynA = std::exp (-1.0 / (0.002 * ctlRate * 4));
        for (auto& b : bands) b = BandState {};
        first = true;
    }
    void reset() { for (auto& b : bands) { b.resetState(); b.env = 0; } }

    // L and R are processed in place
    void process (float* L, float* R, int n, const EqParams& p)
    {
        if (first) { snap (p); first = false; }
        for (int s0 = 0; s0 < n; s0 += kCtl)
        {
            const int len = std::min (kCtl, n - s0);
            control (p);
            for (int s = s0; s < s0 + len; ++s)
            {
                double m = 0.5 * ((double) L[s] + R[s]);
                double sd = 0.5 * ((double) L[s] - R[s]);
                for (auto& b : bands)
                {
                    if (b.mix <= 0.0) continue;
                    if (b.dyn) b.detect (b.channel == Side ? sd : m, b.channel == Stereo ? sd : 0.0, detAtt, detRel);
                    if (b.channel != Side) m = b.run (0, m);
                    if (b.channel != Mid)  sd = b.run (1, sd);
                }
                L[s] = (float) (m + sd);
                R[s] = (float) (m - sd);
            }
        }
    }

    // GUI: current effective gain of each band (moves when the band is dynamic)
    std::array<std::atomic<float>, kBands> meterGain {};
    double sampleRate() const { return fs; }

private:
    struct BandState
    {
        int type = Bell, slope = 1, channel = Stereo, nSec = 1;
        bool dyn = false;
        double lf = std::log (1000.0), g = 0, lq = std::log (0.71);
        double mix = 0, mixTarget = 0;
        double env = 0, dynGain = 0;
        dsp::Biquad c[kMaxSec], det;
        dsp::BiquadState st[2][kMaxSec], detSt[2];
        bool pending = false;     // type/slope/channel change waiting for fade-out

        void resetState() { for (auto& ch : st) for (auto& s : ch) s.reset(); for (auto& d : detSt) d.reset(); }
        inline double run (int ch, double x)
        {
            double y = x;
            for (int k = 0; k < nSec; ++k) y = st[ch][k].process (c[k], y);
            return mix >= 1.0 ? y : x + mix * (y - x);
        }
        inline void detect (double a, double b, double att, double rel)
        {
            const double ya = detSt[0].process (det, a), yb = detSt[1].process (det, b);
            const double pw = ya * ya + yb * yb;
            env = pw > env ? att * env + (1 - att) * pw : rel * env + (1 - rel) * pw;
        }
    };
    std::array<BandState, kBands> bands;
    double fs = 48000, smoothA = 0.9, fadeStep = 0.1, detAtt = 0.9, detRel = 0.99, dynA = 0.5;
    bool first = true;

    void snap (const EqParams& p)
    {
        for (int i = 0; i < kBands; ++i)
        {
            auto& b = bands[(size_t) i]; const auto& bp = p.band[(size_t) i];
            b.type = bp.type; b.slope = bp.slope; b.channel = bp.channel; b.dyn = bp.dynamic && typeCanBeDynamic (bp.type);
            b.lf = std::log ((double) bp.freq); b.g = bp.gainDb; b.lq = std::log ((double) bp.q);
            b.mix = b.mixTarget = bp.on ? 1.0 : 0.0;
            design (b, p);
        }
    }

    void design (BandState& b, const EqParams& p)
    {
        double g = b.g;
        if (b.dyn) g = b.dynGain;
        if (! typeHasGain (b.type)) g = 0;
        b.nSec = designBand (b.type, std::exp (b.lf), g, std::exp (b.lq), b.slope, p.proportionalQ, fs, b.c);
        if (b.dyn)
        {
            const double f = std::exp (b.lf), q = std::exp (b.lq);
            b.det = b.type == Bell ? dsp::bandPass (f, std::max (0.3, effectiveQ (Bell, q, b.g, p.proportionalQ)), fs)
                  : b.type == LowShelf ? dsp::lowPass2 (std::clamp (f, 10.0, 0.45 * fs), 0.71, fs)
                                       : dsp::highPass2 (std::clamp (f, 10.0, 0.45 * fs), 0.71, fs);
        }
    }

    void control (const EqParams& p)
    {
        for (int i = 0; i < kBands; ++i)
        {
            auto& b = bands[(size_t) i]; const auto& bp = p.band[(size_t) i];
            const bool wantDyn = bp.dynamic && typeCanBeDynamic (bp.type);
            const bool structural = bp.type != b.type || (bp.slope != b.slope && (bp.type == LowCut || bp.type == HighCut))
                                    || bp.channel != b.channel || wantDyn != b.dyn;
            // structural changes: fade out, switch, fade back in
            if (structural && b.mix > 0.0) { b.pending = true; b.mixTarget = 0.0; }
            else
            {
                if (structural || b.pending)
                {
                    b.type = bp.type; b.slope = bp.slope; b.channel = bp.channel; b.dyn = wantDyn;
                    b.lf = std::log ((double) bp.freq); b.g = bp.gainDb; b.lq = std::log ((double) bp.q);
                    b.resetState(); b.env = 0; b.dynGain = 0; b.pending = false;
                    design (b, p);
                }
                b.mixTarget = bp.on ? 1.0 : 0.0;
            }
            if (std::abs (b.mix - b.mixTarget) > 0.0)
                b.mix = b.mix < b.mixTarget ? std::min (b.mixTarget, b.mix + fadeStep) : std::max (b.mixTarget, b.mix - fadeStep);
            if (b.mix <= 0.0 && b.mixTarget <= 0.0) { if (! b.pending) b.resetState(); meterGain[(size_t) i].store (0.0f); continue; }

            // parameter glide (log frequency, dB gain, log Q)
            const double tf = std::log ((double) bp.freq), tg = bp.gainDb, tq = std::log ((double) bp.q);
            bool changed = false;
            auto glide = [&] (double& v, double t, double eps)
            {
                if (std::abs (v - t) < eps) { if (std::abs (v - t) > 0.0) { v = t; changed = true; } return; }
                v = t + smoothA * (v - t); changed = true;
            };
            glide (b.lf, tf, 1e-4); glide (b.g, tg, 1e-3); glide (b.lq, tq, 1e-4);

            if (b.dyn)
            {
                const double lvl = 10.0 * std::log10 (b.env + 1e-20);            // peak level of the band (dBFS)
                const double o = lvl - bp.thresholdDb;
                const double ok = o < -3.0 ? 0.0 : o > 3.0 ? o : (o + 3.0) * (o + 3.0) / 12.0;   // 6 dB soft knee
                const double amount = std::min (std::abs (b.g), ok * (1.0 - 1.0 / 3.0));         // 3:1 above threshold
                const double target = b.g < 0 ? -amount : amount;
                b.dynGain = target + dynA * (b.dynGain - target);
                changed = true;
            }
            if (changed) design (b, p);
            meterGain[(size_t) i].store ((float) (b.dyn ? b.dynGain : b.g));
        }
    }
};
} // namespace muzi
