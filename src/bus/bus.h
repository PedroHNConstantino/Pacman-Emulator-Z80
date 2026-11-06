#ifndef BUS_H
#define BUS_H

#include "../core/types.h"
#include "../memory/memory.h"

#include <stdbool.h>

#define PACMAN_INPUT_UP       0x01
#define PACMAN_INPUT_LEFT     0x02
#define PACMAN_INPUT_RIGHT    0x04
#define PACMAN_INPUT_DOWN     0x08
#define PACMAN_INPUT_RACK     0x10
#define PACMAN_INPUT_COIN_1   0x20
#define PACMAN_INPUT_COIN_2   0x40
#define PACMAN_INPUT_CREDIT   0x80

#define PACMAN_INPUT_P2_UP    0x01
#define PACMAN_INPUT_P2_LEFT  0x02
#define PACMAN_INPUT_P2_RIGHT 0x04
#define PACMAN_INPUT_P2_DOWN  0x08
#define PACMAN_INPUT_TEST     0x10
#define PACMAN_INPUT_START_1  0x20
#define PACMAN_INPUT_START_2  0x40
#define PACMAN_INPUT_CABINET  0x80

typedef struct
{
    Memory* memory;

    u8 output_ports[256];
    u8 interrupt_vector;

    /*
     * Entradas ativas em nível baixo.
     * 1 = solto
     * 0 = pressionado
     */
    u8 input_0;
    u8 input_1;
    u8 dip_switches;

    u8 sprite_coordinates[0x10];

    bool interrupt_enabled;
    bool sound_enabled;
    bool flip_screen;

} Bus;

void bus_init(
    Bus* bus,
    Memory* memory);

u8 bus_read(
    Bus* bus,
    u16 address);

void bus_write(
    Bus* bus,
    u16 address,
    u8 value);

void bus_io_write(
    Bus* bus,
    u16 port,
    u8 value);

void bus_set_input(
    Bus* bus,
    u8 input_mask,
    bool pressed);

void bus_set_input_1(
    Bus* bus,
    u8 input_mask,
    bool pressed);

#endif