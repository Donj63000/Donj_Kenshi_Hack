# Etape 10 - Specification precise de /army

## Resultat

La commande `/army` a maintenant une specification figee dans le code.

Cette spec est centralisee dans :
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmyCommandSpec.h`

Le backend terminal s'appuie maintenant sur cette spec dans :
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.cpp`

## Spec figee

Champ | Valeur
--- | ---
Nom de commande | `/army`
Nom affiche | `Invocation de l'armee des morts`
Nombre d'unites | `30`
Duree | `180 secondes`
Comportement cible | allies au joueur, suivent et protegent le leader
Contrainte | une seule armee active a la fois
Templates cibles | `DonJ_ArmyOfDead_Warrior_A/B/C`

## Etats

Les etats restent :
- `Idle`
- `Preparing`
- `Spawning`
- `Active`
- `Dismissing`

Un libelle utilisateur coherent existe maintenant aussi pour le terminal :
- `inactive`
- `en preparation`
- `en spawn`
- `active`
- `en nettoyage`

## Validations d'acceptation

Les validations connues avant acceptation de `/army` sont maintenant explicites :
- une partie doit etre chargee
- un leader doit etre resolvable
- aucune autre armee ne doit etre active
- les templates doivent etre configures
- le systeme de spawn doit etre initialise

Ces validations passent maintenant par `ArmyCommandEnvironment`.

## Messages terminal figes

Messages retenus dans le code :
- `[OK] Invocation de l'armee des morts : preparation de 30 invocations.`
- `[INFO] /army refusee : une armee est deja active.`
- `[INFO] /army refusee : aucune partie chargee.`
- `[INFO] /army refusee : leader introuvable.`
- `[INFO] Spawn en cours : 0 / 30 unites creees.`
- `[OK] Fin d'invocation : nettoyage effectue.`
- `[ERREUR] Le template DonJ_ArmyOfDead_Warrior est introuvable.`

## Verification technique

Compilation plugin :

```powershell
$env:KENSHILIB_DIR='C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\KenshiLib'
$env:KENSHILIB_DEPS_DIR='C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps'
$env:BOOST_INCLUDE_PATH='C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\boost_1_60_0'
$env:BOOST_ROOT='C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples_deps\boost_1_60_0'
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.vcxproj `
  /t:Build /p:Configuration=Release /p:Platform=x64
```

Tests natifs :
- `native_terminal_backend_tests.cpp`
- verification de la spec `/army`
- verification du refus si aucune partie n'est chargee

Tests Python :

```powershell
python -m unittest discover -s tests -v
```
