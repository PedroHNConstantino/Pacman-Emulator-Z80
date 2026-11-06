#include "tilemap.h"

#include <stddef.h>

static const u16 tile_x_offsets[8] =
{
    8 * 8 + 0,
    8 * 8 + 1,
    8 * 8 + 2,
    8 * 8 + 3,

    0,
    1,
    2,
    3
};

static const u16 tile_y_offsets[8] =
{
    0 * 8,
    1 * 8,
    2 * 8,
    3 * 8,
    4 * 8,
    5 * 8,
    6 * 8,
    7 * 8
};

static u8 tile_pixel(
    const u8* tile_rom,
    u8 tile_code,
    u8 x,
    u8 y)
{
    if(tile_rom == NULL ||
       x >= PACMAN_TILE_WIDTH ||
       y >= PACMAN_TILE_HEIGHT)
    {
        return 0;
    }

    u32 tile_base_bit =
        (u32) tile_code *
        16u *
        8u;

    u32 bit_offset =
        tile_base_bit +
        tile_x_offsets[x] +
        tile_y_offsets[y];

    u32 byte_offset =
        bit_offset >> 3;

    u8 bit_index =
        (u8) (
            bit_offset &
            0x07);

    u8 packed =
        tile_rom[byte_offset];

    u8 plane_zero =
        (packed >>
         (7 - bit_index)) &
        0x01;

    u8 plane_one =
        (packed >>
         (3 - bit_index)) &
        0x01;

    return
        (u8) (
            (plane_zero << 1) |
            plane_one);
}

static u16 tilemap_offset(
    int column,
    int row)
{
    int mapped_column =
        column -
        2;

    int mapped_row =
        row +
        2;

    if((mapped_column & 0x20) != 0)
    {
        return
            (u16) (
                mapped_row +
                ((mapped_column &
                  0x1F) << 5));
    }

    return
        (u16) (
            mapped_column +
            (mapped_row << 5));
}

static void draw_tile(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    const u8* tile_rom,
    const Palette* palette,
    u8 tile_code,
    u8 color_code,
    int tile_column,
    int tile_row)
{
    int tile_origin_x =
        tile_column *
        PACMAN_TILE_WIDTH;

    int tile_origin_y =
        tile_row *
        PACMAN_TILE_HEIGHT;

    for(u8 pixel_y = 0;
        pixel_y <
        PACMAN_TILE_HEIGHT;
        pixel_y++)
    {
        for(u8 pixel_x = 0;
            pixel_x <
            PACMAN_TILE_WIDTH;
            pixel_x++)
        {
            int destination_x =
                tile_origin_x +
                pixel_x;

            int destination_y =
                tile_origin_y +
                pixel_y;

            if(destination_x < 0 ||
               destination_x >=
               PACMAN_NATIVE_WIDTH ||
               destination_y < 0 ||
               destination_y >=
               PACMAN_NATIVE_HEIGHT)
            {
                continue;
            }

            u8 pixel =
                tile_pixel(
                    tile_rom,
                    tile_code,
                    pixel_x,
                    pixel_y);

            u32 color =
                palette_get_color(
                    palette,
                    color_code,
                    pixel);

            native_framebuffer
                [destination_y]
                [destination_x] =
                color;
        }
    }
}

bool tilemap_render(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    const u8* video_ram,
    const u8* color_ram,
    const u8* tile_rom,
    const Palette* palette)
{
    if(native_framebuffer == NULL ||
       video_ram == NULL ||
       color_ram == NULL ||
       tile_rom == NULL ||
       palette == NULL ||
       !palette->initialized)
    {
        return false;
    }

    for(int row = 0;
        row <
        PACMAN_TILEMAP_ROWS;
        row++)
    {
        for(int column = 0;
            column <
            PACMAN_TILEMAP_COLUMNS;
            column++)
        {
            u16 offset =
                tilemap_offset(
                    column,
                    row);

            if(offset >=
               PACMAN_VIDEO_RAM_SIZE)
            {
                continue;
            }

            u8 tile_code =
                video_ram[offset];

            u8 color_code =
                color_ram[offset] &
                0x1F;

            draw_tile(
                native_framebuffer,
                tile_rom,
                palette,
                tile_code,
                color_code,
                column,
                row);
        }
    }

    return true;
}