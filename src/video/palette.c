#include "palette.h"

#include <stddef.h>
#include <string.h>

static u8 decode_red(
    u8 value)
{
    u8 bit0 =
        (value >> 0) &
        0x01;

    u8 bit1 =
        (value >> 1) &
        0x01;

    u8 bit2 =
        (value >> 2) &
        0x01;

    return
        (u8) (
            0x21 * bit0 +
            0x47 * bit1 +
            0x97 * bit2);
}

static u8 decode_green(
    u8 value)
{
    u8 bit0 =
        (value >> 3) &
        0x01;

    u8 bit1 =
        (value >> 4) &
        0x01;

    u8 bit2 =
        (value >> 5) &
        0x01;

    return
        (u8) (
            0x21 * bit0 +
            0x47 * bit1 +
            0x97 * bit2);
}

static u8 decode_blue(
    u8 value)
{
    u8 bit0 =
        (value >> 6) &
        0x01;

    u8 bit1 =
        (value >> 7) &
        0x01;

    return
        (u8) (
            0x51 * bit0 +
            0xAE * bit1);
}

void palette_reset(
    Palette* palette)
{
    if(palette == NULL)
    {
        return;
    }

    memset(
        palette,
        0,
        sizeof(Palette));
}

bool palette_decode(
    Palette* palette,
    const u8* palette_prom,
    const u8* color_prom)
{
    if(palette == NULL ||
       palette_prom == NULL ||
       color_prom == NULL)
    {
        return false;
    }

    palette_reset(
        palette);

    for(u32 index = 0;
        index <
        PACMAN_PALETTE_COLOR_COUNT;
        index++)
    {
        u8 value =
            palette_prom[index];

        u8 red =
            decode_red(value);

        u8 green =
            decode_green(value);

        u8 blue =
            decode_blue(value);

        palette->colors[index] =
            0xFF000000u |
            ((u32) red << 16) |
            ((u32) green << 8) |
            (u32) blue;
    }

    for(u32 index = 0;
        index <
        PACMAN_COLOR_TABLE_SIZE;
        index++)
    {
        palette->lookup[index] =
            color_prom[index] &
            0x0F;
    }

    palette->initialized =
        true;

    return true;
}

u32 palette_get_color(
    const Palette* palette,
    u8 color_code,
    u8 pixel)
{
    if(palette == NULL ||
       !palette->initialized)
    {
        return 0xFF000000u;
    }

    u16 lookup_index =
        (u16) (
            ((u16) (
                color_code &
                0x3F) << 2) |
            (pixel &
             0x03));

    lookup_index &=
        PACMAN_COLOR_TABLE_SIZE -
        1;

    u8 palette_index =
        palette
            ->lookup[lookup_index] &
        0x1F;

    return
        palette->colors
            [palette_index];
}
bool palette_pixel_is_transparent(
    const Palette* palette,
    u8 color_code,
    u8 pixel)
{
    if(palette == NULL ||
       !palette->initialized)
    {
        return true;
    }

    u16 lookup_index =
        (u16) (
            ((u16) (
                color_code &
                0x3F) << 2) |
            (pixel &
             0x03));

    lookup_index &=
        PACMAN_COLOR_TABLE_SIZE -
        1;

    return
        (palette->lookup[lookup_index] &
         0x1F) == 0;
}