# Historique des versions

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/). Numérotation `MAJEUR.MINEUR.CORRECTIF` :
- **correctif** (1.1.**1**) : bug corrigé, réglage du son, sans changer les paramètres ;
- **mineur** (1.**2**.0) : nouvelle fonction ;
- **majeur** (**2**.0.0) : changement qui casse les projets ou presets existants.

Le numéro se change dans `CMakeLists.txt` (`project(Frippertron VERSION …)`). Il s'affiche sous le titre du plugin.

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
