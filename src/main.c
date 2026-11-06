#include <stdbool.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "emulator/emulator.h"
#include "plataform/plataform.h"

#define PACMAN_CPU_CLOCK_HZ 3072000.0
#define PACMAN_REFRESH_HZ 60.606061

#define PACMAN_CYCLES_PER_FRAME 50688

#define PACMAN_FRAME_TIME_SECONDS \
    (1.0 / PACMAN_REFRESH_HZ)

#define MAX_ELAPSED_SECONDS 0.25

#define MAX_CATCHUP_FRAMES 5

static bool load_program_roms(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return false;
    }

    if(!emulator_load_rom_chip(
           emulator,
           "roms/pacman.6e",
           0x0000))
    {
        printf(
            "Erro ao carregar "
            "roms/pacman.6e\n");

        return false;
    }

    if(!emulator_load_rom_chip(
           emulator,
           "roms/pacman.6f",
           0x1000))
    {
        printf(
            "Erro ao carregar "
            "roms/pacman.6f\n");

        return false;
    }

    if(!emulator_load_rom_chip(
           emulator,
           "roms/pacman.6h",
           0x2000))
    {
        printf(
            "Erro ao carregar "
            "roms/pacman.6h\n");

        return false;
    }

    if(!emulator_load_rom_chip(
           emulator,
           "roms/pacman.6j",
           0x3000))
    {
        printf(
            "Erro ao carregar "
            "roms/pacman.6j\n");

        return false;
    }

    return true;
}

static u16 get_hl(
    const Emulator* emulator)
{
    if(emulator == NULL)
    {
        return 0;
    }

    return
        ((u16) emulator->cpu.H << 8) |
        (u16) emulator->cpu.L;
}

static void print_cpu_state(
    const Emulator* emulator,
    u64 executed_instructions)
{
    if(emulator == NULL)
    {
        return;
    }

    printf(
        "\nEstado da CPU:\n");

    printf(
        "PC:  %04X\n",
        (unsigned int)
            emulator->cpu.PC);

    printf(
        "SP:  %04X\n",
        (unsigned int)
            emulator->cpu.SP);

    printf(
        "A:   %02X\n",
        (unsigned int)
            emulator->cpu.A);

    printf(
        "F:   %02X\n",
        (unsigned int)
            emulator->cpu.F);

    printf(
        "B:   %02X\n",
        (unsigned int)
            emulator->cpu.B);

    printf(
        "C:   %02X\n",
        (unsigned int)
            emulator->cpu.C);

    printf(
        "D:   %02X\n",
        (unsigned int)
            emulator->cpu.D);

    printf(
        "E:   %02X\n",
        (unsigned int)
            emulator->cpu.E);

    printf(
        "H:   %02X\n",
        (unsigned int)
            emulator->cpu.H);

    printf(
        "L:   %02X\n",
        (unsigned int)
            emulator->cpu.L);

    printf(
        "HL:  %04X\n",
        (unsigned int)
            get_hl(emulator));

    printf(
        "IX:  %04X\n",
        (unsigned int)
            emulator->cpu.IX);

    printf(
        "IY:  %04X\n",
        (unsigned int)
            emulator->cpu.IY);

    printf(
        "I:   %02X\n",
        (unsigned int)
            emulator->cpu.I);

    printf(
        "R:   %02X\n",
        (unsigned int)
            emulator->cpu.R);

    printf(
        "IM:  %u\n",
        (unsigned int)
            emulator->cpu.IM);

    printf(
        "IFF1: %d\n",
        emulator->cpu.IFF1
            ? 1
            : 0);

    printf(
        "IFF2: %d\n",
        emulator->cpu.IFF2
            ? 1
            : 0);

    printf(
        "EI delay: %u\n",
        (unsigned int)
            emulator->cpu.ei_delay);

    printf(
        "HALT: %d\n",
        emulator->cpu.halted
            ? 1
            : 0);

    printf(
        "Vetor de interrupção: %02X\n",
        (unsigned int)
            emulator->bus.interrupt_vector);

    printf(
        "Instruções executadas: %llu\n",
        (unsigned long long)
            executed_instructions);

    printf(
        "Ciclos totais: %u\n",
        (unsigned int)
            emulator->cpu.total_cycles);
}

static void print_unknown_opcode(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return;
    }

    u16 address =
        emulator->cpu
            .last_instruction_address;

    printf(
        "\nExecução interrompida.\n");

    printf(
        "Opcode não implementado: %02X\n",
        (unsigned int)
            emulator->cpu.last_opcode);

    printf(
        "Endereço da instrução: %04X\n",
        (unsigned int)
            address);

    printf(
        "Bytes em memória: "
        "%02X %02X %02X %02X "
        "%02X %02X %02X %02X\n",
        (unsigned int)
            bus_read(
                &emulator->bus,
                address),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 1)),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 2)),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 3)),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 4)),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 5)),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 6)),
        (unsigned int)
            bus_read(
                &emulator->bus,
                (u16) (address + 7)));

    printf(
        "Registradores: "
        "A=%02X F=%02X "
        "BC=%02X%02X "
        "DE=%02X%02X "
        "HL=%02X%02X "
        "SP=%04X\n",
        (unsigned int)
            emulator->cpu.A,
        (unsigned int)
            emulator->cpu.F,
        (unsigned int)
            emulator->cpu.B,
        (unsigned int)
            emulator->cpu.C,
        (unsigned int)
            emulator->cpu.D,
        (unsigned int)
            emulator->cpu.E,
        (unsigned int)
            emulator->cpu.H,
        (unsigned int)
            emulator->cpu.L,
        (unsigned int)
            emulator->cpu.SP);
}

static bool render_frame(
    Emulator* emulator,
    Platform* platform)
{
    if(emulator == NULL ||
       platform == NULL)
    {
        return false;
    }

    if(!video_render(
           &emulator->video,
           &emulator->memory,
           &emulator->bus))
    {
        printf(
            "Falha ao renderizar "
            "o vídeo.\n");

        return false;
    }

    if(!platform_video_present(
           platform,
           &emulator->video))
    {
        printf(
            "Falha ao apresentar "
            "o framebuffer.\n");

        return false;
    }

    return true;
}

static bool execute_emulated_frame(
    Emulator* emulator,
    int* cycle_carry,
    u64* executed_instructions,
    bool trace_enabled)
{
    if(emulator == NULL ||
       cycle_carry == NULL ||
       executed_instructions == NULL)
    {
        return false;
    }

    int frame_cycles =
        *cycle_carry;

    while(frame_cycles <
          PACMAN_CYCLES_PER_FRAME)
    {
        u16 pc_before =
            emulator->cpu.PC;

        u16 hl_before =
            get_hl(emulator);

        int cycles =
            emulator_step(
                emulator);

        u16 hl_after =
            get_hl(emulator);

        if(cycles ==
           Z80_STEP_UNKNOWN_OPCODE)
        {
            print_unknown_opcode(
                emulator);

            return false;
        }

        if(cycles ==
           Z80_STEP_INVALID_ARGUMENT)
        {
            printf(
                "\nErro interno: argumento "
                "inválido na CPU.\n");

            return false;
        }

        if(cycles < 0)
        {
            printf(
                "\nErro desconhecido "
                "na CPU: %d\n",
                cycles);

            return false;
        }

        (*executed_instructions)++;

        frame_cycles +=
            cycles;

        if(trace_enabled)
        {
            printf(
                "#%08llu | "
                "PC %04X -> %04X | "
                "Opcode %02X | "
                "HL %04X -> %04X | "
                "Ciclos %d | "
                "Frame %d | "
                "Total %u\n",
                (unsigned long long)
                    *executed_instructions,
                (unsigned int)
                    pc_before,
                (unsigned int)
                    emulator->cpu.PC,
                (unsigned int)
                    emulator->cpu.last_opcode,
                (unsigned int)
                    hl_before,
                (unsigned int)
                    hl_after,
                cycles,
                frame_cycles,
                (unsigned int)
                    emulator->cpu.total_cycles);
        }
    }
    *cycle_carry =
        frame_cycles -
        PACMAN_CYCLES_PER_FRAME;
    if(emulator->bus.interrupt_enabled)
    {
        int interrupt_cycles =
            emulator_interrupt(
                emulator);

        if(interrupt_cycles < 0)
        {
            printf(
                "Erro ao atender "
                "a interrupção: %d\n",
                interrupt_cycles);

            return false;
        }

        /*
         * Os ciclos utilizados para aceitar a
         * interrupção pertencem ao próximo
         * intervalo de execução.
         */
        *cycle_carry +=
            interrupt_cycles;
    }

    return true;
}

static void update_inputs(
    Emulator* emulator)
{
    if(emulator == NULL)
    {
        return;
    }

    SDL_PumpEvents();

    const Uint8* keys =
        SDL_GetKeyboardState(
            NULL);

    bool up =
        keys[SDL_SCANCODE_UP] ||
        keys[SDL_SCANCODE_W];

    bool left =
        keys[SDL_SCANCODE_LEFT] ||
        keys[SDL_SCANCODE_A];

    bool right =
        keys[SDL_SCANCODE_RIGHT] ||
        keys[SDL_SCANCODE_D];

    bool down =
        keys[SDL_SCANCODE_DOWN] ||
        keys[SDL_SCANCODE_S];

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_UP,
        up);

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_LEFT,
        left);

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_RIGHT,
        right);

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_DOWN,
        down);

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_COIN_1,
        keys[SDL_SCANCODE_5]);

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_COIN_2,
        keys[SDL_SCANCODE_6]);

    bus_set_input(
        &emulator->bus,
        PACMAN_INPUT_CREDIT,
        keys[SDL_SCANCODE_C]);

    bus_set_input_1(
        &emulator->bus,
        PACMAN_INPUT_START_1,
        keys[SDL_SCANCODE_1]);

    bus_set_input_1(
        &emulator->bus,
        PACMAN_INPUT_START_2,
        keys[SDL_SCANCODE_2]);
}

int main(void)
{
    Emulator emulator;
    Platform platform;

    emulator_init(
        &emulator);

    printf(
        "Pacman-Emu\n");

    printf(
        "CPU Z80: %.3f MHz\n",
        PACMAN_CPU_CLOCK_HZ /
        1000000.0);

    printf(
        "Atualização: %.6f Hz\n",
        PACMAN_REFRESH_HZ);

    printf(
        "Ciclos por quadro: %d\n\n",
        PACMAN_CYCLES_PER_FRAME);

    if(!load_program_roms(
           &emulator))
    {
        printf(
            "Falha ao carregar "
            "as ROMs de programa.\n");

        return 1;
    }

    printf(
        "ROMs de programa "
        "carregadas com sucesso.\n");

    if(!video_load_roms(
           &emulator.video,
           "roms"))
    {
        printf(
            "Falha ao carregar "
            "as ROMs de vídeo.\n");

        return 1;
    }

    printf(
        "ROMs de vídeo "
        "carregadas com sucesso.\n");

    if(!platform_video_init(
           &platform,
           "Pacman-Emu",
           2))
    {
        printf(
            "Falha ao inicializar "
            "a janela SDL2.\n");

        return 1;
    }

    printf(
        "Janela SDL2 inicializada "
        "com sucesso.\n");

    const bool trace_enabled =
        false;

    u64 executed_instructions =
        0;

    int cycle_carry =
        0;

    bool execution_error =
        false;

    /*
     * Relógio de alta resolução do SDL.
     */
    Uint64 performance_frequency =
        SDL_GetPerformanceFrequency();

    Uint64 previous_counter =
        SDL_GetPerformanceCounter();

    double accumulator =
        0.0;

    printf(
        "CPU iniciada.\n");

    printf(
        "Feche a janela ou pressione "
        "Esc para encerrar.\n\n");

    /*
     * Apresenta o estado inicial antes do
     * primeiro quadro emulado.
     */
    if(!render_frame(
           &emulator,
           &platform))
    {
        platform_video_shutdown(
            &platform);

        return 1;
    }

    bool running =
        true;

    while(running)
    {

        if(!platform_process_events(
               &platform))
        {
            running =
                false;

            break;
        }

        update_inputs(
        &emulator
            );

        Uint64 current_counter =
            SDL_GetPerformanceCounter();

        double elapsed_seconds =
            (double) (
                current_counter -
                previous_counter) /
            (double)
                performance_frequency;

        previous_counter =
            current_counter;

        if(elapsed_seconds >
           MAX_ELAPSED_SECONDS)
        {
            elapsed_seconds =
                MAX_ELAPSED_SECONDS;
        }

        accumulator +=
            elapsed_seconds;

        int frames_executed =
            0;

        bool video_changed =
            false;

        /*
         * Executa os quadros que já venceram
         * no relógio real. Normalmente será
         * apenas um quadro por passagem.
         */
        while(accumulator >=
              PACMAN_FRAME_TIME_SECONDS &&
              frames_executed <
              MAX_CATCHUP_FRAMES)
        {
            if(!execute_emulated_frame(
                   &emulator,
                   &cycle_carry,
                   &executed_instructions,
                   trace_enabled))
            {
                execution_error =
                    true;

                running =
                    false;

                break;
            }

            accumulator -=
                PACMAN_FRAME_TIME_SECONDS;

            frames_executed++;

            video_changed =
                true;
        }

        if(execution_error)
        {
            break;
        }

        if(frames_executed ==
           MAX_CATCHUP_FRAMES &&
           accumulator >=
           PACMAN_FRAME_TIME_SECONDS)
        {
            accumulator =
                0.0;
        }

        if(video_changed)
        {
            if(!render_frame(
                   &emulator,
                   &platform))
            {
                execution_error =
                    true;

                running =
                    false;

                break;
            }
        }
        else
        {
            SDL_Delay(1);
        }
    }

    platform_video_shutdown(
        &platform);

    print_cpu_state(
        &emulator,
        executed_instructions);

    return
        execution_error
            ? 1
            : 0;
}