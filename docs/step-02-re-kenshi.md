# Etape 2 - Installation et validation de RE_Kenshi

## Base retenue

La release de reference retenue pour ce projet est :
- `RE_Kenshi v0.3.1`

Le jeu local controle pendant cette etape est :
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`

## Fichiers installes

Fichiers confirmes dans le dossier du jeu :
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi.dll`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\KenshiLib.dll`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\CompressToolsLib.dll`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi.ini`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi\...`

Configuration de chargement verifiee :
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\Plugins_x64.cfg` contient `Plugin=RE_Kenshi`

## Verification de version

Controle effectue sur l'executable :
- MD5 de `kenshi_x64.exe` : `8A03C256F0DA1555D9CCEB939B41530A`

Conclusion :
- la base executable correspond bien au cas supporte par `RE_Kenshi v0.3.1` pour Kenshi Steam `1.0.68`

## Verification runtime

Verification effectuee par lancement normal du jeu :
- processus observe : `Kenshi_x64`
- titre de fenetre observe : `Kenshi 1.0.65 - x64 (Newland)`

Preuve visuelle relevee au menu principal :
- le panneau `MENU RE_KENSHI` est visible dans l'interface du jeu

Capture conservee dans le workspace :
- `C:\Users\nodig\kenshi_donj_hack\refs\kenshi_main_menu_rek.png`

Traces logs observees :
- `kenshi.log` contient `Loading library .\RE_Kenshi`
- `kenshi.log` contient `Added resource location './RE_Kenshi' of type 'FileSystem' to resource group 'GUI'`

## Point important a retenir

Apres installation, Kenshi s'expose localement comme :
- `Kenshi 1.0.65 - x64 (Newland)`

Pour ce projet, il faut considerer cela comme normal et valide dans le flux `RE_Kenshi` sur base Steam `1.0.68`.

La preuve d'activation a retenir n'est donc pas seulement la chaine de version, mais l'ensemble suivant :
- les DLL `RE_Kenshi` sont installees
- `Plugins_x64.cfg` charge `RE_Kenshi`
- le menu `RE_KENSHI` est visible in-game
- le log montre le chargement de `RE_Kenshi`

## Chargement futur de notre plugin

Regle a respecter pour la suite :
- le futur plugin `DonJ_Kenshi_Hack.dll` ne sera pas copie a la racine du jeu
- il vivra dans le dossier de notre mod
- il sera reference par un fichier `RE_Kenshi.json` place dans le dossier du mod

Implication projet :
- il faut d'abord valider `RE_Kenshi` seul
- ensuite seulement brancher notre propre DLL de mod
- il ne faut jamais debugger en meme temps un probleme d'installation `RE_Kenshi` et un probleme de plugin `DonJ_Kenshi_Hack`

## Conclusion de l'etape

Validation locale retenue :
- `RE_Kenshi` est installe
- Kenshi demarre normalement avec `RE_Kenshi` actif
- la preuve visuelle de presence `RE_KENSHI` au menu principal a ete obtenue
- le mecanisme cible de chargement par `RE_Kenshi.json` dans le dossier du mod est acte
