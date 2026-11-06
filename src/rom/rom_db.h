#ifndef ROM_DB_H
#define ROM_DB_H

#include "../core/types.h"

typedef struct
{
    const char* filename;

    u32 expected_crc;

    u32 size;

    u32 load_address;

} RomEntry;

extern RomEntry pacman_roms[];

extern const int pacman_roms_count;

#endif