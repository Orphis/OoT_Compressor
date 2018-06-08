#pragma once

#include <cstdlib>

inline uint32_t byteSwap(uint16_t x) {
#ifdef _MSC_VER
    return _byteswap_ushort(x);
#else
    return __builtin_bswap16(x);
#endif
}

inline uint32_t byteSwap(uint32_t x) {
#ifdef _MSC_VER
    return _byteswap_ulong(x);
#else
    return __builtin_bswap32(x);
#endif
}