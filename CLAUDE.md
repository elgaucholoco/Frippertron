# Notes pour Claude Code

Projet : plugin JUCE (CMake) émulant les Frippertronics. Lire README.md d'abord.

Étapes 1 à 3 faites le 13/10/2026. CMake installé via `pip3 install --user cmake` (pas de Homebrew) : ajouter `~/Library/Python/3.9/bin` au PATH.

Prochaines étapes :
1. Vérifier les outils de compilation (`xcode-select -p`, `cmake --version`) et installer ce qui manque : `xcode-select --install`, Homebrew, `brew install cmake`. L'utilisateur saisit lui-même son mot de passe.
2. Compiler avec `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8`. Le plugin est copié automatiquement dans `~/Library/Audio/Plug-Ins/`.
3. Valider l'AU : `auval -v aufx Frpt Crls`.
4. Ajuster le son avec l'utilisateur pendant qu'il écoute dans son logiciel audio. Pour cela, modifier les constantes dans `processBlock` :
   - profondeur du wow et du flutter ;
   - fréquences des oscillateurs ;
   - plage de la saturation (`drive`) ;
   - durée de glissement du Loop Time (`glideA`).

L'utilisateur parle français et préfère des retours directs.

Versions : à chaque itération livrée, incrémenter `project(... VERSION x.y.z)` dans `CMakeLists.txt` et ajouter une entrée en tête de `CHANGELOG.md`, puis faire un commit git (`vX.Y.Z — résumé`) et un tag `vX.Y.Z`.
Publication : zipper les bundles de `build/Frippertron_artefacts/Release/` avec `ditto -c -k --keepParent` (VST3, AU, Standalone), puis `gh release create vX.Y.Z <zips>` (`gh` est dans `~/.local/bin`). Les notes de version reprennent l'entrée du CHANGELOG et la procédure d'installation (`xattr -cr`, le plugin n'étant pas notarisé).
