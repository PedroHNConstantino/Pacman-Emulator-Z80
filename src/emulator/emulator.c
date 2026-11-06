#include "emulator.h"

#include "../rom/rom_loader.h"

#include <stddef.h>

void emulator_init(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return;
    }

    memory_reset(
        &emulator->memory);

    video_reset(
        &emulator->video);

    bus_init(
        &emulator->bus,
        &emulator->memory);

    z80_reset(
        &emulator->cpu);
}

void emulator_reset(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return;
    }

    memory_reset(
        &emulator->memory);

    video_reset(
        &emulator->video);

    z80_reset(
        &emulator->cpu);
}

bool emulator_load_rom_chip(
    Emulator* emulator,
    const char* path,
    u32 destination)
{
    if(emulator == NULL || path == NULL)
    {
        return false;
    }

    RomFile rom = {0};

    if(!rom_load(
           path,
           &rom))
    {
        return false;
    }

    bool result =
        memory_load_rom_chip(
            &emulator->memory,
            rom.data,
            rom.size,
            destination);

    rom_unload(
        &rom);

    return result;
}

u8 emulator_fetch_opcode(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return 0x00;
    }

    return z80_fetch(
        &emulator->cpu,
        &emulator->bus);
}

int emulator_step(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return -2;
    }

    return z80_step(
        &emulator->cpu,
        &emulator->bus);
}
int emulator_interrupt(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return Z80_STEP_INVALID_ARGUMENT;
    }

    return z80_interrupt(
        &emulator->cpu,
        &emulator->bus,
        emulator->bus.interrupt_vector);
}