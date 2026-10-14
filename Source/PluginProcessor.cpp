#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    // globaux
    static const juce::String tone = "tone", sat = "saturation", wow = "wow", flutter = "flutter",
                              hiss = "hiss", input = "input", mix = "mix", out = "output",
                              hold = "hold", rehearse = "rehearse", erase = "erase", routing = "routing";

    // par ligne : préfixe « a_ » ou « b_ »
    static const juce::String time = "time", sync = "sync", length = "length",
                              feedback = "feedback", pan = "pan", motion = "motion";

    static juce::String line (int l, const juce::String& id) { return juce::String (l == 0 ? "a_" : "b_") + id; }
}

// longueurs proposées en mode SYNC : en temps (unité du chiffrage) ou en mesures
struct SyncLength { const char* name; double count; bool bars; };
static constexpr SyncLength syncLengths[] {
    { "1 beat", 1, false }, { "2 beats", 2, false }, { "1 bar", 1, true },  { "2 bars", 2, true },
    { "3 bars", 3, true },  { "4 bars", 4, true },   { "5 bars", 5, true }, { "6 bars", 6, true },
    { "7 bars", 7, true },  { "8 bars", 8, true },   { "12 bars", 12, true }, { "16 bars", 16, true }
};

// pan automatique : durée d'un aller-retour, en mesures (0 = coupé)
static constexpr int motionBars[] { 0, 1, 2, 4, 8, 16, 32 };

// routage de l'entrée : gains (gauche, droite) vers chaque ligne
static constexpr float routeGains[4][FrippertronProcessor::numLines][2] {
    { { 1.0f, 0.0f }, { 0.0f, 1.0f } },     // stéréo : L > A, R > B
    { { 0.5f, 0.5f }, { 0.0f, 0.0f } },     // mono > A
    { { 0.0f, 0.0f }, { 0.5f, 0.5f } },     // mono > B
    { { 0.5f, 0.5f }, { 0.5f, 0.5f } },     // mono > A+B
};

static const juce::StringArray routingNames { "Stereo (L>A R>B)", "Mono > A", "Mono > B", "Mono > A+B" };

//==============================================================================
// presets d'usine : on repart des valeurs par défaut, puis on applique ces valeurs
// (les paramètres à choix prennent leur index)
struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

static const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init", {} },
        { "Classic Revox",     { { "routing", 1 }, { "a_time", 4.0f }, { "a_feedback", 0.9f }, { "a_pan", 0.0f },
                                 { "tone", 6000.0f }, { "wow", 0.3f } } },
        { "Two Revox Drift",   { { "routing", 3 }, { "a_time", 4.0f }, { "b_time", 5.3f },
                                 { "a_pan", -0.8f }, { "b_pan", 0.8f }, { "a_feedback", 0.88f }, { "b_feedback", 0.88f } } },
        { "Polymeter 4:5",     { { "routing", 3 }, { "a_sync", 1 }, { "b_sync", 1 }, { "a_length", 5 }, { "b_length", 6 },
                                 { "a_pan", -0.6f }, { "b_pan", 0.6f } } },
        { "Swirl",             { { "routing", 3 }, { "a_time", 6.0f }, { "b_time", 7.5f }, { "a_motion", 3 }, { "b_motion", 4 },
                                 { "a_pan", -0.9f }, { "b_pan", 0.9f }, { "a_feedback", 0.9f }, { "b_feedback", 0.9f },
                                 { "wow", 0.4f } } },
        { "Worn Tape",         { { "tone", 2500.0f }, { "saturation", 0.7f }, { "hiss", 0.5f }, { "wow", 0.5f },
                                 { "flutter", 0.3f }, { "a_feedback", 0.93f }, { "b_feedback", 0.93f } } },
        { "Long Ambient",      { { "routing", 3 }, { "a_time", 30.0f }, { "b_time", 41.0f }, { "a_feedback", 0.95f },
                                 { "b_feedback", 0.95f }, { "tone", 5000.0f }, { "hiss", 0.3f }, { "mix", 0.6f } } },
    };
    return presets;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout FrippertronProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto secs = AudioParameterFloatAttributes().withLabel ("s")
        .withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; });
    auto pct  = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hz   = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                                              : String (roundToInt (v)) + " Hz"; });
    auto db   = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? String ("-inf dB")
                                                                             : String (v, 1) + " dB"; });
    auto pan  = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) {
            const int p = roundToInt (v * 100.0f);
            return p == 0 ? String ("C") : (p < 0 ? "L" + String (-p) : "R" + String (p)); });

    NormalisableRange<float> timeRange (0.25f, (float) maxLoopSeconds, 0.0f);
    timeRange.setSkewForCentre (6.0f);
    NormalisableRange<float> toneRange (1200.0f, 18000.0f, 0.0f);
    toneRange.setSkewForCentre (5000.0f);

    StringArray lengthNames, motionNames;
    for (const auto& s : syncLengths)
        lengthNames.add (s.name);
    for (auto b : motionBars)
        motionNames.add (b == 0 ? String ("Off") : String (b) + (b == 1 ? " bar" : " bars"));

    const int v = 2;   // version des identifiants de paramètres

    // ── deux lignes de bande ──
    const float defTime[]   { 4.0f, 6.0f };
    const int   defLength[] { 3, 4 };          // 2 mesures, 3 mesures
    const float defPan[]    { -0.7f, 0.7f };
    for (int line = 0; line < numLines; ++line)
    {
        const String n = line == 0 ? "A " : "B ";
        l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::line (line, ids::time), v },     n + "Loop Time", timeRange, defTime[line], secs));
        l.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::line (line, ids::sync), v },     n + "Tempo Sync", false));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::line (line, ids::length), v },   n + "Loop Length", lengthNames, defLength[line]));
        l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::line (line, ids::feedback), v }, n + "Feedback", NormalisableRange<float> (0.0f, 1.0f), 0.85f, pct));
        l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::line (line, ids::pan), v },      n + "Pan", NormalisableRange<float> (-1.0f, 1.0f), defPan[line], pan));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::line (line, ids::motion), v },   n + "Pan Motion", motionNames, 0));
    }

    // ── bande et commandes globales ──
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::tone, v },     "Tape Tone",  toneRange, 7000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::sat, v },      "Saturation", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::wow, v },      "Wow",        NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::flutter, v },  "Flutter",    NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::hiss, v },     "Tape Hiss",  NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::input, v },    "Record Level", NormalisableRange<float> (-60.0f, 6.0f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::mix, v },      "Mix",        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { ids::out, v },      "Output",     NormalisableRange<float> (-24.0f, 6.0f), 0.0f, db));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::routing, v },  "Routing",    routingNames, 0));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::hold, v },     "Hold", false));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::rehearse, v }, "Rehearse", false));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::erase, v },    "Erase", false));
    return l;
}

FrippertronProcessor::FrippertronProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int l = 0; l < numLines; ++l)
    {
        lp[l].time     = apvts.getRawParameterValue (ids::line (l, ids::time));
        lp[l].sync     = apvts.getRawParameterValue (ids::line (l, ids::sync));
        lp[l].length   = apvts.getRawParameterValue (ids::line (l, ids::length));
        lp[l].feedback = apvts.getRawParameterValue (ids::line (l, ids::feedback));
        lp[l].pan      = apvts.getRawParameterValue (ids::line (l, ids::pan));
        lp[l].motion   = apvts.getRawParameterValue (ids::line (l, ids::motion));
    }
    pTone     = apvts.getRawParameterValue (ids::tone);
    pSat      = apvts.getRawParameterValue (ids::sat);
    pWow      = apvts.getRawParameterValue (ids::wow);
    pFlutter  = apvts.getRawParameterValue (ids::flutter);
    pHiss     = apvts.getRawParameterValue (ids::hiss);
    pInput    = apvts.getRawParameterValue (ids::input);
    pMix      = apvts.getRawParameterValue (ids::mix);
    pOut      = apvts.getRawParameterValue (ids::out);
    pRouting  = apvts.getRawParameterValue (ids::routing);
    pHold     = apvts.getRawParameterValue (ids::hold);
    pRehearse = apvts.getRawParameterValue (ids::rehearse);
    pErase    = apvts.getRawParameterValue (ids::erase);
}

bool FrippertronProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto& in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

//==============================================================================
int FrippertronProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String FrippertronProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? factoryPresets()[(size_t) index].name : "";
}

void FrippertronProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;

    // HOLD, REHEARSE et ERASE sont des commandes de jeu : un preset n'y touche pas
    static const juce::StringArray performance { ids::hold, ids::rehearse, ids::erase };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! performance.contains (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : factoryPresets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

//==============================================================================
void FrippertronProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    // longueur max + marge de modulation
    tapeLength = (int) std::ceil (sr * (maxLoopSeconds + 0.1));
    writePos = 0;

    readHostPosition();

    for (int l = 0; l < numLines; ++l)
    {
        auto& line = lines[l];
        line.tape.allocate ((size_t) tapeLength, true);
        line.reset();
        line.wowPhase      = l * 0.37f;   // légers décalages : deux machines ne tournent jamais pareil
        line.flutterPhase  = l * 0.61f;
        line.drift = line.driftTarget = 0.0f;
        line.driftCounter  = 0;
        line.smoothedDelay = computeLoopSeconds (l) * sr;
        line.phaseCounter  = 0.0;
        line.panPhase      = 0.0;
        line.panSmoothed   = lp[l].pan->load();

        for (auto* s : { &line.feedbackS, &line.inLS, &line.inRS })
            s->reset (sr, 0.05);
        line.feedbackS.setCurrentAndTargetValue (lp[l].feedback->load());
        const int route = juce::jlimit (0, 3, (int) pRouting->load());
        line.inLS.setCurrentAndTargetValue (routeGains[route][l][0]);
        line.inRS.setCurrentAndTargetValue (routeGains[route][l][1]);
    }

    for (auto* s : { &inputS, &mixS, &outS })
        s->reset (sr, 0.05);
    inputS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInput->load(), -59.9f));
    mixS.setCurrentAndTargetValue (pMix->load());
    outS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    holdAmount     = pHold->load() > 0.5f ? 1.0f : 0.0f;
    rehearseAmount = pRehearse->load() > 0.5f ? 1.0f : 0.0f;
    lastEraseParam = pErase->load() > 0.5f;
}

void FrippertronProcessor::readHostPosition()
{
    // tempo, chiffrage et position de l'hôte ; on garde les dernières valeurs connues
    hostPpqValid = false;
    bool playing = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
            {
                hostBpm = *bpm;
                hostHasTempo.store (true);
            }
            if (auto ts = pos->getTimeSignature(); ts.hasValue() && ts->numerator > 0 && ts->denominator > 0)
            {
                tsNum = ts->numerator;
                tsDen = ts->denominator;
            }
            playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition(); ppq.hasValue())
            {
                hostPpq = *ppq;
                hostPpqValid = true;
                const double barStart = pos->getPpqPositionOfLastBarStart().orFallback (0.0);
                beatInBar.store ((float) ((hostPpq - barStart) * tsDen / 4.0));
            }
        }

    hostPlaying.store (playing);
    hostBpmOut.store ((float) hostBpm);
    tsNumOut.store (tsNum);
    tsDenOut.store (tsDen);
}

double FrippertronProcessor::computeLoopSeconds (int line) const
{
    const auto& p = lp[line];
    if (p.sync->load() < 0.5f || ! hostHasTempo.load())
        return p.time->load();

    const auto idx = (size_t) juce::jlimit (0, (int) std::size (syncLengths) - 1, (int) p.length->load());
    const auto& len = syncLengths[idx];
    const double beats    = len.bars ? len.count * tsNum : len.count;
    const double quarters = beats * 4.0 / tsDen;
    double secs = quarters * 60.0 / hostBpm;

    // trop long ou trop court pour la bande : on divise / double, la boucle reste sur la grille
    while (secs > maxLoopSeconds) secs *= 0.5;
    while (secs < 0.25)           secs *= 2.0;
    return secs;
}

double FrippertronProcessor::motionCycleSeconds (int line) const
{
    const auto idx = (size_t) juce::jlimit (0, (int) std::size (motionBars) - 1, (int) lp[line].motion->load());
    if (motionBars[idx] == 0)
        return 0.0;
    // sans tempo hôte, on suppose 120 BPM
    const double quarters = motionBars[idx] * tsNum * 4.0 / tsDen;
    return quarters * 60.0 / hostBpm;
}

float FrippertronProcessor::readCubic (const float* buf, double pos) const noexcept
{
    // interpolation d'Hermite à 4 points sur le tampon circulaire
    const int i1 = (int) pos;
    const float t = (float) (pos - i1);
    auto at = [&] (int i) { i %= tapeLength; if (i < 0) i += tapeLength; return buf[i]; };
    const float y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

float FrippertronProcessor::tapeSaturate (float x, float drive) noexcept
{
    // gain unitaire à bas niveau, compression douce au-delà : la bande « tasse » les crêtes
    return std::tanh (x * drive) / drive;
}

void FrippertronProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    readHostPosition();

    // ERASE : bouton de l'interface, ou front montant du paramètre (automation, MIDI)
    const bool eraseParam = pErase->load() > 0.5f;
    if (clearRequested.exchange (false) || (eraseParam && ! lastEraseParam))
        for (auto& line : lines)
        {
            juce::FloatVectorOperations::clear (line.tape.get(), tapeLength);
            line.reset();
        }
    lastEraseParam = eraseParam;

    // ── paramètres de bloc ──
    inputS.setTargetValue (juce::Decibels::decibelsToGain (pInput->load(), -59.9f));
    mixS.setTargetValue (pMix->load());
    outS.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    const float twoPi    = juce::MathConstants<float>::twoPi;
    const float toneA    = 1.0f - std::exp (-twoPi * pTone->load() / (float) sr);
    const float hpA      = 1.0f - std::exp (-twoPi * 35.0f / (float) sr);
    const float panA     = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));   // lissage du pan ~20 ms
    const float drive    = 1.0f + 5.0f * pSat->load();
    const float wow      = pWow->load(), flutter = pFlutter->load();
    const float wowDepth = wow * 0.006f * (float) sr;       // jusqu'à ±6 ms
    const float flutDepth= flutter * 0.0004f * (float) sr;  // jusqu'à ±0,4 ms
    const float wowInc   = 0.45f / (float) sr, flutInc = 6.5f / (float) sr;
    const float hissAmp  = pHiss->load() * 0.003f;          // jusqu'à -50 dBFS par passage
    const float holdTarget     = pHold->load() > 0.5f ? 1.0f : 0.0f;
    const float rehearseTarget = pRehearse->load() > 0.5f ? 1.0f : 0.0f;
    const float fadeStep = 1.0f / (0.05f * (float) sr);     // fondus de 50 ms
    const double glideA  = 1.0 - std::exp (-1.0 / (0.25 * sr)); // glissement « varispeed » ~250 ms
    const double maxDelay = tapeLength - 4.0 - (0.0065 * sr);

    const int route = juce::jlimit (0, 3, (int) pRouting->load());

    double targetDelay[numLines], panInc[numLines];
    float panPos[numLines];
    for (int l = 0; l < numLines; ++l)
    {
        auto& line = lines[l];
        const double loopSecs = computeLoopSeconds (l);
        lineView[l].seconds.store ((float) loopSecs);
        targetDelay[l] = juce::jlimit (16.0, maxDelay, loopSecs * sr);

        line.feedbackS.setTargetValue (lp[l].feedback->load());
        line.inLS.setTargetValue (routeGains[route][l][0]);
        line.inRS.setTargetValue (routeGains[route][l][1]);

        // pan automatique : calé sur la position de l'hôte quand il joue, libre sinon
        panPos[l] = lp[l].pan->load();
        const double cycle = motionCycleSeconds (l);
        panInc[l] = cycle > 0.0 ? 1.0 / (cycle * sr) : 0.0;
        if (cycle > 0.0 && hostPpqValid && hostPlaying.load())
        {
            const double cycleQuarters = cycle * hostBpm / 60.0;
            line.panPhase = std::fmod (hostPpq / cycleQuarters, 1.0);
        }
    }

    float sumSq = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float inG  = inputS.getNextValue();
        const float mix  = mixS.getNextValue();
        const float outG = outS.getNextValue();
        const float dryG = juce::jmin (1.0f, 2.0f * (1.0f - mix));
        const float wetG = juce::jmin (1.0f, 2.0f * mix);

        if (holdAmount < holdTarget)      holdAmount = juce::jmin (holdTarget, holdAmount + fadeStep);
        else if (holdAmount > holdTarget) holdAmount = juce::jmax (holdTarget, holdAmount - fadeStep);
        if (rehearseAmount < rehearseTarget)      rehearseAmount = juce::jmin (rehearseTarget, rehearseAmount + fadeStep);
        else if (rehearseAmount > rehearseTarget) rehearseAmount = juce::jmax (rehearseTarget, rehearseAmount - fadeStep);

        // entrées lues avant d'écrire la sortie (en mono, la droite reprend la gauche)
        const float inL = numIn > 0 ? buffer.getSample (0, i) : 0.0f;
        const float inR = numIn > 1 ? buffer.getSample (1, i) : inL;
        const float recG = inG * (1.0f - rehearseAmount);   // REHEARSE : on entend, on n'enregistre pas

        float wetL = 0.0f, wetR = 0.0f;

        for (int l = 0; l < numLines; ++l)
        {
            auto& s = lines[l];
            float* tp = s.tape.get();

            // longueur de bande : glisse vers la cible (déplacer les machines = varispeed)
            s.smoothedDelay += (targetDelay[l] - s.smoothedDelay) * glideA;

            // instabilités de vitesse
            s.wowPhase += wowInc;         if (s.wowPhase >= 1.0f) s.wowPhase -= 1.0f;
            s.flutterPhase += flutInc;    if (s.flutterPhase >= 1.0f) s.flutterPhase -= 1.0f;
            if (--s.driftCounter <= 0)
            {
                s.driftTarget  = rng.nextFloat() * 2.0f - 1.0f;
                s.driftCounter = (int) (sr * (0.3 + 0.7 * rng.nextDouble()));
            }
            s.drift += (s.driftTarget - s.drift) * (2.0f / (float) sr);

            const float mod = wowDepth * (0.7f * std::sin (twoPi * s.wowPhase) + 0.3f * s.drift)
                            + flutDepth * std::sin (twoPi * s.flutterPhase);

            double readPos = writePos - (s.smoothedDelay + mod);
            while (readPos < 0.0) readPos += tapeLength;

            // tête de lecture de la seconde machine
            const float playback = readCubic (tp, readPos);

            // chaîne de réinjection : chaque passage sur bande use le son
            float t = s.hp.hp (playback, hpA);
            t = s.lp2.lp (s.lp1.lp (t, toneA), toneA);
            t = tapeSaturate (t, drive);

            // HOLD : recopie exacte d'un échantillon entier, sans wow ni interpolation,
            // pour que la boucle figée ne s'use pas d'un passage à l'autre
            int holdIdx = writePos - (int) std::lround (s.smoothedDelay);
            if (holdIdx < 0) holdIdx += tapeLength;
            const float frozen = tp[holdIdx];

            // tête d'enregistrement de la première machine (+ souffle de la bande)
            const float lineIn   = s.inLS.getNextValue() * inL + s.inRS.getNextValue() * inR;
            const float hiss     = hissAmp * (rng.nextFloat() * 2.0f - 1.0f);
            const float recorded = lineIn * recG + s.feedbackS.getNextValue() * t + hiss;
            tp[writePos] = (1.0f - holdAmount) * recorded + holdAmount * frozen;

            // place dans l'image stéréo (balance : gain unitaire au centre)
            float panTarget = panPos[l];
            if (panInc[l] > 0.0)
            {
                panTarget *= std::cos (twoPi * (float) s.panPhase);
                s.panPhase += panInc[l];
                if (s.panPhase >= 1.0) s.panPhase -= 1.0;
            }
            s.panSmoothed += (panTarget - s.panSmoothed) * panA;
            wetL += playback * juce::jmin (1.0f, 1.0f - s.panSmoothed);
            wetR += playback * juce::jmin (1.0f, 1.0f + s.panSmoothed);

            sumSq += playback * playback;

            s.phaseCounter += 1.0;
            if (s.phaseCounter >= s.smoothedDelay) s.phaseCounter -= s.smoothedDelay;
        }

        if (++writePos >= tapeLength) writePos = 0;

        if (numOut >= 2)
        {
            buffer.setSample (0, i, (inL * dryG + wetL * wetG) * outG);
            buffer.setSample (1, i, (inR * dryG + wetR * wetG) * outG);
        }
        else if (numOut == 1)
        {
            buffer.setSample (0, i, (inL * dryG + 0.5f * (wetL + wetR) * wetG) * outG);
        }
    }

    for (int c = 2; c < numOut; ++c)
        buffer.clear (c, 0, n);

    for (int l = 0; l < numLines; ++l)
        lineView[l].phase.store ((float) (lines[l].phaseCounter / lines[l].smoothedDelay));

    const float rms = std::sqrt (sumSq / (float) juce::jmax (1, n * numLines));
    // retombée de l'indicateur indépendante de la taille de bloc (~300 ms)
    levelAcc = juce::jmax (rms, levelAcc * std::exp (-(float) n / (0.3f * (float) sr)));
    loopLevel.store (levelAcc);
}

//==============================================================================
void FrippertronProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void FrippertronProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FrippertronProcessor::createEditor()
{
    return new FrippertronEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrippertronProcessor();
}
