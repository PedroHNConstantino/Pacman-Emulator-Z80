#include "video.h"

#include <stdio.h>
#include <string.h>

static bool video_read_chip(
    const char* directory,
    const char* filename,
    u8* destination,
    u32 expected_size)
{
    if(directory == NULL ||
       filename == NULL ||
       destination == NULL)
    {
        return false;
    }

    char path[512];

    int written =
        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            directory,
            filename);

    if(written < 0 ||
       (u32) written >=
       sizeof(path))
    {
        return false;
    }

    RomFile rom =
    {
        0
    };

    if(!rom_load(
           path,
           &rom))
    {
        return false;
    }

    if(rom.size != expected_size)
    {
        rom_unload(
            &rom);

        return false;
    }

    memcpy(
        destination,
        rom.data,
        expected_size);

    rom_unload(
        &rom);

    return true;
}

static bool video_load_graphics_roms(
    Video* video,
    const char* directory)
{
    /*
     * Primeiro tenta o conjunto oficial,
     * com uma ROM de tiles e uma ROM de
     * sprites, ambas de 0x1000 bytes.
     */
    bool tile_loaded =
        video_read_chip(
            directory,
            "pacman.5e",
            video->tile_rom,
            PACMAN_TILE_ROM_SIZE);

    bool sprite_loaded =
        video_read_chip(
            directory,
            "pacman.5f",
            video->sprite_rom,
            PACMAN_SPRITE_ROM_SIZE);

    if(tile_loaded &&
       sprite_loaded)
    {
        printf(
            "ROMs gráficas carregadas: "
            "pacman.5e e pacman.5f\n");

        return true;
    }

    /*
     * Limpa qualquer conteúdo parcial
     * antes da tentativa alternativa.
     */
    memset(
        video->tile_rom,
        0,
        sizeof(video->tile_rom));

    memset(
        video->sprite_rom,
        0,
        sizeof(video->sprite_rom));

    /*
     * Alternativa para o conjunto
     * dividido em quatro chips de
     * 0x0800 bytes.
     */
    if(!video_read_chip(
           directory,
           "pm1_chg1.5e",
           &video->tile_rom[0x0000],
           0x0800))
    {
        return false;
    }

    if(!video_read_chip(
           directory,
           "pm1_chg2.5h",
           &video->tile_rom[0x0800],
           0x0800))
    {
        return false;
    }

    if(!video_read_chip(
           directory,
           "pm1_chg3.5f",
           &video->sprite_rom[0x0000],
           0x0800))
    {
        return false;
    }

    if(!video_read_chip(
           directory,
           "pm1_chg4.5j",
           &video->sprite_rom[0x0800],
           0x0800))
    {
        return false;
    }

    printf(
        "ROMs gráficas carregadas: "
        "conjunto pm1_chg dividido\n");

    return true;
}

static void video_clear_native(
    Video* video)
{
    for(int y = 0;
        y <
        PACMAN_NATIVE_HEIGHT;
        y++)
    {
        for(int x = 0;
            x <
            PACMAN_NATIVE_WIDTH;
            x++)
        {
            video->native_framebuffer
                [y][x] =
                0xFF000000u;
        }
    }
}

static void video_clear_output(
    Video* video)
{
    for(int y = 0;
        y <
        PACMAN_HEIGHT;
        y++)
    {
        for(int x = 0;
            x <
            PACMAN_WIDTH;
            x++)
        {
            video->framebuffer[y][x] =
                0xFF000000u;
        }
    }
}

static void video_rotate_frame(
    Video* video,
    bool flip_screen)
{
    if(video == NULL)
    {
        return;
    }

    for(int y = 0;
        y < PACMAN_HEIGHT;
        y++)
    {
        for(int x = 0;
            x < PACMAN_WIDTH;
            x++)
        {
            video->framebuffer[y][x] =
                0xFF000000u;
        }
    }

    for(int native_y = 0;
        native_y <
        PACMAN_NATIVE_HEIGHT;
        native_y++)
    {
        for(int native_x = 0;
            native_x <
            PACMAN_NATIVE_WIDTH;
            native_x++)
        {
            int screen_x =
                PACMAN_NATIVE_HEIGHT -
                1 -
                native_y;

            int screen_y =
                native_x;

            if(flip_screen)
            {
                screen_x =
                    PACMAN_WIDTH -
                    1 -
                    screen_x;

                screen_y =
                    PACMAN_HEIGHT -
                    1 -
                    screen_y;
            }

            if(screen_x < 0 ||
               screen_x >= PACMAN_WIDTH ||
               screen_y < 0 ||
               screen_y >= PACMAN_HEIGHT)
            {
                continue;
            }

            video->framebuffer
                [screen_y]
                [screen_x] =
                video->native_framebuffer
                    [native_y]
                    [native_x];
        }
    }
}

void video_reset(
    Video* video)
{
    if(video == NULL)
    {
        return;
    }

    memset(
        video,
        0,
        sizeof(Video));

    palette_reset(
        &video->palette);
}

bool video_load_roms(
    Video* video,
    const char* rom_directory)
{
    if(video == NULL ||
       rom_directory == NULL)
    {
        return false;
    }

    video->roms_loaded =
        false;

    if(!video_load_graphics_roms(
           video,
           rom_directory))
    {
        printf(
            "Falha ao carregar as ROMs "
            "gráficas.\n");

        return false;
    }

    if(!video_read_chip(
           rom_directory,
           "82s123.7f",
           video->palette_prom,
           PACMAN_PALETTE_PROM_SIZE))
    {
        printf(
            "Falha ao carregar "
            "82s123.7f\n");

        return false;
    }

    if(!video_read_chip(
           rom_directory,
           "82s126.4a",
           video->color_prom,
           PACMAN_COLOR_PROM_SIZE))
    {
        printf(
            "Falha ao carregar "
            "82s126.4a\n");

        return false;
    }

    if(!palette_decode(
           &video->palette,
           video->palette_prom,
           video->color_prom))
    {
        printf(
            "Falha ao decodificar "
            "a paleta.\n");

        return false;
    }

    video->roms_loaded =
        true;

    return true;
}

bool video_render(
    Video* video,
    const Memory* memory,
    const Bus* bus)
{
    if(video == NULL ||
       memory == NULL ||
       bus == NULL ||
       !video->roms_loaded ||
       !video->palette.initialized)
    {
        return false;
    }

    video_clear_native(
        video);

    video_clear_output(
        video);

    if(!tilemap_render(
           video->native_framebuffer,
           memory->video_ram,
           memory->color_ram,
           video->tile_rom,
           &video->palette))
    {
        return false;
    }

    if(!sprites_render(
           video->native_framebuffer,
           &memory->ram[0x07F0],
           bus->sprite_coordinates,
           video->sprite_rom,
           &video->palette))
    {
        return false;
    }

    video_rotate_frame(
        video,
        bus->flip_screen);

    return true;
}