#ifndef SPRITES_H
#define SPRITES_H

#include "../core/types.h"
#include "palette.h"
#include "tilemap.h"

#include <stdbool.h>

#define PACMAN_SPRITE_COUNT 8
#define PACMAN_SPRITE_WIDTH 16
#define PACMAN_SPRITE_HEIGHT 16

bool sprites_render(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    const u8* sprite_attributes,
    const u8* sprite_coordinates,
    const u8* sprite_rom,
    const Palette* palette);

#endif