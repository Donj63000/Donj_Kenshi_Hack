# Etape 4 - Preparation du projet plugin

## Objectif

Passer cote code en creant une vraie base plugin `RE_Kenshi` pour `DonJ_Kenshi_Hack`, sans partir d'un projet Visual Studio vide.

## Base retenue

- base de depart choisie : `HelloWorld`
- pattern UI a integrer ensuite : `KillButton`

La logique suivie est volontaire :
- d'abord valider le chargement minimal de la DLL
- ensuite reprendre le pattern `KillButton` pour creer la fenetre native `DonJ Kenshi Hack` sur le bon thread UI

## Projet cree

Le projet plugin a ete cree dans :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack`

Elements prepares :

- `DonJ_Kenshi_Hack.cpp`
- `DonJ_Kenshi_Hack.vcxproj`
- `DonJ_Kenshi_Hack.vcxproj.filters`
- `DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.mod`
- `DonJ_Kenshi_Hack\RE_Kenshi.json`

Le projet a aussi ete ajoute a la solution :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\KenshiLib_Examples.sln`

## Packaging prepare

Structure cible preparee dans le staging local :

- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.mod`
- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack\RE_Kenshi.json`
- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.dll`

Structure de mod egalement posee dans l'installation Kenshi :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.mod`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack\RE_Kenshi.json`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.dll`

## Build verifie

Compilation minimale verifiee avec succes :

- build direct du projet `DonJ_Kenshi_Hack.vcxproj` en `Release|x64`
- build de la solution via la cible `DonJ_Kenshi_Hack` en `Release|x64`

Resultat :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\x64\Release\DonJ_Kenshi_Hack.dll`
- taille constatee : `10752` octets

## Notes techniques utiles

- le projet plugin minimal compile en `v143`
- les variables utilisateur `KENSHILIB_DIR`, `KENSHILIB_DEPS_DIR` et `BOOST_INCLUDE_PATH` doivent etre reinjectees dans la session PowerShell avant l'appel a `MSBuild`
- le sous-dossier mod du projet contient une copie du vrai `DonJ_Kenshi_Hack.mod` cree dans le FCS, pas un simple `.mod` d'exemple

## Validation de l'etape

- la base de depart est bien choisie : `HelloWorld` puis `KillButton`
- un projet `DonJ_Kenshi_Hack` existe et genere `DonJ_Kenshi_Hack.dll`
- la structure finale du dossier de mod est connue et preparee dans `Kenshi/mods`
