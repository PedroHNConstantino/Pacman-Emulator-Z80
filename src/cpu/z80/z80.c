#include "z80.h"

#include <stddef.h>
#include <string.h>

#define Z80_FLAG_S  0x80
#define Z80_FLAG_Z  0x40
#define Z80_FLAG_Y  0x20
#define Z80_FLAG_H  0x10
#define Z80_FLAG_X  0x08
#define Z80_FLAG_PV 0x04
#define Z80_FLAG_N  0x02
#define Z80_FLAG_C  0x01

static int z80_execute_indexed_cb(
    Z80* cpu,
    Bus* bus,
    u16 index);

static u8 z80_fetch_byte(Z80* cpu, Bus* bus)
{
    u8 value = bus_read(bus, cpu->PC);
    cpu->PC++;
    return value;
}

static u16 z80_fetch_word(Z80* cpu, Bus* bus)
{
    u8 low_byte = z80_fetch_byte(cpu, bus);
    u8 high_byte = z80_fetch_byte(cpu, bus);

    return (u16) low_byte | ((u16) high_byte << 8);
}

static u16 z80_get_bc(const Z80* cpu)
{
    return ((u16) cpu->B << 8) | (u16) cpu->C;
}

static u16 z80_get_de(const Z80* cpu)
{
    return ((u16) cpu->D << 8) | (u16) cpu->E;
}

static u16 z80_get_hl(const Z80* cpu)
{
    return ((u16) cpu->H << 8) | (u16) cpu->L;
}

static void z80_set_bc(Z80* cpu, u16 value)
{
    cpu->B = (u8) (value >> 8);
    cpu->C = (u8) (value & 0x00FF);
}

static void z80_set_de(Z80* cpu, u16 value)
{
    cpu->D = (u8) (value >> 8);
    cpu->E = (u8) (value & 0x00FF);
}

static void z80_set_hl(Z80* cpu, u16 value)
{
    cpu->H = (u8) (value >> 8);
    cpu->L = (u8) (value & 0x00FF);
}

static bool z80_even_parity(u8 value)
{
    bool parity = true;

    while(value != 0)
    {
        parity = !parity;
        value &= (u8) (value - 1);
    }

    return parity;
}

static void z80_set_logic_flags(Z80* cpu, u8 result, bool half_carry)
{
    u8 flags = 0;

    if((result & 0x80) != 0)
    {
        flags |= Z80_FLAG_S;
    }

    if(result == 0)
    {
        flags |= Z80_FLAG_Z;
    }

    flags |= result & (Z80_FLAG_Y | Z80_FLAG_X);

    if(half_carry)
    {
        flags |= Z80_FLAG_H;
    }

    if(z80_even_parity(result))
    {
        flags |= Z80_FLAG_PV;
    }

    cpu->F = flags;
}

static void z80_set_inc_flags(Z80* cpu, u8 before, u8 result)
{
    u8 flags = cpu->F & Z80_FLAG_C;

    if((result & 0x80) != 0)
    {
        flags |= Z80_FLAG_S;
    }

    if(result == 0)
    {
        flags |= Z80_FLAG_Z;
    }

    flags |= result & (Z80_FLAG_Y | Z80_FLAG_X);

    if((before & 0x0F) == 0x0F)
    {
        flags |= Z80_FLAG_H;
    }

    if(before == 0x7F)
    {
        flags |= Z80_FLAG_PV;
    }

    cpu->F = flags;
}

static void z80_set_dec_flags(Z80* cpu, u8 before, u8 result)
{
    u8 flags = cpu->F & Z80_FLAG_C;

    if((result & 0x80) != 0)
    {
        flags |= Z80_FLAG_S;
    }

    if(result == 0)
    {
        flags |= Z80_FLAG_Z;
    }

    flags |= result & (Z80_FLAG_Y | Z80_FLAG_X);

    if((before & 0x0F) == 0x00)
    {
        flags |= Z80_FLAG_H;
    }

    if(before == 0x80)
    {
        flags |= Z80_FLAG_PV;
    }

    flags |= Z80_FLAG_N;
    cpu->F = flags;
}

static u8 z80_read_register_operand(Z80* cpu, Bus* bus, u8 register_code)
{
    switch(register_code & 0x07)
    {
        case 0: return cpu->B;
        case 1: return cpu->C;
        case 2: return cpu->D;
        case 3: return cpu->E;
        case 4: return cpu->H;
        case 5: return cpu->L;
        case 6: return bus_read(bus, z80_get_hl(cpu));
        case 7: return cpu->A;
        default: return 0;
    }
}

static void z80_add_ix(
    Z80* cpu,
    u16 operand)
{
    u16 before =
        cpu->IX;

    u32 result =
        (u32) before +
        (u32) operand;

    u8 flags =
        cpu->F &
        (Z80_FLAG_S |
         Z80_FLAG_Z |
         Z80_FLAG_PV);

    if(((before & 0x0FFF) +
        (operand & 0x0FFF)) > 0x0FFF)
    {
        flags |= Z80_FLAG_H;
    }

    if((result & 0x10000) != 0)
    {
        flags |= Z80_FLAG_C;
    }

    cpu->IX =
        (u16) result;

    flags |=
        (u8) ((cpu->IX >> 8) &
              (Z80_FLAG_Y |
               Z80_FLAG_X));

    cpu->F =
        flags;
}
static void z80_add_iy(
    Z80* cpu,
    u16 operand)
{
    u16 before =
        cpu->IY;

    u32 result =
        (u32) before +
        (u32) operand;

    u8 flags =
        cpu->F &
        (Z80_FLAG_S |
         Z80_FLAG_Z |
         Z80_FLAG_PV);

    if(((before & 0x0FFF) +
        (operand & 0x0FFF)) > 0x0FFF)
    {
        flags |= Z80_FLAG_H;
    }

    if((result & 0x10000) != 0)
    {
        flags |= Z80_FLAG_C;
    }

    cpu->IY =
        (u16) result;

    flags |=
        (u8) (
            (cpu->IY >> 8) &
            (Z80_FLAG_Y |
             Z80_FLAG_X));

    cpu->F =
        flags;
}

static void z80_sbc_hl(
    Z80* cpu,
    u16 operand)
{
    u16 before =
        z80_get_hl(cpu);

    u16 carry =
        (cpu->F & Z80_FLAG_C)
            ? 1
            : 0;

    u32 subtrahend =
        (u32) operand +
        (u32) carry;

    u16 result =
        (u16) (
            (u32) before -
            subtrahend);

    u8 flags =
        Z80_FLAG_N |
        (u8) (
            (result >> 8) &
            (Z80_FLAG_S |
             Z80_FLAG_Y |
             Z80_FLAG_X));

    if(result == 0)
    {
        flags |=
            Z80_FLAG_Z;
    }

    if((before & 0x0FFF) <
       ((operand & 0x0FFF) +
        carry))
    {
        flags |=
            Z80_FLAG_H;
    }

    if(((before ^ operand) &
        (before ^ result) &
        0x8000) != 0)
    {
        flags |=
            Z80_FLAG_PV;
    }

    if((u32) before <
       subtrahend)
    {
        flags |=
            Z80_FLAG_C;
    }

    z80_set_hl(
        cpu,
        result);

    cpu->F =
        flags;
}

static void z80_adc_hl(
    Z80* cpu,
    u16 operand)
{
    u16 before =
        z80_get_hl(cpu);

    u16 carry =
        (cpu->F & Z80_FLAG_C)
            ? 1
            : 0;

    u32 wide_result =
        (u32) before +
        (u32) operand +
        (u32) carry;

    u16 result =
        (u16) wide_result;

    u8 flags =
        (u8) (
            (result >> 8) &
            (Z80_FLAG_S |
             Z80_FLAG_Y |
             Z80_FLAG_X));

    if(result == 0)
    {
        flags |=
            Z80_FLAG_Z;
    }

    if(((before & 0x0FFF) +
        (operand & 0x0FFF) +
        carry) > 0x0FFF)
    {
        flags |=
            Z80_FLAG_H;
    }

    if(((~(before ^ operand)) &
        (before ^ result) &
        0x8000) != 0)
    {
        flags |=
            Z80_FLAG_PV;
    }

    if((wide_result &
        0x10000u) != 0)
    {
        flags |=
            Z80_FLAG_C;
    }

    z80_set_hl(
        cpu,
        result);

    cpu->F =
        flags;
}

static void z80_ldi_step(
    Z80* cpu,
    Bus* bus)
{
    u8 value =
        bus_read(
            bus,
            z80_get_hl(cpu));

    bus_write(
        bus,
        z80_get_de(cpu),
        value);

    z80_set_hl(
        cpu,
        (u16) (
            z80_get_hl(cpu) +
            1));

    z80_set_de(
        cpu,
        (u16) (
            z80_get_de(cpu) +
            1));

    z80_set_bc(
        cpu,
        (u16) (
            z80_get_bc(cpu) -
            1));

    u8 preserved_flags =
        cpu->F &
        (Z80_FLAG_S |
         Z80_FLAG_Z |
         Z80_FLAG_C);

    u8 sum =
        (u8) (
            cpu->A +
            value);

    u8 flags =
        preserved_flags |
        (sum &
         (Z80_FLAG_Y |
          Z80_FLAG_X));

    if(z80_get_bc(cpu) != 0)
    {
        flags |=
            Z80_FLAG_PV;
    }

    cpu->F =
        flags;
}
static void z80_cpi_step(
    Z80* cpu,
    Bus* bus)
{
    u8 value =
        bus_read(
            bus,
            z80_get_hl(cpu));

    u8 accumulator =
        cpu->A;

    u8 result =
        (u8) (
            accumulator -
            value);

    bool half_borrow =
        (accumulator &
         0x0F) <
        (value &
         0x0F);

    z80_set_hl(
        cpu,
        (u16) (
            z80_get_hl(cpu) +
            1));

    z80_set_bc(
        cpu,
        (u16) (
            z80_get_bc(cpu) -
            1));

    /*
     * CPI e CPIR preservam Carry.
     */
    u8 flags =
        cpu->F &
        Z80_FLAG_C;

    flags |=
        Z80_FLAG_N;

    if((result &
        0x80) != 0)
    {
        flags |=
            Z80_FLAG_S;
    }

    if(result == 0)
    {
        flags |=
            Z80_FLAG_Z;
    }

    if(half_borrow)
    {
        flags |=
            Z80_FLAG_H;
    }

    if(z80_get_bc(cpu) != 0)
    {
        flags |=
            Z80_FLAG_PV;
    }

    /*
     * Bits não documentados X e Y.
     */
    u8 adjusted_result =
        (u8) (
            result -
            (half_borrow
                ? 1
                : 0));

    if((adjusted_result &
        0x08) != 0)
    {
        flags |=
            Z80_FLAG_X;
    }

    if((adjusted_result &
        0x02) != 0)
    {
        flags |=
            Z80_FLAG_Y;
    }

    cpu->F =
        flags;
}

static int z80_execute_ed(
    Z80* cpu,
    Bus* bus)
{
    u8 opcode =
        z80_fetch_byte(
            cpu,
            bus);

    switch(opcode)
    {
        case 0x47: /* LD I,A */
        {
            cpu->I = cpu->A;

            return 9;
        }
        case 0xA1:
{
    z80_cpi_step(
        cpu,
        bus);

    return 16;
}
case 0xB1:
{
    z80_cpi_step(
        cpu,
        bus);

    if(z80_get_bc(cpu) != 0 &&
       (cpu->F &
        Z80_FLAG_Z) == 0)
    {
        cpu->PC =
            (u16) (
                cpu->PC -
                2);

        return 21;
    }

    return 16;
}
        case 0x44:
{
    u8 before =
        cpu->A;

    u8 result =
        (u8) (
            0 -
            before);

    u8 flags =
        Z80_FLAG_N |
        (result &
         (Z80_FLAG_S |
          Z80_FLAG_Y |
          Z80_FLAG_X));

    if(result == 0)
    {
        flags |=
            Z80_FLAG_Z;
    }

    /*
     * Há empréstimo do bit 4 quando o
     * nibble inferior anterior não é zero.
     */
    if((before &
        0x0F) != 0)
    {
        flags |=
            Z80_FLAG_H;
    }

    /*
     * -128 não pode ser representado como
     * +128 em um byte com sinal.
     */
    if(before == 0x80)
    {
        flags |=
            Z80_FLAG_PV;
    }

    
    if(before != 0)
    {
        flags |=
            Z80_FLAG_C;
    }

    cpu->A =
        result;

    cpu->F =
        flags;

    return 8;
}
        case 0xA0:
{
    z80_ldi_step(
        cpu,
        bus);

    return 16;
}
        case 0x43:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u16 value =
        z80_get_bc(cpu);

    bus_write(
        bus,
        address,
        (u8) (
            value &
            0x00FF));

    bus_write(
        bus,
        (u16) (
            address +
            1),
        (u8) (
            value >>
            8));

    return 20;
}

case 0x53:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u16 value =
        z80_get_de(cpu);

    bus_write(
        bus,
        address,
        (u8) (
            value &
            0x00FF));

    bus_write(
        bus,
        (u16) (
            address +
            1),
        (u8) (
            value >>
            8));

    return 20;
}

case 0x63:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u16 value =
        z80_get_hl(cpu);

    bus_write(
        bus,
        address,
        (u8) (
            value &
            0x00FF));

    bus_write(
        bus,
        (u16) (
            address +
            1),
        (u8) (
            value >>
            8));

    return 20;
}

case 0x73:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    bus_write(
        bus,
        address,
        (u8) (
            cpu->SP &
            0x00FF));

    bus_write(
        bus,
        (u16) (
            address +
            1),
        (u8) (
            cpu->SP >>
            8));

    return 20;
}
        case 0x4A:
{
    z80_adc_hl(
        cpu,
        z80_get_bc(cpu));

    return 15;
}

case 0x5A:
{
    z80_adc_hl(
        cpu,
        z80_get_de(cpu));

    return 15;
}

case 0x6A:
{
    z80_adc_hl(
        cpu,
        z80_get_hl(cpu));

    return 15;
}

case 0x7A:
{
    z80_adc_hl(
        cpu,
        cpu->SP);

    return 15;
}
        case 0x4B:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u8 low_byte =
        bus_read(
            bus,
            address);

    u8 high_byte =
        bus_read(
            bus,
            (u16) (address + 1));

    z80_set_bc(
        cpu,
        (u16) low_byte |
        ((u16) high_byte << 8));

    return 20;
}

case 0x5B:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u8 low_byte =
        bus_read(
            bus,
            address);

    u8 high_byte =
        bus_read(
            bus,
            (u16) (address + 1));

    z80_set_de(
        cpu,
        (u16) low_byte |
        ((u16) high_byte << 8));

    return 20;
}

case 0x6B:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u8 low_byte =
        bus_read(
            bus,
            address);

    u8 high_byte =
        bus_read(
            bus,
            (u16) (address + 1));

    z80_set_hl(
        cpu,
        (u16) low_byte |
        ((u16) high_byte << 8));

    return 20;
}

case 0x7B:
{
    u16 address =
        z80_fetch_word(
            cpu,
            bus);

    u8 low_byte =
        bus_read(
            bus,
            address);

    u8 high_byte =
        bus_read(
            bus,
            (u16) (address + 1));

    cpu->SP =
        (u16) low_byte |
        ((u16) high_byte << 8);

    return 20;
}
        case 0x42:
{
    z80_sbc_hl(
        cpu,
        z80_get_bc(cpu));

    return 15;
}

case 0x52:
{
    z80_sbc_hl(
        cpu,
        z80_get_de(cpu));

    return 15;
}

case 0x62:
{
    z80_sbc_hl(
        cpu,
        z80_get_hl(cpu));

    return 15;
}

case 0x72:
{
    z80_sbc_hl(
        cpu,
        cpu->SP);

    return 15;
}
case 0xB0:
{
    z80_ldi_step(
        cpu,
        bus);

    if(z80_get_bc(cpu) != 0)
    {
        cpu->PC =
            (u16) (
                cpu->PC -
                2);

        return 21;
    }

    return 16;
}
   
        case 0x5E:
        {
            cpu->IM = 2;

            return 8;
        }

        default:
        {
            return Z80_STEP_UNKNOWN_OPCODE;
        }
    }

}

void z80_reset(Z80* cpu)
{
    if(cpu == NULL)
    {
        return;
    }

    memset(cpu, 0, sizeof(Z80));

    cpu->PC = 0x0000;
    cpu->SP = 0xFFFF;
    cpu->IFF1 = false;
    cpu->IFF2 = false;
    cpu->IM = 0;
    cpu->halted = false;
    cpu->ei_delay = 0;
    cpu->last_opcode = 0x00;
    cpu->total_cycles = 0;
}

u8 z80_fetch(Z80* cpu, Bus* bus)
{
    if(cpu == NULL || bus == NULL)
    {
        return 0x00;
    }

    return z80_fetch_byte(cpu, bus);
}

static void z80_push_word(
    Z80* cpu,
    Bus* bus,
    u16 value)
{
    cpu->SP--;

    bus_write(
        bus,
        cpu->SP,
        (u8) (value >> 8));

    cpu->SP--;

    bus_write(
        bus,
        cpu->SP,
        (u8) (value & 0x00FF));
}

static u16 z80_pop_word(
    Z80* cpu,
    Bus* bus)
{
    u8 low_byte =
        bus_read(
            bus,
            cpu->SP);

    cpu->SP++;

    u8 high_byte =
        bus_read(
            bus,
            cpu->SP);

    cpu->SP++;

    return
        (u16) low_byte |
        ((u16) high_byte << 8);
}

static void z80_set_add_flags(
    Z80* cpu,
    u8 left,
    u8 right,
    u8 carry,
    u8 result)
{
    u16 wide_result =
        (u16) left +
        (u16) right +
        (u16) carry;

    u8 flags =
        result &
        (Z80_FLAG_S |
         Z80_FLAG_Y |
         Z80_FLAG_X);

    if(result == 0)
    {
        flags |= Z80_FLAG_Z;
    }

    if(((left & 0x0F) +
        (right & 0x0F) +
        carry) > 0x0F)
    {
        flags |= Z80_FLAG_H;
    }

    if(((~(left ^ right)) &
        (left ^ result) &
        0x80) != 0)
    {
        flags |= Z80_FLAG_PV;
    }

    if(wide_result > 0xFF)
    {
        flags |= Z80_FLAG_C;
    }

    cpu->F = flags;
}

static void z80_set_sub_flags(
    Z80* cpu,
    u8 left,
    u8 right,
    u8 carry,
    u8 result)
{
    u16 subtrahend =
        (u16) right +
        (u16) carry;

    u8 flags =
        Z80_FLAG_N |
        (result &
         (Z80_FLAG_S |
          Z80_FLAG_Y |
          Z80_FLAG_X));

    if(result == 0)
    {
        flags |= Z80_FLAG_Z;
    }

    if((left & 0x0F) <
       ((right & 0x0F) + carry))
    {
        flags |= Z80_FLAG_H;
    }

    if(((left ^ right) &
        (left ^ result) &
        0x80) != 0)
    {
        flags |= Z80_FLAG_PV;
    }

    if((u16) left < subtrahend)
    {
        flags |= Z80_FLAG_C;
    }

    cpu->F = flags;
}

static void z80_add_a(
    Z80* cpu,
    u8 operand,
    u8 carry)
{
    u8 before =
        cpu->A;

    u8 result =
        (u8) (
            before +
            operand +
            carry);

    cpu->A =
        result;

    z80_set_add_flags(
        cpu,
        before,
        operand,
        carry,
        result);
}

static void z80_sub_a(
    Z80* cpu,
    u8 operand,
    u8 carry)
{
    u8 before =
        cpu->A;

    u8 result =
        (u8) (
            before -
            operand -
            carry);

    cpu->A =
        result;

    z80_set_sub_flags(
        cpu,
        before,
        operand,
        carry,
        result);
}

static void z80_compare_a(
    Z80* cpu,
    u8 operand)
{
    u8 result =
        (u8) (
            cpu->A -
            operand);

    z80_set_sub_flags(
        cpu,
        cpu->A,
        operand,
        0,
        result);
}

static void z80_write_register_operand(
    Z80* cpu,
    Bus* bus,
    u8 register_code,
    u8 value)
{
    switch(register_code & 0x07)
    {
        case 0:
        {
            cpu->B = value;
            break;
        }

        case 1:
        {
            cpu->C = value;
            break;
        }

        case 2:
        {
            cpu->D = value;
            break;
        }

        case 3:
        {
            cpu->E = value;
            break;
        }

        case 4:
        {
            cpu->H = value;
            break;
        }

        case 5:
        {
            cpu->L = value;
            break;
        }

        case 6:
        {
            bus_write(
                bus,
                z80_get_hl(cpu),
                value);

            break;
        }

        case 7:
        {
            cpu->A = value;
            break;
        }
    }
}

static u16 z80_indexed_address(
    u16 index,
    s8 displacement)
{
    return
        (u16) (
            index +
            displacement);
}

static void z80_set_rotate_flags(
    Z80* cpu,
    u8 result,
    bool carry)
{
    u8 flags =
        result &
        (Z80_FLAG_S |
         Z80_FLAG_Y |
         Z80_FLAG_X);

    if(result == 0)
    {
        flags |= Z80_FLAG_Z;
    }

    if(z80_even_parity(result))
    {
        flags |= Z80_FLAG_PV;
    }

    if(carry)
    {
        flags |= Z80_FLAG_C;
    }

    cpu->F =
        flags;
}

static void z80_set_bit_flags(
    Z80* cpu,
    u8 value,
    u8 bit)
{
    u8 mask =
        (u8) (1u << bit);

    bool bit_is_zero =
        (value & mask) == 0;

    u8 flags =
        cpu->F & Z80_FLAG_C;

    flags |=
        Z80_FLAG_H;

    if(bit_is_zero)
    {
        flags |=
            Z80_FLAG_Z |
            Z80_FLAG_PV;
    }

    if(bit == 7 &&
       !bit_is_zero)
    {
        flags |=
            Z80_FLAG_S;
    }

    flags |=
        value &
        (Z80_FLAG_Y |
         Z80_FLAG_X);

    cpu->F =
        flags;
}

static int z80_execute_indexed_cb(
    Z80* cpu,
    Bus* bus,
    u16 index)
{
    if(cpu == NULL ||
       bus == NULL)
    {
        return
            Z80_STEP_INVALID_ARGUMENT;
    }

    /*
     * Formato:
     *
     * DD CB displacement opcode
     * FD CB displacement opcode
     */
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u8 opcode =
        z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            index,
            displacement);

    u8 value =
        bus_read(
            bus,
            address);

    u8 group =
        opcode >> 6;

    u8 operation =
        (opcode >> 3) &
        0x07;

    u8 register_code =
        opcode &
        0x07;

    /*
     * Grupo 0:
     * RLC, RRC, RL, RR,
     * SLA, SRA, SLL e SRL.
     */
    if(group == 0)
    {
        u8 result =
            0;

        bool carry =
            false;

        switch(operation)
        {
            case 0:
            {
                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        (value << 1) |
                        (value >> 7));

                break;
            }

            case 1:
            {
                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        (value >> 1) |
                        (value << 7));

                break;
            }

            case 2:
            {
                u8 carry_in =
                    (cpu->F &
                     Z80_FLAG_C)
                        ? 1
                        : 0;

                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        (value << 1) |
                        carry_in);

                break;
            }

            case 3:
            {
                u8 carry_in =
                    (cpu->F &
                     Z80_FLAG_C)
                        ? 0x80
                        : 0x00;

                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        (value >> 1) |
                        carry_in);

                break;
            }

            case 4:
            {
                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        value << 1);

                break;
            }

            case 5:
            {
                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        (value >> 1) |
                        (value & 0x80));

                break;
            }

            case 6:
            {
                /*
                 * SLL é uma operação não
                 * documentada do Z80.
                 */
                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        (value << 1) |
                        0x01);

                break;
            }

            case 7:
            {
                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        value >> 1);

                break;
            }
        }

        bus_write(
            bus,
            address,
            result);

        /*
         * Nas variantes não documentadas,
         * quando o campo final não é 6,
         * o resultado também é copiado para
         * o registrador indicado.
         */
        if(register_code != 6)
        {
            z80_write_register_operand(
                cpu,
                bus,
                register_code,
                result);
        }

        z80_set_rotate_flags(
            cpu,
            result,
            carry);

        return 23;
    }

    /*
     * Grupo 1:
     * BIT b,(IX+d) ou BIT b,(IY+d).
     *
     * Não escreve na memória e não copia
     * valor para registrador.
     */
    if(group == 1)
    {
        u8 mask =
            (u8) (
                1u <<
                operation);

        bool bit_is_zero =
            (value &
             mask) == 0;

        u8 flags =
            cpu->F &
            Z80_FLAG_C;

        flags |=
            Z80_FLAG_H;

        if(bit_is_zero)
        {
            flags |=
                Z80_FLAG_Z |
                Z80_FLAG_PV;
        }

        if(operation == 7 &&
           !bit_is_zero)
        {
            flags |=
                Z80_FLAG_S;
        }

        /*
         * BIT indexado obtém os bits
         * não documentados X e Y do byte
         * alto do endereço efetivo.
         */
        flags |=
            (u8) (
                (address >> 8) &
                (Z80_FLAG_Y |
                 Z80_FLAG_X));

        cpu->F =
            flags;

        return 20;
    }

    /*
     * Grupo 2:
     * RES b,(IX+d) ou RES b,(IY+d).
     */
    if(group == 2)
    {
        u8 result =
            (u8) (
                value &
                (u8) ~(
                    1u <<
                    operation));

        bus_write(
            bus,
            address,
            result);

        if(register_code != 6)
        {
            z80_write_register_operand(
                cpu,
                bus,
                register_code,
                result);
        }

        return 23;
    }

    /*
     * Grupo 3:
     * SET b,(IX+d) ou SET b,(IY+d).
     */
    u8 result =
        (u8) (
            value |
            (u8) (
                1u <<
                operation));

    bus_write(
        bus,
        address,
        result);

    if(register_code != 6)
    {
        z80_write_register_operand(
            cpu,
            bus,
            register_code,
            result);
    }

    return 23;
}

static int z80_execute_cb(
    Z80* cpu,
    Bus* bus)
{
    u8 opcode =
        z80_fetch_byte(
            cpu,
            bus);

    u8 group =
        opcode >> 6;

    u8 operation =
        (opcode >> 3) & 0x07;

    u8 register_code =
        opcode & 0x07;

    u8 value =
        z80_read_register_operand(
            cpu,
            bus,
            register_code);

    if(group == 0)
    {
        u8 result = 0;
        bool carry = false;

        switch(operation)
        {
            case 0:
            {
                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        (value << 1) |
                        (value >> 7));

                break;
            }

            case 1:
            {
                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        (value >> 1) |
                        (value << 7));

                break;
            }

            case 2:
            {
                u8 carry_in =
                    (cpu->F & Z80_FLAG_C)
                        ? 1
                        : 0;

                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        (value << 1) |
                        carry_in);

                break;
            }

            case 3:
            {
                u8 carry_in =
                    (cpu->F & Z80_FLAG_C)
                        ? 0x80
                        : 0x00;

                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        (value >> 1) |
                        carry_in);

                break;
            }

            case 4:
            {
                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (value << 1);

                break;
            }

            case 5:
            {
                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (
                        (value >> 1) |
                        (value & 0x80));

                break;
            }

            case 6:
            {
                carry =
                    (value & 0x80) != 0;

                result =
                    (u8) (
                        (value << 1) |
                        0x01);

                break;
            }

            case 7:
            {
                carry =
                    (value & 0x01) != 0;

                result =
                    (u8) (value >> 1);

                break;
            }
        }

        z80_write_register_operand(
            cpu,
            bus,
            register_code,
            result);

        z80_set_rotate_flags(
            cpu,
            result,
            carry);

        return
            register_code == 6
                ? 15
                : 8;
    }

    if(group == 1)
    {
        z80_set_bit_flags(
            cpu,
            value,
            operation);

        return
            register_code == 6
                ? 12
                : 8;
    }

    if(group == 2)
    {
        u8 result =
            (u8) (
                value &
                (u8) ~(1u << operation));

        z80_write_register_operand(
            cpu,
            bus,
            register_code,
            result);

        return
            register_code == 6
                ? 15
                : 8;
    }

    u8 result =
        (u8) (
            value |
            (u8) (1u << operation));

    z80_write_register_operand(
        cpu,
        bus,
        register_code,
        result);

    return
        register_code == 6
            ? 15
            : 8;
}

static int z80_execute_dd(
    Z80* cpu,
    Bus* bus);

static int z80_execute_fd(
    Z80* cpu,
    Bus* bus);

static int z80_execute_dd(
    Z80* cpu,
    Bus* bus)
{
    u8 opcode =
        z80_fetch_byte(
            cpu,
            bus);

if(opcode == 0xDD)
{
    int cycles =
        z80_execute_dd(
            cpu,
            bus);

    if(cycles < 0)
    {
        return cycles;
    }

    return
        cycles +
        4;
}

if(opcode == 0xFD)
{
    int cycles =
        z80_execute_fd(
            cpu,
            bus);

    if(cycles < 0)
    {
        return cycles;
    }

    return
        cycles +
        4;
}

    switch(opcode)
    {
        case 0x09:
        {
            z80_add_ix(
                cpu,
                z80_get_bc(cpu));

            return 15;
        }
        case 0x34:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 before =
        bus_read(
            bus,
            address);

    u8 result =
        (u8) (
            before +
            1);

    bus_write(
        bus,
        address,
        result);

    z80_set_inc_flags(
        cpu,
        before,
        result);

    return 23;
}

case 0x35:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 before =
        bus_read(
            bus,
            address);

    u8 result =
        (u8) (
            before -
            1);

    bus_write(
        bus,
        address,
        result);

    z80_set_dec_flags(
        cpu,
        before,
        result);

    return 23;
}
        case 0x86:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 operand =
        bus_read(
            bus,
            address);

    z80_add_a(
        cpu,
        operand,
        0);

    return 19;
}

case 0x8E:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 operand =
        bus_read(
            bus,
            address);

    u8 carry =
        (cpu->F & Z80_FLAG_C)
            ? 1
            : 0;

    z80_add_a(
        cpu,
        operand,
        carry);

    return 19;
}

case 0x96:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 operand =
        bus_read(
            bus,
            address);

    z80_sub_a(
        cpu,
        operand,
        0);

    return 19;
}

case 0x9E:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 operand =
        bus_read(
            bus,
            address);

    u8 carry =
        (cpu->F & Z80_FLAG_C)
            ? 1
            : 0;

    z80_sub_a(
        cpu,
        operand,
        carry);

    return 19;
}

case 0xA6:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    cpu->A &=
        bus_read(
            bus,
            address);

    z80_set_logic_flags(
        cpu,
        cpu->A,
        true);

    return 19;
}

case 0xAE:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    cpu->A ^=
        bus_read(
            bus,
            address);

    z80_set_logic_flags(
        cpu,
        cpu->A,
        false);

    return 19;
}

case 0xB6:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    cpu->A |=
        bus_read(
            bus,
            address);

    z80_set_logic_flags(
        cpu,
        cpu->A,
        false);

    return 19;
}

case 0xBE:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IX,
            displacement);

    u8 operand =
        bus_read(
            bus,
            address);

    z80_compare_a(
        cpu,
        operand);

    return 19;
}

        case 0x19:
        {
            z80_add_ix(
                cpu,
                z80_get_de(cpu));

            return 15;
        }

        case 0x21:
        {
            cpu->IX =
                z80_fetch_word(
                    cpu,
                    bus);

            return 14;
        }

        case 0x22:
        {
            u16 address =
                z80_fetch_word(
                    cpu,
                    bus);

            bus_write(
                bus,
                address,
                (u8) (cpu->IX & 0x00FF));

            bus_write(
                bus,
                (u16) (address + 1),
                (u8) (cpu->IX >> 8));

            return 20;
        }

        case 0x23:
        {
            cpu->IX++;

            return 10;
        }

        case 0x29:
        {
            z80_add_ix(
                cpu,
                cpu->IX);

            return 15;
        }

        case 0x2A:
        {
            u16 address =
                z80_fetch_word(
                    cpu,
                    bus);

            u8 low_byte =
                bus_read(
                    bus,
                    address);

            u8 high_byte =
                bus_read(
                    bus,
                    (u16) (address + 1));

            cpu->IX =
                (u16) low_byte |
                ((u16) high_byte << 8);

            return 20;
        }

        case 0x2B:
        {
            cpu->IX--;

            return 10;
        }

        case 0x39:
        {
            z80_add_ix(
                cpu,
                cpu->SP);

            return 15;
        }

        case 0xE1:
        {
            cpu->IX =
                z80_pop_word(
                    cpu,
                    bus);

            return 14;
        }

        case 0xE3:
        {
            u8 low_byte =
                bus_read(
                    bus,
                    cpu->SP);

            u8 high_byte =
                bus_read(
                    bus,
                    (u16) (cpu->SP + 1));

            u16 stack_value =
                (u16) low_byte |
                ((u16) high_byte << 8);

            bus_write(
                bus,
                cpu->SP,
                (u8) (cpu->IX & 0x00FF));

            bus_write(
                bus,
                (u16) (cpu->SP + 1),
                (u8) (cpu->IX >> 8));

            cpu->IX =
                stack_value;

            return 23;
        }

        case 0xE5:
        {
            z80_push_word(
                cpu,
                bus,
                cpu->IX);

            return 15;
        }

        case 0xE9:
        {
            cpu->PC =
                cpu->IX;

            return 8;
        }

        case 0xF9:
        {
            cpu->SP =
                cpu->IX;

            return 10;
        }
                case 0x36:
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            u8 value =
                z80_fetch_byte(
                    cpu,
                    bus);

            u16 address =
                z80_indexed_address(
                    cpu->IX,
                    displacement);

            bus_write(
                bus,
                address,
                value);

            return 19;
        }

        case 0x46:
        case 0x4E:
        case 0x56:
        case 0x5E:
        case 0x66:
        case 0x6E:
        case 0x7E:
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            u16 address =
                z80_indexed_address(
                    cpu->IX,
                    displacement);

            u8 value =
                bus_read(
                    bus,
                    address);

            u8 destination_code =
                (opcode >> 3) & 0x07;

            z80_write_register_operand(
                cpu,
                bus,
                destination_code,
                value);

            return 19;
        }

        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x77:
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            u16 address =
                z80_indexed_address(
                    cpu->IX,
                    displacement);

            u8 source_code =
                opcode & 0x07;

            u8 value =
                z80_read_register_operand(
                    cpu,
                    bus,
                    source_code);

            bus_write(
                bus,
                address,
                value);

            return 19;
        }
        case 0xCB:
{
    return
        z80_execute_indexed_cb(
            cpu,
            bus,
            cpu->IX);
}

        default:
        {
            return Z80_STEP_UNKNOWN_OPCODE;
        }
    }
}

static int z80_execute_fd(
    Z80* cpu,
    Bus* bus)
{
    u8 opcode =
        z80_fetch_byte(
            cpu,
            bus);

if(opcode == 0xFD)
{
    int cycles =
        z80_execute_fd(
            cpu,
            bus);

    if(cycles < 0)
    {
        return cycles;
    }

    return
        cycles +
        4;
}

if(opcode == 0xDD)
{
    int cycles =
        z80_execute_dd(
            cpu,
            bus);

    if(cycles < 0)
    {
        return cycles;
    }

    return
        cycles +
        4;
}

    switch(opcode)
    {
        case 0x09:
        {
            z80_add_iy(
                cpu,
                z80_get_bc(cpu));

            return 15;
        }
        case 0xCB:
{
    return
        z80_execute_indexed_cb(
            cpu,
            bus,
            cpu->IY);
}
        case 0xBE:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IY,
            displacement);

    u8 operand =
        bus_read(
            bus,
            address);

    z80_compare_a(
        cpu,
        operand);

    return 19;
}
        case 0x34:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IY,
            displacement);

    u8 before =
        bus_read(
            bus,
            address);

    u8 result =
        (u8) (
            before +
            1);

    bus_write(
        bus,
        address,
        result);

    z80_set_inc_flags(
        cpu,
        before,
        result);

    return 23;
}

case 0x35:
{
    s8 displacement =
        (s8) z80_fetch_byte(
            cpu,
            bus);

    u16 address =
        z80_indexed_address(
            cpu->IY,
            displacement);

    u8 before =
        bus_read(
            bus,
            address);

    u8 result =
        (u8) (
            before -
            1);

    bus_write(
        bus,
        address,
        result);

    z80_set_dec_flags(
        cpu,
        before,
        result);

    return 23;
}

        case 0x19:
        {
            z80_add_iy(
                cpu,
                z80_get_de(cpu));

            return 15;
        }

        case 0x21:
        {
            cpu->IY =
                z80_fetch_word(
                    cpu,
                    bus);

            return 14;
        }

        case 0x22:
        {
            u16 address =
                z80_fetch_word(
                    cpu,
                    bus);

            bus_write(
                bus,
                address,
                (u8) (
                    cpu->IY &
                    0x00FF));

            bus_write(
                bus,
                (u16) (
                    address +
                    1),
                (u8) (
                    cpu->IY >>
                    8));

            return 20;
        }

        case 0x23:
        {
            cpu->IY++;

            return 10;
        }

        case 0x29:
        {
            z80_add_iy(
                cpu,
                cpu->IY);

            return 15;
        }

        case 0x2A:
        {
            u16 address =
                z80_fetch_word(
                    cpu,
                    bus);

            u8 low_byte =
                bus_read(
                    bus,
                    address);

            u8 high_byte =
                bus_read(
                    bus,
                    (u16) (
                        address +
                        1));

            cpu->IY =
                (u16) low_byte |
                ((u16) high_byte << 8);

            return 20;
        }

        case 0x2B:
        {
            cpu->IY--;

            return 10;
        }

        case 0x36:
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            u8 value =
                z80_fetch_byte(
                    cpu,
                    bus);

            u16 address =
                z80_indexed_address(
                    cpu->IY,
                    displacement);

            bus_write(
                bus,
                address,
                value);

            return 19;
        }

        case 0x39:
        {
            z80_add_iy(
                cpu,
                cpu->SP);

            return 15;
        }

        case 0x46:
        case 0x4E:
        case 0x56:
        case 0x5E:
        case 0x66:
        case 0x6E:
        case 0x7E:
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            u16 address =
                z80_indexed_address(
                    cpu->IY,
                    displacement);

            u8 value =
                bus_read(
                    bus,
                    address);

            u8 destination_code =
                (opcode >> 3) &
                0x07;

            z80_write_register_operand(
                cpu,
                bus,
                destination_code,
                value);

            return 19;
        }

        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x77:
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            u16 address =
                z80_indexed_address(
                    cpu->IY,
                    displacement);

            u8 source_code =
                opcode & 0x07;

            u8 value =
                z80_read_register_operand(
                    cpu,
                    bus,
                    source_code);

            bus_write(
                bus,
                address,
                value);

            return 19;
        }

        case 0xE1:
        {
            cpu->IY =
                z80_pop_word(
                    cpu,
                    bus);

            return 14;
        }

        case 0xE3:
        {
            u8 low_byte =
                bus_read(
                    bus,
                    cpu->SP);

            u8 high_byte =
                bus_read(
                    bus,
                    (u16) (
                        cpu->SP +
                        1));

            u16 stack_value =
                (u16) low_byte |
                ((u16) high_byte << 8);

            bus_write(
                bus,
                cpu->SP,
                (u8) (
                    cpu->IY &
                    0x00FF));

            bus_write(
                bus,
                (u16) (
                    cpu->SP +
                    1),
                (u8) (
                    cpu->IY >>
                    8));

            cpu->IY =
                stack_value;

            return 23;
        }

        case 0xE5:
        {
            z80_push_word(
                cpu,
                bus,
                cpu->IY);

            return 15;
        }

        case 0xE9:
        {
            cpu->PC =
                cpu->IY;

            return 8;
        }

        case 0xF9:
        {
            cpu->SP =
                cpu->IY;

            return 10;
        }

        default:
        {
            return
                Z80_STEP_UNKNOWN_OPCODE;
        }
    }
}

static void z80_add_hl(
    Z80* cpu,
    u16 operand)
{
    u16 before =
        z80_get_hl(cpu);

    u32 result =
        (u32) before +
        (u32) operand;

    u16 result16 =
        (u16) result;

    u8 flags =
        cpu->F &
        (Z80_FLAG_S |
         Z80_FLAG_Z |
         Z80_FLAG_PV);

    if(((before & 0x0FFF) +
        (operand & 0x0FFF)) > 0x0FFF)
    {
        flags |=
            Z80_FLAG_H;
    }

    if((result & 0x10000u) != 0)
    {
        flags |=
            Z80_FLAG_C;
    }

    flags |=
        (u8) (
            (result16 >> 8) &
            (Z80_FLAG_Y |
             Z80_FLAG_X));

    z80_set_hl(
        cpu,
        result16);

    cpu->F =
        flags;
}

static void z80_daa(
    Z80* cpu)
{
    u8 accumulator_before =
        cpu->A;

    bool subtract =
        (cpu->F & Z80_FLAG_N) != 0;

    bool half_carry_before =
        (cpu->F & Z80_FLAG_H) != 0;

    bool carry_before =
        (cpu->F & Z80_FLAG_C) != 0;

    u8 correction =
        0x00;

    bool carry_after =
        carry_before;

    if(!subtract)
    {
        if(half_carry_before ||
           (accumulator_before &
            0x0F) > 0x09)
        {
            correction |=
                0x06;
        }

        if(carry_before ||
           accumulator_before > 0x99)
        {
            correction |=
                0x60;

            carry_after =
                true;
        }

        cpu->A =
            (u8) (
                accumulator_before +
                correction);
    }
    else
    {
        if(half_carry_before)
        {
            correction |=
                0x06;
        }

        if(carry_before)
        {
            correction |=
                0x60;
        }

        cpu->A =
            (u8) (
                accumulator_before -
                correction);
    }

    /*
     * H após DAA é determinado pela
     * mudança do bit 4 entre o valor
     * anterior e o valor ajustado.
     */
    bool half_carry_after =
        ((accumulator_before ^
          cpu->A) &
         0x10) != 0;

    u8 flags =
        cpu->A &
        (Z80_FLAG_S |
         Z80_FLAG_Y |
         Z80_FLAG_X);

    if(cpu->A == 0)
    {
        flags |=
            Z80_FLAG_Z;
    }

    if(half_carry_after)
    {
        flags |=
            Z80_FLAG_H;
    }

    if(z80_even_parity(
           cpu->A))
    {
        flags |=
            Z80_FLAG_PV;
    }

    if(subtract)
    {
        flags |=
            Z80_FLAG_N;
    }

    if(carry_after)
    {
        flags |=
            Z80_FLAG_C;
    }

    cpu->F =
        flags;
}

int z80_step(Z80* cpu, Bus* bus)
{
    if(cpu == NULL || bus == NULL)
    {
        return Z80_STEP_INVALID_ARGUMENT;
    }

    if(cpu->halted)
    {
        cpu->total_cycles += 4;
        return 4;
    }

    u16 instruction_address = cpu->PC;
    u8 opcode = z80_fetch_byte(cpu, bus);
    cpu->last_instruction_address =
    instruction_address;

    cpu->last_opcode = opcode;

    int cycles = 0;

    switch(opcode)
    {
        case 0x00: /* NOP */
        {
            cycles = 4;
            break;
        }

        case 0x01: /* LD BC,nn */
        {
            z80_set_bc(cpu, z80_fetch_word(cpu, bus));
            cycles = 10;
            break;
        }

        case 0x02: /* LD (BC),A */
        {
            bus_write(bus, z80_get_bc(cpu), cpu->A);
            cycles = 7;
            break;
        }

        case 0x03: /* INC BC */
        {
            z80_set_bc(cpu, (u16) (z80_get_bc(cpu) + 1));
            cycles = 6;
            break;
        }

        case 0x04: /* INC B */
        {
            u8 before = cpu->B;
            cpu->B++;
            z80_set_inc_flags(cpu, before, cpu->B);
            cycles = 4;
            break;
        }

        case 0x05: /* DEC B */
        {
            u8 before = cpu->B;
            cpu->B--;
            z80_set_dec_flags(cpu, before, cpu->B);
            cycles = 4;
            break;
        }

        case 0x06: /* LD B,n */
        {
            cpu->B = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }

        case 0x0A: /* LD A,(BC) */
        {
            cpu->A = bus_read(bus, z80_get_bc(cpu));
            cycles = 7;
            break;
        }

        case 0x0B: /* DEC BC */
        {
            z80_set_bc(cpu, (u16) (z80_get_bc(cpu) - 1));
            cycles = 6;
            break;
        }

        case 0x0C: /* INC C */
        {
            u8 before = cpu->C;
            cpu->C++;
            z80_set_inc_flags(cpu, before, cpu->C);
            cycles = 4;
            break;
        }

        case 0x0D: /* DEC C */
        {
            u8 before = cpu->C;
            cpu->C--;
            z80_set_dec_flags(cpu, before, cpu->C);
            cycles = 4;
            break;
        }

        case 0x0E: /* LD C,n */
        {
            cpu->C = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }

        case 0x10: /* DJNZ e */
        {
            s8 displacement = (s8) z80_fetch_byte(cpu, bus);
            cpu->B--;

            if(cpu->B != 0)
            {
                cpu->PC = (u16) (cpu->PC + displacement);
                cycles = 13;
            }
            else
            {
                cycles = 8;
            }

            break;
        }

        case 0x11: /* LD DE,nn */
        {
            z80_set_de(cpu, z80_fetch_word(cpu, bus));
            cycles = 10;
            break;
        }

        case 0x12: /* LD (DE),A */
        {
            bus_write(bus, z80_get_de(cpu), cpu->A);
            cycles = 7;
            break;
        }

        case 0x13: /* INC DE */
        {
            z80_set_de(cpu, (u16) (z80_get_de(cpu) + 1));
            cycles = 6;
            break;
        }

        case 0x14: /* INC D */
        {
            u8 before = cpu->D;
            cpu->D++;
            z80_set_inc_flags(cpu, before, cpu->D);
            cycles = 4;
            break;
        }

        case 0x15: /* DEC D */
        {
            u8 before = cpu->D;
            cpu->D--;
            z80_set_dec_flags(cpu, before, cpu->D);
            cycles = 4;
            break;
        }

        case 0x16: /* LD D,n */
        {
            cpu->D = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }

        case 0x1A: /* LD A,(DE) */
        {
            cpu->A = bus_read(bus, z80_get_de(cpu));
            cycles = 7;
            break;
        }

        case 0x1B: /* DEC DE */
        {
            z80_set_de(cpu, (u16) (z80_get_de(cpu) - 1));
            cycles = 6;
            break;
        }

        case 0x1C: /* INC E */
        {
            u8 before = cpu->E;
            cpu->E++;
            z80_set_inc_flags(cpu, before, cpu->E);
            cycles = 4;
            break;
        }

        case 0x1D: /* DEC E */
        {
            u8 before = cpu->E;
            cpu->E--;
            z80_set_dec_flags(cpu, before, cpu->E);
            cycles = 4;
            break;
        }

        case 0x1E: /* LD E,n */
        {
            cpu->E = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }

        case 0x21: /* LD HL,nn */
        {
            z80_set_hl(cpu, z80_fetch_word(cpu, bus));
            cycles = 10;
            break;
        }

        case 0x22: /* LD (nn),HL */
        {
            u16 address = z80_fetch_word(cpu, bus);
            bus_write(bus, address, cpu->L);
            bus_write(bus, (u16) (address + 1), cpu->H);
            cycles = 16;
            break;
        }

        case 0x23: /* INC HL */
        {
            z80_set_hl(cpu, (u16) (z80_get_hl(cpu) + 1));
            cycles = 6;
            break;
        }

        case 0x24: /* INC H */
        {
            u8 before = cpu->H;
            cpu->H++;
            z80_set_inc_flags(cpu, before, cpu->H);
            cycles = 4;
            break;
        }

        case 0x25: /* DEC H */
        {
            u8 before = cpu->H;
            cpu->H--;
            z80_set_dec_flags(cpu, before, cpu->H);
            cycles = 4;
            break;
        }

        case 0x26: /* LD H,n */
        {
            cpu->H = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }

        case 0x2A: /* LD HL,(nn) */
        {
            u16 address = z80_fetch_word(cpu, bus);
            cpu->L = bus_read(bus, address);
            cpu->H = bus_read(bus, (u16) (address + 1));
            cycles = 16;
            break;
        }

        case 0x2B: /* DEC HL */
        {
            z80_set_hl(cpu, (u16) (z80_get_hl(cpu) - 1));
            cycles = 6;
            break;
        }

        case 0x2C: /* INC L */
        {
            u8 before = cpu->L;
            cpu->L++;
            z80_set_inc_flags(cpu, before, cpu->L);
            cycles = 4;
            break;
        }

        case 0x2D: /* DEC L */
        {
            u8 before = cpu->L;
            cpu->L--;
            z80_set_dec_flags(cpu, before, cpu->L);
            cycles = 4;
            break;
        }

        case 0x2E: /* LD L,n */
        {
            cpu->L = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }

        case 0x31: /* LD SP,nn */
        {
            cpu->SP = z80_fetch_word(cpu, bus);
            cycles = 10;
            break;
        }

        case 0x32: /* LD (nn),A */
        {
            u16 address = z80_fetch_word(cpu, bus);
            bus_write(bus, address, cpu->A);
            cycles = 13;
            break;
        }

        case 0x33: /* INC SP */
        {
            cpu->SP++;
            cycles = 6;
            break;
        }

        case 0x34: /* INC (HL) */
        {
            u16 address = z80_get_hl(cpu);
            u8 before = bus_read(bus, address);
            u8 result = (u8) (before + 1);

            bus_write(bus, address, result);
            z80_set_inc_flags(cpu, before, result);
            cycles = 11;
            break;
        }

        case 0x35: /* DEC (HL) */
        {
            u16 address = z80_get_hl(cpu);
            u8 before = bus_read(bus, address);
            u8 result = (u8) (before - 1);

            bus_write(bus, address, result);
            z80_set_dec_flags(cpu, before, result);
            cycles = 11;
            break;
        }

        case 0x36: /* LD (HL),n */
        {
            u8 value = z80_fetch_byte(cpu, bus);
            bus_write(bus, z80_get_hl(cpu), value);
            cycles = 10;
            break;
        }

        case 0x3A: /* LD A,(nn) */
        {
            u16 address = z80_fetch_word(cpu, bus);
            cpu->A = bus_read(bus, address);
            cycles = 13;
            break;
        }

        case 0x3B: /* DEC SP */
        {
            cpu->SP--;
            cycles = 6;
            break;
        }

        case 0x3C: /* INC A */
        {
            u8 before = cpu->A;
            cpu->A++;
            z80_set_inc_flags(cpu, before, cpu->A);
            cycles = 4;
            break;
        }

        case 0x3D: /* DEC A */
        {
            u8 before = cpu->A;
            cpu->A--;
            z80_set_dec_flags(cpu, before, cpu->A);
            cycles = 4;
            break;
        }

        case 0x3E: /* LD A,n */
        {
            cpu->A = z80_fetch_byte(cpu, bus);
            cycles = 7;
            break;
        }
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA4:
        case 0xA5:
        case 0xA6:
        case 0xA7:
        {
            u8 register_code = opcode & 0x07;
            cpu->A &= z80_read_register_operand(cpu, bus, register_code);
            z80_set_logic_flags(cpu, cpu->A, true);
            cycles = register_code == 6 ? 7 : 4;
            break;
        }

        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xAB:
        case 0xAC:
        case 0xAD:
        case 0xAE:
        case 0xAF:
        {
            u8 register_code = opcode & 0x07;
            cpu->A ^= z80_read_register_operand(cpu, bus, register_code);
            z80_set_logic_flags(cpu, cpu->A, false);
            cycles = register_code == 6 ? 7 : 4;
            break;
        }

        case 0xB0:
        case 0xB1:
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB5:
        case 0xB6:
        case 0xB7:
        {
            u8 register_code = opcode & 0x07;
            cpu->A |= z80_read_register_operand(cpu, bus, register_code);
            z80_set_logic_flags(cpu, cpu->A, false);
            cycles = register_code == 6 ? 7 : 4;
            break;
        }

        case 0xC3: /* JP nn */
        {
            cpu->PC = z80_fetch_word(cpu, bus);
            cycles = 10;
            break;
        }

        case 0xE6: /* AND n */
        {
            cpu->A &= z80_fetch_byte(cpu, bus);
            z80_set_logic_flags(cpu, cpu->A, true);
            cycles = 7;
            break;
        }

        case 0xED: /* Prefixo ED */
        {
            cycles = z80_execute_ed(cpu, bus);

            if(cycles < 0)
            {
                cpu->PC = instruction_address;
            }

            break;
        }

        case 0xEE: /* XOR n */
        {
            cpu->A ^= z80_fetch_byte(cpu, bus);
            z80_set_logic_flags(cpu, cpu->A, false);
            cycles = 7;
            break;
        }

        case 0xF3: /* DI */
        {
            cpu->IFF1 = false;
            cpu->IFF2 = false;

            /*
             * Cancela um EI pendente.
             */
            cpu->ei_delay = 0;

            cycles = 4;

            break;
        }

        case 0xF6: /* OR n */
        {
            cpu->A |= z80_fetch_byte(cpu, bus);
            z80_set_logic_flags(cpu, cpu->A, false);
            cycles = 7;
            break;
        }

        case 0xF9: /* LD SP,HL */
        {
            cpu->SP = z80_get_hl(cpu);
            cycles = 6;
            break;
        }
                case 0x18: /* JR e */
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            cpu->PC =
                (u16) (cpu->PC + displacement);

            cycles = 12;

            break;
        }

        case 0x20: /* JR NZ,e */
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_Z) == 0)
            {
                cpu->PC =
                    (u16) (cpu->PC + displacement);

                cycles = 12;
            }
            else
            {
                cycles = 7;
            }

            break;
        }

        case 0x28: /* JR Z,e */
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_Z) != 0)
            {
                cpu->PC =
                    (u16) (cpu->PC + displacement);

                cycles = 12;
            }
            else
            {
                cycles = 7;
            }

            break;
        }

        case 0x30: /* JR NC,e */
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_C) == 0)
            {
                cpu->PC =
                    (u16) (cpu->PC + displacement);

                cycles = 12;
            }
            else
            {
                cycles = 7;
            }

            break;
        }

        case 0x38: /* JR C,e */
        {
            s8 displacement =
                (s8) z80_fetch_byte(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_C) != 0)
            {
                cpu->PC =
                    (u16) (cpu->PC + displacement);

                cycles = 12;
            }
            else
            {
                cycles = 7;
            }

            break;
        }
                case 0xD3: /* OUT (n),A */
        {
            u8 immediate_port =
                z80_fetch_byte(
                    cpu,
                    bus);
            u16 port =
                ((u16) cpu->A << 8) |
                (u16) immediate_port;

            bus_io_write(
                bus,
                port,
                cpu->A);

            cycles = 11;

            break;
        }
        case 0xFB: /* EI */
        {
            cpu->ei_delay = 2;

            cycles = 4;

            break;
        }
        case 0x76: /* HALT */
        {
            cpu->halted = true;

            cycles = 4;

            break;
        }
        case 0x08: /* EX AF,AF' */
        {
            u8 temporary_a =
                cpu->A;

            u8 temporary_f =
                cpu->F;

            cpu->A =
                cpu->Ap;

            cpu->F =
                cpu->Fp;

            cpu->Ap =
                temporary_a;

            cpu->Fp =
                temporary_f;

            cycles = 4;

            break;
        }
                case 0xD9: /* EXX */
        {
            u8 temporary;

            temporary = cpu->B;
            cpu->B = cpu->Bp;
            cpu->Bp = temporary;

            temporary = cpu->C;
            cpu->C = cpu->Cp;
            cpu->Cp = temporary;

            temporary = cpu->D;
            cpu->D = cpu->Dp;
            cpu->Dp = temporary;

            temporary = cpu->E;
            cpu->E = cpu->Ep;
            cpu->Ep = temporary;

            temporary = cpu->H;
            cpu->H = cpu->Hp;
            cpu->Hp = temporary;

            temporary = cpu->L;
            cpu->L = cpu->Lp;
            cpu->Lp = temporary;

            cycles = 4;

            break;
        }
        case 0xEB: /* EX DE,HL */
        {
            u8 temporary_d =
                cpu->D;

            u8 temporary_e =
                cpu->E;

            cpu->D =
                cpu->H;

            cpu->E =
                cpu->L;

            cpu->H =
                temporary_d;

            cpu->L =
                temporary_e;

            cycles = 4;

            break;
        }
        case 0xE9: /* JP (HL) */
        {
            cpu->PC =
                z80_get_hl(
                    cpu);

            cycles = 4;

            break;
        }
                case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        {
            u8 register_code =
                opcode & 0x07;

            u8 operand =
                z80_read_register_operand(
                    cpu,
                    bus,
                    register_code);

            z80_add_a(
                cpu,
                operand,
                0);

            cycles =
                register_code == 6
                    ? 7
                    : 4;

            break;
        }
                case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        {
            u8 register_code =
                opcode & 0x07;

            u8 operand =
                z80_read_register_operand(
                    cpu,
                    bus,
                    register_code);

            u8 carry =
                (cpu->F & Z80_FLAG_C)
                    ? 1
                    : 0;

            z80_add_a(
                cpu,
                operand,
                carry);

            cycles =
                register_code == 6
                    ? 7
                    : 4;

            break;
        }
                case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x97:
        {
            u8 register_code =
                opcode & 0x07;

            u8 operand =
                z80_read_register_operand(
                    cpu,
                    bus,
                    register_code);

            z80_sub_a(
                cpu,
                operand,
                0);

            cycles =
                register_code == 6
                    ? 7
                    : 4;

            break;
        }
                case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        {
            u8 register_code =
                opcode & 0x07;

            u8 operand =
                z80_read_register_operand(
                    cpu,
                    bus,
                    register_code);

            u8 carry =
                (cpu->F & Z80_FLAG_C)
                    ? 1
                    : 0;

            z80_sub_a(
                cpu,
                operand,
                carry);

            cycles =
                register_code == 6
                    ? 7
                    : 4;

            break;
        }
                case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
        {
            u8 register_code =
                opcode & 0x07;

            u8 operand =
                z80_read_register_operand(
                    cpu,
                    bus,
                    register_code);

            z80_compare_a(
                cpu,
                operand);

            cycles =
                register_code == 6
                    ? 7
                    : 4;

            break;
        }
                case 0xC6:
        {
            u8 operand =
                z80_fetch_byte(
                    cpu,
                    bus);

            z80_add_a(
                cpu,
                operand,
                0);

            cycles = 7;

            break;
        }

        case 0xCE:
        {
            u8 operand =
                z80_fetch_byte(
                    cpu,
                    bus);

            u8 carry =
                (cpu->F & Z80_FLAG_C)
                    ? 1
                    : 0;

            z80_add_a(
                cpu,
                operand,
                carry);

            cycles = 7;

            break;
        }

        case 0xD6:
        {
            u8 operand =
                z80_fetch_byte(
                    cpu,
                    bus);

            z80_sub_a(
                cpu,
                operand,
                0);

            cycles = 7;

            break;
        }

        case 0xDE:
        {
            u8 operand =
                z80_fetch_byte(
                    cpu,
                    bus);

            u8 carry =
                (cpu->F & Z80_FLAG_C)
                    ? 1
                    : 0;

            z80_sub_a(
                cpu,
                operand,
                carry);

            cycles = 7;

            break;
        }

        case 0xFE:
        {
            u8 operand =
                z80_fetch_byte(
                    cpu,
                    bus);

            z80_compare_a(
                cpu,
                operand);

            cycles = 7;

            break;
        }
                case 0xC2:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_Z) == 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xCA:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_Z) != 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xD2:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_C) == 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xDA:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_C) != 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xE2:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_PV) == 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xEA:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_PV) != 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xF2:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_S) == 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }

        case 0xFA:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_S) != 0)
            {
                cpu->PC =
                    destination;
            }

            cycles = 10;

            break;
        }
                case 0xC1:
        {
            u16 value =
                z80_pop_word(
                    cpu,
                    bus);

            z80_set_bc(
                cpu,
                value);

            cycles = 10;

            break;
        }

        case 0xD1:
        {
            u16 value =
                z80_pop_word(
                    cpu,
                    bus);

            z80_set_de(
                cpu,
                value);

            cycles = 10;

            break;
        }

        case 0xE1:
        {
            u16 value =
                z80_pop_word(
                    cpu,
                    bus);

            z80_set_hl(
                cpu,
                value);

            cycles = 10;

            break;
        }

        case 0xF1:
        {
            u16 value =
                z80_pop_word(
                    cpu,
                    bus);

            cpu->A =
                (u8) (value >> 8);

            cpu->F =
                (u8) (value & 0x00FF);

            cycles = 10;

            break;
        }

        case 0xC5:
        {
            z80_push_word(
                cpu,
                bus,
                z80_get_bc(cpu));

            cycles = 11;

            break;
        }

        case 0xD5:
        {
            z80_push_word(
                cpu,
                bus,
                z80_get_de(cpu));

            cycles = 11;

            break;
        }

        case 0xE5:
        {
            z80_push_word(
                cpu,
                bus,
                z80_get_hl(cpu));

            cycles = 11;

            break;
        }

        case 0xF5:
        {
            u16 value =
                ((u16) cpu->A << 8) |
                (u16) cpu->F;

            z80_push_word(
                cpu,
                bus,
                value);

            cycles = 11;

            break;
        }
                case 0xCD:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            z80_push_word(
                cpu,
                bus,
                cpu->PC);

            cpu->PC =
                destination;

            cycles = 17;

            break;
        }

        case 0xC4:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_Z) == 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xCC:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_Z) != 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xD4:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_C) == 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xDC:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_C) != 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xE4:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_PV) == 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xEC:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_PV) != 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xF4:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_S) == 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }

        case 0xFC:
        {
            u16 destination =
                z80_fetch_word(
                    cpu,
                    bus);

            if((cpu->F & Z80_FLAG_S) != 0)
            {
                z80_push_word(
                    cpu,
                    bus,
                    cpu->PC);

                cpu->PC =
                    destination;

                cycles = 17;
            }
            else
            {
                cycles = 10;
            }

            break;
        }
                case 0xC9:
        {
            cpu->PC =
                z80_pop_word(
                    cpu,
                    bus);

            cycles = 10;

            break;
        }

        case 0xC0:
        {
            if((cpu->F & Z80_FLAG_Z) == 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xC8:
        {
            if((cpu->F & Z80_FLAG_Z) != 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xD0:
        {
            if((cpu->F & Z80_FLAG_C) == 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xD8:
        {
            if((cpu->F & Z80_FLAG_C) != 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xE0:
        {
            if((cpu->F & Z80_FLAG_PV) == 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xE8:
        {
            if((cpu->F & Z80_FLAG_PV) != 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xF0:
        {
            if((cpu->F & Z80_FLAG_S) == 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }

        case 0xF8:
        {
            if((cpu->F & Z80_FLAG_S) != 0)
            {
                cpu->PC =
                    z80_pop_word(
                        cpu,
                        bus);

                cycles = 11;
            }
            else
            {
                cycles = 5;
            }

            break;
        }
                case 0xC7:
        case 0xCF:
        case 0xD7:
        case 0xDF:
        case 0xE7:
        case 0xEF:
        case 0xF7:
        case 0xFF:
        {
            u16 destination =
                (u16) (opcode & 0x38);

            z80_push_word(
                cpu,
                bus,
                cpu->PC);

            cpu->PC =
                destination;

            cycles = 11;

            break;
        }
                case 0x40 ... 0x75:
        case 0x77 ... 0x7F:
        {
            u8 destination_code =
                (opcode >> 3) & 0x07;

            u8 source_code =
                opcode & 0x07;

            u8 value =
                z80_read_register_operand(
                    cpu,
                    bus,
                    source_code);

            z80_write_register_operand(
                cpu,
                bus,
                destination_code,
                value);

            if(source_code == 6 ||
               destination_code == 6)
            {
                cycles = 7;
            }
            else
            {
                cycles = 4;
            }

            break;
        }
                case 0xDD:
        {
            cycles =
                z80_execute_dd(
                    cpu,
                    bus);

            if(cycles < 0)
            {
                cpu->PC =
                    instruction_address;
            }

            break;
        }
        case 0xFD:
        {
            cycles =
                z80_execute_fd(
                    cpu,
                    bus);

            if(cycles < 0)
            {
                cpu->PC =
                    instruction_address;
            }

            break;
        }
        case 0xCB:
        {
            cycles =
                z80_execute_cb(
                    cpu,
                    bus);

            if(cycles < 0)
            {
                cpu->PC =
                    instruction_address;
            }

            break;
        }
        case 0x2F:
        {
            cpu->A =
                (u8) ~cpu->A;

            cpu->F =
                (cpu->F &
                (Z80_FLAG_S |
                Z80_FLAG_Z |
                Z80_FLAG_PV |
                Z80_FLAG_C)) |
                Z80_FLAG_H |
                Z80_FLAG_N |
                (cpu->A &
                (Z80_FLAG_Y |
                Z80_FLAG_X));

            cycles = 4;

            break;
        }
        case 0x07:
        {
            bool carry =
                (cpu->A & 0x80) != 0;

            cpu->A =
                (u8) (
                    (cpu->A << 1) |
                    (cpu->A >> 7));

            cpu->F =
                (cpu->F &
                (Z80_FLAG_S |
                Z80_FLAG_Z |
                Z80_FLAG_PV)) |
                (cpu->A &
                (Z80_FLAG_Y |
                Z80_FLAG_X)) |
                (carry
                    ? Z80_FLAG_C
                    : 0);

            cycles = 4;

            break;
        }

        case 0x0F:
        {
            bool carry =
                (cpu->A & 0x01) != 0;

            cpu->A =
                (u8) (
                    (cpu->A >> 1) |
                    (cpu->A << 7));

            cpu->F =
                (cpu->F &
                (Z80_FLAG_S |
                Z80_FLAG_Z |
                Z80_FLAG_PV)) |
                (cpu->A &
                (Z80_FLAG_Y |
                Z80_FLAG_X)) |
                (carry
                    ? Z80_FLAG_C
                    : 0);

            cycles = 4;

            break;
        }

        case 0x17:
        {
            u8 carry_in =
                (cpu->F & Z80_FLAG_C)
                    ? 1
                    : 0;

            bool carry_out =
                (cpu->A & 0x80) != 0;

            cpu->A =
                (u8) (
                    (cpu->A << 1) |
                    carry_in);

            cpu->F =
                (cpu->F &
                (Z80_FLAG_S |
                Z80_FLAG_Z |
                Z80_FLAG_PV)) |
                (cpu->A &
                (Z80_FLAG_Y |
                Z80_FLAG_X)) |
                (carry_out
                    ? Z80_FLAG_C
                    : 0);

            cycles = 4;

            break;
        }

        case 0x1F:
        {
            u8 carry_in =
                (cpu->F & Z80_FLAG_C)
                    ? 0x80
                    : 0x00;

            bool carry_out =
                (cpu->A & 0x01) != 0;

            cpu->A =
                (u8) (
                    (cpu->A >> 1) |
                    carry_in);

            cpu->F =
                (cpu->F &
                (Z80_FLAG_S |
                Z80_FLAG_Z |
                Z80_FLAG_PV)) |
                (cpu->A &
                (Z80_FLAG_Y |
                Z80_FLAG_X)) |
                (carry_out
                    ? Z80_FLAG_C
                    : 0);

            cycles = 4;

            break;
        }
        case 0x09:
        {
            z80_add_hl(
                cpu,
                z80_get_bc(cpu));

            cycles = 11;

            break;
        }

        case 0x19:
        {
            z80_add_hl(
                cpu,
                z80_get_de(cpu));

            cycles = 11;

            break;
        }

        case 0x29:
        {
            z80_add_hl(
                cpu,
                z80_get_hl(cpu));

            cycles = 11;

            break;
        }

        case 0x39:
        {
            z80_add_hl(
                cpu,
                cpu->SP);

            cycles = 11;

            break;
        }
        case 0x37:
{
    cpu->F =
        (cpu->F &
         (Z80_FLAG_S |
          Z80_FLAG_Z |
          Z80_FLAG_PV)) |
        (cpu->A &
         (Z80_FLAG_Y |
          Z80_FLAG_X)) |
        Z80_FLAG_C;

    cycles = 4;

    break;
}
case 0x3F: /* CCF */
{
    bool carry_before =
        (cpu->F &
         Z80_FLAG_C) != 0;

    u8 flags =
        cpu->F &
        (Z80_FLAG_S |
         Z80_FLAG_Z |
         Z80_FLAG_PV);

    /*
     * Os bits não documentados Y e X
     * recebem os bits 5 e 3 de A.
     */
    flags |=
        cpu->A &
        (Z80_FLAG_Y |
         Z80_FLAG_X);

    /*
     * H recebe o Carry anterior.
     */
    if(carry_before)
    {
        flags |=
            Z80_FLAG_H;
    }

    /*
     * C recebe o inverso do Carry
     * anterior.
     */
    if(!carry_before)
    {
        flags |=
            Z80_FLAG_C;
    }

    /*
     * N não é adicionado, portanto
     * permanece limpo.
     */
    cpu->F =
        flags;

    cycles =
        4;

    break;
}
case 0x27:
{
    z80_daa(
        cpu);

    cycles =
        4;

    break;
}

        default:
        {
            cpu->PC = instruction_address;
            return Z80_STEP_UNKNOWN_OPCODE;
        }
    }

    if(cycles < 0)
    {
        return cycles;
    }

    cpu->total_cycles +=
    (u32) cycles;

if(cpu->ei_delay > 0)
{
    cpu->ei_delay--;

    if(cpu->ei_delay == 0)
    {
        cpu->IFF1 = true;
        cpu->IFF2 = true;
    }
}



return cycles;
}
int z80_interrupt(
    Z80* cpu,
    Bus* bus,
    u8 vector)
{
    if(cpu == NULL ||
       bus == NULL)
    {
        return Z80_STEP_INVALID_ARGUMENT;
    }

    if(!cpu->IFF1 ||
       cpu->ei_delay != 0)
    {
        return 0;
    }

    if(cpu->IM != 2)
    {
        return Z80_STEP_UNKNOWN_OPCODE;
    }

    cpu->halted = false;
    cpu->IFF1 = false;
    cpu->IFF2 = false;

    z80_push_word(
        cpu,
        bus,
        cpu->PC);

    u16 table_address =
        ((u16) cpu->I << 8) |
        (u16) vector;

    u8 low_byte =
        bus_read(
            bus,
            table_address);

    u8 high_byte =
        bus_read(
            bus,
            (u16) (table_address + 1));

    cpu->PC =
        (u16) low_byte |
        ((u16) high_byte << 8);

    cpu->total_cycles += 19;

    return 19;
}