# Etape 5 - Construire le package de mod

## Objectif

Verifier que le projet dispose deja d'un package de mod distribuable, propre et complet, pret a etre copie dans `Kenshi/mods`.

## Resultat obtenu

Le dossier de packaging existe bien :

- `C:\Users\nodig\kenshi_donj_hack\package\DonJ_Kenshi_Hack`

Contenu verifie :

- `DonJ_Kenshi_Hack.mod`
- `RE_Kenshi.json`
- `DonJ_Kenshi_Hack.dll`

Le fichier `RE_Kenshi.json` est correct :

```json
{
	"Plugins" : [ "DonJ_Kenshi_Hack.dll" ]
}
```

## Copie de test cote Kenshi

Une copie equivalente existe aussi dans l'installation du jeu :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack`

Contenu verifie :

- `DonJ_Kenshi_Hack.mod`
- `RE_Kenshi.json`
- `DonJ_Kenshi_Hack.dll`

## Regle de packaging a retenir

Le dossier `package\DonJ_Kenshi_Hack\` est la reference de travail hors de Steam.

Il doit etre copie tel quel dans :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack`

Autrement dit, la structure finale attendue est bien :

- `DonJ_Kenshi_Hack.mod`
- `RE_Kenshi.json`
- `DonJ_Kenshi_Hack.dll`

## Load order

Point constate localement :

- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\data\mods.cfg` est vide au dernier controle

Interpretation :

- le package est pret
- la copie dans `Kenshi/mods` est prete
- mais l'activation persistante du mod dans l'onglet `Mods` du jeu n'a pas encore ete ecrite dans `mods.cfg`

Pour les futurs tests in-game, il faudra donc verifier :

- que `DonJ_Kenshi_Hack` est bien coche dans l'onglet `Mods`
- que le load order ecrit dans `mods.cfg` correspond a ce qu'on veut

## Validation de l'etape

- le dossier `package\DonJ_Kenshi_Hack\` existe
- `RE_Kenshi.json` y est present
- le `.mod` et la DLL compilee y sont deja places
- ce dossier est bien pense pour etre copie tel quel dans `Kenshi/mods`
