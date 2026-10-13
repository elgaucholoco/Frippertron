#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    static const juce::String time = "time", feedback = "feedback", tone = "tone",
                              sat = "saturation", wow = "wow", flutter = "flutter",
                              spread = "spread", input = "input", mix = "mix",
                              out = "output", hold = "hold", sync = "sync", length = "length";
}

static constexpr float spreadMax = 0.2f;   // le canal droit peut être jusqu'à 20 % plus long

// longueurs proposées en mode SYNC : en temps (unité du chiffrage) ou en mesures
struct SyncLength { const char* name; double count; bool bars; };
static constexpr SyncLength syncLengths[] {
    { "1 beat", 1, false }, { "2 beats", 2, false }, { "1 bar", 1, true },  { "2 bars", 2, true },
    { "3 bars", 3, true },  { "4 bars", 4, true },   { "6 bars", 6, true }, { "8 bars", 8, true },
    { "12 bars", 12, true },{ "16 bars", 16, true }
};

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

    NormalisableRange<float> timeRange (0.25f, (float) maxLoopSeconds, 0.0f);
    timeRange.setSkewForCentre (5.0f);
    NormalisableRange<float> toneRange (1200.0f, 18000.0f, 0.0f);
    toneRange.setSkewForCentre (5000.0f);

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::time, 1 },     "Loop Time",  timeRange, 4.0f, secs));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::feedback, 1 }, "Feedback",   NormalisableRange<float> (0.0f, 1.0f), 0.85f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::tone, 1 },     "Tape Tone",  toneRange, 7000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::sat, 1 },      "Saturation", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::wow, 1 },      "Wow",        NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::flutter, 1 },  "Flutter",    NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::spread, 1 },   "Stereo Spread", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, 1 },    "Record Level", NormalisableRange<float> (-60.0f, 6.0f), 0.0f, db));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 },      "Mix",        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::out, 1 },      "Output",     NormalisableRange<float> (-24.0f, 6.0f), 0.0f, db));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { ids::hold, 1 },     "Hold", false));

    StringArray lengthNames;
    for (const auto& s : syncLengths)
        lengthNames.add (s.name);
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { ids::sync, 1 },   "Tempo Sync", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::length, 1 }, "Loop Length", lengthNames, 3));
    return l;
}

FrippertronProcessor::FrippertronProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pTime     = apvts.getRawParameterValue (ids::time);
    pFeedback = apvts.getRawParameterValue (ids::feedback);
    pTone     = apvts.getRawParameterValue (ids::tone);
    pSat      = apvts.getRawParameterValue (ids::sat);
    pWow      = apvts.getRawParameterValue (ids::wow);
    pFlutter  = apvts.getRawParameterValue (ids::flutter);
    pSpread   = apvts.getRawParameterValue (ids::spread);
    pInput    = apvts.getRawParameterValue (ids::input);
    pMix      = apvts.getRawParameterValue (ids::mix);
    pOut      = apvts.getRawParameterValue (ids::out);
    pHold     = apvts.getRawParameterValue (ids::hold);
    pSync     = apvts.getRawParameterValue (ids::sync);
    pLength   = apvts.getRawParameterValue (ids::length);
}

double FrippertronProcessor::computeLoopSeconds()
{
    // tempo et chiffrage de l'hôte ; on garde les dernières valeurs connues
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
        }

    hostBpmOut.store ((float) hostBpm);
    tsNumOut.store (tsNum);
    tsDenOut.store (tsDen);

    if (pSync->load() < 0.5f || ! hostHasTempo.load())
        return pTime->load();

    const auto idx = (size_t) juce::jlimit (0, (int) std::size (syncLengths) - 1, (int) pLength->load());
    const auto& len = syncLengths[idx];
    const double beats    = len.bars ? len.count * tsNum : len.count;
    const double quarters = beats * 4.0 / tsDen;
    double secs = quarters * 60.0 / hostBpm;

    // trop long ou trop court pour la bande : on divise / double, la boucle reste sur la grille
    while (secs > maxLoopSeconds) secs *= 0.5;
    while (secs < 0.25)           secs *= 2.0;
    return secs;
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
void FrippertronProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    // longueur max + étalement stéréo + marge de modulation
    tapeLength = (int) std::ceil (sr * (maxLoopSeconds * (1.0 + spreadMax) + 0.1));
    tape.setSize (2, tapeLength);
    tape.clear();
    writePos = 0;

    const double initial = computeLoopSeconds() * sr;
    smoothedDelay[0] = initial;
    smoothedDelay[1] = initial * (1.0 + spreadMax * pSpread->load());

    for (int c = 0; c < 2; ++c)
    {
        ch[c] = Channel();
        ch[c].wowPhase     = c * 0.37f;   // légers décalages : deux machines ne tournent jamais pareil
        ch[c].flutterPhase = c * 0.61f;
    }

    for (auto* s : { &feedbackS, &inputS, &mixS, &outS })
        s->reset (sr, 0.05);
    feedbackS.setCurrentAndTargetValue (pFeedback->load());
    inputS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInput->load(), -59.9f));
    mixS.setCurrentAndTargetValue (pMix->load());
    outS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    holdAmount = pHold->load() > 0.5f ? 1.0f : 0.0f;
    phaseCounter = 0.0;
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
    const int numCh  = juce::jmin (2, numOut);
    const int n      = buffer.getNumSamples();

    if (clearRequested.exchange (false))
    {
        tape.clear();
        for (auto& c : ch)
            c.hp = c.lp1 = c.lp2 = OnePole();
    }

    // ── paramètres de bloc ──
    feedbackS.setTargetValue (pFeedback->load());
    inputS.setTargetValue (juce::Decibels::decibelsToGain (pInput->load(), -59.9f));
    mixS.setTargetValue (pMix->load());
    outS.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    const float twoPi    = juce::MathConstants<float>::twoPi;
    const float toneA    = 1.0f - std::exp (-twoPi * pTone->load() / (float) sr);
    const float hpA      = 1.0f - std::exp (-twoPi * 35.0f / (float) sr);
    const float drive    = 1.0f + 5.0f * pSat->load();
    const float wow      = pWow->load(), flutter = pFlutter->load();
    const float wowDepth = wow * 0.006f * (float) sr;       // jusqu'à ±6 ms
    const float flutDepth= flutter * 0.0004f * (float) sr;  // jusqu'à ±0,4 ms
    const float wowInc   = 0.45f / (float) sr, flutInc = 6.5f / (float) sr;
    const float holdTarget = pHold->load() > 0.5f ? 1.0f : 0.0f;
    const float holdStep   = 1.0f / (0.05f * (float) sr);    // fondu de 50 ms
    const double glideA    = 1.0 - std::exp (-1.0 / (0.25 * sr)); // glissement « varispeed » ~250 ms

    const double maxDelay = tapeLength - 4.0 - (0.0065 * sr);
    double targetDelay[2];
    const double loopSecs = computeLoopSeconds();
    loopSecondsOut.store ((float) loopSecs);
    targetDelay[0] = juce::jlimit (16.0, maxDelay, loopSecs * sr);
    targetDelay[1] = juce::jlimit (16.0, maxDelay, targetDelay[0] * (1.0 + spreadMax * pSpread->load()));

    float sumSq = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float fb   = feedbackS.getNextValue();
        const float inG  = inputS.getNextValue();
        const float mix  = mixS.getNextValue();
        const float outG = outS.getNextValue();
        const float dryG = juce::jmin (1.0f, 2.0f * (1.0f - mix));
        const float wetG = juce::jmin (1.0f, 2.0f * mix);

        if (holdAmount < holdTarget)      holdAmount = juce::jmin (holdTarget, holdAmount + holdStep);
        else if (holdAmount > holdTarget) holdAmount = juce::jmax (holdTarget, holdAmount - holdStep);

        // entrées lues avant d'écrire la sortie : en mono → stéréo, le canal droit
        // relit l'entrée du canal 0, qui serait sinon déjà écrasée
        float dryIn[2] {};
        for (int c = 0; c < numCh; ++c)
            dryIn[c] = numIn > 0 ? buffer.getSample (juce::jmin (c, numIn - 1), i) : 0.0f;

        for (int c = 0; c < numCh; ++c)
        {
            auto& s = ch[c];
            float* tp = tape.getWritePointer (c);

            // longueur de bande : glisse vers la cible (déplacer les machines = varispeed)
            smoothedDelay[c] += (targetDelay[c] - smoothedDelay[c]) * glideA;

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

            double readPos = writePos - (smoothedDelay[c] + mod);
            while (readPos < 0.0) readPos += tapeLength;

            // tête de lecture de la seconde machine
            const float playback = readCubic (tp, readPos);

            // chaîne de réinjection : chaque passage sur bande use le son
            float t = s.hp.hp (playback, hpA);
            t = s.lp2.lp (s.lp1.lp (t, toneA), toneA);
            t = tapeSaturate (t, drive);

            const float dry = dryIn[c];

            // HOLD : recopie exacte d'un échantillon entier, sans wow ni interpolation,
            // pour que la boucle figée ne s'use pas d'un passage à l'autre
            int holdIdx = writePos - (int) std::lround (smoothedDelay[c]);
            if (holdIdx < 0) holdIdx += tapeLength;
            const float frozen = tp[holdIdx];

            // tête d'enregistrement de la première machine
            const float recorded = dry * inG + fb * t;
            tp[writePos] = (1.0f - holdAmount) * recorded + holdAmount * frozen;

            buffer.setSample (c, i, (dry * dryG + playback * wetG) * outG);
            sumSq += playback * playback;
        }

        if (++writePos >= tapeLength) writePos = 0;

        phaseCounter += 1.0;
        if (phaseCounter >= smoothedDelay[0]) phaseCounter -= smoothedDelay[0];
    }

    for (int c = numCh; c < numOut; ++c)
        buffer.clear (c, 0, n);

    loopPhase.store ((float) (phaseCounter / smoothedDelay[0]));
    const float rms = std::sqrt (sumSq / (float) juce::jmax (1, n * numCh));
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
