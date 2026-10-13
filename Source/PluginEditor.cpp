#include "PluginEditor.h"

namespace colours
{
    static const juce::Colour panel   { 0xff1e1b18 };
    static const juce::Colour metal   { 0xff8a8279 };
    static const juce::Colour cream   { 0xffe9dfc8 };
    static const juce::Colour amber   { 0xffe0a040 };
    static const juce::Colour tape    { 0xff5a3a22 };
    static const juce::Colour dim     { 0xff6b645b };
    static const juce::Colour red     { 0xffc8452f };
}

//==============================================================================
TapeLookAndFeel::TapeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::cream);
    setColour (juce::TextButton::buttonColourId, colours::panel.brighter (0.15f));
    setColour (juce::TextButton::textColourOffId, colours::cream);
}

void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider&)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);

    // piste
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    value.addCentredArc (c.x, c.y, r, r, 0.0f, start, angle, true);
    g.setColour (colours::dim.withAlpha (0.5f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::amber);
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

void TapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::red : colours::panel.brighter (over ? 0.25f : 0.15f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::cream : colours::dim);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
FrippertronEditor::FrippertronEditor (FrippertronProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    const std::array<std::pair<const char*, const char*>, 10> defs {{
        { "time", "LOOP TIME" }, { "feedback", "FEEDBACK" }, { "input", "REC LEVEL" },
        { "mix", "MIX" },        { "output", "OUTPUT" },
        { "tone", "TAPE TONE" }, { "saturation", "SATURATION" }, { "wow", "WOW" },
        { "flutter", "FLUTTER" },{ "spread", "SPREAD" }
    }};

    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob (knobs[i], defs[i].first, defs[i].second);
    setupKnob (lengthKnob, "length", "LOOP LENGTH");

    addAndMakeVisible (holdButton);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "hold", holdButton);

    addAndMakeVisible (syncButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "sync", syncButton);
    updateSyncView();

    clearButton.onClick = [this] { proc.requestClear(); };
    addAndMakeVisible (clearButton);

    setSize (720, 480);
    startTimerHz (30);
}

FrippertronEditor::~FrippertronEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FrippertronEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k.slider);
}

void FrippertronEditor::updateSyncView()
{
    const bool sync = syncButton.getToggleState();
    knobs[0].slider.setVisible (! sync);
    knobs[0].label.setVisible (! sync);
    lengthKnob.slider.setVisible (sync);
    lengthKnob.label.setVisible (sync);
}

void FrippertronEditor::timerCallback()
{
    if (lengthKnob.slider.isVisible() != syncButton.getToggleState())
        updateSyncView();

    phase = proc.getLoopPhase();
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
    g.drawText ("FRIPPERTRON", 20, 10, 300, 28, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("two-deck tape loop  ·  v" JucePlugin_VersionString, 20, 34, 300, 14, juce::Justification::centredLeft);

    // ── les deux magnétophones ──
    auto d = deckArea;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (d, 8.0f);

    const float reelR = d.getHeight() * 0.36f;
    const float cy = d.getY() + d.getHeight() * 0.45f;
    const juce::Point<float> deckA (d.getX() + d.getWidth() * 0.14f, cy),
                             deckB (d.getRight() - d.getWidth() * 0.14f, cy);

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
    g.setColour (colours::amber);
    g.fillRoundedRectangle (playHead.x - 8, playHead.y - 7, 16, 14, 2.0f);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("REC", (int) recHead.x - 20, (int) recHead.y - 22, 40, 12, juce::Justification::centred);
    g.drawText ("PLAY", (int) playHead.x - 20, (int) playHead.y - 22, 40, 12, juce::Justification::centred);

    // position dans la boucle : un point de son qui voyage de REC à PLAY
    const auto blip = recHead + (playHead - recHead) * phase;
    const float glow = juce::jlimit (0.15f, 1.0f, level * 6.0f);
    g.setColour (colours::amber.withAlpha (glow));
    g.fillEllipse (blip.x - 5, blip.y - 5, 10, 10);

    drawReel (g, deckA, reelR, reelAngle, 0.75f);
    drawReel (g, deckB, reelR, reelAngle * 1.3f, 0.35f);

    // indicateur de niveau de la bande
    auto meter = juce::Rectangle<float> (d.getCentreX() - 80, d.getY() + 18, 160, 6);
    g.setColour (colours::dim.withAlpha (0.4f));
    g.fillRoundedRectangle (meter, 3.0f);
    g.setColour (colours::amber);
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, level * 4.0f)), 3.0f);
    g.setColour (colours::dim);
    g.drawText ("LOOP LEVEL", meter.translated (0, 8).withHeight (14).toNearestInt(), juce::Justification::centred);

    // tempo de l'hôte et durée réelle de la boucle
    const bool sync = syncButton.getToggleState();
    juce::String info;
    if (sync && ! proc.hasHostTempo())
        info = "SYNC : pas de tempo hôte";
    else
    {
        if (sync)
            info << juce::String (proc.getHostBpm(), 1) << " BPM  " << proc.getTimeSigNum()
                 << "/" << proc.getTimeSigDen() << "  ·  ";
        info << juce::String (proc.getLoopSeconds(), 2) << " s";
    }
    g.setColour (sync ? colours::amber : colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (info, meter.translated (-40, 28).withWidth (meter.getWidth() + 80).withHeight (14).toNearestInt(),
                juce::Justification::centred);
}

void FrippertronEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (44);
    clearButton.setBounds (top.removeFromRight (80).reduced (0, 8));
    top.removeFromRight (8);
    holdButton.setBounds (top.removeFromRight (80).reduced (0, 8));
    top.removeFromRight (8);
    syncButton.setBounds (top.removeFromRight (80).reduced (0, 8));

    deckArea = area.removeFromTop (170).toFloat();
    area.removeFromTop (12);

    const int rowH = area.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto r = area.removeFromTop (rowH);
        const int w = r.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
        {
            auto cell = r.removeFromLeft (w);
            auto& k = knobs[(size_t) (row * 5 + i)];
            k.label.setBounds (cell.removeFromTop (16));
            k.slider.setBounds (cell.reduced (8, 0));
        }
    }

    lengthKnob.label.setBounds (knobs[0].label.getBounds());
    lengthKnob.slider.setBounds (knobs[0].slider.getBounds());
}
