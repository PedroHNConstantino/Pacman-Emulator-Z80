#include "memory.h"

#include <string.h>

void memory_reset(
    Memory* memory)
{
    memset(
        memory,
        0,
        sizeof(Memory));
}

bool memory_load_rom_chip(
    Memory* memory,
    const u8* data,
    u32 size,
    u32 destination)
{
    if(destination >= sizeof(memory->rom))
    {
        return false;
    }

    if(destination + size >
       sizeof(memory->rom))
    {
        return false;
    }

    memcpy(
        &memory->rom[destination],
        data,
        size);

    return true;
}