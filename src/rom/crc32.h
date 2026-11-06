#ifndef CRC32_H
#define CRC32_H

#include "../core/types.h"

u32 crc32_calculate(
    const u8* data,
    u32 size);

#endif