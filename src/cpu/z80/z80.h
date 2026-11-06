#ifndef Z80_H
#define Z80_H

#include "../../core/types.h"
#include "../../bus/bus.h"



typedef enum
{
    Z80_STEP_UNKNOWN_OPCODE = -1,

    Z80_STEP_INVALID_ARGUMENT = -2

} Z80StepResult;

typedef struct
{
    u8 A;
    u8 F;

    u8 B;
    u8 C;

    u8 D;
    u8 E;

    u8 H;
    u8 L;

    u8 Ap;
    u8 Fp;

    u8 Bp;
    u8 Cp;

    u8 Dp;
    u8 Ep;

    u8 Hp;
    u8 Lp;

    u16 IX;
    u16 IY;

    u16 SP;

    u16 PC;

    u8 I;

    u8 R;
    bool IFF1;
    bool IFF2;
    u8 IM;
    bool halted;
    u8 ei_delay;
    u8 last_opcode;
    u16 last_instruction_address;
    u32 total_cycles;

} Z80;
void z80_reset(
    Z80* cpu);
u8 z80_fetch(
    Z80* cpu,
    Bus* bus);
int z80_step(
    Z80* cpu,
    Bus* bus);

int z80_interrupt(
    Z80* cpu,
    Bus* bus,
    u8 vector);


#endif