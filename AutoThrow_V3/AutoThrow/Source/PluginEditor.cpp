#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kW = 820, kH = 484;

    FontOptions mono (float size, bool bold = true)
    {
        return FontOptions (Font::getDefaultMonospacedFontName(), size, bold ? Font::bold : Font::plain);
    }

    void drawScrew (Graphics& g, Point<float> c, float r, float angle)
    {
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - r + 0.8f, c.y - r + 1.2f, r * 2.0f, r * 2.0f);
        g.setGradientFill (ColourGradient (ui::chromeHi, c.x - r, c.y - r, ui::chromeLo, c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (Colours::black.withAlpha (0.55f));
        Path slot;
        slot.addRectangle (-r * 0.75f, -0.8f, r * 1.5f, 1.6f);
        g.fillPath (slot, AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawGlowLine (Graphics& g, Line<float> l, Colour c, float intensity)
    {
        g.setColour (c.withAlpha (0.10f * intensity)); g.drawLine (l, 11.0f);
        g.setColour (c.withAlpha (0.22f * intensity)); g.drawLine (l, 5.0f);
        g.setColour (c.interpolatedWith (Colours::white, 0.25f).withAlpha (intensity)); g.drawLine (l, 2.0f);
    }
}

// ===========================================================================
// Look & feel
// ===========================================================================

ThrowLookAndFeel::ThrowLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, ui::amber);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, ui::text);
    setColour (TooltipWindow::backgroundColourId, ui::glass);
    setColour (TooltipWindow::textColourId, ui::amber);
    setColour (TooltipWindow::outlineColourId, ui::amberDim);
    setColour (TextEditor::backgroundColourId, ui::glass);
    setColour (TextEditor::textColourId, ui::amber);
    setColour (TextEditor::highlightColourId, ui::orange.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId, ui::amber);
}

void ThrowLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool hover = s.isMouseOverOrDragging();

    // Graduation 0..10 autour du bouton : les crans "passés" s'allument en orange
    for (int i = 0; i <= 20; ++i)
    {
        const float t = (float) i / 20.0f;
        const float a = startAngle + t * (endAngle - startAngle);
        const bool major = (i % 2) == 0;
        const float r0 = R * (major ? 0.84f : 0.88f), r1 = R * 0.97f;
        const auto p0 = c.getPointOnCircumference (r0, a), p1 = c.getPointOnCircumference (r1, a);
        const bool lit = t <= pos + 0.001f;
        g.setColour (lit ? ui::orange.withAlpha (major ? 1.0f : 0.8f) : ui::textDim.withAlpha (major ? 0.55f : 0.3f));
        g.drawLine ({ p0, p1 }, major ? 1.6f : 1.0f);
    }

    const float kr = R * 0.74f;

    // Ombre portée
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);

    // Jupe moletée (chrome)
    g.setGradientFill (ColourGradient (ui::chromeHi, c.x - kr, c.y - kr, ui::chromeLo.darker (0.3f), c.x + kr, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    for (int i = 0; i < 60; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 60.0f;
        g.setColour (Colours::black.withAlpha ((i & 1) ? 0.28f : 0.08f));
        g.drawLine ({ c.getPointOnCircumference (kr * 0.86f, a), c.getPointOnCircumference (kr * 0.995f, a) }, 1.0f);
    }

    // Dessus du bouton : alu brossé circulaire
    const float cr = kr * 0.80f;
    ColourGradient cap (hover ? Colours::white : ui::chromeHi, c.x - cr * 0.4f, c.y - cr * 0.5f,
                        ui::chromeLo, c.x + cr, c.y + cr, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    for (int i = 1; i < 7; ++i)
    {
        const float rr = cr * (float) i / 7.0f;
        g.setColour (Colours::black.withAlpha (0.045f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.8f);
    }
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);

    // Repère orange lumineux
    drawGlowLine (g, { c.getPointOnCircumference (cr * 0.25f, angle), c.getPointOnCircumference (cr * 0.92f, angle) },
                  ui::orange, 1.0f);
}

void ThrowLookAndFeel::drawKey (Graphics& g, Rectangle<float> r, const String& text, bool on, bool highlighted, Colour led)
{
    // Socle (la touche "sort" de la façade)
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);

    const float travel = on ? 3.0f : 0.0f;
    auto cap = r.withTrimmedBottom (3.0f).translated (0.0f, travel);

    // Flanc
    g.setColour (on ? ui::chromeLo.darker (0.4f) : ui::chromeLo);
    g.fillRoundedRectangle (cap.withTop (cap.getBottom() - 6.0f).translated (0.0f, 3.0f - travel), 3.0f);

    // Dessus
    const auto top = on ? ui::chromeMid : (highlighted ? Colours::white : ui::chromeHi);
    g.setGradientFill (ColourGradient (top, cap.getX(), cap.getY(), on ? ui::chromeLo : ui::chromeMid,
                                       cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.5f);
    g.setColour (Colours::white.withAlpha (on ? 0.15f : 0.5f));
    g.drawLine (cap.getX() + 4.0f, cap.getY() + 1.0f, cap.getRight() - 4.0f, cap.getY() + 1.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (cap, 3.5f, 1.0f);

    // LED
    const auto ledC = Point<float> (cap.getX() + 11.0f, cap.getCentreY());
    if (on)
    {
        g.setColour (led.withAlpha (0.35f));
        g.fillEllipse (ledC.x - 6.0f, ledC.y - 6.0f, 12.0f, 12.0f);
    }
    g.setColour (on ? led : led.darker (0.85f).withSaturation (0.4f));
    g.fillEllipse (ledC.x - 3.0f, ledC.y - 3.0f, 6.0f, 6.0f);
    g.setColour (Colours::white.withAlpha (on ? 0.7f : 0.15f));
    g.fillEllipse (ledC.x - 1.5f, ledC.y - 2.0f, 2.0f, 2.0f);

    // Inscription
    g.setColour (Colour (0xff2a2520));
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawFittedText (text, cap.withTrimmedLeft (18.0f).toNearestInt(), Justification::centred, 1);
}

void ThrowLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const bool isFreeze = b.getButtonText() == "FREEZE";
    drawKey (g, b.getLocalBounds().toFloat().reduced (1.0f), b.getButtonText(), b.getToggleState(), highlighted,
             isFreeze ? ui::amber : ui::red);
}

void ThrowLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool)
{
    drawKey (g, b.getLocalBounds().toFloat().reduced (1.0f), b.getButtonText(), b.getToggleState(), highlighted, ui::orange);
}

void ThrowLookAndFeel::drawButtonText (Graphics&, TextButton&, bool, bool)
{
    // le texte est gravé sur la touche dans drawKey
}

Label* ThrowLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (mono (12.0f));
    l->setColour (Label::textColourId, ui::amber);
    l->setColour (Label::outlineWhenEditingColourId, ui::orange);
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

    setupKnob (decay,   "decay",   "DECAY",   "Longueur de la queue de reverb. ROOM sonne mieux court (0,5-2 s), HALL long");
    setupKnob (shimmer, "shimmer", "SHIMMER", "Octave +12 qui monte dans la queue : effet aérien / angélique");
    setupKnob (tone,    "tone",    "TONE",    "Couleur de la reverb : à gauche sombre, à droite brillante");
    setupKnob (mix,     "mix",     "MIX",     "Volume du throw");
    setupKnob (sens,    "sens",    "SENS",    "Seuil de détection de la voix (le petit triangle sur l'écran). Si le throw part en pleine phrase, baisse-le");
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

    // Touches de type de reverb (radio) reliées au paramètre "type"
    typeParam = proc.apvts.getParameter ("type");
    const char* names[4] = { "ROOM", "CHAMBER", "PLATE", "HALL" };
    const char* tips[4] = { "Petite pièce : rebonds audibles, son proche et sombre",
                            "Chambre d'écho de studio : chaude, dense, taille moyenne",
                            "Plaque : ultra dense, brillante, le classique des voix",
                            "Grande salle : large, aérée, le son de la V1" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = typeBtns[i];
        b.setButtonText (names[i]);
        b.setTooltip (tips[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (4242);
        b.onClick = [this, i]
        {
            if (! typeBtns[i].getToggleState() || typeParam == nullptr) return;
            typeParam->beginChangeGesture();
            typeParam->setValueNotifyingHost (typeParam->convertTo0to1 ((float) i));
            typeParam->endChangeGesture();
        };
        addAndMakeVisible (b);
    }
    syncTypeButtons();

    // Les sliders ont créé leur zone de texte avec le look par défaut : on leur renvoie le nôtre.
    sendLookAndFeelChange();

    setSize (kW, kH);
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
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (FontOptions (11.5f, Font::bold));
    k.label.setColour (Label::textColourId, ui::chromeMid);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

// ---------------------------------------------------------------------------
// Fond statique (bois, alu brossé, chromes, sérigraphies) : dessiné une seule fois, en 2x pour les écrans HiDPI
// ---------------------------------------------------------------------------

void AutoThrowEditor::renderStaticBackground()
{
    const float sc = 2.0f;
    background = Image (Image::ARGB, roundToInt (kW * sc), roundToInt (kH * sc), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (sc));
    Random rng (80);

    const auto all = Rectangle<float> (0.0f, 0.0f, (float) kW, (float) kH);
    const float cheek = 16.0f;

    // Joues en bois (planches verticales avec veinage)
    for (auto woodR : { all.withWidth (cheek), all.withLeft ((float) kW - cheek) })
    {
        g.setGradientFill (ColourGradient (ui::wood, woodR.getX(), 0.0f, ui::woodDark, woodR.getRight(), 0.0f, false));
        g.fillRect (woodR);
        for (int i = 0; i < 9; ++i)
        {
            Path grain;
            const float x0 = woodR.getX() + rng.nextFloat() * woodR.getWidth();
            const float amp = 0.6f + rng.nextFloat() * 1.6f, freq = 0.01f + rng.nextFloat() * 0.03f, ph = rng.nextFloat() * 6.0f;
            grain.startNewSubPath (x0, 0.0f);
            for (float yy = 0.0f; yy <= (float) kH; yy += 6.0f)
                grain.lineTo (x0 + amp * std::sin (yy * freq + ph), yy);
            g.setColour (ui::woodDark.withAlpha (0.25f + rng.nextFloat() * 0.3f));
            g.strokePath (grain, PathStrokeType (0.6f + rng.nextFloat()));
        }
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawRect (woodR, 1.0f);
    }

    // Façade alu brossé
    const auto face = all.reduced (cheek, 0.0f);
    g.setGradientFill (ColourGradient (ui::face, 0.0f, 0.0f, ui::faceLo, 0.0f, (float) kH, false));
    g.fillRect (face);
    for (float yy = face.getY(); yy < face.getBottom(); yy += 1.0f)
    {
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (rng.nextFloat() * 0.035f));
        g.drawHorizontalLine ((int) yy, face.getX(), face.getRight());
    }

    // Baguettes chromées haut / bas
    for (auto trim : { face.withHeight (5.0f), face.withTop ((float) kH - 5.0f) })
    {
        g.setGradientFill (ColourGradient (ui::chromeHi, 0.0f, trim.getY(), ui::chromeLo, 0.0f, trim.getBottom(), false));
        g.fillRect (trim);
    }

    // Logo : trois bandes rayées années 80 + nom
    {
        const Colour stripes[3] = { Colour (0xffffb300), ui::orange, ui::red };
        for (int i = 0; i < 3; ++i)
        {
            Path p;
            const float x0 = 36.0f + (float) i * 9.0f, y0 = 20.0f, y1 = 50.0f;
            p.startNewSubPath (x0 + 10.0f, y0);
            p.lineTo (x0 + 16.0f, y0);
            p.lineTo (x0 + 6.0f, y1);
            p.lineTo (x0, y1);
            p.closeSubPath();
            g.setColour (stripes[i]);
            g.fillPath (p);
        }
        const auto titleFont = FontOptions (30.0f, Font::bold | Font::italic);
        const int autoW = GlyphArrangement::getStringWidthInt (titleFont, "AUTO ");
        g.setFont (titleFont);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawText ("AUTO", 70, 17, 200, 36, Justification::centredLeft);
        g.setColour (ui::chromeHi);
        g.drawText ("AUTO", 68, 15, 200, 36, Justification::centredLeft);
        g.setGradientFill (ColourGradient (Colour (0xffffb300), 0.0f, 18.0f, ui::red, 0.0f, 48.0f, false));
        g.drawText ("THROW", 68 + autoW, 15, 220, 36, Justification::centredLeft);

        g.setColour (ui::textDim);
        g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText ("STEREO  THROW  RECEIVER    AT-80", Rectangle<int> (kW - 330, 18, 290, 16), Justification::centredRight);
        g.setColour (ui::orange.withAlpha (0.85f));
        g.setFont (FontOptions (10.0f, Font::italic));
        g.drawText ("by LUCIF", Rectangle<int> (kW - 330, 34, 290, 14), Justification::centredRight);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawHorizontalLine (60, face.getX() + 20.0f, face.getRight() - 20.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (61, face.getX() + 20.0f, face.getRight() - 20.0f);
    }

    // Vis de façade
    for (auto pt : { Point<float> (face.getX() + 10.0f, 16.0f), Point<float> (face.getRight() - 10.0f, 16.0f),
                     Point<float> (face.getX() + 10.0f, (float) kH - 16.0f), Point<float> (face.getRight() - 10.0f, (float) kH - 16.0f) })
        drawScrew (g, pt, 4.5f, rng.nextFloat() * MathConstants<float>::pi);

    // Cadre chromé de l'écran
    {
        const auto d = displayArea.toFloat();
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (d.expanded (7.0f).translated (0.0f, 2.0f), 9.0f);
        g.setGradientFill (ColourGradient (ui::chromeHi, 0.0f, d.getY() - 6.0f, ui::chromeLo, 0.0f, d.getBottom() + 6.0f, false));
        g.fillRoundedRectangle (d.expanded (6.0f), 8.0f);
        g.setColour (Colours::black);
        g.fillRoundedRectangle (d.expanded (1.5f), 5.0f);
    }

    // Haut-parleur : anneau chromé + fond
    {
        const auto sp = speakerArea.toFloat();
        const auto c = sp.getCentre();
        const float R = sp.getWidth() * 0.5f;
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillEllipse (sp.translated (0.0f, 3.0f));
        g.setGradientFill (ColourGradient (ui::chromeHi, c.x - R, c.y - R, ui::chromeLo.darker (0.3f), c.x + R, c.y + R, false));
        g.fillEllipse (sp);
        const float gr = R * 0.86f;
        g.setColour (Colour (0xff0d0b09));
        g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);
    }

    // Fente des touches
    {
        const auto k = keysArea.toFloat().expanded (6.0f, 5.0f);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (k, 6.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (k.translated (0.0f, 1.0f), 6.0f, 1.0f);
        g.setColour (ui::textDim);
        g.setFont (FontOptions (9.5f, Font::bold));
        g.drawText ("ACOUSTICS", typeBtns[0].getX(), keysArea.getBottom() + 7, 120, 12, Justification::centredLeft);
        g.drawText ("MODE", freezeBtn.getX(), keysArea.getBottom() + 7, 120, 12, Justification::centredLeft);
    }

    // Sections des boutons rotatifs, sérigraphiées
    for (auto [area, title] : { std::pair { reverbGroup, String ("REVERB") }, std::pair { detectGroup, String ("DETECTION  /  THROW") } })
    {
        const auto a = area.toFloat();
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (a, 8.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (a.translated (0.0f, 1.0f), 8.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (a, 8.0f, 1.0f);

        g.setFont (FontOptions (10.0f, Font::bold));
        const int tw = GlyphArrangement::getStringWidthInt (FontOptions (10.0f, Font::bold), title) + 16;
        const auto titleR = Rectangle<int> (area.getCentreX() - tw / 2, area.getY() - 7, tw, 14);
        g.setColour (ui::face);
        g.fillRect (titleR);
        g.setColour (ui::orange);
        g.drawText (title, titleR, Justification::centred);
    }
}

// ---------------------------------------------------------------------------
// Parties animées
// ---------------------------------------------------------------------------

void AutoThrowEditor::drawDisplay (Graphics& g)
{
    const auto d = displayArea.toFloat();
    const bool frozen = freezeBtn.getToggleState();

    // Verre fumé avec halo ambré
    g.setGradientFill (ColourGradient (ui::glass, 0.0f, d.getY(), Colour (0xff1c1006), 0.0f, d.getBottom(), false));
    g.fillRoundedRectangle (d, 5.0f);
    g.setColour (ui::amber.withAlpha (0.04f + 0.06f * throwSm));
    g.fillRoundedRectangle (d, 5.0f);

    auto vfd = [&] (const String& txt, Rectangle<float> r, float size, bool lit, Justification j, Colour col)
    {
        g.setFont (mono (size));
        if (lit)
        {
            g.setColour (col.withAlpha (0.25f));
            g.drawText (txt, r.translated (0.0f, 0.5f).expanded (1.0f, 0.0f), j);
        }
        g.setColour (lit ? col : col.withAlpha (0.12f)); // segments éteints encore visibles, comme un vrai VFD
        g.drawText (txt, r, j);
    };

    // Ligne d'état + nom du type
    const char* typeNames[4] = { "ROOM", "CHAMBER", "PLATE", "HALL" };
    auto top = d.reduced (14.0f, 8.0f).removeFromTop (20.0f);
    vfd ("VOICE",  top.removeFromLeft (62.0f), 12.0f, voiceOn && ! frozen, Justification::centredLeft, ui::amber);
    vfd ("THROW",  top.removeFromLeft (62.0f), 12.0f, ! voiceOn && ! frozen, Justification::centredLeft, ui::orange);
    vfd ("FROZEN", top.removeFromLeft (70.0f), 12.0f, frozen, Justification::centredLeft, ui::amber);
    vfd ("STEREO", top.removeFromLeft (70.0f), 12.0f, throwSm > 0.5f, Justification::centredLeft, ui::red);
    vfd (typeNames[jlimit (0, 3, typeIndex)], top, 20.0f, true, Justification::centredRight, ui::amber);

    // Échelle du tuner 0..10 : l'aiguille montre l'ouverture du throw
    const float sx = d.getX() + 34.0f, sw = d.getWidth() - 68.0f;
    const float base = d.getY() + 82.0f;
    for (int i = 0; i <= 50; ++i)
    {
        const float t = (float) i / 50.0f;
        const float xx = sx + t * sw;
        const bool major = i % 5 == 0;
        const Colour col = t > 0.8f ? ui::red : ui::amber;
        g.setColour (col.withAlpha (major ? 0.8f : 0.4f));
        g.drawLine (xx, base - (major ? 14.0f : 7.0f), xx, base, major ? 1.4f : 1.0f);
        if (major)
        {
            g.setFont (mono (10.0f, false));
            g.drawText (String (i / 5), Rectangle<float> (xx - 10.0f, base - 30.0f, 20.0f, 12.0f), Justification::centred);
        }
    }
    g.setColour (ui::amber.withAlpha (0.5f));
    g.drawLine (sx, base + 0.5f, sx + sw, base + 0.5f, 1.0f);
    g.setFont (mono (9.0f));
    g.setColour (ui::amber.withAlpha (0.55f));
    g.drawText ("THROW", Rectangle<float> (d.getX() + 6.0f, base - 12.0f, 26.0f, 10.0f), Justification::centredLeft);
    g.drawText ("x10%", Rectangle<float> (sx + sw + 4.0f, base - 12.0f, 30.0f, 10.0f), Justification::centredLeft);

    const float nx = sx + jlimit (0.0f, 1.0f, needlePos) * sw;
    drawGlowLine (g, { nx, d.getY() + 36.0f, nx, base + 12.0f }, frozen ? ui::amber : ui::red, 1.0f);

    // Barre SIGNAL (niveau d'entrée) avec le seuil SENS
    const float ly = d.getBottom() - 26.0f;
    g.setFont (mono (9.0f));
    g.setColour (ui::amber.withAlpha (0.55f));
    g.drawText ("SIGNAL", Rectangle<float> (d.getX() + 6.0f, ly, 50.0f, 10.0f), Justification::centredLeft);
    const int segs = 32;
    const float lx = sx + 22.0f, lw = sw - 22.0f, segW = lw / (float) segs;
    auto dbNorm = [] (float db) { return jlimit (0.0f, 1.0f, (db + 70.0f) / 70.0f); };
    const float level = dbNorm (Decibels::gainToDecibels (inputSm, -100.0f));
    for (int i = 0; i < segs; ++i)
    {
        const float t = (float) (i + 1) / (float) segs;
        const Colour col = t > 0.85f ? ui::red : (t > 0.65f ? ui::orange : ui::amber);
        const bool lit = t <= level;
        g.setColour (lit ? col : col.withAlpha (0.1f));
        g.fillRect (Rectangle<float> (lx + (float) i * segW + 1.0f, ly, segW - 2.0f, 9.0f));
    }
    const float tx = lx + dbNorm ((float) sens.slider.getValue()) * lw;
    Path tri;
    tri.addTriangle (tx - 4.0f, ly + 17.0f, tx + 4.0f, ly + 17.0f, tx, ly + 11.0f);
    g.setColour (ui::chromeHi);
    g.fillPath (tri);

    // Reflet sur la vitre
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), d.getX(), d.getY(),
                                       Colours::transparentWhite, d.getX(), d.getY() + d.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (d.withHeight (d.getHeight() * 0.45f), 5.0f);
}

void AutoThrowEditor::drawSpeaker (Graphics& g)
{
    const auto sp = speakerArea.toFloat();
    const auto c = sp.getCentre();
    const float R = sp.getWidth() * 0.5f, gr = R * 0.86f;
    const bool frozen = freezeBtn.getToggleState();
    const float glow = frozen ? 0.8f : throwSm;
    const Colour lc = frozen ? ui::amber : ui::orange;

    // Grille en métal perforé ; derrière, la membrane "s'allume" et la lumière passe par les trous
    g.setGradientFill (ColourGradient (Colour (0xff35302b), c.x - gr, c.y - gr, Colour (0xff171411), c.x + gr, c.y + gr, false));
    g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);

    const float step = 7.0f;
    for (float yy = c.y - gr + step * 0.5f; yy < c.y + gr; yy += step)
        for (float xx = c.x - gr + step * 0.5f; xx < c.x + gr; xx += step)
        {
            const float dx = xx - c.x, dy = yy - c.y;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 > (gr - 5.0f) * (gr - 5.0f)) continue;
            const float k = 1.0f - std::sqrt (dist2) / gr; // plus lumineux vers le centre
            g.setColour (Colour (0xff060504).interpolatedWith (lc, jlimit (0.0f, 1.0f, glow * (0.35f + 0.65f * k))));
            g.fillEllipse (xx - 2.3f, yy - 2.3f, 4.6f, 4.6f);
            g.setColour (Colours::white.withAlpha (0.06f));
            g.drawLine (xx - 1.8f, yy + 2.6f, xx + 1.8f, yy + 2.6f, 0.6f); // reflet sous chaque trou
        }

    // Anneau lumineux (s'allume quand le throw s'ouvre)
    g.setColour (lc.withAlpha (0.08f + 0.45f * glow));
    g.drawEllipse (c.x - gr + 2.0f, c.y - gr + 2.0f, gr * 2.0f - 4.0f, gr * 2.0f - 4.0f, 6.0f);
    g.setColour (lc.withAlpha (0.25f + 0.75f * glow));
    g.drawEllipse (c.x - gr + 1.0f, c.y - gr + 1.0f, gr * 2.0f - 2.0f, gr * 2.0f - 2.0f, 1.5f);

    // Cache central chromé
    const float cap = R * 0.2f;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - cap + 1.0f, c.y - cap + 2.0f, cap * 2.0f, cap * 2.0f);
    g.setGradientFill (ColourGradient (ui::chromeHi, c.x - cap, c.y - cap, ui::chromeLo, c.x + cap, c.y + cap, false));
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2.0f, cap * 2.0f);
    g.setColour (Colour (0xff2a2520));
    g.setFont (FontOptions (9.0f, Font::bold | Font::italic));
    g.drawText ("AT-80", Rectangle<float> (c.x - cap, c.y - 6.0f, cap * 2.0f, 12.0f), Justification::centred);
}

void AutoThrowEditor::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
    drawDisplay (g);
    drawSpeaker (g);
}

void AutoThrowEditor::resized()
{
    displayArea = { 40, 76, 532, 138 };
    speakerArea = Rectangle<int> (0, 0, 196, 196).withCentre ({ 694, 172 });
    keysArea    = { 40, 230, 532, 36 };

    auto keys = keysArea;
    for (auto& b : typeBtns) b.setBounds (keys.removeFromLeft (84).reduced (2, 0));
    keys.removeFromLeft (28);
    freezeBtn.setBounds (keys.removeFromLeft (94).reduced (2, 0));
    wetOnlyBtn.setBounds (keys.removeFromLeft (94).reduced (2, 0));

    knobsArea = { 34, 300, kW - 68, kH - 300 - 28 };
    auto ka = knobsArea;
    reverbGroup = ka.removeFromLeft (ka.getWidth() / 2 - 6);
    ka.removeFromLeft (12);
    detectGroup = ka;

    auto layoutRow = [] (Rectangle<int> area, std::initializer_list<Knob*> knobs)
    {
        area = area.reduced (6, 12);
        const int w = area.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = area.removeFromLeft (w);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell.reduced (2, 0));
        }
    };
    layoutRow (reverbGroup, { &decay, &shimmer, &tone, &mix });
    layoutRow (detectGroup, { &sens, &gap, &duck, &bloom });

    renderStaticBackground();
}

void AutoThrowEditor::timerCallback()
{
    const float t = proc.meterThrow.load();
    const float in = proc.meterInput.load();
    throwSm += (t - throwSm) * 0.5f;
    // l'aiguille a un peu d'inertie, comme une vraie aiguille de tuner
    needlePos += (throwSm - needlePos) * 0.25f;
    inputSm = in > inputSm ? in : inputSm * 0.85f + in * 0.15f;
    voiceOn = proc.meterVoice.load();
    syncTypeButtons();
    repaint (displayArea.expanded (2));
    repaint (speakerArea);
}

void AutoThrowEditor::syncTypeButtons()
{
    if (typeParam == nullptr) return;
    const int idx = jlimit (0, 3, roundToInt (typeParam->convertFrom0to1 (typeParam->getValue())));
    typeIndex = idx;
    for (int i = 0; i < 4; ++i)
        if (typeBtns[i].getToggleState() != (i == idx))
            typeBtns[i].setToggleState (i == idx, dontSendNotification);
}
