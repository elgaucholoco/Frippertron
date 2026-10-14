#pragma once
#include "PluginProcessor.h"

class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TapeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

class FrippertronEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FrippertronEditor (FrippertronProcessor&);
    ~FrippertronEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawReel (juce::Graphics&, juce::Point<float> centre, float radius, float angle, float fill);

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct LineUI                        // commandes d'une ligne de bande
    {
        Knob time, length, feedback, pan, motion;   // length remplace time quand SYNC est actif
        juce::ToggleButton sync { "SYNC" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
        juce::Rectangle<int> panel;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& text, juce::Colour);
    void updateSyncView();
    void updateDeckView();

    FrippertronProcessor& proc;
    TapeLookAndFeel lnf;
    std::array<LineUI, FrippertronProcessor::numLines> lineUI;
    std::array<Knob, 8> globalKnobs;
    juce::ToggleButton holdButton { "HOLD" }, rehearseButton { "REHEARSE" };
    juce::TextButton   clearButton { "ERASE" };
    juce::ComboBox     presetBox, routingBox;
    juce::TextButton   oneDeckButton { "1 REVOX" }, twoDeckButton { "2 REVOX" };
    std::unique_ptr<juce::ParameterAttachment> decksAttachment;
    bool singleDeck = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment, rehearseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> routingAttachment;

    float reelAngle = 0.0f, level = 0.0f;
    float phase[FrippertronProcessor::numLines] {};
    juce::Rectangle<float> deckArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrippertronEditor)
};
