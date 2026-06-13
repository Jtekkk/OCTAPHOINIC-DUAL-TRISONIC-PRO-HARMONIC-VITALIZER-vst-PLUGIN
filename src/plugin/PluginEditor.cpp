// =============================================================================
//  PluginEditor.cpp
// =============================================================================
#include "PluginEditor.h"

using namespace juce;

// JUCE 8 prefers FontOptions over the old Font(height, style) constructor.
static Font octaFont (float height, bool bold = false)
{
    auto opts = FontOptions().withHeight (height);
    if (bold) opts = opts.withStyle ("Bold");
    return Font (opts);
}

// ---- palette ---------------------------------------------------------------
const Colour OctaphonicLookAndFeel::bg      { 0xff0e1320 };
const Colour OctaphonicLookAndFeel::panel   { 0xff161d2e };
const Colour OctaphonicLookAndFeel::accentA { 0xff37e6d0 };  // cyan
const Colour OctaphonicLookAndFeel::accentB { 0xffff8a3c };  // amber
const Colour OctaphonicLookAndFeel::text    { 0xffe7ecf5 };
const Colour OctaphonicLookAndFeel::dim     { 0xff39435c };

OctaphonicLookAndFeel::OctaphonicLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, text);
    setColour (ComboBox::backgroundColourId, panel);
    setColour (ComboBox::textColourId, text);
    setColour (ComboBox::outlineColourId, dim);
    setColour (PopupMenu::backgroundColourId, panel);
    setColour (PopupMenu::textColourId, text);
    setColour (TextButton::buttonColourId, panel);
    setColour (TextButton::buttonOnColourId, accentA.withAlpha (0.85f));
    setColour (TextButton::textColourOnId, bg);
    setColour (TextButton::textColourOffId, text);
}

void OctaphonicLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                              float pos, float startAngle, float endAngle,
                                              Slider&)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const float track  = radius * 0.82f;

    // Track
    Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, endAngle, true);
    g.setColour (dim);
    g.strokePath (bgArc, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Value arc
    Path valArc;
    valArc.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, angle, true);
    g.setColour (accentA);
    g.strokePath (valArc, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // Knob body
    const float knobR = radius * 0.62f;
    g.setColour (panel.brighter (0.08f));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (dim);
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.2f);

    // Pointer
    Point<float> tip (centre.x + std::cos (angle - MathConstants<float>::halfPi) * knobR * 0.95f,
                      centre.y + std::sin (angle - MathConstants<float>::halfPi) * knobR * 0.95f);
    g.setColour (accentB);
    g.drawLine ({ centre, tip }, 2.4f);
}

//==============================================================================
OctaphonicAudioProcessorEditor::OctaphonicAudioProcessorEditor (OctaphonicAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    using namespace octa;

    // Global controls
    inputKnob  = &addKnob (pid::inputGain,  "INPUT");
    mixKnob    = &addKnob (pid::mix,         "VITALIZE");
    airKnob    = &addKnob (pid::air,         "AIR");
    outputKnob = &addKnob (pid::outputGain, "OUTPUT");

    osBox.addItemList ({ "Off (1x)", "2x", "4x", "8x" }, 1);
    addAndMakeVisible (osBox);
    osAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, pid::oversampling, osBox);

    bypassBtn = &addToggle (pid::bypass, "BYPASS");

    // Crossover
    lowMidKnob  = &addKnob (pid::xLowMid,  "LOW / MID");
    midHighKnob = &addKnob (pid::xMidHigh, "MID / HIGH");

    // Bands
    static const char* bandTitles[kNumBands] = { "LOW", "MID", "HIGH" };
    for (int b = 0; b < kNumBands; ++b)
    {
        bands[b].title.setText (bandTitles[b], dontSendNotification);
        bands[b].title.setJustificationType (Justification::centred);
        bands[b].title.setFont (octaFont (16.0f, true));
        bands[b].title.setColour (Label::textColourId, OctaphonicLookAndFeel::accentA);
        addAndMakeVisible (bands[b].title);

        bands[b].drive     = &addKnob (pid::drive[b],     "DRIVE");
        bands[b].character = &addKnob (pid::character[b], "EVEN ↔ ODD");
        bands[b].gain      = &addKnob (pid::bandGain[b],  "GAIN");
        bands[b].mute      = &addToggle (pid::mute[b], "M");
        bands[b].solo      = &addToggle (pid::solo[b], "S");
    }

    setResizable (false, false);
    setSize (780, 580);
}

OctaphonicAudioProcessorEditor::~OctaphonicAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::Slider& OctaphonicAudioProcessorEditor::addKnob (const String& paramID, const String& label)
{
    auto s = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow);
    s->setTextBoxStyle (Slider::TextBoxBelow, false, 78, 16);
    s->setColour (Slider::textBoxTextColourId, OctaphonicLookAndFeel::text);
    addAndMakeVisible (*s);
    sliderAtts.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramID, *s));

    auto l = std::make_unique<Label> ();
    l->setText (label, dontSendNotification);
    l->setJustificationType (Justification::centred);
    l->setFont (octaFont (11.0f, true));
    l->setColour (Label::textColourId, OctaphonicLookAndFeel::text.withAlpha (0.85f));
    addAndMakeVisible (*l);

    Slider* raw = s.get();
    ownedSliders.push_back (std::move (s));
    knobLabels.push_back ({ raw, l.get() });
    ownedLabels.push_back (std::move (l));
    return *raw;
}

juce::TextButton& OctaphonicAudioProcessorEditor::addToggle (const String& paramID, const String& label)
{
    auto btn = std::make_unique<TextButton> (label);
    btn->setClickingTogglesState (true);
    addAndMakeVisible (*btn);
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramID, *btn));

    TextButton* raw = btn.get();
    ownedButtons.push_back (std::move (btn));
    return *raw;
}

//==============================================================================
void OctaphonicAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (OctaphonicLookAndFeel::bg);

    // Header banner
    auto header = getLocalBounds().removeFromTop (64);
    g.setColour (OctaphonicLookAndFeel::panel);
    g.fillRect (header);

    g.setColour (OctaphonicLookAndFeel::accentA);
    g.setFont (octaFont (22.0f, true));
    g.drawText ("OCTAPHONIC", header.reduced (16, 8).removeFromTop (28), Justification::centredLeft);

    g.setColour (OctaphonicLookAndFeel::text.withAlpha (0.7f));
    g.setFont (octaFont (12.0f));
    g.drawText ("DUAL-TRISONIC · PRO-HARMONIC VITALIZER",
                header.reduced (16, 8).removeFromBottom (22), Justification::centredLeft);

    g.setColour (OctaphonicLookAndFeel::accentB);
    g.setFont (octaFont (11.0f, true));
    g.drawText ("3-BAND · DUAL SHAPER · 8x OS",
                header.reduced (16, 8), Justification::centredRight);

    // Panel backers
    auto drawPanel = [&] (Rectangle<int> r, const String& caption)
    {
        g.setColour (OctaphonicLookAndFeel::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (OctaphonicLookAndFeel::dim);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
        g.setColour (OctaphonicLookAndFeel::text.withAlpha (0.55f));
        g.setFont (octaFont (11.0f, true));
        g.drawText (caption, r.removeFromTop (18).reduced (8, 2), Justification::centredLeft);
    };

    drawPanel (globalArea, "GLOBAL");
    drawPanel (xoverArea, "CROSSOVER");
    for (int b = 0; b < octa::kNumBands; ++b)
        drawPanel (bandsArea.getProportion<float> ({ b / 3.0f, 0.0f, 1.0f / 3.0f, 1.0f }).toNearestInt().reduced (5, 0),
                   String());
}

void OctaphonicAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (64);
    r.reduce (10, 10);

    // Top row: global strip (left) + crossover (right).
    auto top = r.removeFromTop (150);
    globalArea = top.removeFromLeft (proportionOfWidth (0.62f)).reduced (2);
    top.removeFromLeft (8);
    xoverArea = top.reduced (2);

    r.removeFromTop (10);
    bandsArea = r;

    // ---- helper to place a knob + its label inside a rect -----------------
    auto place = [this] (Rectangle<int> area, Slider* s)
    {
        Label* lab = nullptr;
        for (auto& kv : knobLabels) if (kv.first == s) { lab = kv.second; break; }
        auto a = area.reduced (4);
        if (lab) lab->setBounds (a.removeFromTop (14));
        s->setBounds (a);
    };

    // ---- global panel: 4 knobs + OS + bypass ------------------------------
    {
        auto g = globalArea.reduced (8); g.removeFromTop (16);
        auto knobs = g.removeFromTop (104);
        const int kw = knobs.getWidth() / 4;
        place (knobs.removeFromLeft (kw), inputKnob);
        place (knobs.removeFromLeft (kw), mixKnob);
        place (knobs.removeFromLeft (kw), airKnob);
        place (knobs, outputKnob);

        auto bottom = g.reduced (2, 2);
        osBox.setBounds (bottom.removeFromLeft (proportionOfWidth (0.28f)).removeFromTop (26));
        bottom.removeFromLeft (10);
        bypassBtn->setBounds (bottom.removeFromLeft (110).removeFromTop (26));
    }

    // ---- crossover panel: 2 knobs -----------------------------------------
    {
        auto g = xoverArea.reduced (8); g.removeFromTop (16);
        auto knobs = g.removeFromTop (104);
        const int kw = knobs.getWidth() / 2;
        place (knobs.removeFromLeft (kw), lowMidKnob);
        place (knobs, midHighKnob);
    }

    // ---- band strips ------------------------------------------------------
    const int bw = bandsArea.getWidth() / octa::kNumBands;
    for (int b = 0; b < octa::kNumBands; ++b)
    {
        auto col = bandsArea.removeFromLeft (bw).reduced (5);
        col.removeFromTop (6);
        bands[b].title.setBounds (col.removeFromTop (24));

        auto knobsRow = col.removeFromTop (120);
        const int kw = knobsRow.getWidth() / 3;
        place (knobsRow.removeFromLeft (kw), bands[b].drive);
        place (knobsRow.removeFromLeft (kw), bands[b].character);
        place (knobsRow, bands[b].gain);

        auto btns = col.removeFromTop (30).reduced (knobsRow.getWidth() / 6, 2);
        bands[b].mute->setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (3, 0));
        bands[b].solo->setBounds (btns.reduced (3, 0));
    }
}
