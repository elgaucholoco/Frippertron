# Historique des versions

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/). Numérotation `MAJEUR.MINEUR.CORRECTIF` :
- **correctif** (1.1.**1**) : bug corrigé, réglage du son, sans changer les paramètres ;
- **mineur** (1.**2**.0) : nouvelle fonction ;
- **majeur** (**2**.0.0) : changement qui casse les projets ou presets existants.

Le numéro se change dans `CMakeLists.txt` (`project(Frippertron VERSION …)`). Il s'affiche sous le titre du plugin.

## [2.1.0] — 2026-10-14

### Ajouté
- **1 REVOX / 2 REVOX** : en 1 REVOX, la ligne B suit la durée, la synchro et la réinjection de A (une seule machine, en stéréo). Ses réglages sont grisés ; son pan et son pan automatique restent libres.

### Modifié
- Le preset **Classic Revox** utilise 1 REVOX, en stéréo large (A à gauche, B à droite).

## [2.0.0] — 2026-10-14

Version majeure : les paramètres ont été renommés, les projets et presets de la v1 ne sont pas relus.

### Ajouté
- **Deux lignes de bande indépendantes (A et B)**, chacune avec sa durée (ou sa synchro au tempo), sa réinjection, ses instabilités de vitesse et sa place dans l'image stéréo.
- **Routage de l'entrée** : Stereo (L → A, R → B), Mono → A, Mono → B, Mono → A+B.
- **Pan Motion** : pan automatique par ligne, calé sur le tempo (1 à 32 mesures).
- **REHEARSE** : jouer sans enregistrer.
- **Hiss** : souffle de bande qui entre dans la boucle.
- **ERASE** devient un paramètre automatisable (pilotable en MIDI).
- **Presets d'usine** : Init, Classic Revox, Two Revox Drift, Polymeter 4:5, Swirl, Worn Tape, Long Ambient.
- **Métronome visuel** calé sur le logiciel audio.
- Loop Length : 5 et 7 mesures en plus.
- Tests hors ligne (`-DFRIPPERTRON_TESTS=ON`) et capture de l'interface.

### Modifié
- Boucles jusqu'à **60 s** (au lieu de 30).
- Interface agrandie (880 × 540), un panneau par ligne, arc du Pan centré.

### Supprimé
- **Spread** : remplacé par les deux lignes, plus riches.

### Corrigé
- Le point médian sous le titre s'affichait « Â· ».

## [1.1.0] — 2026-10-13

### Ajouté
- **SYNC** : la boucle se cale sur le tempo et le chiffrage du logiciel audio.
- **Loop Length** : de 1 temps à 16 mesures (remplace Loop Time quand SYNC est actif). Au-delà de 30 s, la durée est divisée par deux.
- Affichage du tempo, du chiffrage et de la durée réelle de la boucle.
- Numéro de version affiché sous le titre.

## [1.0.1] — 2026-10-13

### Corrigé
- Entrée mono → sortie stéréo : le canal droit relisait la sortie déjà traitée du canal gauche au lieu de l'entrée.
- HOLD : la boucle figée s'usait encore (wow, flutter, interpolation). Elle est désormais recopiée à l'identique.
- ERASE remet aussi les filtres à zéro.
- L'indicateur de niveau retombe à la même vitesse quelle que soit la taille de bloc.

### Technique
- Première compilation sur Mac (universel arm64 + x86_64), validation AU réussie.

## [1.0.0]

- Version initiale : boucle de bande à deux machines, Loop Time, Feedback, Rec Level, Mix, Output, Tape Tone, Saturation, Wow, Flutter, Spread, HOLD, ERASE.
