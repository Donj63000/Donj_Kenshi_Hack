# DonJ Kenshi Hack

Mod **Kenshi Steam** en cours de developpement qui ajoute une fenetre native in-game nommee **`DonJ Kenshi Hack`** avec un terminal de commandes.

## Important

**Etat public actuel : ce projet est encore en construction et comporte encore des bugs.**

Il peut etre montre, teste et inspecte, mais il ne faut **pas** encore le considerer comme un mod stable ou finalise.

Point sensible actuel :

- la commande **`/army`** et son pipeline de spawn runtime sont encore en phase de debug/stabilisation
- des crashes en jeu peuvent encore se produire pendant les tests
- le depot est publie aussi pour faciliter la relecture et l'aide au debug

Le projet vise un MVP tres precis :

- ajouter une fenetre UI native dans Kenshi
- permettre de taper des commandes slash
- implementer la commande **`/army`**
- invoquer **30 unites alliees**
- leur donner un comportement **follow + protect**
- les retirer automatiquement apres **180 secondes reelles**

## Etat actuel du projet

Le projet a deja beaucoup avance. Aujourd'hui, ce qui est en place :

- mod de donnees FCS `DonJ_Kenshi_Hack.mod`
- templates FCS :
  - `DonJ_ArmyOfDead_Warrior_A`
  - `DonJ_ArmyOfDead_Warrior_B`
  - `DonJ_ArmyOfDead_Warrior_C`
- plugin natif C++ charge par **RE_Kenshi**
- fenetre in-game **`DonJ Kenshi Hack`**
- terminal MyGUI avec historique + saisie
- commandes `/help`, `/status` et squelette complet de `/army`
- file de commandes executee sur le **game tick**
- session `/army` avec etats, timer, cleanup et reinitialisation
- `SpawnManager` dedie
- `ArmyRuntimeManager` dedie pour :
  - faction
  - positionnement en formation
  - suivi du leader
  - ordres d'escorte
  - dissolution defensive
- documentation pas a pas des etapes deja faites dans `docs/`

## Honnetete technique

Le projet n'est **pas encore considere comme final stable**.

Ce qui est deja valide :

- le chargement du plugin
- la fenetre UI
- la saisie terminal
- `/help`
- `/status`
- l'architecture de `/army`
- le timer 180 secondes
- le cleanup de session

Le point encore sensible :

- la materialisation runtime finale des 30 unites via la **factory Kenshi / replay hook**

Autrement dit :

- l'architecture serieuse est en place
- la logique de session et de post-traitement est codee
- mais le spawn runtime final est encore en phase de stabilisation/test en jeu

## Architecture retenue

Le projet suit volontairement une architecture hybride :

- **FCS** : donnees du mod
- **plugin natif C++** : runtime in-game
- **RE_Kenshi / KenshiLib** : chargement et hooks
- **MyGUI** : fenetre et terminal
- **C# / OpenConstructionSet** : outillage optionnel uniquement

Ce n'est **pas** un mod FCS seul.

## Arborescence utile

Les dossiers importants du depot :

- `plugin/DonJ_Kenshi_Hack/`
  - code source du plugin
- `package/DonJ_Kenshi_Hack/`
  - package du mod pret a copier dans Kenshi
- `docs/`
  - journal technique des etapes du projet
- `tests/`
  - tests natifs C++

Note utile :

- dans le workspace de developpement d'origine, une copie de travail du plugin a aussi existe sous `KenshiLib_Examples/DonJ_Kenshi_Hack/`
- pour GitHub, la version source autonome a publier est celle de `plugin/DonJ_Kenshi_Hack/`
- l'outillage local d'automatisation utilise pendant le developpement n'est pas publie dans ce depot GitHub

## Installation du mod

### Prerequis

- **Kenshi Steam**
- **RE_Kenshi** installe et fonctionnel
- dossier Kenshi typique :
  - `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`

### Installation rapide

1. Copier le dossier :
   - `package/DonJ_Kenshi_Hack/`

   dans :

   - `Kenshi/mods/DonJ_Kenshi_Hack/`

2. Verifier que le dossier final contient bien :
   - `DonJ_Kenshi_Hack.mod`
   - `RE_Kenshi.json`
   - `DonJ_Kenshi_Hack.dll`

3. Lancer Kenshi.

4. Ouvrir l'onglet **Mods** et activer :
   - `DonJ_Kenshi_Hack`

5. Arriver au menu principal puis lancer une partie.

### Installation manuelle precise

Le dossier final dans Kenshi doit ressembler a ceci :

```text
Kenshi/
\- mods/
   \- DonJ_Kenshi_Hack/
      |- DonJ_Kenshi_Hack.mod
      |- RE_Kenshi.json
      \- DonJ_Kenshi_Hack.dll
```

## Utilisation actuelle

En jeu :

- ouvrir la fenetre `DonJ Kenshi Hack`
- appuyer sur `Entree` pour activer la saisie
- taper une commande slash

Commandes disponibles actuellement :

- `/help`
- `/status`
- `/army`

## Comment modifier le mod

### 1. Modifier les donnees FCS

Le mod FCS se trouve dans :

- `package/DonJ_Kenshi_Hack/DonJ_Kenshi_Hack.mod`

Ou, dans l'installation Kenshi :

- `Kenshi/mods/DonJ_Kenshi_Hack/DonJ_Kenshi_Hack.mod`

Pour modifier les donnees :

1. Lancer `forgotten construction set.exe`
2. Charger le mod `DonJ_Kenshi_Hack`
3. Le mettre en `*ACTIVE*`
4. Modifier les templates `DonJ_ArmyOfDead_*`
5. Faire un `Cleanup`
6. Faire un `Scan Errors`
7. Sauvegarder

### 2. Modifier le plugin C++

Le coeur du plugin est dans :

- `plugin/DonJ_Kenshi_Hack/DonJ_Kenshi_Hack.cpp`
- `plugin/DonJ_Kenshi_Hack/TerminalBackend.*`
- `plugin/DonJ_Kenshi_Hack/SpawnManager.*`
- `plugin/DonJ_Kenshi_Hack/ArmyRuntimeManager.*`
- `plugin/DonJ_Kenshi_Hack/ArmyCommandSpec.h`
- `plugin/DonJ_Kenshi_Hack/ArmySession.h`

## Comment recompiler le mod

### Prerequis de build

- Visual Studio 2022 avec charge de travail **Desktop development with C++**
- dependances KenshiLib
- RE_Kenshi / KenshiLib compatibles avec Kenshi

Le projet s'appuie sur des variables d'environnement :

- `KENSHILIB_DIR`
- `KENSHILIB_DEPS_DIR`
- `BOOST_INCLUDE_PATH`
- `BOOST_ROOT`

Sur cette machine, elles pointaient vers :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\KenshiLib`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\boost_1_60_0`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\boost_1_60_0`

### Build du plugin

Le projet a ete compile en :

- `Release`
- `x64`

Commande type :

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && MSBuild "C:\chemin\vers\DonJ_Kenshi_Hack.vcxproj" /t:Build /p:Configuration=Release /p:Platform=x64'
```

Fichier cible genere :

- `plugin/DonJ_Kenshi_Hack/x64/Release/DonJ_Kenshi_Hack.dll`

### Redeploiement du package

Apres compilation :

1. copier la DLL generee dans :
   - `package/DonJ_Kenshi_Hack/DonJ_Kenshi_Hack.dll`
2. recopier ensuite le package dans :
   - `Kenshi/mods/DonJ_Kenshi_Hack/`

## Lancer les tests

### Tests natifs C++

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cl /nologo /EHsc /std:c++14 /I C:\chemin\vers\plugin\DonJ_Kenshi_Hack C:\chemin\vers\tests\native_terminal_backend_tests.cpp C:\chemin\vers\plugin\DonJ_Kenshi_Hack\ArmyRuntimeManager.cpp C:\chemin\vers\plugin\DonJ_Kenshi_Hack\CommandRegistry.cpp C:\chemin\vers\plugin\DonJ_Kenshi_Hack\SpawnManager.cpp C:\chemin\vers\plugin\DonJ_Kenshi_Hack\TerminalBackend.cpp /Fe:C:\chemin\vers\tests\native_terminal_backend_tests.exe && C:\chemin\vers\tests\native_terminal_backend_tests.exe'
```

## Protocole de test recommande

Ordre de validation recommande :

1. verifier que le plugin charge
2. verifier que la fenetre s'affiche
3. verifier que le terminal repond
4. tester `/help`
5. tester `/status`
6. tester `/army` progressivement en zone peuplee
7. verifier le timer
8. verifier le cleanup
9. verifier qu'une nouvelle session peut repartir proprement

Pendant les tests de spawn, il est recommande de se placer **pres d'une zone peuplee**.

## Documentation detaillee

Les etapes deja traitees sont documentees dans :

- `docs/step-01-environment.md`
- `docs/step-02-re-kenshi.md`
- `docs/step-03-fcs-data-mod.md`
- `docs/step-04-plugin-project.md`
- `docs/step-05-package-mod.md`
- `docs/step-06-minimal-plugin-load.md`
- `docs/step-07-ui-window.md`
- `docs/step-08-terminal-parser.md`
- `docs/step-09-command-queue-game-tick.md`
- `docs/step-10-army-spec.md`
- `docs/step-11-safe-spawn-manager.md`
- `docs/step-12-allies-follow-protect.md`
- `docs/step-13-timer-cleanup.md`

## Notes importantes

- le terminal runtime **ne vient pas du FCS**
- le coeur in-game passe par **RE_Kenshi + KenshiLib + MyGUI**
- la commande `/army` reste le coeur du MVP
- le projet avance avec une logique de validation progressive pour eviter les crashes et les regressions

## Auteur / depot cible

Depot GitHub cible :

- [Donj63000/Donj_Kenshi_Hack](https://github.com/Donj63000/Donj_Kenshi_Hack)
