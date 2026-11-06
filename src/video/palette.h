#ifndef PALETTE_H
#define PALETTE_H

#include "../core/types.h"

#include <stdbool.h>

#define PACMAN_PALETTE_COLOR_COUNT 32
#define PACMAN_COLOR_TABLE_SIZE 256

typedef struct
{
    u32 colors
        [PACMAN_PALETTE_COLOR_COUNT];

    u8 lookup
        [PACMAN_COLOR_TABLE_SIZE];

    bool initialized;

} Palette;

void palette_reset(
    Palette* palette);

bool palette_decode(
    Palette* palette,
    const u8* palette_prom,
    const u8* color_prom);

bool palette_pixel_is_transparent(
    const Palette* palette,
    u8 color_code,
    u8 pixel);

u32 palette_get_color(
    const Palette* palette,
    u8 color_code,
    u8 pixel);

#endif