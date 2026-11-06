// pluginval-style automation fuzz: random parameter values every 32 samples on noise.
#include "../../Source/eq/MuziEqEngine.h"
#include "../../Source/glue/MuziGlueEngine.h"
#include <cstdio>
#include <random>
#include <vector>
using namespace muzi;
std::mt19937 rng(12345);
double u() { return std::uniform_real_distribution<double>(0, 1)(rng); }
double pick() { double v = u(); return v < 0.1 ? 0.0 : v > 0.9 ? 1.0 : u(); }      // extremes often
double lg(double lo, double hi, double t) { return lo * std::pow(hi / lo, t); }
int main()
{
    const float ratios[] = { 1.5f, 2, 3, 4, 10 }, atts[] = { 0.1f, 0.3f, 1, 3, 10, 30 }, rels[] = { 100, 300, 600, 1200 };
    long nanEq = 0, nanGlue = 0;
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int bs : { 64, 128, 256, 512, 1024, 2048 })
        {
            MuziEqEngine eq; eq.prepare (fs); EqParams ep;
            MuziGlueEngine gl; gl.prepare (fs); GlueParams gp;
            std::vector<float> L (bs), R (bs), L2 (bs), R2 (bs);
            for (int rep = 0; rep < 200; ++rep)
                for (int s0 = 0; s0 < bs; s0 += 32)
                {
                    const int n = std::min (32, bs - s0);
                    for (int k = 0; k < 10; ++k)
                    {
                        auto& b = ep.band[(size_t) (rng() % 8)];
                        switch (rng() % 9)
                        {
                            case 0: b.on = pick() > 0.5; break;
                            case 1: b.type = (int) std::min (6.0, pick() * 7); break;
                            case 2: b.freq = (float) lg (20, 20000, pick()); break;
                            case 3: b.gainDb = (float) (-24 + 48 * pick()); break;
                            case 4: b.q = (float) lg (0.1, 18, pick()); break;
                            case 5: b.slope = (int) std::min (5.0, pick() * 6); break;
                            case 6: b.channel = (int) std::min (2.0, pick() * 3); break;
                            case 7: b.dynamic = pick() > 0.5; break;
                            default: b.thresholdDb = (float) (-60 + 60 * pick()); break;
                        }
                        switch (rng() % 8)
                        {
                            case 0: gp.thresholdDb = (float) (-40 + 40 * pick()); break;
                            case 1: gp.ratio = ratios[std::min (4, (int) (pick() * 5))]; break;
                            case 2: gp.kneeDb = (float) (12 * pick()); break;
                            case 3: gp.attackMs = atts[std::min (5, (int) (pick() * 6))]; break;
                            case 4: { int r = std::min (4, (int) (pick() * 5)); gp.autoRelease = r == 4; gp.releaseMs = r == 4 ? 300 : rels[r]; break; }
                            case 5: gp.scHpfHz = (float) lg (20, 500, pick()); break;
                            case 6: gp.makeupDb = (float) (-6 + 24 * pick()); break;
                            default: gp.mix = (float) pick(); break;
                        }
                    }
                    for (int i = 0; i < n; ++i) { L[i] = L2[i] = (float) (u() * 2 - 1); R[i] = R2[i] = (float) (u() * 2 - 1); }
                    eq.process (L.data(), R.data(), n, ep);
                    gl.process (L2.data(), R2.data(), n, gp);
                    for (int i = 0; i < n; ++i) { nanEq += ! std::isfinite (L[i]) || ! std::isfinite (R[i]); nanGlue += ! std::isfinite (L2[i]) || ! std::isfinite (R2[i]); }
                }
        }
    printf ("non-finite samples: EQ %ld, Glue %ld\n", nanEq, nanGlue);
}
