#ifndef TILEMAP_H
#define TILEMAP_H

#include "../core/types.h"
#include "palette.h"

#include <stdbool.h>

#define PACMAN_TILE_WIDTH 8
#define PACMAN_TILE_HEIGHT 8

#define PACMAN_TILEMAP_COLUMNS 36
#define PACMAN_TILEMAP_ROWS 28

#define PACMAN_NATIVE_WIDTH \
    (PACMAN_TILEMAP_COLUMNS * \
     PACMAN_TILE_WIDTH)

#define PACMAN_NATIVE_HEIGHT \
    (PACMAN_TILEMAP_ROWS * \
     PACMAN_TILE_HEIGHT)

#define PACMAN_VIDEO_RAM_SIZE 0x0400
#define PACMAN_TILE_ROM_SIZE 0x1000

bool tilemap_render(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    const u8* video_ram,
    const u8* color_ram,
    const u8* tile_rom,
    const Palette* palette);

#endif