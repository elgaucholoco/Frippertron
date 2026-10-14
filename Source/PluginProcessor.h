#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

/*  Frippertron — émulation du dispositif « Frippertronics »
    Deux magnétophones Revox : la bande enregistrée par la tête d'enregistrement
    du premier passe par la tête de lecture du second, dont la sortie est renvoyée
    à l'entrée du premier. La distance entre les deux machines fixe la longueur
    de la boucle ; le niveau de réinjection fixe la vitesse à laquelle les couches
    anciennes s'effacent. Chaque passage sur bande ajoute perte d'aigus, souffle,
    saturation et instabilités de vitesse (wow & flutter).

    Depuis la v2, deux lignes indépendantes (A et B) : chacune est une paire de
    Revox avec sa propre longueur de boucle, sa réinjection et sa place dans
    l'image stéréo.                                                             */

class FrippertronProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numLines = 2;
    static constexpr double maxLoopSeconds = 60.0;

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

    // presets d'usine
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── interface pour l'éditeur ──
    void requestClear() noexcept { clearRequested.store (true); }
    float getLoopPhase (int line) const noexcept   { return lineView[line].phase.load(); }   // 0..1
    float getLoopSeconds (int line) const noexcept { return lineView[line].seconds.load(); }
    float getLoopLevel() const noexcept  { return loopLevel.load(); }   // niveau RMS des bandes
    bool  hasHostTempo() const noexcept  { return hostHasTempo.load(); }
    bool  isHostPlaying() const noexcept { return hostPlaying.load(); }
    float getHostBpm() const noexcept    { return hostBpmOut.load(); }
    int   getTimeSigNum() const noexcept { return tsNumOut.load(); }
    int   getTimeSigDen() const noexcept { return tsDenOut.load(); }
    float getBeatInBar() const noexcept  { return beatInBar.load(); }  // en temps, depuis le début de mesure

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct OnePole              // filtre passe-bas / passe-haut 1 pôle
    {
        float z = 0.0f;
        float lp (float x, float a) noexcept { z += a * (x - z); return z; }
        float hp (float x, float a) noexcept { return x - lp (x, a); }
    };

    struct LineParams           // pointeurs vers les paramètres d'une ligne
    {
        std::atomic<float> *time, *sync, *length, *feedback, *pan, *motion;
    };

    struct Line                 // une paire de Revox : une bande, une tête REC, une tête PLAY
    {
        juce::HeapBlock<float> tape;
        OnePole lp1, lp2, hp;
        float wowPhase = 0.0f, flutterPhase = 0.0f;
        float driftTarget = 0.0f, drift = 0.0f;
        int   driftCounter = 0;
        double smoothedDelay = 0.0;         // en échantillons, glisse comme un varispeed
        double phaseCounter = 0.0;          // position dans la boucle, pour l'affichage
        double panPhase = 0.0;              // phase du pan automatique, 0..1
        float panSmoothed = 0.0f;
        juce::SmoothedValue<float> feedbackS, inLS, inRS;   // gains d'entrée selon le routage
        void reset() { lp1 = lp2 = hp = OnePole(); }
    };

    struct LineView             // copies lisibles par l'éditeur
    {
        std::atomic<float> phase { 0.0f }, seconds { 4.0f };
    };

    void readHostPosition();
    double computeLoopSeconds (int line) const;
    double motionCycleSeconds (int line) const;   // 0 si pan automatique coupé
    float readCubic (const float* buf, double pos) const noexcept;
    static float tapeSaturate (float x, float drive) noexcept;

    Line lines[numLines];
    LineParams lp[numLines];
    LineView lineView[numLines];
    int tapeLength = 0;
    int writePos = 0;
    double sr = 44100.0;
    juce::Random rng;

    juce::SmoothedValue<float> inputS, mixS, outS;
    float holdAmount = 0.0f;                // 0..1, fondus pour éviter les clics
    float rehearseAmount = 0.0f;

    std::atomic<bool>  clearRequested { false };
    bool lastEraseParam = false;
    std::atomic<float> loopLevel { 0.0f };
    float levelAcc = 0.0f;

    // tempo et position de l'hôte (thread audio) et copies lisibles par l'éditeur
    double hostBpm = 120.0, hostPpq = 0.0;
    int tsNum = 4, tsDen = 4;
    bool hostPpqValid = false;
    std::atomic<bool>  hostHasTempo { false }, hostPlaying { false };
    std::atomic<float> hostBpmOut { 120.0f }, beatInBar { 0.0f };
    std::atomic<int>   tsNumOut { 4 }, tsDenOut { 4 };

    int currentProgram = 0;

    // pointeurs paramètres globaux
    std::atomic<float> *pTone, *pSat, *pWow, *pFlutter, *pHiss, *pInput, *pMix, *pOut,
                       *pHold, *pRehearse, *pErase, *pRouting;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrippertronProcessor)
};
