#ifndef MEMORY_H
#define MEMORY_H

#include "../core/types.h"

#include <stdbool.h>

typedef struct
{
    /* Program ROM */

    u8 rom[0x4000];

    /* Video RAM */

    u8 video_ram[0x0400];

    /* Color RAM */

    u8 color_ram[0x0400];

    /* Main RAM */

    u8 ram[0x0800];

} Memory;

void memory_reset(
    Memory* memory);

bool memory_load_rom_chip(
    Memory* memory,
    const u8* data,
    u32 size,
    u32 destination);

#endif