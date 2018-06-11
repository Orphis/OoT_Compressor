#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readwrite.h"
#include "tables.h"

#include "yaz0.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* internal declarations */
u32 simpleEnc(const u8* src, int size, int pos, u32* pMatchPos);
u32 nintendoEnc(const u8* src, int size, int pos, u32* pMatchPos, u32*, u32*, int*);
int yaz0_encode_internal(const u8* src, int srcSize, u8* Data);

int yaz0_get_size(u8* src) { return U32(src + 0x4); }

u32 toDWORD(u32 d) {
  u8 w1 = d & 0xFF;
  u8 w2 = (d >> 8) & 0xFF;
  u8 w3 = (d >> 16) & 0xFF;
  u8 w4 = d >> 24;
  return (w1 << 24) | (w2 << 16) | (w3 << 8) | w4;
}

// simple and straight encoding scheme for Yaz0
u32 simpleEnc(const u8* src, int size, int pos, u32* pMatchPos) {
  int startPos = pos - 0x1000, j, i;
  int smp = size - pos;
  u32 numBytes = 1;
  u32 matchPos = 0;

  if (startPos < 0) startPos = 0;

  if (smp > 0x111) smp = 0x111;

  for (i = startPos; i < pos; i++) {
    for (j = 0; j < smp; j++) {
      if (src[i + j] != src[j + pos]) break;
    }
    if (j > numBytes) {
      numBytes = j;
      matchPos = i;
    }
  }
  *pMatchPos = matchPos;
  if (numBytes == 2) numBytes = 1;
  return numBytes;
}

// a lookahead encoding scheme for ngc Yaz0
u32 nintendoEnc(const u8* src, int size, int pos, u32* pMatchPos, u32* numBytes1,
                u32* matchPos, int* prevFlag) {
  u32 numBytes = 1;

  // if prevFlag is set, it means that the previous position was determined by
  // look-ahead try. so just use it. this is not the best optimization, but
  // nintendo's choice for speed.
  if (*prevFlag == 1) {
    *pMatchPos = *matchPos;
    *prevFlag = 0;
    return *numBytes1;
  }
  *prevFlag = 0;
  numBytes = simpleEnc(src, size, pos, matchPos);
  *pMatchPos = *matchPos;

  // if this position is RLE encoded, then compare to copying 1 byte and next
  // position(pos+1) encoding
  if (numBytes >= 3) {
    *numBytes1 = simpleEnc(src, size, pos + 1, matchPos);
    // if the next position encoding is +2 longer than current position, choose
    // it. this does not guarantee the best optimization, but fairly good
    // optimization with speed.
    if (*numBytes1 >= numBytes + 2) {
      numBytes = 1;
      *prevFlag = 1;
    }
  }
  return numBytes;
}

int yaz0_encode_internal(const u8* src, int srcSize, u8* Data) {
  u8 dst[24];  // 8 codes * 3 bytes maximum
  int dstSize = 0;
  int i;
  int pos = 0;
  int srcPos = 0, dstPos = 0;

  u32 numBytes1, matchPos2;
  int prevFlag = 0;

  u32 validBitCount = 0;  // number of valid bits left in "code" byte
  u8 currCodeByte = 0;
  while (srcPos < srcSize) {
    u32 numBytes;
    u32 matchPos;
    u32 srcPosBak;

    numBytes = nintendoEnc(src, srcSize, srcPos, &matchPos, &numBytes1,
                           &matchPos2, &prevFlag);
    if (numBytes < 3) {
      // straight copy
      dst[dstPos] = src[srcPos];
      dstPos++;
      srcPos++;
      // set flag for straight copy
      currCodeByte |= (0x80 >> validBitCount);
    } else {
      // RLE part
      u32 dist = srcPos - matchPos - 1;
      u8 byte1, byte2, byte3;

      if (numBytes >= 0x12)  // 3 byte encoding
      {
        byte1 = 0 | (dist >> 8);
        byte2 = dist & 0xff;
        dst[dstPos++] = byte1;
        dst[dstPos++] = byte2;
        // maximum runlength for 3 byte encoding
        if (numBytes > 0xff + 0x12) numBytes = 0xff + 0x12;
        byte3 = numBytes - 0x12;
        dst[dstPos++] = byte3;
      } else  // 2 byte encoding
      {
        byte1 = ((numBytes - 2) << 4) | (dist >> 8);
        byte2 = dist & 0xff;
        dst[dstPos++] = byte1;
        dst[dstPos++] = byte2;
      }
      srcPos += numBytes;
    }
    validBitCount++;
    // write eight codes
    if (validBitCount == 8) {
      Data[pos] = currCodeByte;
      pos++;
      for (i = 0; i < /*=*/dstPos; pos++, i++) Data[pos] = dst[i];
      dstSize += dstPos + 1;

      srcPosBak = srcPos;
      currCodeByte = 0;
      validBitCount = 0;
      dstPos = 0;
    }
  }
  if (validBitCount > 0) {
    Data[pos] = currCodeByte;
    pos++;
    for (i = 0; i < /*=*/dstPos; pos++, i++) Data[pos] = dst[i];
    dstSize += dstPos + 1;

    currCodeByte = 0;
    validBitCount = 0;
    dstPos = 0;
  }

  return dstSize;
}

std::vector<uint8_t> yaz0_encode(const u8* src, int src_size) {
  std::vector<uint8_t> buffer(src_size + 0x160);
  u8* dst = buffer.data();

  // write 4 bytes yaz0 header
  memcpy(dst, "Yaz0", 4);

  // write 4 bytes uncompressed size
  W32(dst + 4, src_size);

  // encode
  int dst_size = yaz0_encode_internal(src, src_size, dst + 16);
  int aligned_size = (dst_size + 31) & -16;
  buffer.resize(dst_size);
  return buffer;
}

void yaz0_decode(const uint8_t* source, uint8_t* decomp, int32_t decompSize) {
  uint32_t srcPlace = 0, dstPlace = 0;
  uint32_t i, dist, copyPlace, numBytes;
  uint8_t codeByte, byte1, byte2;
  uint8_t bitCount = 0;

  source += 0x10;
  while (dstPlace < decompSize) {
    /* If there are no more bits to test, get a new byte */
    if (!bitCount) {
      codeByte = source[srcPlace++];
      bitCount = 8;
    }

    /* If bit 7 is a 1, just copy 1 byte from source to destination */
    /* Else do some decoding */
    if (codeByte & 0x80) {
      decomp[dstPlace++] = source[srcPlace++];
    } else {
      /* Get 2 bytes from source */
      byte1 = source[srcPlace++];
      byte2 = source[srcPlace++];

      /* Calculate distance to move in destination */
      /* And the number of bytes to copy */
      dist = ((byte1 & 0xF) << 8) | byte2;
      copyPlace = dstPlace - (dist + 1);
      numBytes = byte1 >> 4;

      /* Do more calculations on the number of bytes to copy */
      if (!numBytes)
        numBytes = source[srcPlace++] + 0x12;
      else
        numBytes += 2;

      /* Copy data from a previous point in destination */
      /* to current point in destination */
      for (i = 0; i < numBytes; i++) decomp[dstPlace++] = decomp[copyPlace++];
    }

    /* Set up for the next read cycle */
    codeByte = codeByte << 1;
    bitCount--;
  }
}
