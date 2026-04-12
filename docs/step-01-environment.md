# Etape 1 - Preparation de l'environnement

## Workspace retenu

Le projet est travaille directement dans :
- `C:\Users\nodig\kenshi_donj_hack`

Je n'ai pas cree d'autre workspace externe, conformement a la consigne.

## Arborescence preparee

Les dossiers suivants ont ete crees dans le depot courant :
- `refs`
- `src`
- `package`
- `docs`
- `backups`

Les depots de reference suivants ont ete clones a la racine du projet :
- `KenshiLib_Examples`
- `KenshiLib_Examples_deps`

## Kenshi localise

Chemins verifies localement :
- jeu : `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`
- mods : `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods`
- data : `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data`
- FCS : `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\forgotten construction set.exe`
- mods.cfg : `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data\mods.cfg`

## References preparees

Depots presents :
- `KenshiLib_Examples`
- `KenshiLib_Examples_deps`

Etat des outils :
- `git` present
- `git lfs` present
- `MSBuild` 2019 present
- `MSBuild` 2022 present
- `Visual Studio Community 2022` present

Variables d'environnement utilisateur renseignees :
- `KENSHILIB_DIR = C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\KenshiLib`
- `KENSHILIB_DEPS_DIR = C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps`
- `BOOST_INCLUDE_PATH = C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\boost_1_60_0`
- `BOOST_ROOT = C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\boost_1_60_0`

Boost a ete extrait localement depuis `boost_1_60_0\boost.zip`.

## Setup.bat

`Setup.bat` a ete lance.

Resultat observe :
- le script affiche `Not elevated, restarting...`
- son mecanisme de relance elevee est interactif
- dans cette session, il n'a pas pu finaliser son elevation de facon pilotee

Pour ne pas rester bloque, j'ai applique manuellement l'effet utile du setup :
- extraction de Boost
- definition des variables d'environnement utilisateur attendues par les projets

## Visual Studio

Visual Studio Community 2022 a ete installe sur la machine.

La solution suivante est maintenant ouvrable dans l'IDE :
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\KenshiLib_Examples.sln`

Un `dotnet restore` a aussi ete execute sur la solution pour remettre en etat les projets C#.

## Validation de build

Tentative effectuee :
- `MSBuild KenshiLib_Examples.sln /p:Configuration=Release /p:Platform=x64`

Resultat :
- apres installation de Visual Studio et `dotnet restore`, le blocage restant est confirme sur le toolset `v100`

Erreurs structurantes constatees :
- `MSB8020` : outils Visual Studio 2010 / `v100` introuvables

## Tentatives sur le toolset legacy

Actions deja tentees :
- telechargement officiel de `GRMSDKX_EN_DVD.iso`
- telechargement officiel de `VC-Compiler-KB2519277.exe`
- tentative d'installation via `setup.exe`
- tentative d'installation silencieuse du SDK
- tentative d'installation directe des MSI composants du SDK

Resultat :
- l'installation automatique du SDK legacy n'a pas abouti proprement sur cette machine
- le toolset `Visual C++ 2010 x64 / v100` reste absent

## Conclusion de l'etape

Ce qui est valide :
- Kenshi et les chemins utiles sont localises
- le workspace propre est prepare dans `C:\Users\nodig\kenshi_donj_hack`
- `KenshiLib_Examples` et `KenshiLib_Examples_deps` sont clones
- Boost est extrait
- les variables d'environnement utiles sont posees
- la solution de reference est ouvrable dans Visual Studio
- un restore des projets C# a ete fait
- la solution de reference a ete analysee et une vraie tentative de build a ete faite

Ce qui reste bloque avant validation complete de l'etape :
- installer le toolset **Visual C++ 2010 x64 / v100**

Tant que `v100` n'est pas present, il ne faut pas considerer la compilation des plugins d'exemple comme validee.
