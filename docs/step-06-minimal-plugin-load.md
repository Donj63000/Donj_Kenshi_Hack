# Etape 6 - Verifier le chargement du plugin minimal

## Objectif

Prouver que `DonJ_Kenshi_Hack.dll` est bien chargee par `RE_Kenshi`, avant d'aller vers l'UI MyGUI et la commande `/army`.

## Code minimal verifie

Le plugin exporte bien `startPlugin()` dans :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.cpp`

Le code actuellement en place ecrit un message de debug au chargement du plugin.

Point de tracabilite utile :

- la premiere execution validee en jeu a logge l'ancien message `DonJ_Kenshi_Hack: plugin minimal charge.`
- ensuite, le message source a ete normalise vers `DonJ Kenshi Hack : plugin charge.`
- la chaine embarquee a ete reverifiee directement dans les trois DLL `Release`, `package` et `Kenshi/mods`

## Build Release

La compilation du projet plugin a ete verifiee en `Release|x64`.

Artefact produit :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\x64\Release\DonJ_Kenshi_Hack.dll`

## Packaging verifie

Le package local contient bien :

- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.mod`
- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack\RE_Kenshi.json`
- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.dll`

Le dossier du mod installe dans Kenshi contient aussi bien :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.mod`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack\RE_Kenshi.json`
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.dll`

## Activation du mod

La validation visuelle a ete faite dans l'onglet `Mods` du launcher Kenshi pendant la session :

- `DonJ_Kenshi_Hack (v. 1)` apparait bien dans la liste
- la case du mod est bien cochee

Le load order persistant confirme aussi l'activation :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data\mods.cfg`

Contenu constate :

```text
DonJ_Kenshi_Hack.mod
```

## Preuve du chargement runtime

Le fichier :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi_log.txt`

contient les lignes suivantes lors de la validation du plugin minimal :

```text
6.550: Loading post-load plugins...
6.551: DonJ_Kenshi_Hack -> DonJ_Kenshi_Hack.dll
6.580: DonJ_Kenshi_Hack: plugin minimal charge.
6.581: Loading mod overrides from "DonJ_Kenshi_Hack"...
7.614: Main menu loaded.
```

Interpretation :

- `RE_Kenshi` trouve bien `RE_Kenshi.json`
- `RE_Kenshi` charge bien `DonJ_Kenshi_Hack.dll`
- `startPlugin()` s'execute bien

Une preuve visuelle supplementaire a aussi ete obtenue dans le journal de debogage `RE_KENSHI` affiche au menu principal.

## Point d'attention appris pendant la validation

Un piege pratique a ete confirme :

- si `kenshi_x64.exe` est lance sans `WorkingDirectory` positionne sur `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`, Kenshi peut afficher `No available renderers found.`

La relance fiable a retenir est donc :

```powershell
Start-Process `
  -FilePath 'C:\Program Files (x86)\Steam\steamapps\common\Kenshi\kenshi_x64.exe' `
  -WorkingDirectory 'C:\Program Files (x86)\Steam\steamapps\common\Kenshi'
```

## Validation de l'etape

- la DLL compile en `Release` -> OK
- le dossier du mod dans `Kenshi/mods` contient `.mod`, `RE_Kenshi.json` et la DLL -> OK
- le mod est visible et activable dans l'onglet `Mods` de Kenshi -> OK
- la preuve que `startPlugin()` est execute existe dans `RE_Kenshi_log.txt` et dans le journal de debogage `RE_KENSHI` -> OK
