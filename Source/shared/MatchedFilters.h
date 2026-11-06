#pragma once
// Analog-matched digital filters.
//
// Poles come from the analog prototype by impulse invariance (exact pole mapping),
// and the numerator is solved so the digital magnitude equals the analog magnitude
// exactly at three frequencies: DC, Nyquist and a match point (normally the filter's
// own frequency). This is the "matched second-order" method of M. Vicanek (2016).
// Unlike the bilinear transform (RBJ cookbook), curves are not squeezed toward
// Nyquist: a bell at 16 kHz keeps the width and shape it has in the analog domain.
#include <cmath>
#include <algorithm>

namespace muzi::dsp
{
constexpr double kPi = 3.14159265358979323846;

struct Biquad      // y = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    // magnitude in dB at f (Hz)
    double magDb (double f, double fs) const
    {
        const double w = 2 * kPi * f / fs, c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return 10 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-300);
    }
};

// analog prototype: H(s) = (n2 s^2 + n1 s + n0) / (s^2 + (wp/qp) s + wp^2), all in rad/s
struct Analog2
{
    double n2, n1, n0, wp, qp;
    double mag2 (double W) const
    {
        const double nr = n0 - n2 * W * W, ni = n1 * W;
        const double dr = wp * wp - W * W, di = wp / qp * W;
        return (nr * nr + ni * ni) / (dr * dr + di * di);
    }
};

// Second-order matched design. wMatch: digital match frequency (rad/sample, 0 < wMatch < pi).
inline Biquad matched2 (const Analog2& h, double fs, double wMatch)
{
    Biquad q;
    const double T = 1.0 / fs;
    const double zeta = 1.0 / (2.0 * h.qp);
    const double e = std::exp (-zeta * h.wp * T);
    if (zeta <= 1.0) q.a1 = -2.0 * e * std::cos (std::sqrt (1.0 - zeta * zeta) * h.wp * T);
    else             q.a1 = -2.0 * e * std::cosh (std::sqrt (zeta * zeta - 1.0) * h.wp * T);
    q.a2 = e * e;

    const double A0 = (1 + q.a1 + q.a2) * (1 + q.a1 + q.a2);
    const double A1 = (1 - q.a1 + q.a2) * (1 - q.a1 + q.a2);
    const double A2 = -4.0 * q.a2;

    const double B0 = A0 * h.mag2 (0.0);
    const double B1 = A1 * h.mag2 (kPi * fs);
    const double s = std::sin (wMatch * 0.5), p1 = s * s, p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
    double B2 = (h.mag2 (wMatch * fs) * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;

    const double r0 = std::sqrt (std::max (0.0, B0)), r1 = std::sqrt (std::max (0.0, B1));
    const double W = 0.5 * (r0 + r1);
    B2 = std::max (B2, -W * W);                       // keep it realisable
    q.b0 = 0.5 * (W + std::sqrt (W * W + B2));
    q.b1 = 0.5 * (r0 - r1);
    q.b2 = q.b0 > 1e-30 ? -B2 / (4.0 * q.b0) : 0.0;
    return q;
}

// First-order matched design: H(s) = (n1 s + n0) / (s + wp); matched at DC and Nyquist.
inline Biquad matched1 (double n1, double n0, double wp, double fs)
{
    Biquad q;
    q.a1 = -std::exp (-wp / fs);
    auto mag = [&] (double W) { return std::sqrt ((n0 * n0 + n1 * n1 * W * W) / (wp * wp + W * W)); };
    const double sDC = mag (0.0) * (1 + q.a1), sNy = mag (kPi * fs) * (1 - q.a1);
    q.b0 = 0.5 * (sDC + sNy);
    q.b1 = 0.5 * (sDC - sNy);
    return q;
}

// Match point: the filter's own frequency, kept away from Nyquist where it would be ill-conditioned.
inline double matchPoint (double f, double fs) { return std::clamp (2 * kPi * f / fs, 1e-4, 0.82 * kPi); }

// ---------------------------------------------------------------- prototypes
// g = linear amplitude gain (10^(dB/20)); q = Q; w = 2 pi f
inline Biquad bell (double f, double gainDb, double q, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f;
    // RBJ analog peaking: boost and cut are exact mirror images
    return matched2 ({ 1.0, w * A / q, w * w, w, q * A }, fs, matchPoint (f, fs));
}
inline Biquad lowShelf (double f, double gainDb, double q, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f, sA = std::sqrt (A);
    return matched2 ({ 1.0, sA / q * w, A * w * w, w / sA, q }, fs, matchPoint (f, fs));
}
inline Biquad highShelf (double f, double gainDb, double q, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f, sA = std::sqrt (A);
    return matched2 ({ A * A, A * sA / q * w, A * w * w, w * sA, q }, fs, matchPoint (f, fs));
}
inline Biquad lowPass2 (double f, double q, double fs)
{
    const double w = 2 * kPi * f;
    return matched2 ({ 0.0, 0.0, w * w, w, q }, fs, matchPoint (f, fs));
}
inline Biquad highPass2 (double f, double q, double fs)
{
    const double w = 2 * kPi * f;
    return matched2 ({ 1.0, 0.0, 0.0, w, q }, fs, matchPoint (f, fs));
}
inline Biquad bandPass (double f, double q, double fs)          // unity gain at f
{
    const double w = 2 * kPi * f;
    return matched2 ({ 0.0, w / q, 0.0, w, q }, fs, matchPoint (f, fs));
}
inline Biquad notch (double f, double q, double fs)
{
    const double w = 2 * kPi * f;
    return matched2 ({ 1.0, 0.0, w * w, w, q }, fs, matchPoint (f, fs));
}
inline Biquad lowPass1 (double f, double fs)  { const double w = 2 * kPi * f; return matched1 (0.0, w, w, fs); }
inline Biquad highPass1 (double f, double fs) { const double w = 2 * kPi * f; return matched1 (1.0, 0.0, w, fs); }
// Tilt: -dB/2 at DC, +dB/2 at the top, 0 dB at the pivot f (first order, very gentle)
inline Biquad tilt (double f, double gainDb, double fs)
{
    const double K = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f;
    return matched1 (K, w, K * w, fs);
}

// analog reference magnitudes (dB) used by the tests
inline double analogBellDb (double fq, double f, double gainDb, double q)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f;
    return 10 * std::log10 (Analog2 { 1.0, w * A / q, w * w, w, q * A }.mag2 (2 * kPi * fq));
}

// Transposed direct form II, double precision. Coefficients may change between samples.
struct BiquadState
{
    double z1 = 0, z2 = 0;
    inline double process (const Biquad& c, double x)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};
} // namespace muzi::dsp
