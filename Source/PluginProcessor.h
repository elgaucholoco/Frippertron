#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

/*  Frippertron — émulation du dispositif « Frippertronics »
    Deux magnétophones Revox : la bande enregistrée par la tête d'enregistrement
    du premier passe par la tête de lecture du second, dont la sortie est renvoyée
    à l'entrée du premier. La distance entre les deux machines fixe la longueur
    de la boucle ; le niveau de réinjection fixe la vitesse à laquelle les couches
    anciennes s'effacent. Chaque passage sur bande ajoute perte d'aigus,
    saturation et instabilités de vitesse (wow & flutter).                     */

class FrippertronProcessor : public juce::AudioProcessor
{
public:
    FrippertronProcessor();
    ~FrippertronProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Frippertron"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return maxLoopSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── interface pour l'éditeur ──
    void requestClear() noexcept { clearRequested.store (true); }
    float getLoopPhase() const noexcept  { return loopPhase.load(); }   // 0..1
    float getLoopLevel() const noexcept  { return loopLevel.load(); }   // niveau RMS de la bande
    float getLoopSeconds() const noexcept { return loopSecondsOut.load(); }
    bool  hasHostTempo() const noexcept  { return hostHasTempo.load(); }
    float getHostBpm() const noexcept    { return hostBpmOut.load(); }
    int   getTimeSigNum() const noexcept { return tsNumOut.load(); }
    int   getTimeSigDen() const noexcept { return tsDenOut.load(); }

    juce::AudioProcessorValueTreeState apvts;
    static constexpr double maxLoopSeconds = 30.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct OnePole              // filtre passe-bas / passe-haut 1 pôle
    {
        float z = 0.0f;
        float lp (float x, float a) noexcept { z += a * (x - z); return z; }
        float hp (float x, float a) noexcept { return x - lp (x, a); }
    };

    struct Channel
    {
        OnePole lp1, lp2, hp;
        float wowPhase = 0.0f, flutterPhase = 0.0f;
        float driftTarget = 0.0f, drift = 0.0f;
        int   driftCounter = 0;
    };

    double computeLoopSeconds();   // Loop Time libre, ou calé sur le tempo de l'hôte en mode SYNC
    float readCubic (const float* buf, double pos) const noexcept;
    static float tapeSaturate (float x, float drive) noexcept;

    juce::AudioBuffer<float> tape;          // la « bande » : un tampon circulaire par canal
    int tapeLength = 0;
    int writePos = 0;
    double sr = 44100.0;

    Channel ch[2];
    double smoothedDelay[2] { 0.0, 0.0 };   // en échantillons, glisse comme un varispeed
    juce::Random rng;

    juce::SmoothedValue<float> feedbackS, inputS, mixS, outS;
    float holdAmount = 0.0f;                // 0..1, fondu pour éviter les clics

    std::atomic<bool>  clearRequested { false };
    std::atomic<float> loopPhase { 0.0f }, loopLevel { 0.0f };
    double phaseCounter = 0.0;
    float levelAcc = 0.0f;

    // tempo de l'hôte (thread audio) et copies lisibles par l'éditeur
    double hostBpm = 120.0;
    int tsNum = 4, tsDen = 4;
    std::atomic<bool>  hostHasTempo { false };
    std::atomic<float> hostBpmOut { 120.0f }, loopSecondsOut { 4.0f };
    std::atomic<int>   tsNumOut { 4 }, tsDenOut { 4 };

    // pointeurs paramètres
    std::atomic<float> *pTime, *pFeedback, *pTone, *pSat, *pWow, *pFlutter,
                       *pSpread, *pInput, *pMix, *pOut, *pHold, *pSync, *pLength;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrippertronProcessor)
};
