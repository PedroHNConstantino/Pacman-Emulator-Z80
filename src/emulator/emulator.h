#ifndef EMULATOR_H
#define EMULATOR_H

#include "../cpu/z80/z80.h"
#include "../memory/memory.h"
#include "../video/video.h"
#include "../bus/bus.h"

#include <stdbool.h>

typedef struct
{
    Z80 cpu;

    Memory memory;

    Video video;

    Bus bus;

} Emulator;

void emulator_init(
    Emulator* emulator);

void emulator_reset(
    Emulator* emulator);

bool emulator_load_rom_chip(
    Emulator* emulator,
    const char* path,
    u32 destination);

u8 emulator_fetch_opcode(
    Emulator* emulator);

int emulator_step(
    Emulator* emulator);

int emulator_interrupt(
    Emulator* emulator);

#endif