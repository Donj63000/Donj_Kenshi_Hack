# Etape 12 - Unites alliees, suivi et protection

## Resultat

L'etape 12 est integree dans le code du plugin avec une couche runtime dediee :

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmyRuntimeManager.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmyRuntimeManager.cpp`

Cette couche ne remplace pas le `SpawnManager` : elle prend le relai **apres** la creation d'une unite et gere :

- la capture robuste du leader joueur
- le bootstrap de faction depuis le leader si la faction joueur n'est pas resolue directement
- le positionnement en anneaux autour du leader
- l'application du comportement d'escorte (`follow + protect`)
- la dissolution defensive si le contexte devient incoherent

## Modifications principales

### 1. Etat de session enrichi

`ArmySession` suit maintenant aussi :

- `escortRefreshAccumulator`
- `factionBootstrappedFromLeader`
- `leaderHandleId`
- `leaderPlatoonHandleId`
- `activeUnitHandleIds`

Le but est d'arreter de raisonner uniquement avec `Character*` et de garder une trace plus sure du leader et des invocations.

### 2. Post-traitement des unites spawnees

Le `SpawnManager` expose maintenant un callback `onUnitSpawned(...)`.

Quand une unite est creee par la voie Kenshi, le post-traitement fait :

- resolution du leader
- resolution de la faction joueur avec fallback sur la faction du leader
- application de la faction apres le spawn
- renommage de l'unite
- teleport en formation autour du leader
- application des ordres d'escorte
- enregistrement de l'unite via un handle runtime

### 3. Strategie de positionnement

Le placement suit une logique en anneaux :

- premier anneau a ~2 m
- second anneau a ~4 m
- troisieme anneau a ~6 m
- borne superieure a ~8 m

L'objectif est d'eviter l'empilement visuel au point d'apparition et de garder l'armee compactee autour du leader.

### 4. Comportement suivi + protection

Le runtime reapplique regulierement :

- l'ancrage sur le leader
- les ordres d'escorte defensifs
- le rattrapage de position si une unite derive trop loin

Sur cette build, le comportement retenu reste volontairement sobre et robuste :

- suivre le leader via la couche de mouvement
- garder une posture de combat defensive
- retomber pres du leader si l'unite se retrouve trop loin

## Regles des cas particuliers

Les regles simples retenues sont :

- leader introuvable, mort ou KO : dissolution
- changement d'escouade / platoon actif : dissolution
- retour menu / reload monde : dissolution
- certaines unites meurent : elles sont retirees du suivi sans faire planter la session
- toutes les unites ont disparu : dissolution
- changement de zone : on garde la session tant que le leader reste resolvable, sinon dissolution

## Fichiers modifies

- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmySession.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmyRuntimeManager.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\ArmyRuntimeManager.cpp`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\SpawnManager.h`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\SpawnManager.cpp`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\TerminalBackend.cpp`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.cpp`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.vcxproj`
- `C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.vcxproj.filters`
- `C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.cpp`

## Verification

Build plugin :

- `MSBuild.exe C:\Users\nodig\kenshi_donj_hack\KenshiLib_Examples\DonJ_Kenshi_Hack\DonJ_Kenshi_Hack.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64`

Tests natifs :

- compilation et execution de `C:\Users\nodig\kenshi_donj_hack\tests\native_terminal_backend_tests.cpp`

Tests Python :

- `python -m unittest discover -s tests -v`

## Nuance importante

Cette etape valide la couche de **post-traitement runtime** des invocations.

Elle ne change pas l'honnetete deja posee a l'etape 11 :

- l'architecture sure du spawn et du post-spawn est en place
- la materialisation finale de l'armee en runtime depend encore du hook de replay/factory exact de Kenshi

Autrement dit :

- `faction / formation / escorte / dissolution` sont codees et testees
- le spawn reel des 30 unites reste lie a la finalisation du pipeline de creation Kenshi
