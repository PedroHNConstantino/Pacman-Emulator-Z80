#include "sprites.h"

#include <stddef.h>

#define PACMAN_SPRITE_CLIP_LEFT 16
#define PACMAN_SPRITE_CLIP_RIGHT 271
#define PACMAN_SPRITE_CLIP_TOP 0
#define PACMAN_SPRITE_CLIP_BOTTOM 223

static const u16 sprite_x_offsets[16] =
{
    8 * 8 + 0,
    8 * 8 + 1,
    8 * 8 + 2,
    8 * 8 + 3,

    16 * 8 + 0,
    16 * 8 + 1,
    16 * 8 + 2,
    16 * 8 + 3,

    24 * 8 + 0,
    24 * 8 + 1,
    24 * 8 + 2,
    24 * 8 + 3,

    0,
    1,
    2,
    3
};

static const u16 sprite_y_offsets[16] =
{
    0 * 8,
    1 * 8,
    2 * 8,
    3 * 8,
    4 * 8,
    5 * 8,
    6 * 8,
    7 * 8,

    32 * 8,
    33 * 8,
    34 * 8,
    35 * 8,
    36 * 8,
    37 * 8,
    38 * 8,
    39 * 8
};

static u8 sprite_pixel(
    const u8* sprite_rom,
    u8 sprite_code,
    u8 x,
    u8 y)
{
    if(sprite_rom == NULL ||
       x >= PACMAN_SPRITE_WIDTH ||
       y >= PACMAN_SPRITE_HEIGHT)
    {
        return 0;
    }

    u32 sprite_base_bit =
        (u32) sprite_code *
        64u *
        8u;

    u32 bit_offset =
        sprite_base_bit +
        sprite_x_offsets[x] +
        sprite_y_offsets[y];

    u32 byte_offset =
        bit_offset >> 3;

    u8 bit_index =
        (u8) (
            bit_offset &
            0x07);

    u8 packed =
        sprite_rom[byte_offset];

    u8 plane_zero =
        (packed >>
         (7 - bit_index)) &
        0x01;

    u8 plane_one =
        (packed >>
         (3 - bit_index)) &
        0x01;

    /*
     * A ordem abaixo é a que corrigiu as
     * cores do labirinto e dos sprites.
     */
    return
        (u8) (
            (plane_zero << 1) |
            plane_one);
}

static bool sprite_pixel_is_visible(
    int x,
    int y)
{
    return
        x >= PACMAN_SPRITE_CLIP_LEFT &&
        x <= PACMAN_SPRITE_CLIP_RIGHT &&
        y >= PACMAN_SPRITE_CLIP_TOP &&
        y <= PACMAN_SPRITE_CLIP_BOTTOM;
}

static void draw_sprite_pixel(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    int x,
    int y,
    u32 color)
{
    if(!sprite_pixel_is_visible(
           x,
           y))
    {
        return;
    }

    native_framebuffer[y][x] =
        color;
}

static void draw_sprite(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    const u8* sprite_rom,
    const Palette* palette,
    u8 sprite_code,
    u8 color_code,
    bool flip_x,
    bool flip_y,
    int sprite_x,
    int sprite_y)
{
    for(u8 output_y = 0;
        output_y <
        PACMAN_SPRITE_HEIGHT;
        output_y++)
    {
        for(u8 output_x = 0;
            output_x <
            PACMAN_SPRITE_WIDTH;
            output_x++)
        {
            u8 source_x =
                flip_x
                    ? (u8) (
                        PACMAN_SPRITE_WIDTH -
                        1 -
                        output_x)
                    : output_x;

            u8 source_y =
                flip_y
                    ? (u8) (
                        PACMAN_SPRITE_HEIGHT -
                        1 -
                        output_y)
                    : output_y;

            u8 pixel =
                sprite_pixel(
                    sprite_rom,
                    sprite_code,
                    source_x,
                    source_y);

            if(palette_pixel_is_transparent(
                   palette,
                   color_code,
                   pixel))
            {
                continue;
            }

            u32 color =
                palette_get_color(
                    palette,
                    color_code,
                    pixel);

            int destination_x =
                sprite_x +
                output_x;

            int destination_y =
                sprite_y +
                output_y;

            /*
             * Sprite principal.
             */
            draw_sprite_pixel(
                native_framebuffer,
                destination_x,
                destination_y,
                color);

            /*
             * Cópia de contorno para o
             * túnel horizontal.
             *
             * O recorte impede que essa
             * cópia apareça no placar.
             */
            draw_sprite_pixel(
                native_framebuffer,
                destination_x -
                    256,
                destination_y,
                color);
        }
    }
}

bool sprites_render(
    u32 native_framebuffer
        [PACMAN_NATIVE_HEIGHT]
        [PACMAN_NATIVE_WIDTH],
    const u8* sprite_attributes,
    const u8* sprite_coordinates,
    const u8* sprite_rom,
    const Palette* palette)
{
    if(native_framebuffer == NULL ||
       sprite_attributes == NULL ||
       sprite_coordinates == NULL ||
       sprite_rom == NULL ||
       palette == NULL ||
       !palette->initialized)
    {
        return false;
    }

    /*
     * Os slots são desenhados em ordem
     * reversa para reproduzir a prioridade
     * do hardware.
     */
    for(int offset = 14;
        offset > 4;
        offset -= 2)
    {
        u8 attributes =
            sprite_attributes[offset];

        u8 sprite_code =
            attributes >>
            2;

        u8 color_code =
            sprite_attributes
                [offset + 1] &
            0x1F;

        /*
         * Conforme o driver do MAME:
         *
         * bit 0 -> flip X
         * bit 1 -> flip Y
         */
        bool flip_x =
            (attributes &
             0x01) != 0;

        bool flip_y =
            (attributes &
             0x02) != 0;

        int sprite_x =
            272 -
            (int)
                sprite_coordinates
                    [offset + 1];

        int sprite_y =
            (int)
                sprite_coordinates
                    [offset] -
            31;

        draw_sprite(
            native_framebuffer,
            sprite_rom,
            palette,
            sprite_code,
            color_code,
            flip_x,
            flip_y,
            sprite_x,
            sprite_y);
    }

    /*
     * Os três primeiros slots precisam de
     * um ajuste adicional de um pixel.
     */
    for(int offset = 4;
        offset >= 0;
        offset -= 2)
    {
        u8 attributes =
            sprite_attributes[offset];

        u8 sprite_code =
            attributes >>
            2;

        u8 color_code =
            sprite_attributes
                [offset + 1] &
            0x1F;

        bool flip_x =
            (attributes &
             0x01) != 0;

        bool flip_y =
            (attributes &
             0x02) != 0;

        int sprite_x =
            272 -
            (int)
                sprite_coordinates
                    [offset + 1];

        int sprite_y =
            (int)
                sprite_coordinates
                    [offset] -
            31;

        /*
         * No espaço nativo, o ajuste usado
         * pelo driver é aplicado em Y.
         */
        sprite_y++;

        draw_sprite(
            native_framebuffer,
            sprite_rom,
            palette,
            sprite_code,
            color_code,
            flip_x,
            flip_y,
            sprite_x,
            sprite_y);
    }

    return true;
}