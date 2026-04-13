#pragma once

// La je verrouille explicitement le build pour ne plus laisser passer une
// toolchain moderne qui casserait l'ABI avec RE_Kenshi / KenshiLib.

#if !defined(_WIN64)
#error DonJ_Kenshi_Hack doit etre compile en x64.
#endif

#if defined(_DEBUG)
#error DonJ_Kenshi_Hack ne doit pas etre deploye en Debug. Utilise Release x64.
#endif

#if !defined(_MSC_VER)
#error DonJ_Kenshi_Hack doit etre compile avec MSVC.
#endif

#if (_MSC_VER != 1600)
#error DonJ_Kenshi_Hack doit etre compile avec Visual C++ 2010 (toolset v100) pour rester ABI-compatible avec RE_Kenshi/KenshiLib.
#endif
