#ifndef ROM_LOADER_H
#define ROM_LOADER_H

#include "../core/types.h"

#include <stdbool.h>

typedef struct
{
    char filename[256];
    u8* data;
    u32 size;

} RomFile;

bool rom_load(
    const char* path,
    RomFile* rom);

void rom_unload(
    RomFile* rom);

#endif