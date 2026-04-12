# Etape 13 - Timer 180 secondes et nettoyage

## Resultat

L'etape 13 est validee sur la base du comportement runtime deja present dans le plugin :

- le timer de `/army` reste bien en **180 secondes reelles**
- le timer est decremente sur le **game tick**
- quand `remainingSeconds <= 0`, la session passe en `Dismissing`
- le cleanup complet ramene ensuite la session a `Idle`
- `/army` peut etre relancee apres la fin d'une session

Le flux retenu dans le plugin est le suivant :

1. `TerminalBackend::TickGameplay(...)` depile les commandes gameplay puis decremente le timer.
2. Quand le timer arrive a zero, `ArmySession` passe en `Dismissing`.
3. Dans le meme tick, `ArmyRuntimeManager::Tick(...)` finalise le dismiss et appelle `ResetArmySession(...)`.

Ce point est important : le timer et le cleanup ne dependent pas de l'UI. Ils vivent bien dans la boucle gameplay.

## Fichiers concernes

Comportement runtime verifie dans :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.cpp`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmyRuntimeManager.cpp`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmySession.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.cpp`

Tests ajoutes pour verrouiller l'etape :

- `C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.cpp`

## Garanties verifiees

### 1. Timer sur le game tick

Le timer est decremente par :

- `TerminalBackend::TickGameplay(...)`

Il ne depend ni du callback UI, ni du focus terminal, ni de la fenetre MyGUI.

### 2. Declenchement du cleanup

Quand `remainingSeconds <= 0` :

- `state` passe a `Dismissing`
- `active` passe a `false`
- un message terminal de fin est ajoute

Puis `ArmyRuntimeManager::Tick(...)` detecte `Dismissing` et finalise le reset.

### 3. Reset complet de session

`ResetArmySession(...)` remet notamment a zero :

- `spawnedCount`
- `pendingRequestCount`
- `totalSpawnAttempts`
- `failedSpawnAttempts`
- `deferredSpawnAttempts`
- `currentWaveTarget`
- `remainingSeconds`
- `leaderHandleId`
- `leaderPlatoonHandleId`
- `pendingRequests`
- `activeUnitHandleIds`
- `activeUnits`

L'etat repasse a `Idle`, ce qui rend `/army` reutilisable.

### 4. Reutilisation de /army

Apres cleanup :

- la session est de nouveau acceptee par les verifications de preflight
- `/army` peut recreer une session `Preparing`
- une nouvelle queue de `30` requetes est regeneree

## Verification

Build plugin :

- `MSBuild.exe C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64`

Tests natifs :

- compilation et execution de `C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.cpp`

Tests Python :

- `python -m unittest discover -s tests -v`

## Tests natifs ajoutes pour cette etape

- `TestArmyTimerTransitionsToDismissingOnGameplayTick`
- `TestArmyRuntimeFinalizeDismissResetsSessionState`
- `TestArmyCanBeReusedAfterCleanup`

## Decision fonctionnelle confirmee

Le timer reste sur la spec MVP :

- **180 secondes reelles**

On ne bascule pas sur `3 heures en jeu`, afin de garder un comportement simple, stable et predictible pour le MVP.
