#pragma once
#include "PluginProcessor.h"

class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TapeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
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

    FrippertronProcessor& proc;
    TapeLookAndFeel lnf;
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& text);
    void updateSyncView();

    std::array<Knob, 10> knobs;
    Knob lengthKnob;                     // remplace LOOP TIME quand SYNC est actif
    juce::ToggleButton holdButton { "HOLD" }, syncButton { "SYNC" };
    juce::TextButton   clearButton { "ERASE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment, syncAttachment;

    float reelAngle = 0.0f, phase = 0.0f, level = 0.0f;
    juce::Rectangle<float> deckArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrippertronEditor)
};
