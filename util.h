#pragma once

#include <cstdlib>

class Endian {
 private:
  static constexpr uint32_t uint32_ = 0x01020304;
  static constexpr uint8_t magic_ = (const uint8_t&)uint32_;

 public:
  static constexpr bool little = magic_ == 0x04;
  static constexpr bool middle = magic_ == 0x02;
  static constexpr bool big = magic_ == 0x01;
  static_assert(little || middle || big, "Cannot determine endianness!");

 private:
  Endian() = delete;
};

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

inline uint32_t bigendian(uint32_t x) {
  if constexpr (Endian::little) {
    return byteSwap(x);
  }
  return x;
}

inline uint16_t bigendian(uint16_t x) {
  if constexpr (Endian::little) {
    return byteSwap(x);
  }
  return x;
}