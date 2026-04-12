# Etape 3 - Mod de donnees FCS

## Objectif

Preparer la couche donnees du projet dans le `Forgotten Construction Set` avec un mod propre et des templates dedies pour l'armee des morts.

## Resultat obtenu

- `DonJ_Kenshi_Hack.mod` existe dans `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\DonJ_Kenshi_Hack`
- le mod a ete ouvert en `ACTIVE` dans le FCS
- la categorie `Characters` a ete utilisee comme base de travail
- l'archetype de duplication retenu est `Armour King's Thrall`
- les templates suivants existent dans la liste des personnages :
  - `DonJ_ArmyOfDead_Warrior_A`
  - `DonJ_ArmyOfDead_Warrior_B`
  - `DonJ_ArmyOfDead_Warrior_C`

## Methode retenue

- creation du mod via le dialogue `New Kenshi Mod`
- passage par la categorie `Characters`
- duplication repetee de `Armour King's Thrall`
- renommage des duplicatas dans le `PropertyGrid`
- sauvegarde entre les iterations pour figer les entrees sur disque

## Validation

- verification scriptable de la presence des trois entrees dans la `SysListView32` du FCS :
  - `DonJ_ArmyOfDead_Warrior_A`
  - `DonJ_ArmyOfDead_Warrior_B`
  - `DonJ_ArmyOfDead_Warrior_C`
- taille du fichier `DonJ_Kenshi_Hack.mod` apres sauvegarde finale : `2011` octets
- `Cleanup` execute dans le FCS
- resume `Cleanup` :
  - `Removed 0 fields from 0 items`
  - `Removed 0 deprecated items`
  - `Removed 0 orphaned items`
  - `Removed 0 deleted items`
  - `Removed 0 invalid references`
- fenetre `Errors` apres scan : `Errors (0 / 5515)` avec une liste vide

## Points importants pour la suite

- ces trois templates sont pour l'instant des duplicatas renommes de l'archetype de depart
- la base de donnees est donc propre pour le MVP, mais le tuning thematique detaille reste a faire si on veut pousser l'apparence, l'equipement ou les stats plus loin
- la faction finale des invocations ne doit pas etre consideree comme verrouillee cote FCS
- la faction runtime devra etre imposee cote plugin apres le spawn

## Captures utiles

- creation et activation du mod : `refs\fcs_after_mod_create_full.png`
- categorie `Characters` selectionnee : `refs\fcs_characters_selected_full3.png`
- archetype source visible : `refs\fcs_selected_armour_king_thrall.png`
- premier template renomme : `refs\step3_name_set_a.png`
- deuxieme template renomme : `refs\step3_name_set_b.png`
- troisieme template renomme : `refs\step3_name_set_c.png`
- scan final / etat FCS : `refs\step3_final_state.png`
