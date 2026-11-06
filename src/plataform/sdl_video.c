#include <SDL2/SDL.h>
#include <stddef.h>
#include <string.h>
#include "plataform.h"


bool platform_video_init(
    Platform* platform,
    const char* title,
    int scale)
{
    if(platform == NULL ||
       title == NULL ||
       scale <= 0)
    {
        return false;
    }

    memset(
        platform,
        0,
        sizeof(Platform));

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        return false;
    }

    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY,
        "0");

    platform->window =
        SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            PACMAN_WIDTH * scale,
            PACMAN_HEIGHT * scale,
            SDL_WINDOW_SHOWN);

    if(platform->window == NULL)
    {
        platform_video_shutdown(platform);
        return false;
    }

    platform->renderer =
        SDL_CreateRenderer(
            platform->window,
            -1,
            SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC);

    if(platform->renderer == NULL)
    {
        platform->renderer =
            SDL_CreateRenderer(
                platform->window,
                -1,
                SDL_RENDERER_SOFTWARE);
    }

    if(platform->renderer == NULL)
    {
        platform_video_shutdown(platform);
        return false;
    }
    if(SDL_RenderSetLogicalSize(
            platform->renderer,
            PACMAN_WIDTH,
            PACMAN_HEIGHT) != 0)
        {
        platform_video_shutdown(platform);
        return false;
    }

    platform->texture =
        SDL_CreateTexture(
            platform->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            PACMAN_WIDTH,
            PACMAN_HEIGHT);

    if(platform->texture == NULL)
    {
        platform_video_shutdown(platform);
        return false;
    }

    SDL_SetTextureBlendMode(
        platform->texture,
        SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(
        platform->renderer,
        0,
        0,
        0,
        255);

    SDL_RenderClear(
        platform->renderer);

    SDL_RenderPresent(
        platform->renderer);

    platform->running = true;

    return true;
}

bool platform_process_events(
    Platform* platform)
{
    if(platform == NULL ||
       !platform->running)
    {
        return false;
    }

    SDL_Event event;

    while(SDL_PollEvent(&event))
    {
        if(event.type == SDL_QUIT)
        {
            platform->running = false;
        }
        else if(event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE)
        {
            platform->running = false;
        }
    }

    return platform->running;
}

bool platform_video_present(
    Platform* platform,
    const Video* video)
{
    if(platform == NULL ||
       video == NULL ||
       platform->renderer == NULL ||
       platform->texture == NULL)
    {
        return false;
    }

    if(SDL_UpdateTexture(
           platform->texture,
           NULL,
           video->framebuffer,
           PACMAN_WIDTH * (int) sizeof(u32)) != 0)
    {
        return false;
    }

    if(SDL_RenderClear(
           platform->renderer) != 0)
    {
        return false;
    }

    if(SDL_RenderCopy(
           platform->renderer,
           platform->texture,
           NULL,
           NULL) != 0)
    {
        return false;
    }

    SDL_RenderPresent(
        platform->renderer);

    return true;
}

void platform_video_shutdown(
    Platform* platform)
{
    if(platform == NULL)
    {
        return;
    }

    if(platform->texture != NULL)
    {
        SDL_DestroyTexture(
            platform->texture);
    }

    if(platform->renderer != NULL)
    {
        SDL_DestroyRenderer(
            platform->renderer);
    }

    if(platform->window != NULL)
    {
        SDL_DestroyWindow(
            platform->window);
    }

    memset(
        platform,
        0,
        sizeof(Platform));

    SDL_Quit();
}
