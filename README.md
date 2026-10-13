# Frippertron

Plugin d'effet (VST3 / AU / application autonome) pour macOS, qui reprend le principe des **Frippertronics** de Robert Fripp. Le dispositif d'origine relie deux magnétophones Revox par une même bande. La machine A enregistre. La machine B, placée à distance, relit la bande et renvoie le signal à l'entrée de A. Le son se superpose ainsi en couches qui s'effacent peu à peu.

## Ce que fait le plugin

| Réglage | Rôle dans le dispositif d'origine |
|---|---|
| **Loop Time** (0,25–30 s) | Distance entre les deux machines. La changer en cours de jeu fait glisser la hauteur (effet varispeed). |
| **Feedback** | Niveau renvoyé de B vers A : vitesse d'effacement des couches anciennes. |
| **Rec Level** | Niveau d'entrée enregistré sur la bande. |
| **Mix** | Dosage entre le signal direct et la bande. À 50 %, les deux sont à plein niveau. |
| **Output** | Volume de sortie. |
| **Tape Tone** | Perte d'aigus à chaque passage sur bande (filtre passe-bas 2 pôles, plus un passe-haut fixe à 35 Hz). |
| **Saturation** | Compression douce de la bande sur les crêtes. |
| **Wow / Flutter** | Instabilités de vitesse : wow lent (0,45 Hz) avec dérive aléatoire, flutter rapide (6,5 Hz). |
| **Spread** | Canal droit jusqu'à 20 % plus long : les deux boucles se décalent progressivement. |
| **SYNC / Loop Length** | Cale la boucle sur le tempo du logiciel audio : de 1 temps à 16 mesures, selon le chiffrage. Au-delà de 30 s, la durée est divisée par deux jusqu’à rentrer. Un changement de tempo fait glisser la bande comme un varispeed. |
| **HOLD** | Fige la boucle : l'entrée n'est plus enregistrée et la bande tourne sans aucune usure. |
| **ERASE** | Efface la bande. |

## Compiler sur Mac

Prérequis :
- outils en ligne de commande Apple (`xcode-select --install`) ;
- CMake (`brew install cmake`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

JUCE 8.0.9 est téléchargé automatiquement au premier lancement de CMake. Après la compilation, le plugin est copié dans :
- `~/Library/Audio/Plug-Ins/VST3/Frippertron.vst3`
- `~/Library/Audio/Plug-Ins/Components/Frippertron.component` (AU)

Le binaire est universel (Apple Silicon et Intel), compatible macOS 11 et plus.

Si le plugin n'apparaît pas dans le logiciel audio, relance un scan des plugins. Pour Logic, valide l'AU avec `auval -v aufx Frpt Crls`.

## Architecture

- `Source/PluginProcessor.*` : traitement audio. Tampon circulaire de 36 s par canal (la « bande »), lecture avec interpolation d'Hermite. Chaîne de réinjection : passe-haut → passe-bas 2 pôles → saturation `tanh` → feedback. HOLD fait un fondu de 50 ms vers une recopie exacte de la bande.
- `Source/PluginEditor.*` : interface avec deux bobines animées, têtes REC/PLAY, un point qui suit la position dans la boucle, un indicateur de niveau et dix boutons rotatifs.

## État

- Compile sans avertissement (sous Linux, en VST3 et application autonome).
- Testé hors ligne :
  - les échos tombent au bon intervalle et décroissent comme prévu (×0,8 par passage, un peu moins à cause de l'usure) ;
  - la boucle reste stable après 60 s à 100 % de feedback, saturation au maximum.
- Corrections après relecture : entrée mono → sortie stéréo (le canal droit relisait la sortie du gauche), HOLD désormais sans aucune usure (recopie exacte, sans wow ni interpolation), ERASE remet aussi les filtres à zéro.
- Compilé sur Mac (macOS 15, universel arm64 + x86_64) sans avertissement ; `auval` : AU VALIDATION SUCCEEDED (13/10/2026).
- **Pas encore écouté dans un logiciel audio.** C’est la prochaine étape.
