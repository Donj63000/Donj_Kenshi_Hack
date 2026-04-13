#pragma once

#include <cstddef>
#include <cstdint>

// La je centralise les RVA des hooks moteur que j'utilise deja dans les
// headers KenshiLib, pour ne plus dependre de GetRealAddress(...) quand la
// representation du pointeur de methode declenche une assertion runtime.
namespace DonJHookAddressResolver
{
    static const std::uintptr_t kGuiWindowUpdateRva = 0x6E1A90u;
    static const std::uintptr_t kInputHandlerKeyDownEventRva = 0x360680u;
    static const std::uintptr_t kTitleScreenConstructorRva = 0x917740u;
    static const std::uintptr_t kTitleScreenShowRva = 0x9116D0u;
    static const std::uintptr_t kTitleScreenUpdateRva = 0x9120D0u;
    static const std::uintptr_t kGameWorldMainLoopRva = 0x7877A0u;
    static const std::uintptr_t kCreateRandomCharacterRva = 0x582F60u;

    inline std::uintptr_t ResolveModuleRva(
        std::uintptr_t moduleBase,
        std::uintptr_t rva)
    {
        if (moduleBase == 0 || rva == 0)
        {
            return 0;
        }

        return moduleBase + rva;
    }
}
