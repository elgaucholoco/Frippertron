// Tests hors ligne du traitement audio de Frippertron.
// On envoie des impulsions dans le plugin et on mesure où tombent les échos.
#include "../Source/PluginProcessor.h"
#include <cstdio>

namespace
{
    constexpr double sr = 44100.0;
    constexpr int block = 512;
    int failures = 0;

    std::string num (float v, int decimals = 3) { return juce::String (v, decimals).toStdString(); }

    void check (bool ok, const std::string& what)
    {
        std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what.c_str());
        if (! ok) ++failures;
    }

    struct TestPlayHead : juce::AudioPlayHead
    {
        double bpm = 120.0, ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setTimeSignature (TimeSignature { 4, 4 });
            p.setPpqPosition (ppq);
            p.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
            p.setIsPlaying (true);
            return p;
        }
    };

    struct Rig
    {
        FrippertronProcessor proc;
        TestPlayHead playHead;
        std::vector<float> outL, outR;

        explicit Rig (bool withHost = false)
        {
            if (withHost) proc.setPlayHead (&playHead);
            // son « propre » pour mesurer : pas de wow, flutter, souffle ni dry
            set ("wow", 0); set ("flutter", 0); set ("hiss", 0); set ("mix", 1);
            proc.setRateAndBufferSizeDetails (sr, block);
            proc.prepareToPlay (sr, block);
        }

        void set (const juce::String& id, float v)
        {
            auto* p = proc.apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }

        // traite `seconds` secondes ; input(t) donne (gauche, droite) ; atTime() appelé avant chaque bloc
        template <typename In, typename Hook>
        void run (double seconds, In input, Hook atTime)
        {
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            const int total = (int) (seconds * sr);
            for (int start = 0; start < total; start += block)
            {
                atTime (start / sr);
                for (int i = 0; i < block; ++i)
                {
                    const auto [l, r] = input (start + i);
                    buf.setSample (0, i, l);
                    buf.setSample (1, i, r);
                }
                proc.processBlock (buf, midi);
                playHead.ppq += block / sr * playHead.bpm / 60.0;
                for (int i = 0; i < block; ++i)
                {
                    outL.push_back (buf.getSample (0, i));
                    outR.push_back (buf.getSample (1, i));
                }
            }
        }

        template <typename In> void run (double seconds, In input) { run (seconds, input, [] (double) {}); }

        static float peak (const std::vector<float>& x, double t0, double t1)
        {
            float m = 0.0f;
            for (int i = (int) (t0 * sr); i < juce::jmin ((int) x.size(), (int) (t1 * sr)); ++i)
                m = juce::jmax (m, std::abs (x[(size_t) i]));
            return m;
        }

        float peakAround (double t, bool left = true) const { return peak (left ? outL : outR, t - 0.01, t + 0.01); }
    };

    // une impulsion à t = 0, sur le canal choisi
    auto impulse (bool left, bool right)
    {
        return [=] (int i) { const float v = i == 0 ? 1.0f : 0.0f;
                             return std::pair<float, float> { left ? v : 0.0f, right ? v : 0.0f }; };
    }
    auto silence() { return [] (int) { return std::pair<float, float> { 0.0f, 0.0f }; }; }

    // icône de l'application : une bobine de bande vue de face, sur le fond de l'interface
    juce::Image drawIcon (int size)
    {
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Graphics g (img);
        const float s = (float) size;

        // carré arrondi, marges au format des icônes macOS (824 / 1024)
        const auto box = juce::Rectangle<float> (s, s).reduced (s * 100.0f / 1024.0f);
        const float corner = box.getWidth() * 0.225f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2824), box.getX(), box.getY(),
                                                 juce::Colour (0xff141210), box.getX(), box.getBottom(), false));
        g.fillRoundedRectangle (box, corner);

        const auto c = box.getCentre().translated (0.0f, -box.getHeight() * 0.04f);
        const float r = box.getWidth() * 0.33f;

        // bande qui part de la bobine vers la tête d'enregistrement
        const juce::Point<float> head (box.getX() + box.getWidth() * 0.80f, box.getBottom() - box.getHeight() * 0.12f);
        juce::Path strand;
        strand.startNewSubPath (c.x - r * 0.55f, c.y + r * 0.62f);
        strand.lineTo (head.x - r * 0.12f, head.y);
        g.setColour (juce::Colour (0xff8a5a36));
        g.strokePath (strand, juce::PathStrokeType (s * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffc8452f));
        g.fillRoundedRectangle (juce::Rectangle<float> (r * 0.30f, r * 0.24f).withCentre (head), r * 0.05f);

        // bande enroulée
        const float tr = r * 0.80f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff70482a), c.x, c.y - tr,
                                                 juce::Colour (0xff4a2f1b), c.x, c.y + tr, false));
        g.fillEllipse (c.x - tr, c.y - tr, tr * 2, tr * 2);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float k = 0.48f; k < 0.80f; k += 0.07f)
            g.drawEllipse (c.x - r * k, c.y - r * k, r * k * 2, r * k * 2, s * 0.003f);

        // flasque métallique et ses trois fenêtres
        juce::Path flange;
        flange.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
        for (int i = 0; i < 3; ++i)
        {
            const float a = -juce::MathConstants<float>::halfPi + i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path window;
            window.addCentredArc (c.x, c.y, r * 0.66f, r * 0.66f, 0.0f, a - 0.62f, a + 0.62f, true);
            window.addCentredArc (c.x, c.y, r * 0.36f, r * 0.36f, 0.0f, a + 0.62f, a - 0.62f, false);
            window.closeSubPath();
            flange.addPath (window);
        }
        flange.setUsingNonZeroWinding (false);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d2c8), c.x, c.y - r,
                                                 juce::Colour (0xff6d665e), c.x, c.y + r, false));
        g.fillPath (flange);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, s * 0.004f);

        // moyeu
        const float hr = r * 0.22f;
        g.setColour (juce::Colour (0xff8a8279));
        g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
        g.setColour (juce::Colour (0xff1e1b18));
        for (int i = 0; i < 3; ++i)
        {
            const auto h = c.getPointOnCircumference (hr * 0.55f, i * juce::MathConstants<float>::twoPi / 3.0f);
            g.fillEllipse (h.x - hr * 0.22f, h.y - hr * 0.22f, hr * 0.44f, hr * 0.44f);
        }
        g.fillEllipse (c.x - hr * 0.16f, c.y - hr * 0.16f, hr * 0.32f, hr * 0.32f);

        // point ambre : le son qui circule
        g.setColour (juce::Colour (0xffe0a040));
        const auto dot = c.getPointOnCircumference (r * 1.0f, juce::MathConstants<float>::pi * 0.25f);
        g.fillEllipse (dot.x - r * 0.07f, dot.y - r * 0.07f, r * 0.14f, r * 0.14f);
        return img;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    // FrippertronTests --icon fichier.png : dessine l'icône 1024 × 1024 de l'application
    if (argc > 2 && juce::String (argv[1]) == "--icon")
    {
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        out.deleteFile();
        juce::FileOutputStream stream (out);
        return juce::PNGImageFormat().writeImageToStream (drawIcon (1024), stream) ? 0 : 1;
    }

    // FrippertronTests --snapshot fichier.png [single] : capture de l'interface, sans lancer les tests
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        Rig rig (true);
        rig.set ("b_sync", 1);
        if (argc > 3 && juce::String (argv[3]) == "single")
            rig.set ("decks", 0);
        rig.run (1.0, [] (int i) { const float v = std::sin (i * 0.03f) * 0.5f; return std::pair<float, float> { v, v }; });
        std::unique_ptr<juce::AudioProcessorEditor> editor (rig.proc.createEditor());
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        out.deleteFile();
        juce::FileOutputStream stream (out);
        return juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
    }

    std::printf ("Routage stéréo (L > A 4 s, R > B 6 s)\n");
    {
        Rig rig;
        rig.run (7.0, impulse (true, false));
        check (rig.peakAround (4.0) > 0.5f, std::string ("écho de la ligne A à 4 s (") + num (rig.peakAround (4.0), 3) + ")");
        check (rig.peakAround (6.0) < 0.01f && rig.peakAround (6.0, false) < 0.01f, "rien sur la ligne B : l'entrée droite est muette");
        check (rig.peakAround (4.0, false) < rig.peakAround (4.0) * 0.5f, "A est placée à gauche (pan L70)");
    }

    std::printf ("Routage Mono > A+B\n");
    {
        Rig rig;
        rig.set ("routing", 3);
        rig.proc.prepareToPlay (sr, block);
        rig.run (7.0, impulse (true, false));
        check (rig.peakAround (4.0) > 0.2f, "écho de A à 4 s");
        check (rig.peakAround (6.0, false) > 0.2f, "écho de B à 6 s, à droite");
        check (Rig::peak (rig.outL, 0.05, 3.9) < 0.01f, "rien avant le premier écho");
    }

    std::printf ("Routage Mono > B\n");
    {
        Rig rig;
        rig.set ("routing", 2);
        rig.proc.prepareToPlay (sr, block);
        rig.run (7.0, impulse (true, true));
        check (rig.peakAround (4.0) < 0.01f && rig.peakAround (4.0, false) < 0.01f, "A reste muette");
        check (rig.peakAround (6.0, false) > 0.5f, "B reçoit la somme mono");
    }

    std::printf ("1 REVOX : la ligne B suit la durée de A\n");
    {
        Rig rig;
        rig.set ("decks", 0); rig.set ("routing", 3);
        rig.set ("b_time", 6.0f); rig.set ("b_feedback", 0.0f);   // ignorés en 1 REVOX
        rig.proc.prepareToPlay (sr, block);
        rig.run (9.0, impulse (true, false));
        check (rig.peakAround (4.0, false) > 0.2f, "écho de B à 4 s, comme A");
        check (rig.peakAround (6.0, false) < 0.01f, "rien à 6 s");
        check (rig.peakAround (8.0, false) > 0.05f, "B réinjecte avec le feedback de A (second passage à 8 s)");
        check (std::abs (rig.proc.getLoopSeconds (1) - 4.0f) < 0.001f, "durée affichée de B : 4.00 s");
    }

    std::printf ("Réinjection : les passages suivants décroissent\n");
    {
        Rig rig;
        rig.set ("b_feedback", 0); rig.set ("a_feedback", 0.8f);
        rig.run (17.0, impulse (true, false));
        const float e1 = rig.peakAround (4.0), e2 = rig.peakAround (8.0), e3 = rig.peakAround (12.0);
        check (e2 < e1 && e3 < e2 && e3 > 0.05f, std::string ("4 s > 8 s > 12 s (") + num (e1, 3) + ", "
                                                  + num (e2, 3) + ", " + num (e3, 3) + ")");
    }

    std::printf ("REHEARSE\n");
    {
        Rig rig;
        rig.set ("rehearse", 1);
        rig.proc.prepareToPlay (sr, block);
        rig.run (5.0, impulse (true, true));
        check (rig.peakAround (4.0) < 0.001f, "rien n'est enregistré");
    }

    std::printf ("ERASE (automation du paramètre)\n");
    {
        Rig rig;
        rig.run (5.0, impulse (true, false), [&] (double t) { if (t > 2.0 && t < 2.1) rig.set ("erase", 1); });
        check (rig.peakAround (4.0) < 0.001f, "la bande est effacée avant l'écho");
    }

    std::printf ("HOLD : la boucle figée ne s'use pas\n");
    {
        Rig rig;
        rig.set ("a_feedback", 0.5f);
        rig.run (25.0, impulse (true, false), [&] (double t) { if (t > 1.0 && t < 1.1) rig.set ("hold", 1); });
        const float first = rig.peakAround (4.0), late = rig.peakAround (24.0);
        check (std::abs (late - first) < 0.02f * first, "même niveau à 4 s et à 24 s ("
                                                        + num (first, 3) + " / " + num (late, 3) + ")");
    }

    std::printf ("SYNC : 2 mesures à 120 BPM = 4 s\n");
    {
        Rig rig (true);
        rig.set ("a_time", 1.0f);           // ignoré en SYNC
        rig.set ("a_sync", 1); rig.set ("a_length", 3);
        rig.proc.prepareToPlay (sr, block);
        rig.run (5.0, impulse (true, false));
        check (rig.peakAround (4.0) > 0.5f && rig.peakAround (1.0) < 0.01f, "écho à 4 s, pas à 1 s");
        check (std::abs (rig.proc.getLoopSeconds (0) - 4.0f) < 0.001f, "durée affichée 4.00 s");
    }

    std::printf ("SYNC : 16 mesures à 60 BPM = 64 s, divisé par 2 -> 32 s\n");
    {
        Rig rig (true);
        rig.playHead.bpm = 60.0;
        rig.set ("a_sync", 1); rig.set ("a_length", 11);
        rig.run (0.1, silence());
        check (std::abs (rig.proc.getLoopSeconds (0) - 32.0f) < 0.001f, "durée 32 s ("
                                                                          + num (rig.proc.getLoopSeconds (0), 2) + ")");
    }

    std::printf ("Pan automatique\n");
    {
        Rig rig (true);
        rig.set ("routing", 1); rig.set ("a_time", 0.25f); rig.set ("a_feedback", 1.0f);
        rig.set ("a_pan", -1.0f); rig.set ("a_motion", 1);   // aller-retour en 1 mesure (2 s)
        rig.proc.prepareToPlay (sr, block);
        rig.run (4.0, [] (int i) { const float v = std::sin (i * 0.05f) * 0.3f; return std::pair<float, float> { v, v }; });
        // à t = 3 s (phase 0,5), la ligne est passée à droite
        const float l = Rig::peak (rig.outL, 2.95, 3.05), r = Rig::peak (rig.outR, 2.95, 3.05);
        check (r > 3.0f * l, std::string ("la ligne a traversé vers la droite (L ") + num (l, 3) + ", R " + num (r, 3) + ")");
    }

    std::printf ("Stabilité : 100 %% de réinjection, saturation et souffle au maximum, 60 s de bruit\n");
    {
        Rig rig;
        rig.set ("routing", 3); rig.set ("a_feedback", 1); rig.set ("b_feedback", 1);
        rig.set ("saturation", 1); rig.set ("hiss", 1); rig.set ("wow", 1); rig.set ("flutter", 1);
        rig.set ("a_time", 0.5f); rig.set ("b_time", 0.7f); rig.set ("input", 6);
        juce::Random rnd (1);
        rig.run (60.0, [&] (int) { const float v = rnd.nextFloat() - 0.5f; return std::pair<float, float> { v, v }; });
        bool finite = true;
        for (auto v : rig.outL) finite &= std::isfinite (v);
        const float p = Rig::peak (rig.outL, 50.0, 60.0);
        check (finite && p < 4.0f, std::string ("sortie finie et bornée (crête ") + num (p, 2) + ")");
    }

    std::printf ("Presets\n");
    {
        Rig rig;
        rig.proc.setCurrentProgram (3);     // Polymeter 4:5
        auto get = [&] (const char* id) { auto* p = rig.proc.apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
        check (get ("a_sync") > 0.5f && (int) get ("a_length") == 5 && (int) get ("b_length") == 6, "Polymeter 4:5 : A 4 mesures, B 5 mesures");
        rig.proc.setCurrentProgram (1);     // Classic Revox
        check ((int) get ("decks") == 0, "Classic Revox : 1 REVOX");
        rig.set ("hold", 1);
        rig.proc.setCurrentProgram (0);
        check (get ("a_sync") < 0.5f && std::abs (get ("a_time") - 4.0f) < 0.001f, "Init remet les valeurs par défaut");
        check (get ("hold") > 0.5f, "un preset ne touche pas à HOLD");
    }

    std::printf ("\n%s (%d échec%s)\n", failures == 0 ? "TOUT EST BON" : "ÉCHECS", failures, failures > 1 ? "s" : "");
    return failures == 0 ? 0 : 1;
}
