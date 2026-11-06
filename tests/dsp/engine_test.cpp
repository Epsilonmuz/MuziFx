#include "../../Source/eq/MuziEqEngine.h"
#include "../../Source/glue/MuziGlueEngine.h"
#include <cstdio>
#include <vector>
#include <complex>
#include <random>
using namespace muzi;
const double fs = 48000;
// magnitude (dB) of the engine's response at f, measured with a steady sine
static double sineGainDb (MuziEqEngine& e, const EqParams& p, double f, bool left = true, bool mono = true)
{
    int n = (int) fs; std::vector<float> L (n), R (n);
    for (int i = 0; i < n; ++i) { double v = 0.25 * std::sin (2 * M_PI * f * i / fs); L[i] = (float) v; R[i] = (float) (mono ? v : -v); }
    e.reset(); e.process (L.data(), R.data(), n, p);
    double a = 0, b = 0; const auto& X = left ? L : R;
    for (int i = n / 2; i < n; ++i) { a += X[i] * X[i]; double v = 0.25 * std::sin (2 * M_PI * f * i / fs); b += v * v; }
    return 10 * std::log10 (a / b);
}
int main()
{
    // 1. engine response equals the designed curve
    MuziEqEngine e; e.prepare (fs); EqParams p;
    p.band[0] = { true, Bell, 2000, 6, 1.0f }; p.band[1] = { true, HighShelf, 10000, 3, 0.71f };
    p.band[2] = { true, LowCut, 40, 0, 0.71f, 3 };
    double maxErr = 0;
    for (double f : { 30., 60., 200., 1000., 2000., 4000., 8000., 12000., 16000. })
    {
        dsp::Biquad c[4]; double d = 0;
        for (int b = 0; b < 3; ++b) { int k = designBand (p.band[b].type, p.band[b].freq, p.band[b].gainDb, p.band[b].q, p.band[b].slope, true, fs, c); for (int j = 0; j < k; ++j) d += c[j].magDb (f, fs); }
        double m = sineGainDb (e, p, f); maxErr = std::max (maxErr, std::abs (m - d));
    }
    printf ("1. engine vs designed curve, max error: %.3f dB\n", maxErr);

    // 2. mid/side routing: a Mid band leaves a pure-side signal untouched and vice versa
    EqParams ms; ms.band[0] = { true, Bell, 1000, 12, 1.0f, 1, Mid };
    double sideOnMid = sineGainDb (e, ms, 1000, true, false), midOnMid = sineGainDb (e, ms, 1000, true, true);
    ms.band[0].channel = Side;
    double sideOnSide = sineGainDb (e, ms, 1000, true, false), midOnSide = sineGainDb (e, ms, 1000, true, true);
    printf ("2. M/S: Mid band on mono %+.2f, on side %+.2f | Side band on mono %+.2f, on side %+.2f (dB)\n", midOnMid, sideOnMid, midOnSide, sideOnSide);

    // 3. automation clicks: sweep a +12 dB Q4 bell 100 Hz -> 10 kHz in 0.5 s over a 1 kHz sine,
    //    toggle bands and change type mid-signal. Click = sample jump beyond what the sine can make.
    {
        MuziEqEngine z; z.prepare (fs); EqParams q; q.band[0] = { true, Bell, 100, 12, 4.0f };
        q.band[1] = { false, LowCut, 200, 0, 0.71f, 3 };
        const int n = (int) (2 * fs), blk = 64; std::vector<float> L (n), R (n);
        for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (0.1 * std::sin (2 * M_PI * 1000 * i / fs));
        double worst = 0, prev = 0;
        for (int s = 0; s < n; s += blk)
        {
            double t = s / fs;
            q.band[0].freq = (float) (100 * std::pow (100.0, std::min (1.0, t / 0.5)));
            q.band[1].on = t > 0.8 && t < 1.3;           // low cut in and out
            q.band[0].type = t > 1.5 ? HighShelf : Bell;  // type change
            z.process (L.data() + s, R.data() + s, blk, q);
            for (int i = s; i < s + blk; ++i) { worst = std::max (worst, std::abs (L[i] - prev)); prev = L[i]; }
        }
        double sineMax = 0.1 * std::pow (10, 12 / 20.0) * 2 * M_PI * 1000 / fs;
        printf ("3. automation: largest sample step %.4f vs %.4f max a clean sine can make -> %s\n", worst, sineMax, worst <= sineMax * 1.05 ? "no clicks" : "CLICKS");
    }

    // 4. dynamic band: Bell 2 kHz, -6 dB max, threshold -20 dB. Quiet tone (-30 dBFS) vs loud tone (-6 dBFS)
    {
        EqParams d; d.band[0] = { true, Bell, 2000, -6, 1.0f, 1, Stereo, true, -20 };
        MuziEqEngine z; z.prepare (fs);
        auto lvl = [&] (double amp) { int n = (int) fs; std::vector<float> L (n), R (n);
            for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (amp * std::sin (2 * M_PI * 2000 * i / fs));
            z.reset(); z.process (L.data(), R.data(), n, d); double a = 0, b = 0;
            for (int i = n / 2; i < n; ++i) { a += L[i] * L[i]; double v = amp * std::sin (2 * M_PI * 2000 * i / fs); b += v * v; } return 10 * std::log10 (a / b); };
        printf ("4. dynamic bell: tone at -30 dBFS %+.2f dB, at -14 dBFS %+.2f dB, at -6 dBFS %+.2f dB\n", lvl (0.0316), lvl (0.2), lvl (0.5));
    }

    // ---------------- Glue
    {
        GlueParams g; g.thresholdDb = -20; g.ratio = 4; g.kneeDb = 6; g.attackMs = 10; g.releaseMs = 100; g.autoRelease = false;
        printf ("5. static curve (4:1, -20 dB, 6 dB knee): input dBFS -> gain reduction measured / formula\n");
        for (double in : { -30., -23., -20., -17., -10., 0. })
        {
            MuziGlueEngine c; c.prepare (fs); int n = (int) fs; std::vector<float> L (n), R (n);
            for (int i = 0; i < n; ++i) L[i] = R[i] = (float) (std::pow (10, in / 20) * std::sin (2 * M_PI * 997 * i / fs));
            c.process (L.data(), R.data(), n, g);
            // steady state: peak in the last 100 ms vs input peak
            double pk = 0; for (int i = n - 4800; i < n; ++i) pk = std::max (pk, (double) std::abs (L[i]));
            printf ("   %+5.0f -> %5.2f / %5.2f\n", in, in - 20 * std::log10 (pk), MuziGlueEngine::reductionDb (in, -20, 4, 6));
        }
        // 6. attack / release timing (step from -30 to -5 dBFS and back), 10 ms / 100 ms
        auto timing = [&] (GlueParams gp, double loudS, double& att, double& rel)
        {
            MuziGlueEngine c; c.prepare (fs); int a = (int) (0.5 * fs), b = (int) (loudS * fs), n = a + b + (int) (3 * fs);
            std::vector<float> L (n), R (n), gr (n);
            for (int i = 0; i < n; ++i) { double amp = (i >= a && i < a + b) ? 0.56 : 0.0316; L[i] = R[i] = (float) amp; }   // DC steps: level is exact
            std::vector<float> L2 = L;
            for (int i = 0; i < n; i += 32) c.process (L.data() + i, R.data() + i, std::min (32, n - i), gp);
            for (int i = 0; i < n; ++i) gr[i] = (float) (20 * std::log10 (L2[i] / L[i]));
            double grMax = gr[a + b - 1];
            int i63 = a; while (gr[i63] < 0.632 * grMax) ++i63;
            int j = a + b; while (j < n - 1 && gr[j] > 0.368 * grMax) ++j;
            att = (i63 - a) / fs * 1000; rel = (j - a - b) / fs * 1000;
        };
        double at, rl; GlueParams gp = g; timing (gp, 1.0, at, rl);
        printf ("6. timing, set 10 ms / 100 ms: measured attack %.1f ms, release %.1f ms\n", at, rl);
        gp.autoRelease = true; gp.attackMs = 10;
        double at2, rShort, rLong; timing (gp, 0.05, at2, rShort); timing (gp, 3.0, at2, rLong);
        printf ("7. auto release: after a 50 ms burst %.0f ms, after 3 s of compression %.0f ms\n", rShort, rLong);
    }
}
