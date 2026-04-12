# Etape 9 - File de commandes et execution sur tick

## Resultat

L'etape 9 est implemente.

Le terminal ne traite plus directement la logique depuis la soumission UI.
La chaine est maintenant separee en trois niveaux :
- la saisie UI ajoute une `PendingCommand`
- un tick d'execution depile cette file et execute les handlers
- la file `GameplayCommand` est ensuite traitee sur `GameWorld::_NV_mainLoop_GPUSensitiveStuff`

## Architecture retenue

Dans `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.*` :
- `SubmitCurrentInput()` n'execute plus la commande tout de suite
- la ligne est poussee dans `pendingCommands_`
- `ProcessPendingCommands()` depile et appelle le registre de commandes
- `TickGameplay()` depile `gameplayCommandQueue_` et gere les timers

Dans `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.cpp` :
- `TitleScreen::_NV_update` reste le frontend de saisie cote menu
- `GameWorld::_NV_mainLoop_GPUSensitiveStuff` sert de point de traitement gameplay
- l'entree terminal est maintenant aussi prise en charge pendant la partie

## Hooks poses

- `TitleScreen::_CONSTRUCTOR`
- `TitleScreen::_NV_update`
- `GameWorld::_NV_mainLoop_GPUSensitiveStuff`

## Commandes stabilisees

- `/help`
- `/status`

Elles passent maintenant par la file de commandes au lieu d'etre executees directement au moment du `submit`.

## Etat /army

- `/army` prepare toujours une `ArmySession`
- la commande cree une `GameplayCommand`
- le depilement gameplay se fait maintenant sur le hook `GameWorld`
- le vrai spawn runtime reste a brancher a l'etape suivante

## Validation technique

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

Test natif du backend :

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cl /nologo /EHsc /std:c++14 /I C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.cpp C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\CommandRegistry.cpp C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.cpp /Fe:C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.exe && C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.exe'
```

Tests Python du projet :

```powershell
python -m unittest discover -s tests -v
```

## Preuves runtime

Log runtime :
- `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\RE_Kenshi_log.txt`
  - `DonJ Kenshi Hack : hooks constructeur, update et game tick installes.`
  - `DonJ Kenshi Hack : 1 commande(s) executee(s) sur le tick titre.`
  - `DonJ Kenshi Hack : 1 commande(s) executee(s) sur le game tick.`

Captures utiles :
- `C:\Users\nodig\kenshi_donj_hack\refs\step9_mainmenu.png`
- `C:\Users\nodig\kenshi_donj_hack\refs\step9_bug_report_current.png`
- `C:\Users\nodig\kenshi_donj_hack\refs\step9_ingame_help_status.png`

## Lecon importante

Un bug reel est apparu pendant la transition menu principal -> partie :
- l'entree pouvait sembler inactive en jeu tant que `Entrée` n'avait pas ete utilisee au menu

Correctif retenu :
- reinitialiser l'etat de detection de bord clavier quand on change de contexte d'execution
- cette logique est maintenant centralisee dans `SyncExecutionContextState()`
