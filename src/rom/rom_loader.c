#include "rom_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool rom_load(
    const char* path,
    RomFile* rom)
{
    if(path == NULL ||
       rom == NULL)
    {
        return false;
    }

    memset(
        rom,
        0,
        sizeof(RomFile));

    FILE* fp =
        fopen(
            path,
            "rb");

    if(fp == NULL)
    {
        return false;
    }

    if(fseek(
           fp,
           0,
           SEEK_END) != 0)
    {
        fclose(fp);

        return false;
    }

    long file_size =
        ftell(fp);

    if(file_size <= 0)
    {
        fclose(fp);

        return false;
    }

    rewind(fp);

    rom->size =
        (u32) file_size;

    rom->data =
        malloc(
            rom->size);

    if(rom->data == NULL)
    {
        fclose(fp);

        return false;
    }

    size_t bytes_read =
        fread(
            rom->data,
            1,
            rom->size,
            fp);

    fclose(fp);

    if(bytes_read != rom->size)
    {
        free(
            rom->data);

        memset(
            rom,
            0,
            sizeof(RomFile));

        return false;
    }

    snprintf(
        rom->filename,
        sizeof(rom->filename),
        "%s",
        path);

    return true;
}

void rom_unload(
    RomFile* rom)
{
    if(rom == NULL)
    {
        return;
    }

    free(
        rom->data);

    memset(
        rom,
        0,
        sizeof(RomFile));
}