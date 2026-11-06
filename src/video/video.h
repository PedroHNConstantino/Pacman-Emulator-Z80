#ifndef VIDEO_H
#define VIDEO_H

#include "../bus/bus.h"
#include "../core/types.h"
#include "../memory/memory.h"
#include "../rom/rom_loader.h"

#include "palette.h"
#include "sprites.h"
#include "tilemap.h"

#include <stdbool.h>

#define PACMAN_WIDTH 224
#define PACMAN_HEIGHT 288

#define PACMAN_SPRITE_ROM_SIZE 0x1000

#define PACMAN_PALETTE_PROM_SIZE 0x0020
#define PACMAN_COLOR_PROM_SIZE 0x0100

typedef struct
{
    /*
     * Quadro final apresentado pelo SDL:
     * 224 × 288.
     */
    u32 framebuffer
        [PACMAN_HEIGHT]
        [PACMAN_WIDTH];

    /*
     * Superfície nativa do hardware:
     * 288 × 224.
     */
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH];

    u8 tile_rom
        [PACMAN_TILE_ROM_SIZE];

    u8 sprite_rom
        [PACMAN_SPRITE_ROM_SIZE];

    u8 palette_prom
        [PACMAN_PALETTE_PROM_SIZE];

    u8 color_prom
        [PACMAN_COLOR_PROM_SIZE];

    Palette palette;

    bool roms_loaded;

} Video;

void video_reset(
    Video* video);

bool video_load_roms(
    Video* video,
    const char* rom_directory);

bool video_render(
    Video* video,
    const Memory* memory,
    const Bus* bus);

#endif