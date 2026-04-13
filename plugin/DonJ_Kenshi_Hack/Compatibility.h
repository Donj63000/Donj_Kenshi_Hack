#pragma once

#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <string>

// La je centralise mes petits adaptateurs de compatibilite pour VC++ 2010,
// afin d'eviter de polluer tout le code metier avec des details de toolchain.

inline int DonjSnprintf(char* buffer, std::size_t bufferSize, const char* format, ...)
{
    va_list args;
    va_start(args, format);
#if defined(_MSC_VER) && (_MSC_VER <= 1600)
    const int result = _vsnprintf_s(buffer, bufferSize, _TRUNCATE, format, args);
#else
    const int result = std::vsnprintf(buffer, bufferSize, format, args);
#endif
    va_end(args);
    return result;
}

template <typename TValue>
inline std::string DonjToString(const TValue& value)
{
    std::ostringstream builder;
    builder << value;
    return builder.str();
}

#if defined(_MSC_VER) && (_MSC_VER <= 1600)
#define constexpr const
#define alignas(value) __declspec(align(value))
#define DONJ_ALIGNAS_16 __declspec(align(16))

namespace std
{
    inline int snprintf(char* buffer, std::size_t bufferSize, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        const int result = _vsnprintf_s(buffer, bufferSize, _TRUNCATE, format, args);
        va_end(args);
        return result;
    }
}
#else
#define DONJ_ALIGNAS_16 alignas(16)
#endif
