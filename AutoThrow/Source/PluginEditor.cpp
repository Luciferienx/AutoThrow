#include "PluginEditor.h"

using namespace juce;

// ===========================================================================
// Look & feel
// ===========================================================================

ThrowLookAndFeel::ThrowLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, ui::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, ui::text);
    setColour (TooltipWindow::backgroundColourId, ui::panelHi);
    setColour (TooltipWindow::textColourId, ui::text);
    setColour (TooltipWindow::outlineColourId, ui::track);
}

void ThrowLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcW = jmax (3.0f, radius * 0.11f);
    const float arcR = radius - arcW * 0.5f;
    const bool hover = s.isMouseOverOrDragging();

    // Piste
    Path trackArc;
    trackArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (ui::track);
    g.strokePath (trackArc, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    // Valeur (rouge, avec halo)
    Path valueArc;
    valueArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (ui::accent.withAlpha (0.18f));
    g.strokePath (valueArc, PathStrokeType (arcW * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (ui::accentLo, c.x - radius, c.y + radius, ui::accent, c.x + radius, c.y - radius, false));
    g.strokePath (valueArc, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    // Corps du knob
    const float bodyR = arcR - arcW * 1.4f;
    g.setGradientFill (ColourGradient (hover ? Colour (0xff2c2c36) : Colour (0xff24242c), c.x, c.y - bodyR,
                                       Colour (0xff111116), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Aiguille
    Path needle;
    const float nw = jmax (2.0f, bodyR * 0.09f);
    needle.addRoundedRectangle (-nw * 0.5f, -bodyR * 0.9f, nw, bodyR * 0.45f, nw * 0.5f);
    g.setColour (ui::text);
    g.fillPath (needle, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void ThrowLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool isFreeze = b.getButtonText() == "FREEZE";
    const auto onColour = isFreeze ? ui::ice : ui::accent;

    if (on)
    {
        g.setColour (onColour.withAlpha (0.22f));
        g.fillRoundedRectangle (r.expanded (1.0f), 7.0f);
    }
    g.setColour (on ? onColour.withAlpha (0.9f) : (highlighted ? ui::panelHi.brighter (0.1f) : ui::panelHi));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? onColour : ui::track);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    g.setColour (on ? ui::bg : ui::textDim);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), Justification::centred, 1);
}

Label* ThrowLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (FontOptions (12.0f));
    l->setColour (Label::textColourId, ui::textDim);
    l->setColour (Label::outlineWhenEditingColourId, ui::accent);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setJustificationType (Justification::centred);
    return l;
}

// ===========================================================================
// Editor
// ===========================================================================

AutoThrowEditor::AutoThrowEditor (AutoThrowProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (decay,   "decay",   "DECAY",   "Longueur de la queue de reverb");
    setupKnob (shimmer, "shimmer", "SHIMMER", "Octave +12 qui monte dans la queue : effet aérien / angélique");
    setupKnob (tone,    "tone",    "TONE",    "Couleur de la reverb : à gauche sombre, à droite brillante");
    setupKnob (mix,     "mix",     "MIX",     "Volume du throw");
    setupKnob (sens,    "sens",    "SENS",    "Seuil de détection de la voix. Si le throw part en pleine phrase, baisse-le");
    setupKnob (gap,     "gap",     "GAP",     "Silence minimum avant que le throw parte. Court = throw entre les mots, long = seulement en fin de phrase");
    setupKnob (duck,    "duck",    "DUCK",    "Combien la reverb s'efface quand tu rappes. 100 % = voix totalement sèche");
    setupKnob (bloom,   "bloom",   "BLOOM",   "Vitesse d'ouverture du throw. Court = claque, long = monte en douceur");

    for (auto* b : { &freezeBtn, &wetOnlyBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    freezeBtn.setTooltip ("Gèle la queue à l'infini. À automatiser sur un mot ou une fin de mesure");
    wetOnlyBtn.setTooltip ("Coupe la voix sèche : à activer si tu mets le plugin sur un bus en send");
    freezeAttach  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "freeze", freezeBtn);
    wetOnlyAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "wetonly", wetOnlyBtn);

    // Les sliders ont créé leur zone de texte avec le look par défaut : on leur renvoie le nôtre.
    sendLookAndFeelChange();

    setSize (680, 400);
    startTimerHz (30);
}

AutoThrowEditor::~AutoThrowEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AutoThrowEditor::setupKnob (Knob& k, const String& paramId, const String& name, const String& tip)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 18);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (FontOptions (12.5f, Font::bold));
    k.label.setColour (Label::textColourId, ui::text);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void AutoThrowEditor::paint (Graphics& g)
{
    g.fillAll (ui::bg);

    // Titre
    auto header = getLocalBounds().removeFromTop (52).reduced (22, 0);
    g.setColour (ui::text);
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("AUTO", header, Justification::centredLeft);
    const int autoW = GlyphArrangement::getStringWidthInt (FontOptions (24.0f, Font::bold), "AUTO ");
    g.setColour (ui::accent);
    g.drawText ("THROW", header.withTrimmedLeft (autoW), Justification::centredLeft);
    g.setColour (ui::textDim);
    g.setFont (FontOptions (11.0f));
    g.drawText ("by LUCIF", header, Justification::centredRight);

    // Panneaux
    auto drawPanel = [&] (Rectangle<int> r, const String& title)
    {
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (ui::textDim);
        g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText (title, r.reduced (14, 8).removeFromTop (14), Justification::topLeft);
    };
    drawPanel (reverbArea, "REVERB");
    drawPanel (detectArea, "DETECTION / THROW");
    drawPanel (sideArea, "");

    // LED d'état : VOICE (voix détectée, reverb baissée) / THROW (reverb ouverte)
    const auto led = ledArea.toFloat();
    const bool frozen = freezeBtn.getToggleState();
    const auto stateColour = frozen ? ui::ice : ui::accent;
    const float glow = frozen ? 1.0f : throwSm;
    g.setColour (stateColour.withAlpha (0.10f + 0.35f * glow));
    g.fillEllipse (led.reduced (2.0f));
    g.setColour (stateColour.withAlpha (0.25f + 0.75f * glow));
    g.drawEllipse (led.reduced (2.0f), 2.0f);
    g.setColour (glow > 0.5f ? ui::text : ui::textDim);
    g.setFont (FontOptions (15.0f, Font::bold));
    g.drawText (frozen ? "FROZEN" : (voiceOn ? "VOICE" : "THROW"), ledArea, Justification::centred);

    // Barres : entrée (avec seuil SENS) et ouverture du throw
    auto drawMeter = [&] (Rectangle<int> r, float value, Colour col, const String& name, float markerNorm)
    {
        auto lab = r.removeFromBottom (16).withSizeKeepingCentre (60, 16);
        g.setColour (ui::textDim);
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText (name, lab, Justification::centred, false);
        auto bar = r.toFloat();
        g.setColour (ui::bg);
        g.fillRoundedRectangle (bar, 4.0f);
        auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * jlimit (0.0f, 1.0f, value));
        g.setColour (col);
        g.fillRoundedRectangle (fill, 4.0f);
        if (markerNorm >= 0.0f)
        {
            const float yM = bar.getBottom() - bar.getHeight() * markerNorm;
            g.setColour (ui::text);
            g.fillRect (bar.getX() - 3.0f, yM - 1.0f, bar.getWidth() + 6.0f, 2.0f);
        }
    };
    // Échelle d'entrée : -70 dB .. 0 dB
    auto dbNorm = [] (float db) { return jlimit (0.0f, 1.0f, (db + 70.0f) / 70.0f); };
    const float inDb = Decibels::gainToDecibels (inputSm, -100.0f);
    drawMeter (inputMeterArea, dbNorm (inDb), voiceOn ? ui::text.withAlpha (0.8f) : ui::textDim, "IN",
               dbNorm ((float) sens.slider.getValue()));
    drawMeter (throwMeterArea, throwSm, ui::accent, "THROW", -1.0f);
}

void AutoThrowEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (40);

    sideArea = r.removeFromRight (150);
    r.removeFromRight (12);
    reverbArea = r.removeFromTop (r.getHeight() / 2 - 6);
    r.removeFromTop (12);
    detectArea = r;

    auto layoutRow = [] (Rectangle<int> area, std::initializer_list<Knob*> knobs)
    {
        area = area.reduced (10, 6);
        area.removeFromTop (16);
        const int w = area.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = area.removeFromLeft (w);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell.reduced (6, 0));
        }
    };
    layoutRow (reverbArea, { &decay, &shimmer, &tone, &mix });
    layoutRow (detectArea, { &sens, &gap, &duck, &bloom });

    auto s = sideArea.reduced (14);
    ledArea = s.removeFromTop (s.getWidth() - 20).withSizeKeepingCentre (s.getWidth() - 20, s.getWidth() - 20);
    s.removeFromTop (10);
    wetOnlyBtn.setBounds (s.removeFromBottom (30));
    s.removeFromBottom (8);
    freezeBtn.setBounds (s.removeFromBottom (36));
    s.removeFromBottom (12);
    auto meters = s;
    inputMeterArea = meters.removeFromLeft (meters.getWidth() / 2).withSizeKeepingCentre (14, meters.getHeight());
    throwMeterArea = meters.withSizeKeepingCentre (14, meters.getHeight());
}

void AutoThrowEditor::timerCallback()
{
    const float t = proc.meterThrow.load();
    const float in = proc.meterInput.load();
    throwSm += (t - throwSm) * 0.5f;
    inputSm = in > inputSm ? in : inputSm * 0.85f + in * 0.15f;
    voiceOn = proc.meterVoice.load();
    repaint (sideArea);
}
