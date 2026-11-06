#ifndef PLATFORM_H
#define PLATFORM_H

#include "../core/types.h"
#include "../video/video.h"

#include <stdbool.h>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    bool running;
} Platform;

bool platform_video_init(
    Platform* platform,
    const char* title,
    int scale);

bool platform_process_events(
    Platform* platform);

bool platform_video_present(
    Platform* platform,
    const Video* video);

void platform_video_shutdown(
    Platform* platform);

#endif
