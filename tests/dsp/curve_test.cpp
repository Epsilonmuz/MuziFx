// Compares matched filters and RBJ bilinear filters against the analog prototype.
#include "../../Source/shared/MatchedFilters.h"
#include <cstdio>
#include <vector>
using namespace muzi::dsp;
static Biquad rbjBell (double f, double db, double q, double fs)
{
    double A = std::pow (10, db / 40), w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), a0 = 1 + al / A;
    return { (1 + al * A) / a0, -2 * std::cos (w) / a0, (1 - al * A) / a0, -2 * std::cos (w) / a0, (1 - al / A) / a0 };
}
static double analogMag (const Analog2& h, double f) { return 10 * std::log10 (h.mag2 (2 * kPi * f)); }
int main()
{
    double rates[] = { 44100, 48000, 96000 };
    printf ("max |error| vs analog over 20 Hz - 20 kHz (dB)\n");
    printf ("%-34s %8s %8s %8s\n", "filter", "44.1k", "48k", "96k");
    struct Case { const char* name; int type; double f, g, q; };
    Case cs[] = { { "Bell 1k +6 Q1", 0, 1000, 6, 1 }, { "Bell 5k +6 Q1", 0, 5000, 6, 1 }, { "Bell 10k +6 Q1", 0, 10000, 6, 1 },
                  { "Bell 16k +6 Q1", 0, 16000, 6, 1 }, { "Bell 16k -12 Q4", 0, 16000, -12, 4 }, { "Bell 12k +3 Q0.5", 0, 12000, 3, 0.5 },
                  { "LowShelf 100 +6 Q.71", 1, 100, 6, .707 }, { "HighShelf 10k +4 Q.71", 2, 10000, 4, .707 }, { "HighShelf 16k +4 Q.71", 2, 16000, 4, .707 },
                  { "HighShelf 4k -6 Q1.2", 2, 4000, -6, 1.2 }, { "LowPass 12k Q.71", 3, 12000, 0, .707 }, { "HighPass 30 Q.71", 4, 30, 0, .707 },
                  { "Notch 15k Q8 (excl. +-3%)", 5, 15000, 0, 8 } };
    for (auto& c : cs)
    {
        printf ("%-34s", c.name);
        for (double fs : rates)
        {
            double w = 2 * kPi * c.f, A = std::pow (10, c.g / 40), sA = std::sqrt (A);
            Analog2 h; Biquad b;
            switch (c.type)
            {
                case 0: h = { 1, w * A / c.q, w * w, w, c.q * A }; b = bell (c.f, c.g, c.q, fs); break;
                case 1: h = { 1, sA / c.q * w, A * w * w, w / sA, c.q }; b = lowShelf (c.f, c.g, c.q, fs); break;
                case 2: h = { A * A, A * sA / c.q * w, A * w * w, w * sA, c.q }; b = highShelf (c.f, c.g, c.q, fs); break;
                case 3: h = { 0, 0, w * w, w, c.q }; b = lowPass2 (c.f, c.q, fs); break;
                case 4: h = { 1, 0, 0, w, c.q }; b = highPass2 (c.f, c.q, fs); break;
                default: h = { 1, 0, w * w, w, c.q }; b = notch (c.f, c.q, fs); break;
            }
            double e = 0;
            for (double f = 20; f <= 20000; f *= 1.005)
            {
                if (c.type == 5 && std::abs (f / c.f - 1) < 0.03) continue;
                e = std::max (e, std::abs (b.magDb (f, fs) - analogMag (h, f)));
            }
            printf (" %8.2f", e);
        }
        printf ("\n");
    }
    printf ("\nRBJ (bilinear) for comparison:\n");
    for (auto c : { Case { "RBJ Bell 10k +6 Q1", 0, 10000, 6, 1 }, Case { "RBJ Bell 16k +6 Q1", 0, 16000, 6, 1 } })
    {
        printf ("%-34s", c.name);
        for (double fs : rates)
        {
            double w = 2 * kPi * c.f, A = std::pow (10, c.g / 40); Analog2 h { 1, w * A / c.q, w * w, w, c.q * A };
            Biquad b = rbjBell (c.f, c.g, c.q, fs); double e = 0;
            for (double f = 20; f <= 20000; f *= 1.005) e = std::max (e, std::abs (b.magDb (f, fs) - analogMag (h, f)));
            printf (" %8.2f", e);
        }
        printf ("\n");
    }
    // where the error of a matched 16k bell lands at 44.1k
    printf ("\nBell 16k +6 Q1 @44.1k: f, analog, matched, rbj\n");
    double f0 = 16000, fs = 44100, w = 2 * kPi * f0, A = std::pow (10, 6 / 40.0); Analog2 h { 1, w * A, w * w, w, A };
    auto m = bell (f0, 6, 1, fs), r = rbjBell (f0, 6, 1, fs);
    for (double f : { 4000., 8000., 12000., 14000., 16000., 18000., 20000., 21000. }) printf ("  %6.0f %6.2f %6.2f %6.2f\n", f, analogMag (h, f), m.magDb (f, fs), r.magDb (f, fs));
}
