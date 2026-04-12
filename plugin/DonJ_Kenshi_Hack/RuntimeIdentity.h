#pragma once

#include "ArmySession.h"

#include <cstdint>

inline ArmyHandleId MakeRuntimePointerIdentity(const void* pointer)
{
    return pointer != nullptr
        ? static_cast<ArmyHandleId>(reinterpret_cast<std::uintptr_t>(pointer))
        : 0;
}
