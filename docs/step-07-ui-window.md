# Etape 7 - Creer la fenetre in-game DonJ Kenshi Hack

## Objectif

Afficher une fenetre native `DonJ Kenshi Hack` au menu principal de Kenshi, avec un bouton de test, en respectant un pattern de creation UI sur le bon thread.

## Base technique retenue

La creation UI reste faite via un hook sur :

- `TitleScreen::_CONSTRUCTOR`

Fichier implemente :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.cpp`

Ce choix reste conforme a l'intention du pattern `KillButton` :

- ne pas construire l'UI depuis un thread arbitraire
- attendre que l'ecran titre soit construit
- creer la fenetre native via `MyGUI::Gui`

## Resultat obtenu

La fenetre native est creee au menu principal avec :

- caption : `DonJ Kenshi Hack`
- bouton : `Test UI`

Le journal `RE_Kenshi` confirme la creation :

```text
DonJ Kenshi Hack : hook TitleScreen installe.
DonJ Kenshi Hack : bouton test rect = (236,187)-(876,337).
DonJ Kenshi Hack : fenetre UI creee.
```

Le menu principal charge ensuite normalement :

```text
Kenshi 1.0.65 - x64 (Newland)
```

## Point important sur le clic du bouton

La liaison directe du clic via :

```cpp
g_testButton->eventMouseButtonClick += MyGUI::newDelegate(...)
```

a provoque des crashes reproductibles sur cette machine avec la toolchain `v143`, alors que la creation pure de la fenetre et du bouton reste stable.

Pour terminer proprement l'etape 7 sans perdre la stabilite obtenue, une adaptation temporaire a ete retenue :

- le bouton est bien cree sur le thread UI Kenshi
- son rectangle ecran est capture au moment de la creation
- un petit thread Win32 surveille ensuite les clics souris dans ce rectangle et ecrit le log :
  - `DonJ Kenshi Hack : clic sur le bouton de test.`

Cette adaptation ne change pas le point essentiel de l'etape :

- l'UI est bien creee avec un pattern sur
- aucun crash n'apparait a la creation de la fenetre retenue
- le bouton de test dispose d'une validation de clic exploitable pour poursuivre le chantier

## Validation pratique

Validation locale obtenue :

- la fenetre `DonJ Kenshi Hack` apparait au menu principal
- le bouton `Test UI` est visible
- le clic de test ecrit bien des lignes `DonJ Kenshi Hack : clic sur le bouton de test.` dans le journal `RE_KENSHI`
- le jeu ne plante plus a la creation de la fenetre retenue
- la validation finale en session a ete confirmee visuellement par l'utilisateur

## Risque restant connu

Le fallback de clic actuel est une solution de stabilisation de cette etape, pas la solution finale ideale du terminal.

Pour la suite du projet, il faudra garder en tete :

- la creation UI via `TitleScreen` est bonne
- la delegate MyGUI de clic reste a reetudier proprement si on veut un routage d'evenements 100 % natif MyGUI
- il ne faudra pas reintroduire aveuglement `eventMouseButtonClick += MyGUI::newDelegate(...)` sans repasser par une validation de stabilite

## Validation de l'etape

- la fenetre `DonJ Kenshi Hack` apparait en jeu -> OK
- le bouton de test fonctionne avec une validation de clic exploitable -> OK
- aucun crash n'apparait sur la creation retenue de la fenetre -> OK
- l'UI est bien creee selon un pattern sur inspire de `KillButton` -> OK
