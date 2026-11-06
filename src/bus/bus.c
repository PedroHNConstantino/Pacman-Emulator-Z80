#include "bus.h"

#include <stddef.h>
#include <string.h>

void bus_init(
    Bus* bus,
    Memory* memory)
{
    if(bus == NULL)
    {
        return;
    }

    memset(
        bus,
        0,
        sizeof(Bus));

    bus->memory =
        memory;

    bus->interrupt_vector =
        0x00;

    /*
     * Todas as entradas começam soltas,
     * portanto todos os bits começam em 1.
     */
    bus->input_0 =
        0xFF;

    bus->input_1 =
        0xFF;

    /*
     * Configuração inicial:
     * moeda/jogo, três vidas e bonus
     * padrão poderão ser ajustados depois.
     *
     * Por enquanto, bits em 1 evitam que
     * DIP switches sejam interpretados
     * como acionados.
     */
    bus->dip_switches =
        0xFF;
}

void bus_set_input(
    Bus* bus,
    u8 input_mask,
    bool pressed)
{
    if(bus == NULL)
    {
        return;
    }

    if(pressed)
    {
        bus->input_0 &=
            (u8) ~input_mask;
    }
    else
    {
        bus->input_0 |=
            input_mask;
    }
}

void bus_set_input_1(
    Bus* bus,
    u8 input_mask,
    bool pressed)
{
    if(bus == NULL)
    {
        return;
    }

    if(pressed)
    {
        bus->input_1 &=
            (u8) ~input_mask;
    }
    else
    {
        bus->input_1 |=
            input_mask;
    }
}

u8 bus_read(
    Bus* bus,
    u16 address)
{
    if(bus == NULL ||
       bus->memory == NULL)
    {
        return 0xFF;
    }

    /*
     * Espelhamento do barramento do
     * Pac-Man. O hardware repete as
     * regiões principais acima de 0x7FFF.
     */
    address &=
        0x7FFF;

    if(address <= 0x3FFF)
    {
        return
            bus->memory->rom[address];
    }

    if(address >= 0x4000 &&
       address <= 0x43FF)
    {
        return
            bus->memory
                ->video_ram
                [address - 0x4000];
    }

    if(address >= 0x4400 &&
       address <= 0x47FF)
    {
        return
            bus->memory
                ->color_ram
                [address - 0x4400];
    }

    if(address >= 0x4800 &&
       address <= 0x4FFF)
    {
        return
            bus->memory
                ->ram
                [address - 0x4800];
    }

    /*
     * 0x5000–0x503F: IN0
     */
    if(address >= 0x5000 &&
       address <= 0x503F)
    {
        return
            bus->input_0;
    }

    /*
     * 0x5040–0x507F: IN1
     */
    if(address >= 0x5040 &&
       address <= 0x507F)
    {
        return
            bus->input_1;
    }

    /*
     * 0x5080–0x50BF: DIP switches.
     */
    if(address >= 0x5080 &&
       address <= 0x50BF)
    {
        return
            bus->dip_switches;
    }

    return 0xFF;
}

void bus_write(
    Bus* bus,
    u16 address,
    u8 value)
{
    if(bus == NULL ||
       bus->memory == NULL)
    {
        return;
    }

    address &=
        0x7FFF;

    if(address <= 0x3FFF)
    {
        return;
    }

    if(address >= 0x4000 &&
       address <= 0x43FF)
    {
        bus->memory
            ->video_ram
            [address - 0x4000] =
            value;

        return;
    }

    if(address >= 0x4400 &&
       address <= 0x47FF)
    {
        bus->memory
            ->color_ram
            [address - 0x4400] =
            value;

        return;
    }

    if(address >= 0x4800 &&
       address <= 0x4FFF)
    {
        bus->memory
            ->ram
            [address - 0x4800] =
            value;

        return;
    }

    if(address == 0x5000)
    {
        bus->interrupt_enabled =
            (value & 0x01) != 0;

        return;
    }

    if(address == 0x5001)
    {
        bus->sound_enabled =
            (value & 0x01) != 0;

        return;
    }

    if(address == 0x5003)
    {
        bus->flip_screen =
            (value & 0x01) != 0;

        return;
    }

    /*
     * Registradores de coordenadas dos
     * oito sprites.
     */
    if(address >= 0x5060 &&
       address <= 0x506F)
    {
        bus->sprite_coordinates
            [address - 0x5060] =
            value;

        return;
    }

    /*
     * Demais registradores de som,
     * watchdog e lâmpadas ainda não são
     * implementados.
     */
}

void bus_io_write(
    Bus* bus,
    u16 port,
    u8 value)
{
    if(bus == NULL)
    {
        return;
    }

    u8 port_number =
        (u8) (
            port &
            0x00FF);

    bus->output_ports
        [port_number] =
        value;

    if(port_number == 0x00)
    {
        bus->interrupt_vector =
            value;
    }
}