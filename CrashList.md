# CrashList

## Regle projet

- A chaque crash, je dois ajouter une entree ici avant tout reset de logs ou nouvelle relance Kenshi.
- Je note systematiquement : date/heure, contexte, symptome, logs disponibles, extraits utiles, hypothese et statut.
- Si un log est manquant, je l'ecris explicitement.

## 2026-04-13 02:19:47 - Crash en chargement de partie pendant la validation du terminal

- Contexte : test runtime apres clic sur `Continuer` pour valider le terminal et le toggle `F10`.
- Symptome : fermeture de Kenshi avec generation de l'archive `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\crashDump1.0.65_x64.zip`.
- Logs disponibles :
  - archive crash : `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\crashDump1.0.65_x64.zip`
  - `save.log` embarque dans l'archive
  - `MyGUI.log` embarque dans l'archive
  - `Havok.log` embarque dans l'archive
  - `RE_Kenshi_log.txt` specifique a ce crash : non preserve au moment de l'incident
- Extraits utiles :

```text
save.log
02:13:45 [Info] Session start.
02:17:51 [Info] Creating temporary folder C:\Users\nodig\AppData\Local\kenshi\save\/_current1/
02:19:47 [Info] Exit.
```

```text
MyGUI.log
02:17:40 | Warning | Widget property 'FontName' not found [Kenshi_MainPanel.layout]
02:17:40 | Warning | Widget with name 'LifeBar10' not found. [Kenshi_MainPanel.layout]
02:17:41 | Warning | Widget property 'MoveToClick' not found []
```

```text
Havok.log
02:17:51: Loading file./data/newland//land/navtiles/tile20.32.hkt
02:17:51: Create zone instance 20,32 [2014]
02:18:06: Door Bar open at -50619.1 1094.59 -1156.04
```

- Hypothese : crash survenu pendant la transition menu -> partie, sans preuve suffisante a ce stade pour lier formellement l'incident au toggle `F10`.
- Statut : a reinvestiguer si le crash se reproduit apres la build qui ajoute le suivi global de `F10`.

## 2026-04-13 - Assertion Visual C++ sur `GetRealAddress(...)`

- Contexte : tentative de fallback hook avec `GetRealAddress(...)` sur les points d'entree `TitleScreen`.
- Symptome : popup `Microsoft Visual C++ Runtime Library` avec assertion `Source\core\Functions.cpp`, ligne `111`, expression sur `(uintptr_t&)fun`.
- Logs disponibles :
  - source principale : capture utilisateur de la popup d'assertion
  - logs Kenshi relies a cet incident : non preserves avant mise en place de cette regle
- Extraits utiles :

```text
File: Source\core\Functions.cpp
Line: 111
Expression: (uintptr_t&)fun >= (uintptr_t)&FUNC_BEGIN && (uintptr_t&)fun <= (uintptr_t)&FUNC_END
```

- Hypothese : `GetRealAddress(...)` n'est pas fiable pour ce chemin de hook sur cette build.
- Statut : corrige en retirant ce fallback du plugin.
