# Etape 11 - SpawnManager sur

## Resultat

L'etape 11 est maintenant implementee sous la forme d'un vrai `SpawnManager` dedie dans :
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\SpawnManager.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\SpawnManager.cpp`

Ce manager est branche au `game tick` depuis :
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.cpp`

L'etat `/army` a aussi ete enrichi dans :
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmySession.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.cpp`

## Ce qui est maintenant robuste

- une vraie queue dediee de requetes de spawn existe dans `SpawnManager`
- les requetes produites par `/army` sont adoptees sur le `game tick`
- le pipeline de validation avance par sous-vagues :
  - `1`
  - `3`
  - `10`
  - `30`
- le terminal journalise :
  - la mise en file
  - la progression
  - les echecs
  - les reports
  - les hints de test

Le statut `/army` expose maintenant aussi :
- `pending`
- `wave`
- `tentatives`
- `differees`
- `echecs`
- `mode_spawn`

## Decision technique importante

Le code reconnait maintenant explicitement que le spawn doit passer par la **factory Kenshi**.

Dans les headers KenshiLib locaux, on a bien :
- `GameWorld::theFactory`
- `RootObjectFactory::create(...)`

Mais la **voie finale de replay sur creation native** reste volontairement **non branchee** a ce stade dans le plugin. Concretement :
- la structure de `SpawnManager` est prete
- les callbacks runtime sont poses
- la resolution des templates FCS est reelle
- la resolution du leader et de l'origine de spawn est reelle
- la consommation de queue sur `game tick` est reelle
- le branchement final du replay factory reste encore a valider sur build Steam en essais in-game

Autrement dit : on a construit l'architecture sure et les garde-fous de l'etape 11, sans mentir sur le fait que le hook factory final est encore une integration sensible.

## Hints de debug integres

Quand le replay n'est pas disponible, le terminal peut maintenant rappeler :
- `[INFO] Spawn differe : en attente d'une opportunite de replay Kenshi.`
- `[INFO] Astuce test : approche-toi d'une zone peuplee pour declencher des creations natives.`

Cela encode directement la contrainte documentee par la source publique : le replay peut attendre qu'une creation native se produise dans le jeu.

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

```powershell
cmd /c "\"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat\" -arch=x64 -host_arch=x64 && cl /nologo /EHsc /std:c++14 /I C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.cpp C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\CommandRegistry.cpp C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\SpawnManager.cpp C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.cpp /Fe:C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.exe && C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.exe"
```

Tests Python :

```powershell
python -m unittest discover -s tests -v
```

## Sources utilisees pour cadrer cette etape

- [Kenshi-Online Technical Reference](https://github.com/The404Studios/Kenshi-Online/blob/main/docs/Kenshi-Online-Technical-Reference.md)
- [Kenshi-Online english.md](https://github.com/The404Studios/Kenshi-Online/blob/main/docs/english.md)
- [RootObjectFactory.h dans KenshiLib](https://github.com/KenshiReclaimer/KenshiLib)
