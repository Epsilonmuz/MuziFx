#include "EqEditor.h"

using namespace muzi::ui;
using muzi::BandType;

namespace
{
constexpr float kBaseW = 1000.0f, kBaseH = 650.0f;
constexpr double fMin = 20.0, fMax = 20000.0;
constexpr float specRange = 90.0f, tiltPerOct = 4.5f;
const char* kTypeShort[] = { "Bell", "Low Shelf", "High Shelf", "Tilt", "Low Cut", "High Cut", "Notch" };
}

// ================================================================ EqDisplay
EqDisplay::EqDisplay (MuziEqProcessor& p) : proc (p) { setWantsKeyboardFocus (false); }

juce::Rectangle<float> EqDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedRight (34.0f * textScale).withTrimmedBottom (20.0f * textScale);
}
float EqDisplay::xForFreq (double f) const
{
    const auto p = plot();
    return p.getX() + (float) (std::log (f / fMin) / std::log (fMax / fMin)) * p.getWidth();
}
double EqDisplay::freqForX (float x) const
{
    const auto p = plot();
    return fMin * std::pow (fMax / fMin, (double) ((x - p.getX()) / p.getWidth()));
}
float EqDisplay::yForDb (float db) const
{
    const auto p = plot();
    const float top = p.getY() + 10.0f * textScale, bot = p.getBottom() - 10.0f * textScale;
    return top + (range - juce::jlimit (-range * 1.3f, range * 1.3f, db)) / (2.0f * range) * (bot - top);
}
float EqDisplay::dbForY (float y) const
{
    const auto p = plot();
    const float top = p.getY() + 10.0f * textScale, bot = p.getBottom() - 10.0f * textScale;
    return range - (y - top) / (bot - top) * 2.0f * range;
}
float EqDisplay::ySpec (float db) const
{
    const auto p = plot();
    return p.getBottom() - juce::jlimit (0.0f, 1.0f, (db + specRange) / specRange) * p.getHeight() * 0.9f;
}

void EqDisplay::resized()
{
    const auto n = (size_t) juce::jmax (8, (int) plot().getWidth());
    colIn.assign (n, -140.0f); colOut.assign (n, -140.0f); colMid.assign (n, 0.0f); colSide.assign (n, 0.0f);
    for (auto& c : colBand) c.assign (n, 0.0f);
    for (auto& c : colBandStatic) c.assign (n, 0.0f);
    colFreq.resize (n);
    const auto p = plot();
    for (size_t c = 0; c < n; ++c) colFreq[c] = freqForX (p.getX() + (float) c);
}

juce::RangedAudioParameter* EqDisplay::prm (int band, const char* what) const
{
    return proc.apvts.getParameter (MuziEqProcessor::bandId (band, what));
}
void EqDisplay::setPrm (int band, const char* what, float value)
{
    if (auto* p = prm (band, what)) p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqDisplay::computeSpectrum (std::vector<float>& ring, std::vector<float>& spec)
{
    for (int i = 0; i < kN; ++i) work[(size_t) i] = ring[(size_t) ((ringPos + i) % kN)];
    std::fill (work.begin() + kN, work.end(), 0.0f);
    window.multiplyWithWindowingTable (work.data(), (size_t) kN);
    fft.performFrequencyOnlyForwardTransform (work.data());
    for (int k = 0; k <= kN / 2; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (work[(size_t) k] * 4.0f / (float) kN, -140.0f);
        float& sv = spec[(size_t) k];
        sv = db > sv ? sv + 0.6f * (db - sv) : juce::jmax (db, sv - 1.1f);
    }
}

void EqDisplay::toColumns (const std::vector<float>& spec, std::vector<float>& cols)
{
    const double binHz = proc.getCurrentSampleRate() / kN;
    const int n = (int) cols.size();
    const auto p = plot();
    for (int c = 0; c < n; ++c)
    {
        const double f0 = colFreq[(size_t) c], f1 = freqForX (p.getX() + (float) c + 1.0f) > 0 ? freqForX (p.getX() + (float) c + 1.0f) : f0;
        const int k0 = (int) std::floor (f0 / binHz), k1 = (int) std::floor (f1 / binHz);
        float v;
        if (k1 > k0 + 1)
        {
            v = -140.0f;
            for (int k = k0 + 1; k <= juce::jmin (k1, kN / 2); ++k) v = juce::jmax (v, spec[(size_t) k]);
        }
        else
        {
            const double x = juce::jlimit (0.0, (double) (kN / 2 - 1), f0 / binHz);
            const int k = (int) x; const float fr = (float) (x - k);
            v = spec[(size_t) k] * (1.0f - fr) + spec[(size_t) k + 1] * fr;
        }
        cols[(size_t) c] = v + tiltPerOct * (float) std::log2 (f0 / 1000.0);
    }
    for (int pass = 0; pass < 2; ++pass)
        for (int c = 1; c < n - 1; ++c)
            cols[(size_t) c] = 0.25f * cols[(size_t) c - 1] + 0.5f * cols[(size_t) c] + 0.25f * cols[(size_t) c + 1];
}

void EqDisplay::update()
{
    const int nIn = proc.preTap.pull (scratch.data(), (int) scratch.size());
    int pos = ringPos;
    for (int i = juce::jmax (0, nIn - kN); i < nIn; ++i) { ringIn[(size_t) pos] = scratch[(size_t) i]; pos = (pos + 1) % kN; }
    const int nOut = proc.postTap.pull (scratch.data(), (int) scratch.size());
    int pos2 = ringPos;
    for (int i = juce::jmax (0, nOut - kN); i < nOut; ++i) { ringOut[(size_t) pos2] = scratch[(size_t) i]; pos2 = (pos2 + 1) % kN; }
    ringPos = pos;
    if (analyser > 0) { computeSpectrum (ringIn, specIn); toColumns (specIn, colIn); }
    if (analyser > 1) { computeSpectrum (ringOut, specOut); toColumns (specOut, colOut); }

    // curves, from the parameters (and the engine's live gain for dynamic bands)
    params = proc.readParams();
    const double fs = proc.getCurrentSampleRate();
    std::fill (colMid.begin(), colMid.end(), 0.0f);
    std::fill (colSide.begin(), colSide.end(), 0.0f);
    anySide = anyMidOnly = false;
    for (int b = 0; b < 8; ++b)
    {
        const auto& bp = params.band[(size_t) b];
        dynGain[(size_t) b] = proc.getEngine().meterGain[(size_t) b].load();
        const bool dyn = bp.on && bp.dynamic && muzi::typeCanBeDynamic (bp.type);
        muzi::dsp::Biquad cs[4], cd[4];
        const int ns = muzi::designBand (bp.type, bp.freq, muzi::typeHasGain (bp.type) ? bp.gainDb : 0.0, bp.q, bp.slope, params.proportionalQ, fs, cs);
        const int nd = dyn ? muzi::designBand (bp.type, bp.freq, dynGain[(size_t) b], bp.q, bp.slope, params.proportionalQ, fs, cd) : 0;
        for (size_t c = 0; c < colFreq.size(); ++c)
        {
            const double f = juce::jmin (colFreq[c], fs * 0.499);
            double v = 0;
            for (int k = 0; k < ns; ++k) v += cs[k].magDb (f, fs);
            colBandStatic[(size_t) b][c] = (float) juce::jmax (-60.0, v);
            if (dyn) { v = 0; for (int k = 0; k < nd; ++k) v += cd[k].magDb (f, fs); }
            colBand[(size_t) b][c] = (float) juce::jmax (-60.0, v);
        }
        if (! bp.on) continue;
        if (bp.channel == muzi::Side) anySide = true;
        if (bp.channel == muzi::Mid) anyMidOnly = true;
        for (size_t c = 0; c < colFreq.size(); ++c)
        {
            if (bp.channel != muzi::Side) colMid[c] += colBand[(size_t) b][c];
            if (bp.channel != muzi::Mid) colSide[c] += colBand[(size_t) b][c];
        }
    }
    repaint();
}

juce::Point<float> EqDisplay::nodePos (int b) const
{
    const auto& bp = params.band[(size_t) b];
    float db = 0.0f;
    if (bp.type == BandType::Tilt) db = bp.gainDb * 0.5f;
    else if (muzi::typeHasGain (bp.type)) db = bp.gainDb;
    return { xForFreq (bp.freq), yForDb (juce::jlimit (-range, range, db)) };
}

int EqDisplay::nodeAt (juce::Point<float> pt) const
{
    int best = -1; float bestD = 12.0f * textScale;
    for (int b = 0; b < 8; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (pt);
        if (d < bestD && (params.band[(size_t) b].on || b == selected)) { bestD = d; best = b; }
    }
    if (best < 0)                                 // off bands can be grabbed too, with a tighter radius
        for (int b = 0; b < 8; ++b)
            if (nodePos (b).getDistanceFrom (pt) < 8.0f * textScale) return b;
    return best;
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    hover = nodeAt (e.position);
    hoverX = plot().contains (e.position) ? (int) e.position.x : -1;
    hoverY = (int) e.position.y;
    setMouseCursor (hover >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}
void EqDisplay::mouseExit (const juce::MouseEvent&) { hover = -1; hoverX = -1; repaint(); }

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int b = nodeAt (e.position);
    if (b < 0) return;
    selected = b;
    if (onSelect) onSelect (b);
    if (e.mods.isPopupMenu()) { showMenu (b); return; }
    dragging = b;
    dragStart = e.position;
    dragF0 = params.band[(size_t) b].freq;
    dragG0 = params.band[(size_t) b].gainDb;
    prm (b, "freq")->beginChangeGesture();
    prm (b, "gain")->beginChangeGesture();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const int b = dragging;
    const auto p = plot();
    const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const auto d = (e.position - dragStart) * fine;
    if (! params.band[(size_t) b].on && d.getDistanceFromOrigin() > 2.0f) setPrm (b, "on", 1.0f);
    const double f = dragF0 * std::pow (fMax / fMin, (double) (d.x / p.getWidth()));
    setPrm (b, "freq", (float) juce::jlimit (fMin, fMax, f));
    const int type = params.band[(size_t) b].type;
    if (muzi::typeHasGain (type))
    {
        const float top = p.getY() + 10.0f * textScale, bot = p.getBottom() - 10.0f * textScale;
        float dDb = -d.y / (bot - top) * 2.0f * range;
        if (type == BandType::Tilt) dDb *= 2.0f;
        setPrm (b, "gain", juce::jlimit (-24.0f, 24.0f, dragG0 + dDb));
    }
    params = proc.readParams();
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0) { prm (dragging, "freq")->endChangeGesture(); prm (dragging, "gain")->endChangeGesture(); }
    dragging = -1;
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = nodeAt (e.position);
    if (b >= 0)
    {
        auto* p = prm (b, "on");
        p->beginChangeGesture(); p->setValueNotifyingHost (params.band[(size_t) b].on ? 0.0f : 1.0f); p->endChangeGesture();
        return;
    }
    if (! plot().contains (e.position)) return;
    for (int k = 0; k < 8; ++k)                   // add: first free band becomes a bell here
        if (! params.band[(size_t) k].on)
        {
            const double f = juce::jlimit (fMin, fMax, freqForX (e.position.x));
            setPrm (k, "type", (float) BandType::Bell);
            setPrm (k, "freq", (float) f);
            setPrm (k, "gain", juce::jlimit (-24.0f, 24.0f, dbForY (e.position.y)));
            setPrm (k, "q", 1.0f);
            setPrm (k, "on", 1.0f);
            selected = k;
            if (onSelect) onSelect (k);
            return;
        }
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int b = hover >= 0 ? hover : selected;
    auto* p = prm (b, "q");
    const float q = params.band[(size_t) b].q * std::exp ((std::abs (w.deltaY) > 0.0f ? w.deltaY : w.deltaX) * 1.2f);
    p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.1f, 18.0f, q))); p->endChangeGesture();
}

void EqDisplay::showMenu (int b)
{
    const auto& bp = params.band[(size_t) b];
    juce::PopupMenu m, types, slopes, chans;
    for (int t = 0; t < muzi::kNumTypes; ++t) types.addItem (100 + t, kTypeShort[t], true, bp.type == t);
    const char* slopeNames[] = { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
    for (int k = 0; k < 6; ++k) slopes.addItem (200 + k, slopeNames[k], true, bp.slope == k);
    const char* chanNames[] = { "Stereo", "Mid", "Side" };
    for (int k = 0; k < 3; ++k) chans.addItem (300 + k, chanNames[k], true, bp.channel == k);
    m.addSectionHeader ("Band " + juce::String (b + 1));
    m.addItem (1, bp.on ? "Turn off" : "Turn on");
    m.addSubMenu ("Type", types);
    if (bp.type == BandType::LowCut || bp.type == BandType::HighCut) m.addSubMenu ("Slope", slopes);
    m.addSubMenu ("Channel", chans);
    if (muzi::typeCanBeDynamic (bp.type)) m.addItem (2, "Dynamic", true, bp.dynamic);
    m.addSeparator();
    m.addItem (3, "Reset gain and Q");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, b] (int r)
    {
        auto gesture = [this, b] (const char* w, float v) { auto* p = prm (b, w); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); };
        const auto& cur = params.band[(size_t) b];
        if (r == 1) gesture ("on", cur.on ? 0.0f : 1.0f);
        else if (r == 2) gesture ("dyn", cur.dynamic ? 0.0f : 1.0f);
        else if (r == 3) { gesture ("gain", 0.0f); gesture ("q", cur.type == BandType::Bell ? 1.0f : 0.71f); }
        else if (r >= 100 && r < 100 + muzi::kNumTypes) gesture ("type", (float) (r - 100));
        else if (r >= 200 && r < 206) gesture ("slope", (float) (r - 200));
        else if (r >= 300 && r < 303) gesture ("chan", (float) (r - 300));
    });
}

juce::Path EqDisplay::curvePath (const std::vector<float>& cols, bool closeToZero) const
{
    juce::Path path;
    const auto p = plot();
    const float y0 = yForDb (0.0f);
    for (size_t c = 0; c < cols.size(); ++c)
    {
        const float x = p.getX() + (float) c, y = yForDb (cols[c]);
        if (c == 0) { if (closeToZero) { path.startNewSubPath (x, y0); path.lineTo (x, y); } else path.startNewSubPath (x, y); }
        else path.lineTo (x, y);
    }
    if (closeToZero) { path.lineTo (p.getX() + (float) cols.size() - 1.0f, y0); path.closeSubPath(); }
    return path;
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto p = plot();
    const float ts = textScale;
    juce::ColourGradient bg (col::bg1.brighter (0.04f), 0, b.getY(), col::bg0, 0, b.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (b, 10.0f * ts);

    juce::Graphics::ScopedSaveState clip (g);
    juce::Path rr; rr.addRoundedRectangle (b, 10.0f * ts);
    g.reduceClipRegion (rr);

    // grid
    g.setFont (font (10.0f * ts));
    for (double f : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForFreq (f);
        g.setColour (col::line.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), p.getY(), p.getBottom());
        g.setColour (col::faint);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 20 * ts, p.getBottom() + 3 * ts, 40 * ts, 14 * ts), juce::Justification::centred);
    }
    const float step = range > 13.0f ? 6.0f : 3.0f;
    for (float db = -range; db <= range + 0.01f; db += step)
    {
        const float y = yForDb (db);
        g.setColour (std::abs (db) < 0.01f ? col::faint.withAlpha (0.55f) : col::line.withAlpha (0.45f));
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (col::faint);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (p.getRight() + 3 * ts, y - 7 * ts, 30 * ts, 14 * ts), juce::Justification::centredLeft);
    }

    // analyser
    if (analyser > 0 && ! colIn.empty())
    {
        juce::Path pin;
        pin.startNewSubPath (p.getX(), p.getBottom());
        for (size_t c = 0; c < colIn.size(); ++c) pin.lineTo (p.getX() + (float) c, ySpec (colIn[c]));
        pin.lineTo (p.getRight(), p.getBottom()); pin.closeSubPath();
        g.setColour (col::quiet.withAlpha (0.13f));
        g.fillPath (pin);
        if (analyser > 1)
        {
            juce::Path pout;
            for (size_t c = 0; c < colOut.size(); ++c)
                if (c == 0) pout.startNewSubPath (p.getX(), ySpec (colOut[c])); else pout.lineTo (p.getX() + (float) c, ySpec (colOut[c]));
            g.setColour (col::accent.withAlpha (0.35f));
            g.strokePath (pout, juce::PathStrokeType (1.0f));
        }
    }

    // band fills
    for (int k = 0; k < 8; ++k)
    {
        const auto& bp = params.band[(size_t) k];
        if (! bp.on) continue;
        const auto c = bandColours[k];
        g.setColour (c.withAlpha (k == selected ? 0.20f : 0.09f));
        g.fillPath (curvePath (colBand[(size_t) k], true));
        if (bp.dynamic && muzi::typeCanBeDynamic (bp.type))       // dashed: the most the band may move
        {
            juce::Path dashed; const float d[] = { 4.0f * ts, 4.0f * ts };
            juce::PathStrokeType (1.2f).createDashedStroke (dashed, curvePath (colBandStatic[(size_t) k], false), d, 2);
            g.setColour (c.withAlpha (0.8f));
            g.fillPath (dashed);
        }
        if (k == selected)
        {
            g.setColour (c.withAlpha (0.8f));
            g.strokePath (curvePath (colBand[(size_t) k], false), juce::PathStrokeType (1.2f));
        }
    }
    // selected band when off: show where it would act
    if (! params.band[(size_t) selected].on)
    {
        juce::Path dashed; const float d[] = { 3.0f * ts, 5.0f * ts };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, curvePath (colBandStatic[(size_t) selected], false), d, 2);
        g.setColour (bandColours[selected].withAlpha (0.45f));
        g.fillPath (dashed);
    }

    // totals
    if (anySide)
    {
        g.setColour (col::amber.withAlpha (0.9f));
        g.strokePath (curvePath (colSide, false), juce::PathStrokeType (2.0f * ts, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    {
        const auto path = curvePath (colMid, false);
        g.setColour (col::text.withAlpha (0.12f));
        g.strokePath (path, juce::PathStrokeType (6.0f * ts, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::text);
        g.strokePath (path, juce::PathStrokeType (2.2f * ts, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (anySide || anyMidOnly)
    {
        g.setFont (font (10.5f * ts, true, 0.08f));
        g.setColour (col::text); g.drawText ("MID", juce::Rectangle<float> (p.getX() + 10 * ts, p.getY() + 8 * ts, 40 * ts, 14 * ts), juce::Justification::left);
        g.setColour (col::amber); g.drawText ("SIDE", juce::Rectangle<float> (p.getX() + 44 * ts, p.getY() + 8 * ts, 40 * ts, 14 * ts), juce::Justification::left);
    }

    // nodes
    for (int k = 0; k < 8; ++k)
    {
        const auto& bp = params.band[(size_t) k];
        const auto pos = nodePos (k);
        const auto c = bandColours[k];
        const float r = (k == selected ? 9.5f : 8.0f) * ts;
        if (! bp.on && k != selected && k != hover) { g.setColour (c.withAlpha (0.25f)); g.drawEllipse (pos.x - r * 0.6f, pos.y - r * 0.6f, r * 1.2f, r * 1.2f, 1.0f); continue; }
        if (bp.on && bp.dynamic && muzi::typeCanBeDynamic (bp.type))   // live position of a dynamic band
        {
            const float yd = yForDb (dynGain[(size_t) k]);
            g.setColour (c.withAlpha (0.5f));
            g.drawLine (pos.x, yForDb (0.0f), pos.x, pos.y, 1.0f);
            g.fillEllipse (pos.x - 3 * ts, yd - 3 * ts, 6 * ts, 6 * ts);
        }
        if (k == selected || k == hover) { g.setColour (c.withAlpha (0.25f)); g.fillEllipse (pos.x - r * 1.7f, pos.y - r * 1.7f, r * 3.4f, r * 3.4f); }
        g.setColour (bp.on ? c : col::bg2);
        g.fillEllipse (pos.x - r, pos.y - r, r * 2, r * 2);
        g.setColour (bp.on ? juce::Colours::black.withAlpha (0.35f) : c);
        g.drawEllipse (pos.x - r, pos.y - r, r * 2, r * 2, 1.2f);
        g.setColour (bp.on ? col::bg0 : c);
        g.setFont (font (10.5f * ts, true));
        g.drawText (juce::String (k + 1), juce::Rectangle<float> (pos.x - r, pos.y - r, r * 2, r * 2), juce::Justification::centred);
    }

    // readout
    juce::String txt;
    juce::Point<float> at;
    const int show = dragging >= 0 ? dragging : hover;
    if (show >= 0)
    {
        const auto& bp = params.band[(size_t) show];
        txt = juce::String (show + 1) + "  " + kTypeShort[bp.type] + "   " + freqText (bp.freq);
        if (muzi::typeHasGain (bp.type)) txt += "   " + juce::String (bp.gainDb > 0 ? "+" : "") + juce::String (bp.gainDb, 1) + " dB";
        if (bp.type != BandType::Tilt) txt += "   Q " + juce::String (bp.q, 2);
        if (bp.channel != muzi::Stereo) txt += bp.channel == muzi::Mid ? "   MID" : "   SIDE";
        at = nodePos (show);
    }
    else if (hoverX >= 0)
    {
        txt = freqText (freqForX ((float) hoverX)) + "   " + juce::String (dbForY ((float) hoverY), 1) + " dB";
        at = { (float) hoverX, (float) hoverY };
    }
    if (txt.isNotEmpty())
    {
        const auto f = font (12.0f * ts, true);
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (f, txt) + 16.0f * ts, h = 22.0f * ts;
        float x = at.x + 14 * ts, y = at.y - h - 12 * ts;
        if (x + w > p.getRight()) x = at.x - w - 14 * ts;
        if (y < p.getY()) y = at.y + 14 * ts;
        g.setColour (col::bg2.withAlpha (0.94f));
        g.fillRoundedRectangle (x, y, w, h, 5 * ts);
        g.setColour (col::line);
        g.drawRoundedRectangle (x, y, w, h, 5 * ts, 1.0f);
        g.setColour (col::text); g.setFont (f);
        g.drawText (txt, juce::Rectangle<float> (x, y, w, h), juce::Justification::centred);
    }
}

// ================================================================ band chip
void BandChip::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = bandColours[index];
    const bool sel = getToggleState();
    g.setColour (sel ? c.withAlpha (0.16f) : col::bg2.brighter (over ? 0.06f : 0.0f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (sel ? c.withAlpha (0.8f) : col::line);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    const float d = r.getHeight() * 0.36f;
    const auto dot = juce::Rectangle<float> (r.getX() + r.getHeight() * 0.35f, r.getCentreY() - d * 0.5f, d, d);
    if (bandOn) { g.setColour (c); g.fillEllipse (dot); }
    else { g.setColour (c.withAlpha (0.6f)); g.drawEllipse (dot, 1.0f); }
    g.setColour (bandOn ? col::text : col::faint);
    g.setFont (font (10.5f * textScale, true, 0.04f));
    g.drawText (juce::String (index + 1) + "  " + typeName.toUpperCase() + (dynamic ? "  D" : ""),
                r.withTrimmedLeft (dot.getRight() - r.getX() + 4.0f * textScale), juce::Justification::centredLeft);
}

// ================================================================ editor
MuziEqEditor::MuziEqEditor (MuziEqProcessor& p)
    : AudioProcessorEditor (p), proc (p), display (p),
      freq (p.apvts, MuziEqProcessor::bandId (4, "freq"), "Frequency", "Centre or corner frequency. Drag the node in the display, or this knob."),
      gain (p.apvts, MuziEqProcessor::bandId (4, "gain"), "Gain", "Boost or cut. For a dynamic band: the most it will move.", true),
      q (p.apvts, MuziEqProcessor::bandId (4, "q"), "Q", "Width. Bells: higher = narrower. Shelves: above 0.71 adds the classic bump and dip. Cuts: resonance at the corner. Scroll on a node to change it."),
      thr (p.apvts, MuziEqProcessor::bandId (4, "thr"), "Threshold", "Dynamic bands only: the band starts moving when its own level passes this."),
      drive (p.apvts, "drive", "Drive", "How hard the Color stage is pushed. Quiet parts keep their level; peaks are rounded."),
      output (p.apvts, "output", "Output", "Output level.", true)
{
    setLookAndFeel (&lnf);
    rangeBtn.setButtonText (juce::CharPointer_UTF8 ("\xc2\xb1" "12 dB"));
    addAndMakeVisible (display);
    display.onSelect = [this] (int b) { selectBand (b); };

    for (int i = 0; i < 8; ++i)
    {
        chips[(size_t) i].index = i;
        chips[(size_t) i].onClick = [this, i] { selectBand (i); };
        addAndMakeVisible (chips[(size_t) i]);
    }
    for (auto* k : { &freq, &gain, &q, &thr, &drive, &output }) addAndMakeVisible (*k);

    typeBox.addItemList ({ "Bell", "Low Shelf", "High Shelf", "Tilt", "Low Cut", "High Cut", "Notch" }, 1);
    slopeBox.addItemList ({ "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    charBox.addItemList ({ "Tape", "Tube" }, 1);
    typeBox.setTooltip ("Filter shape of the selected band");
    slopeBox.setTooltip ("Steepness of the cut");
    charBox.setTooltip ("Tape: rounds peaks, odd harmonics. Tube: warmer, adds even harmonics.");
    for (auto* c : { &typeBox, &slopeBox, &charBox, &presetBox }) addAndMakeVisible (*c);

    for (int k = 0; k < 3; ++k)
    {
        chanBtn[k].getProperties().set ("edges", k == 0 ? 1 : k == 2 ? 2 : 3);
        chanBtn[k].setTooltip ("Process both sides together (Stereo), only the centre (Mid), or only the sides (Side)");
        chanBtn[k].onClick = [this, k] { if (chanAtt) chanAtt->setValueAsCompleteGesture ((float) k); };
        addAndMakeVisible (chanBtn[k]);
    }
    onBtn.setTooltip ("Switch the selected band on or off (double-click its node)");
    dynBtn.setTooltip ("Dynamic: the band only moves when that part of the spectrum gets louder than Threshold");
    for (auto* b : { &onBtn, &dynBtn, &colorBtn, &propQBtn, &rangeBtn, &anaBtn }) addAndMakeVisible (*b);
    colorAtt = bindToggle (p.apvts, "color", colorBtn, "Color: analog-style saturation after the EQ (adds a few samples of latency)");
    propQAtt = bindToggle (p.apvts, "propq", propQBtn, "Proportional Q: small boosts and cuts are wider, big ones narrower, like classic console EQs");
    rangeBtn.setTooltip ("Display range");
    rangeBtn.onClick = [this] { display.range = display.range > 13.0f ? 12.0f : 24.0f; rangeBtn.setButtonText (juce::CharPointer_UTF8 (display.range > 13.0f ? "\xc2\xb1" "24 dB" : "\xc2\xb1" "12 dB")); display.repaint(); };
    anaBtn.setTooltip ("Analyser: off, input, or input and output");
    anaBtn.setToggleState (true, juce::dontSendNotification);
    anaBtn.onClick = [this] { display.analyser = (display.analyser + 1) % 3; anaBtn.setToggleState (display.analyser > 0, juce::dontSendNotification);
                              anaBtn.setButtonText (display.analyser == 0 ? "ANALYSER" : display.analyser == 1 ? "ANALYSER IN" : "ANALYSER"); };
    charAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "character", charBox);

    addAndMakeVisible (bypassBtn);
    bypassBtn.setTooltip ("Bypass");
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassBtn);
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);

    for (auto* b : { &prevPreset, &nextPreset }) addAndMakeVisible (*b);
    refreshPresetBox();
    presetBox.onChange = [this] { if (presetBox.getSelectedItemIndex() >= 0) proc.loadFactoryPreset (presetBox.getSelectedItemIndex()); };
    prevPreset.onClick = [this] { proc.loadFactoryPreset ((proc.currentPreset + proc.getNumPrograms() - 1) % proc.getNumPrograms()); refreshPresetBox(); };
    nextPreset.onClick = [this] { proc.loadFactoryPreset ((proc.currentPreset + 1) % proc.getNumPrograms()); refreshPresetBox(); };

    // start on the first active band, if any
    const auto prm = proc.readParams();
    int first = 4;
    for (int b = 0; b < 8; ++b) if (prm.band[(size_t) b].on) { first = b; break; }
    selectBand (first);

    setResizable (true, true);
    setResizeLimits (760, 494, 1600, 1040);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (kBaseW / kBaseH);
    setSize ((int) kBaseW, (int) kBaseH);
    startTimerHz (30);
}

MuziEqEditor::~MuziEqEditor() { setLookAndFeel (nullptr); }

void MuziEqEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.currentPreset, juce::dontSendNotification);
}

void MuziEqEditor::selectBand (int b)
{
    band = b;
    display.selected = b;
    auto& st = proc.apvts;
    freq.attach (st, MuziEqProcessor::bandId (b, "freq"));
    gain.attach (st, MuziEqProcessor::bandId (b, "gain"));
    q.attach (st, MuziEqProcessor::bandId (b, "q"));
    thr.attach (st, MuziEqProcessor::bandId (b, "thr"));
    onAtt.reset(); dynAtt.reset(); typeAtt.reset(); slopeAtt.reset(); chanAtt.reset();
    onAtt = bindToggle (st, MuziEqProcessor::bandId (b, "on"), onBtn, onBtn.getTooltip());
    dynAtt = bindToggle (st, MuziEqProcessor::bandId (b, "dyn"), dynBtn, dynBtn.getTooltip());
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, MuziEqProcessor::bandId (b, "type"), typeBox);
    slopeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, MuziEqProcessor::bandId (b, "slope"), slopeBox);
    chanAtt = std::make_unique<juce::ParameterAttachment> (*st.getParameter (MuziEqProcessor::bandId (b, "chan")), [this] (float v)
    {
        for (int k = 0; k < 3; ++k) chanBtn[k].setToggleState (juce::roundToInt (v) == k, juce::dontSendNotification);
    });
    chanAtt->sendInitialUpdate();
    for (int i = 0; i < 8; ++i) chips[(size_t) i].setToggleState (i == b, juce::dontSendNotification);
    refreshBandControls();
    repaint();
}

void MuziEqEditor::refreshBandControls()
{
    const auto prm = proc.readParams();
    const auto& bp = prm.band[(size_t) band];
    const bool cut = bp.type == BandType::LowCut || bp.type == BandType::HighCut;
    gain.setEnabled (muzi::typeHasGain (bp.type));
    q.setEnabled (bp.type != BandType::Tilt && ! (cut && bp.slope == 0));
    slopeBox.setVisible (cut);
    dynBtn.setEnabled (muzi::typeCanBeDynamic (bp.type));
    thr.setEnabled (bp.dynamic && muzi::typeCanBeDynamic (bp.type));
    gain.setName2 (bp.dynamic && muzi::typeCanBeDynamic (bp.type) ? "Max Gain" : "Gain");
    drive.setEnabled (proc.apvts.getRawParameterValue ("color")->load() > 0.5f);
    charBox.setEnabled (drive.isEnabled());
    for (int i = 0; i < 8; ++i)
    {
        auto& c = chips[(size_t) i];
        const auto& x = prm.band[(size_t) i];
        const juce::String t = juce::StringArray { "Bell", "Lo Shelf", "Hi Shelf", "Tilt", "Lo Cut", "Hi Cut", "Notch" }[x.type];
        const bool dyn = x.dynamic && muzi::typeCanBeDynamic (x.type);
        if (c.bandOn != x.on || c.typeName != t || c.dynamic != dyn) { c.bandOn = x.on; c.typeName = t; c.dynamic = dyn; c.repaint(); }
    }
}

void MuziEqEditor::timerCallback()
{
    display.update();
    inMeter.tick (proc.inPeak.take());
    outMeter.tick (proc.outPeak.take());
    refreshBandControls();
    if (presetBox.getSelectedItemIndex() != proc.currentPreset) presetBox.setSelectedItemIndex (proc.currentPreset, juce::dontSendNotification);
}

void MuziEqEditor::refreshForSnapshot()
{
    for (int i = 0; i < 40; ++i) display.update();
    for (int i = 0; i < 6; ++i) { inMeter.tick (0.5f); outMeter.tick (0.45f); }
    refreshBandControls();
}

juce::Rectangle<int> MuziEqEditor::sc (float x, float y, float w, float h) const
{
    return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
}

void MuziEqEditor::resized()
{
    s = (float) getWidth() / kBaseW;
    lnf.scale = s;
    display.textScale = s;
    for (auto* k : { &freq, &gain, &q, &thr, &drive, &output }) k->textScale = s;
    inMeter.textScale = outMeter.textScale = s;

    header = sc (0, 0, kBaseW, 56);
    presetBox.setBounds (sc (370, 14, 260, 28));
    prevPreset.setBounds (sc (334, 14, 30, 28));
    nextPreset.setBounds (sc (636, 14, 30, 28));
    anaBtn.setBounds (sc (690, 16, 100, 24));
    rangeBtn.setBounds (sc (796, 16, 70, 24));
    propQBtn.setBounds (sc (872, 16, 64, 24));
    bypassBtn.setBounds (sc (950, 14, 30, 30));

    display.setBounds (sc (16, 60, 968, 346));
    for (int i = 0; i < 8; ++i) { chips[(size_t) i].textScale = s; chips[(size_t) i].setBounds (sc (16.0f + (float) i * 92.0f, 416, 88, 26)); }

    stripBand = sc (16, 452, 676, 182);
    stripOut = sc (702, 452, 282, 182);
    onBtn.setBounds (sc (30, 486, 52, 26));
    typeBox.setBounds (sc (90, 486, 130, 26));
    slopeBox.setBounds (sc (228, 486, 110, 26));
    for (int k = 0; k < 3; ++k) chanBtn[k].setBounds (sc (350.0f + (float) k * 66.0f, 486, 66, 26));
    dynBtn.setBounds (sc (560, 486, 118, 26));
    const float ky = 522, kh = 104, kw = 128;
    freq.setBounds (sc (40, ky, kw, kh));
    gain.setBounds (sc (190, ky, kw, kh));
    q.setBounds (sc (340, ky, kw, kh));
    thr.setBounds (sc (550, ky, kw, kh));

    colorBtn.setBounds (sc (716, 486, 72, 26));
    charBox.setBounds (sc (794, 486, 84, 26));
    drive.setBounds (sc (712, ky, 92, kh));
    output.setBounds (sc (808, ky, 92, kh));
    inMeter.setBounds (sc (912, 486, 30, 140));
    outMeter.setBounds (sc (946, 486, 30, 140));
}

void MuziEqEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg0);
    // header
    g.setColour (col::accent);
    g.setFont (font (22.0f * s, true, 0.14f));
    g.drawText ("MUZI", sc (20, 12, 80, 32), juce::Justification::centredLeft);
    g.setColour (col::text);
    g.drawText ("EQ", sc (96, 12, 60, 32), juce::Justification::centredLeft);
    g.setColour (col::faint);
    g.setFont (font (10.0f * s, true, 0.1f));
    g.drawText ("ANALOG-MATCHED", sc (136, 18, 140, 20), juce::Justification::centredLeft);


    for (auto r : { stripBand, stripOut })
    {
        g.setColour (col::bg1);
        g.fillRoundedRectangle (r.toFloat(), 10.0f * s);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f * s, 1.0f);
    }
    const auto prm = proc.readParams();
    const auto& bp = prm.band[(size_t) band];
    g.setFont (font (11.0f * s, true, 0.14f));
    g.setColour (bandColours[band]);
    g.drawText ("BAND " + juce::String (band + 1), sc (30, 458, 80, 22), juce::Justification::centredLeft);
    g.setColour (col::quiet);
    g.drawText (bp.on ? "" : "OFF", sc (100, 458, 60, 22), juce::Justification::centredLeft);
    g.setColour (col::faint);
    g.setFont (font (10.5f * s));
    g.drawText (juce::CharPointer_UTF8 ("Double-click the display to add a band \xc2\xb7 drag nodes (Shift = fine) \xc2\xb7 scroll for Q \xc2\xb7 right-click for options"),
                sc (160, 458, 518, 22), juce::Justification::centredRight);
    g.setFont (font (11.0f * s, true, 0.14f));
    g.setColour (col::quiet);
    g.drawText ("OUTPUT", sc (716, 458, 120, 22), juce::Justification::centredLeft);
}
