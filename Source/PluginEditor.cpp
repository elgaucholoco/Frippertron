#include "PluginEditor.h"

namespace colours
{
    static const juce::Colour panel   { 0xff1e1b18 };
    static const juce::Colour metal   { 0xff8a8279 };
    static const juce::Colour cream   { 0xffe9dfc8 };
    static const juce::Colour amber   { 0xffe0a040 };
    static const juce::Colour teal    { 0xff6fb3a8 };
    static const juce::Colour tape    { 0xff5a3a22 };
    static const juce::Colour dim     { 0xff6b645b };
    static const juce::Colour red     { 0xffc8452f };

    static juce::Colour line (int l) { return l == 0 ? amber : teal; }   // ligne A, ligne B
}

//==============================================================================
TapeLookAndFeel::TapeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colours::amber);
    setColour (juce::Label::textColourId, colours::cream);
    setColour (juce::TextButton::buttonColourId, colours::panel.brighter (0.15f));
    setColour (juce::TextButton::textColourOffId, colours::cream);
    setColour (juce::TextButton::buttonOnColourId, colours::cream.darker (0.1f));
    setColour (juce::TextButton::textColourOnId, colours::panel);
    setColour (juce::ToggleButton::tickColourId, colours::red);
    setColour (juce::ComboBox::backgroundColourId, colours::panel.brighter (0.15f));
    setColour (juce::ComboBox::textColourId, colours::cream);
    setColour (juce::ComboBox::outlineColourId, colours::dim);
    setColour (juce::ComboBox::arrowColourId, colours::cream);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel.brighter (0.1f));
    setColour (juce::PopupMenu::textColourId, colours::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amber.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::cream);
}

void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider& s)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);

    // piste
    // réglage bipolaire (pan) : l'arc part du centre
    const bool bipolar = s.getMinimum() < 0.0 && juce::approximatelyEqual (s.getMinimum(), -s.getMaximum());
    const float from = bipolar ? (start + end) * 0.5f : start;

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    value.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (colours::dim.withAlpha (0.5f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bouton façon Revox
    const float kr = r * 0.72f;
    g.setGradientFill (juce::ColourGradient (colours::metal.brighter (0.4f), c.x, c.y - kr,
                                             colours::metal.darker (0.8f), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr + 3.0f, 3.0f, kr * 0.45f, 1.5f);
    g.setColour (colours::panel);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c));
}

juce::Font TapeLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::FontOptions (12.0f, juce::Font::bold);
}

void TapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? b.findColour (juce::ToggleButton::tickColourId) : colours::panel.brighter (over ? 0.25f : 0.15f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::cream : colours::dim);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? colours::panel : colours::cream);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
FrippertronEditor::FrippertronEditor (FrippertronProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    // ── les deux lignes ──
    for (int l = 0; l < FrippertronProcessor::numLines; ++l)
    {
        auto& ui = lineUI[(size_t) l];
        const juce::String pre = l == 0 ? "a_" : "b_";
        const auto col = colours::line (l);
        setupKnob (ui.time,     pre + "time",     "LOOP TIME",   col);
        setupKnob (ui.length,   pre + "length",   "LOOP LENGTH", col);
        setupKnob (ui.feedback, pre + "feedback", "FEEDBACK",    col);
        setupKnob (ui.pan,      pre + "pan",      "PAN",         col);
        setupKnob (ui.motion,   pre + "motion",   "PAN MOTION",  col);

        ui.sync.setColour (juce::ToggleButton::tickColourId, col);
        addAndMakeVisible (ui.sync);
        ui.syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, pre + "sync", ui.sync);
    }

    // ── bande et sortie ──
    const std::array<std::pair<const char*, const char*>, 8> globals {{
        { "input", "REC LEVEL" }, { "mix", "MIX" },       { "output", "OUTPUT" },   { "tone", "TAPE TONE" },
        { "saturation", "SATURATION" }, { "wow", "WOW" }, { "flutter", "FLUTTER" }, { "hiss", "HISS" }
    }};
    for (size_t i = 0; i < globalKnobs.size(); ++i)
        setupKnob (globalKnobs[i], globals[i].first, globals[i].second, colours::cream.darker (0.15f));

    // ── en-tête ──
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    if (auto* routing = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("routing")))
        routingBox.addItemList (routing->choices, 1);
    addAndMakeVisible (routingBox);
    routingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "routing", routingBox);

    rehearseButton.setColour (juce::ToggleButton::tickColourId, colours::amber);
    addAndMakeVisible (rehearseButton);
    rehearseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "rehearse", rehearseButton);

    addAndMakeVisible (holdButton);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "hold", holdButton);

    clearButton.onClick = [this] { proc.requestClear(); };
    addAndMakeVisible (clearButton);

    // ── 1 REVOX / 2 REVOX ──
    oneDeckButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    twoDeckButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    for (auto* b : { &oneDeckButton, &twoDeckButton })
        addAndMakeVisible (b);
    decksAttachment = std::make_unique<juce::ParameterAttachment> (
        *proc.apvts.getParameter ("decks"),
        [this] (float v) { singleDeck = v < 0.5f; updateDeckView(); });
    oneDeckButton.onClick = [this] { decksAttachment->setValueAsCompleteGesture (0.0f); };
    twoDeckButton.onClick = [this] { decksAttachment->setValueAsCompleteGesture (1.0f); };
    decksAttachment->sendInitialUpdate();

    updateSyncView();
    setSize (880, 540);
    startTimerHz (30);
}

FrippertronEditor::~FrippertronEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FrippertronEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& text, juce::Colour col)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, col);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k.slider);
}

void FrippertronEditor::updateSyncView()
{
    for (auto& ui : lineUI)
    {
        const bool sync = ui.sync.getToggleState();
        ui.time.slider.setVisible (! sync);
        ui.time.label.setVisible (! sync);
        ui.length.slider.setVisible (sync);
        ui.length.label.setVisible (sync);
    }
}

void FrippertronEditor::updateDeckView()
{
    oneDeckButton.setToggleState (singleDeck, juce::dontSendNotification);
    twoDeckButton.setToggleState (! singleDeck, juce::dontSendNotification);

    // 1 REVOX : la durée et la réinjection de B suivent A ; son pan reste réglable
    auto& b = lineUI[1];
    for (juce::Component* c : { (juce::Component*) &b.time.slider, (juce::Component*) &b.time.label,
                                (juce::Component*) &b.length.slider, (juce::Component*) &b.length.label,
                                (juce::Component*) &b.feedback.slider, (juce::Component*) &b.feedback.label,
                                (juce::Component*) &b.sync })
    {
        c->setEnabled (! singleDeck);
        c->setAlpha (singleDeck ? 0.3f : 1.0f);
    }
    repaint();
}

void FrippertronEditor::timerCallback()
{
    for (auto& ui : lineUI)
        if (ui.length.slider.isVisible() != ui.sync.getToggleState())
            updateSyncView();

    // le preset peut aussi être changé par l'hôte
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    for (int l = 0; l < FrippertronProcessor::numLines; ++l)
        phase[l] = proc.getLoopPhase (l);
    level = proc.getLoopLevel();
    reelAngle += 0.06f;   // les bobines tournent toujours : la bande défile, même en HOLD
    repaint (deckArea.toNearestInt());
}

void FrippertronEditor::drawReel (juce::Graphics& g, juce::Point<float> c, float r, float angle, float fill)
{
    // bande enroulée
    const float tr = r * (0.45f + 0.5f * fill);
    g.setColour (colours::tape);
    g.fillEllipse (c.x - tr, c.y - tr, tr * 2, tr * 2);

    // flasque transparente
    g.setColour (colours::metal.withAlpha (0.35f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);

    // moyeu à trois trous
    g.setColour (colours::metal);
    g.fillEllipse (c.x - r * 0.3f, c.y - r * 0.3f, r * 0.6f, r * 0.6f);
    g.setColour (colours::panel);
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + i * juce::MathConstants<float>::twoPi / 3.0f;
        const auto h = c.getPointOnCircumference (r * 0.17f, a);
        g.fillEllipse (h.x - r * 0.07f, h.y - r * 0.07f, r * 0.14f, r * 0.14f);
    }
    g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
}

void FrippertronEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // ── titre ──
    g.setColour (colours::cream);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("FRIPPERTRON", 16, 10, 220, 28, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String::fromUTF8 ("two-deck tape loop  ·  v" JucePlugin_VersionString), 16, 34, 220, 14, juce::Justification::centredLeft);

    // ── les deux magnétophones ──
    auto d = deckArea;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (d, 8.0f);

    const float reelR = d.getHeight() * 0.36f;
    const float cy = d.getY() + d.getHeight() * 0.45f;
    const juce::Point<float> deckA (d.getX() + d.getWidth() * 0.12f, cy),
                             deckB (d.getRight() - d.getWidth() * 0.12f, cy);

    // la bande : de la machine A (enregistrement) à la machine B (lecture), et retour
    const float headY = d.getBottom() - 16.0f;
    const auto recHead  = juce::Point<float> (deckA.x + reelR + 30.0f, headY);
    const auto playHead = juce::Point<float> (deckB.x - reelR - 30.0f, headY);

    juce::Path tapePath;
    tapePath.startNewSubPath (deckA.x, cy + reelR * 0.75f);
    tapePath.lineTo (recHead);
    tapePath.lineTo (playHead);
    tapePath.lineTo (deckB.x, cy + reelR * 0.75f);
    g.setColour (colours::tape.brighter (0.3f));
    g.strokePath (tapePath, juce::PathStrokeType (3.0f));

    // têtes
    g.setColour (colours::red);
    g.fillRoundedRectangle (recHead.x - 8, recHead.y - 7, 16, 14, 2.0f);
    g.setColour (colours::cream.darker (0.2f));
    g.fillRoundedRectangle (playHead.x - 8, playHead.y - 7, 16, 14, 2.0f);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("REC", (int) recHead.x - 20, (int) recHead.y - 22, 40, 12, juce::Justification::centred);
    g.drawText ("PLAY", (int) playHead.x - 20, (int) playHead.y - 22, 40, 12, juce::Justification::centred);

    // position dans chaque boucle : un point de son qui voyage de REC à PLAY
    const float glow = juce::jlimit (0.25f, 1.0f, level * 6.0f);
    for (int l = 0; l < FrippertronProcessor::numLines; ++l)
    {
        const auto blip = recHead + (playHead - recHead) * phase[l];
        const float dy = l == 0 ? -5.0f : 5.0f;
        g.setColour (colours::line (l).withAlpha (glow));
        g.fillEllipse (blip.x - 5, blip.y - 5 + dy, 10, 10);
    }

    drawReel (g, deckA, reelR, reelAngle, 0.75f);
    drawReel (g, deckB, reelR, reelAngle * 1.3f, 0.35f);

    // indicateur de niveau des bandes
    auto meter = juce::Rectangle<float> (d.getCentreX() - 80, d.getY() + 14, 160, 6);
    g.setColour (colours::dim.withAlpha (0.4f));
    g.fillRoundedRectangle (meter, 3.0f);
    g.setColour (colours::cream.darker (0.1f));
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, level * 4.0f)), 3.0f);
    g.setColour (colours::dim);
    g.drawText ("LOOP LEVEL", meter.translated (0, 8).withHeight (14).toNearestInt(), juce::Justification::centred);

    // tempo de l'hôte
    auto textRow = [&] (int dyText) { return meter.translated (-60, (float) dyText).withWidth (meter.getWidth() + 120)
                                                    .withHeight (14).toNearestInt(); };
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colours::cream.darker (0.2f));
    g.drawText (proc.hasHostTempo() ? juce::String (proc.getHostBpm(), 1) + " BPM  " + juce::String (proc.getTimeSigNum())
                                          + "/" + juce::String (proc.getTimeSigDen())
                                    : juce::String::fromUTF8 ("pas de tempo hôte"),
                textRow (26), juce::Justification::centred);

    // durée réelle de chaque boucle
    auto durations = textRow (42);
    for (int l = 0; l < FrippertronProcessor::numLines; ++l)
    {
        auto half = l == 0 ? durations.removeFromLeft (durations.getWidth() / 2) : durations;
        g.setColour (colours::line (l));
        g.drawText ((l == 0 ? "A  " : "B  ") + juce::String (proc.getLoopSeconds (l), 2) + " s",
                    half.reduced (6, 0), l == 0 ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }

    // métronome visuel : un voyant par temps, calé sur l'hôte quand il joue
    const int beats = juce::jlimit (1, 12, proc.getTimeSigNum());
    const int lit = proc.isHostPlaying() ? (int) std::floor (proc.getBeatInBar()) : -1;
    const float ledSize = 8.0f, ledGap = 6.0f;
    const float ledsW = beats * ledSize + (beats - 1) * ledGap;
    float lx = d.getCentreX() - ledsW * 0.5f;
    const float ly = meter.getY() + 64.0f;
    for (int b = 0; b < beats; ++b, lx += ledSize + ledGap)
    {
        g.setColour (b == lit ? (b == 0 ? colours::red : colours::amber) : colours::dim.withAlpha (0.4f));
        g.fillEllipse (lx, ly, ledSize, ledSize);
    }

    // ── panneaux des deux lignes ──
    for (int l = 0; l < FrippertronProcessor::numLines; ++l)
    {
        const auto& ui = lineUI[(size_t) l];
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillRoundedRectangle (ui.panel.toFloat(), 6.0f);
        g.setColour (colours::line (l));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (l == 0 ? "LINE A" : (singleDeck ? "LINE B  (suit A)" : "LINE B"),
                    ui.panel.reduced (10, 4).removeFromTop (20), juce::Justification::centredLeft);
    }
}

void FrippertronEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // en-tête : titre, preset, routage, REHEARSE, HOLD, ERASE
    auto top = area.removeFromTop (44);
    top.removeFromLeft (220);
    clearButton.setBounds (top.removeFromRight (74).reduced (0, 8));
    top.removeFromRight (6);
    holdButton.setBounds (top.removeFromRight (74).reduced (0, 8));
    top.removeFromRight (6);
    rehearseButton.setBounds (top.removeFromRight (96).reduced (0, 8));
    top.removeFromRight (14);
    routingBox.setBounds (top.removeFromRight (150).reduced (0, 9));
    top.removeFromRight (8);
    presetBox.setBounds (top.removeFromRight (150).reduced (0, 9));

    deckArea = area.removeFromTop (150).toFloat();
    {
        // sélecteur sous le métronome, au centre de l'affichage
        auto sel = juce::Rectangle<int> (0, 0, 170, 22).withCentre ({ (int) deckArea.getCentreX(), (int) deckArea.getY() + 106 });
        oneDeckButton.setBounds (sel.removeFromLeft (85));
        twoDeckButton.setBounds (sel);
    }
    area.removeFromTop (10);

    auto layoutRow = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
    {
        const int w = r.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = r.removeFromLeft (w);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell.reduced (8, 0));
        }
    };

    // deux panneaux côte à côte
    auto linesArea = area.removeFromTop (150);
    for (int l = 0; l < FrippertronProcessor::numLines; ++l)
    {
        auto& ui = lineUI[(size_t) l];
        ui.panel = l == 0 ? linesArea.removeFromLeft ((linesArea.getWidth() - 10) / 2) : linesArea;
        if (l == 0) linesArea.removeFromLeft (10);

        auto inner = ui.panel.reduced (6, 4);
        auto header = inner.removeFromTop (24);
        ui.sync.setBounds (header.removeFromRight (70).reduced (0, 1));
        layoutRow (inner, { &ui.time, &ui.feedback, &ui.pan, &ui.motion });
        ui.length.label.setBounds (ui.time.label.getBounds());
        ui.length.slider.setBounds (ui.time.slider.getBounds());
    }

    area.removeFromTop (10);
    layoutRow (area, { &globalKnobs[0], &globalKnobs[1], &globalKnobs[2], &globalKnobs[3],
                       &globalKnobs[4], &globalKnobs[5], &globalKnobs[6], &globalKnobs[7] });
}
