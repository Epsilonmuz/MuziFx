#include "GlueEditor.h"

using namespace muzi::ui;

namespace
{
constexpr float kBaseW = 960.0f, kBaseH = 560.0f;
constexpr float grMax = 20.0f;
float grPos (float gr) { return std::sqrt (juce::jlimit (0.0f, grMax, gr) / grMax); }   // expands the first few dB

void panelBg (juce::Graphics& g, juce::Rectangle<float> r, float s)
{
    g.setColour (col::bg1);
    g.fillRoundedRectangle (r, 10.0f * s);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f * s, 1.0f);
}
}

// ================================================================ needle
void GrNeedle::push (float grDb)
{
    shown = grDb > shown ? shown + 0.7f * (grDb - shown) : juce::jmax (grDb, shown - 0.35f);   // fast in, ~300 ms visual fall
    if (grDb >= hold) { hold = grDb; holdTicks = 60; }
    else if (--holdTicks <= 0) hold = juce::jmax (0.0f, hold - 0.25f);
    repaint();
}

void GrNeedle::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float ts = textScale;
    const juce::Point<float> c (b.getCentreX(), b.getBottom() - 40.0f * ts);
    const float R = juce::jmin (b.getWidth() * 0.46f, (b.getHeight() - 52.0f * ts));
    const float a0 = 0.95f, a1 = -0.95f;                 // radians from vertical: rest (0 dB) right, full scale left
    auto angleFor = [&] (float gr) { return a0 + (a1 - a0) * grPos (gr); };

    // scale arc
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, R, R, 0.0f, a1, a0, true);
    g.setColour (col::line);
    g.strokePath (arc, juce::PathStrokeType (2.0f * ts));
    juce::Path hot;
    hot.addCentredArc (c.x, c.y, R, R, 0.0f, angleFor (grMax), angleFor (8.0f), true);
    g.setColour (col::red.withAlpha (0.5f));
    g.strokePath (hot, juce::PathStrokeType (3.0f * ts));

    g.setFont (font (10.5f * ts, true));
    for (float t : { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 10.0f, 14.0f, 20.0f })
    {
        const float a = angleFor (t);
        const auto p0 = c.getPointOnCircumference (R - 7.0f * ts, a), p1 = c.getPointOnCircumference (R + 2.0f * ts, a);
        g.setColour (col::quiet);
        g.drawLine ({ p0, p1 }, 1.5f * ts);
        const auto pt = c.getPointOnCircumference (R + 14.0f * ts, a);
        g.setColour (t >= 8.0f ? col::red.withAlpha (0.8f) : col::quiet);
        g.drawText (juce::String ((int) t), juce::Rectangle<float> (pt.x - 14 * ts, pt.y - 8 * ts, 28 * ts, 16 * ts), juce::Justification::centred);
    }
    // peak hold tick
    if (hold > 0.2f)
    {
        const float a = angleFor (hold);
        g.setColour (col::amber);
        g.drawLine ({ c.getPointOnCircumference (R - 10.0f * ts, a), c.getPointOnCircumference (R + 2.0f * ts, a) }, 2.5f * ts);
    }
    // needle
    const float a = angleFor (shown);
    const auto tip = c.getPointOnCircumference (R - 4.0f * ts, a);
    g.setColour (col::amber.withAlpha (0.18f));
    g.drawLine ({ c, tip }, 7.0f * ts);
    g.setColour (col::amber);
    g.drawLine ({ c, tip }, 2.2f * ts);
    g.setColour (col::bg2.brighter (0.2f));
    g.fillEllipse (c.x - 7 * ts, c.y - 7 * ts, 14 * ts, 14 * ts);
    g.setColour (col::line);
    g.drawEllipse (c.x - 7 * ts, c.y - 7 * ts, 14 * ts, 14 * ts, 1.0f);

    g.setColour (col::text);
    g.setFont (font (22.0f * ts, true));
    g.drawText ((shown > 0.05f ? "-" : "") + juce::String (shown, 1) + " dB", juce::Rectangle<float> (b.getX(), c.y + 6 * ts, b.getWidth(), 28 * ts), juce::Justification::centred);
}

// ================================================================ history
void GlueHistory::push (float inDb, float outDb, float grDb)
{
    in[(size_t) pos] = inDb; out[(size_t) pos] = outDb; gr[(size_t) pos] = grDb;
    pos = (pos + 1) % kLen;
    repaint();
}

void GlueHistory::paint (juce::Graphics& g)
{
    // IN (grey) and OUT (teal) as filled level traces; the grey sliver above the teal is what was taken off.
    // GR is the amber line hanging from the top on its own 0-12 dB scale (left labels).
    const auto r = getLocalBounds().toFloat();
    const float ts = textScale;
    const float lo = -48.0f, grScale = 12.0f;
    auto yLvl = [&] (float db) { return r.getBottom() - juce::jlimit (0.0f, 1.0f, (db - lo) / -lo) * r.getHeight() * 0.92f; };
    auto yGr = [&] (float d) { return r.getY() + juce::jlimit (0.0f, 1.0f, d / grScale) * r.getHeight(); };
    auto x = [&] (int i) { return r.getX() + (float) i / (float) (kLen - 1) * r.getWidth(); };

    g.setFont (font (9.5f * ts));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
    {
        g.setColour (col::line.withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (yLvl (db)), r.getX(), r.getRight());
        g.setColour (col::faint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getRight() - 30 * ts, yLvl (db) + 1 * ts, 28 * ts, 12 * ts), juce::Justification::right);
    }
    for (float d : { 3.0f, 6.0f, 9.0f })
    {
        g.setColour (col::amber.withAlpha (0.55f));
        g.drawText ("-" + juce::String ((int) d), juce::Rectangle<float> (r.getX() + 2 * ts, yGr (d) - 6 * ts, 24 * ts, 12 * ts), juce::Justification::left);
    }

    auto fillTrace = [&] (const std::array<float, kLen>& v, juce::Colour c)
    {
        juce::Path p;
        p.startNewSubPath (x (0), r.getBottom());
        for (int i = 0; i < kLen; ++i) p.lineTo (x (i), yLvl (v[(size_t) ((pos + i) % kLen)]));
        p.lineTo (x (kLen - 1), r.getBottom()); p.closeSubPath();
        g.setColour (c);
        g.fillPath (p);
    };
    fillTrace (in, col::quiet.withAlpha (0.22f));
    fillTrace (out, col::accent.withAlpha (0.10f));
    {
        juce::Path o;
        for (int i = 0; i < kLen; ++i)
        {
            const float y = yLvl (out[(size_t) ((pos + i) % kLen)]);
            if (i == 0) o.startNewSubPath (x (i), y); else o.lineTo (x (i), y);
        }
        g.setColour (col::accent.withAlpha (0.9f));
        g.strokePath (o, juce::PathStrokeType (1.2f * ts));
    }

    juce::Path grLine, grFill;
    grFill.startNewSubPath (x (0), r.getY());
    for (int i = 0; i < kLen; ++i)
    {
        const float y = yGr (gr[(size_t) ((pos + i) % kLen)]);
        if (i == 0) grLine.startNewSubPath (x (i), y); else grLine.lineTo (x (i), y);
        grFill.lineTo (x (i), y);
    }
    grFill.lineTo (x (kLen - 1), r.getY()); grFill.closeSubPath();
    g.setColour (col::amber.withAlpha (0.12f));
    g.fillPath (grFill);
    g.setColour (col::amber);
    g.strokePath (grLine, juce::PathStrokeType (1.8f * ts));

    g.setFont (font (9.5f * ts, true, 0.1f));
    g.setColour (col::quiet);  g.drawText ("IN",  juce::Rectangle<float> (r.getX() + 6 * ts, r.getBottom() - 16 * ts, 30 * ts, 12 * ts), juce::Justification::left);
    g.setColour (col::accent); g.drawText ("OUT", juce::Rectangle<float> (r.getX() + 30 * ts, r.getBottom() - 16 * ts, 30 * ts, 12 * ts), juce::Justification::left);
    g.setColour (col::amber);  g.drawText ("GR",  juce::Rectangle<float> (r.getX() + 62 * ts, r.getBottom() - 16 * ts, 30 * ts, 12 * ts), juce::Justification::left);
}

// ================================================================ transfer curve
void TransferCurve::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float ts = textScale;
    const float lo = -48.0f;
    auto px = [&] (float db) { return r.getX() + (db - lo) / -lo * r.getWidth(); };
    auto py = [&] (float db) { return r.getBottom() - juce::jlimit (0.0f, 1.0f, (db - lo) / -lo) * r.getHeight(); };
    g.setColour (col::bg0);
    g.fillRect (r);
    for (float db : { -12.0f, -24.0f, -36.0f })
    {
        g.setColour (col::line.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (px (db)), r.getY(), r.getBottom());
        g.drawHorizontalLine (juce::roundToInt (py (db)), r.getX(), r.getRight());
    }
    const auto p = proc.readParams();
    g.setColour (col::faint);
    g.drawLine (px (lo), py (lo), px (0), py (0), 1.0f);
    juce::Path curve;
    for (int i = 0; i <= 96; ++i)
    {
        const float xin = lo + (float) i * 0.5f;
        const float yout = xin - (float) muzi::MuziGlueEngine::reductionDb (xin, p.thresholdDb, p.ratio, p.kneeDb);
        if (i == 0) curve.startNewSubPath (px (xin), py (yout)); else curve.lineTo (px (xin), py (yout));
    }
    g.setColour (col::accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f * ts));
    g.setColour (col::amber.withAlpha (0.6f));
    g.drawVerticalLine (juce::roundToInt (px (p.thresholdDb)), r.getY(), r.getBottom());
    if (det > lo)
    {
        const float yo = det - (float) muzi::MuziGlueEngine::reductionDb (det, p.thresholdDb, p.ratio, p.kneeDb);
        g.setColour (col::amber);
        g.fillEllipse (px (det) - 4 * ts, py (yo) - 4 * ts, 8 * ts, 8 * ts);
    }
    g.setColour (col::faint);
    g.setFont (font (9.5f * ts));
    g.drawText ("in", juce::Rectangle<float> (r.getRight() - 24 * ts, r.getBottom() - 14 * ts, 20 * ts, 12 * ts), juce::Justification::right);
    g.drawText ("out", juce::Rectangle<float> (r.getX() + 4 * ts, r.getY() + 2 * ts, 30 * ts, 12 * ts), juce::Justification::left);
}

// ================================================================ editor
MuziGlueEditor::MuziGlueEditor (MuziGlueProcessor& p)
    : AudioProcessorEditor (p), proc (p), transfer (p),
      threshold (p.apvts, "threshold", "Threshold", "Level where compression starts. Lower it until the needle shows the gain reduction you want (1-4 dB glues a mix)."),
      ratio (p.apvts, "ratio", "Ratio", "How strongly levels above the threshold are turned down. 1.5:1 and 2:1 glue; 4:1 controls; 10:1 limits."),
      attack (p.apvts, "attack", "Attack", "How fast compression grabs. Slower attack (10-30 ms) lets drum hits through for punch."),
      release (p.apvts, "release", "Release", "How fast it lets go. Auto recovers quickly after single hits and slowly after sustained loud parts, which avoids pumping."),
      knee (p.apvts, "knee", "Knee", "Softens the start of compression. Higher = gentler, more transparent."),
      schpf (p.apvts, "schpf", "SC HPF", "Sidechain high-pass: stops the kick and 808 from driving the compressor. 60-120 Hz on a mix bus."),
      makeup (p.apvts, "makeup", "Makeup", "Gain after compression, to match the level you took off.", true),
      mix (p.apvts, "mix", "Mix", "Blend of compressed and dry signal. Below 100% = parallel (New York) compression."),
      drive (p.apvts, "drive", "Drive", "How hard the Color stage is pushed. Quiet parts keep their level; peaks are rounded.")
{
    setLookAndFeel (&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &needle, &history, &transfer }) addAndMakeVisible (*c);
    for (auto* k : { &threshold, &ratio, &attack, &release, &knee, &schpf, &makeup, &mix, &drive }) addAndMakeVisible (*k);
    threshold.hero = true;
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
    addAndMakeVisible (colorBtn);
    colorAtt = bindToggle (p.apvts, "color", colorBtn, "Color: analog-style saturation after the compressor (adds a few samples of latency)");
    charBox.addItemList ({ "Tape", "Tube" }, 1);
    charBox.setTooltip ("Tape: rounds peaks, odd harmonics. Tube: warmer, adds even harmonics.");
    addAndMakeVisible (charBox);
    charAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "character", charBox);
    addAndMakeVisible (bypassBtn);
    bypassBtn.setTooltip ("Bypass");
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassBtn);

    addAndMakeVisible (presetBox);
    for (auto* b : { &prevPreset, &nextPreset }) addAndMakeVisible (*b);
    refreshPresetBox();
    presetBox.onChange = [this] { if (presetBox.getSelectedItemIndex() >= 0) proc.loadFactoryPreset (presetBox.getSelectedItemIndex()); };
    prevPreset.onClick = [this] { proc.loadFactoryPreset ((proc.currentPreset + proc.getNumPrograms() - 1) % proc.getNumPrograms()); refreshPresetBox(); };
    nextPreset.onClick = [this] { proc.loadFactoryPreset ((proc.currentPreset + 1) % proc.getNumPrograms()); refreshPresetBox(); };

    setResizable (true, true);
    setResizeLimits (720, 420, 1440, 840);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (kBaseW / kBaseH);
    setSize ((int) kBaseW, (int) kBaseH);
    startTimerHz (30);
}

MuziGlueEditor::~MuziGlueEditor() { setLookAndFeel (nullptr); }

void MuziGlueEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.currentPreset, juce::dontSendNotification);
}

void MuziGlueEditor::timerCallback()
{
    const float in = proc.inPeak.take(), out = proc.outPeak.take(), gr = proc.grPeak.take(), det = proc.detPeak.take();
    needle.push (gr);
    history.push (juce::Decibels::gainToDecibels (in, -100.0f), juce::Decibels::gainToDecibels (out, -100.0f), gr);
    transfer.setLevel (juce::Decibels::gainToDecibels (det, -100.0f), gr);
    inMeter.tick (in); outMeter.tick (out);
    const bool c = proc.apvts.getRawParameterValue ("color")->load() > 0.5f;
    drive.setEnabled (c); charBox.setEnabled (c);
    if (presetBox.getSelectedItemIndex() != proc.currentPreset) presetBox.setSelectedItemIndex (proc.currentPreset, juce::dontSendNotification);
}

void MuziGlueEditor::refreshForSnapshot (int frames)
{
    for (int i = 0; i < frames; ++i) timerCallback();
}

juce::Rectangle<int> MuziGlueEditor::sc (float x, float y, float w, float h) const
{
    return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
}

void MuziGlueEditor::resized()
{
    s = (float) getWidth() / kBaseW;
    lnf.scale = s;
    needle.textScale = history.textScale = transfer.textScale = s;
    for (auto* k : { &threshold, &ratio, &attack, &release, &knee, &schpf, &makeup, &mix, &drive }) k->textScale = s;
    inMeter.textScale = outMeter.textScale = s;

    presetBox.setBounds (sc (350, 14, 260, 28));
    prevPreset.setBounds (sc (314, 14, 30, 28));
    nextPreset.setBounds (sc (616, 14, 30, 28));
    bypassBtn.setBounds (sc (910, 14, 30, 30));

    panelMeter = sc (16, 60, 290, 256);
    panelHist = sc (316, 60, 418, 256);
    panelCurve = sc (744, 60, 200, 256);
    panelKnobs = sc (16, 326, 738, 218);
    panelColor = sc (764, 326, 180, 218);

    needle.setBounds (sc (28, 92, 266, 216));
    history.setBounds (sc (328, 92, 394, 212));
    transfer.setBounds (sc (758, 104, 172, 172));

    threshold.setBounds (sc (28, 362, 130, 160));
    const float kw = 84, ky = 380, kh = 124;
    float x = 166;
    for (auto* k : { &ratio, &attack, &release, &knee, &schpf, &makeup, &mix }) { k->setBounds (sc (x, ky, kw, kh)); x += kw; }

    colorBtn.setBounds (sc (776, 358, 70, 26));
    charBox.setBounds (sc (776, 392, 70, 26));
    drive.setBounds (sc (772, 426, 78, 104));
    inMeter.setBounds (sc (862, 358, 34, 172));
    outMeter.setBounds (sc (900, 358, 34, 172));
}

void MuziGlueEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg0);
    epsilon_ui::drawProductMark (g, sc (20, 13, 1, 1).getPosition().toFloat(), s, epsilon_ui::Tier::Muzi, "Glue", {},
                                 [] (float h, bool b) { return font (h, b); });

    for (auto r : { panelMeter, panelHist, panelCurve, panelKnobs, panelColor }) panelBg (g, r.toFloat(), s);
    g.setFont (font (11.0f * s, true, 0.14f));
    g.setColour (col::quiet);
    g.drawText ("GAIN REDUCTION", sc (30, 66, 200, 22), juce::Justification::centredLeft);
    g.drawText ("HISTORY", sc (330, 66, 200, 22), juce::Justification::centredLeft);
    g.drawText ("TRANSFER", sc (758, 66, 160, 22), juce::Justification::centredLeft);
    g.drawText ("COMPRESSOR", sc (30, 332, 200, 22), juce::Justification::centredLeft);
    g.drawText ("COLOR / OUT", sc (776, 332, 160, 22), juce::Justification::centredLeft);
}
