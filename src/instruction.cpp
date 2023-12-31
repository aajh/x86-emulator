#include "instruction.hpp"
#include <limits>
#include <fmt/core.h>

using enum Instruction::Type;

#define COMMON_MOD_RM_DEFINITIONS\
    u8 mod = (b & 0b1100'0000) >> 6;\
    u8 rm = b & 0b111;\
    bool is_direct_access = mod == 0 && rm == 0b110;\
    auto eac = lookup_effective_address_calculation(rm);\
    u8 displacement_bytes = mod == 3 ? 0 : mod;\
    if (is_direct_access) {\
        displacement_bytes = 2;\
    }\
    i16 displacement = 0;\
    if (read_data(program, start + 2, displacement_bytes, s, displacement)) return;

static constexpr std::array operations_1 = {
    Add, Or, Adc, Sbb,
    And, Sub, Xor, Cmp,
};

static constexpr std::array operations_2 = {
    Test, Invalid, Not, Neg,
    Mul, Imul, Div, Idiv,
};

static constexpr std::array operations_3 = {
    Invalid, Invalid, Call, Call,
    Jmp, Jmp, Push, Invalid,
};

static constexpr std::array shift_operations = {
    Rol, Ror, Rcl, Rcr,
    Shl, Shr, Invalid, Sar,
};

static constexpr std::array string_instructions = {
    Invalid, Invalid, Movs, Cmps,
    Invalid, Stos, Lods, Scas,
};

static constexpr std::array jmp_instructions = {
    Jo, Jno, Jb, Jnb, Je, Jnz, Jbe, Ja,
    Js, Jns, Jp, Jnp, Jl, Jnl, Jle, Jg,
};

static constexpr std::array loop_instructions = {
    Loopnz, Loopz, Loop, Jcxz,
};

static constexpr std::array processor_control_instructions = {
    Clc, Stc, Cli, Sti,
    Cld, Std, Invalid, Invalid,
};

typedef Instruction::Type (*lookup_function)(u8);

template<const auto& array>
static constexpr Instruction::Type lookup(u8 i) {
    static_assert(std::size(array) <= std::numeric_limits<decltype(i)>::max());
    if (i >= std::size(array)) return Invalid;
    return array[i];
}

static constexpr Register lookup_register(bool w, u8 reg) {
    assert(reg < 8);
    return static_cast<Register>(w ? reg : reg + 8);
}

static constexpr Register lookup_segment_register(u8 reg) {
    assert(reg < 4);
    return static_cast<Register>(reg + 16);
}

static constexpr EffectiveAddressCalculation lookup_effective_address_calculation(u8 rm) {
    assert(rm < 8);
    return static_cast<EffectiveAddressCalculation>(rm);
}

template<typename T>
[[nodiscard]] static bool read_data(std::span<const u8> program, u32 start, u32 displacement_bytes, bool sign_extension_8_bit, T& result) {
    if (displacement_bytes > 2) return true;
    if (start + displacement_bytes > program.size()) return true;

    u8 displacement_lo = displacement_bytes >= 1 ? program[start] : 0;
    u8 displacement_hi = displacement_bytes == 2 ? program[start + 1] : 0;

    if (displacement_bytes == 2) {
        result = (T)(displacement_lo | (displacement_hi << 8));
    } else {
        result = (T)(sign_extension_8_bit ? (i8)displacement_lo : displacement_lo);
    }

    return false;
}

template<typename T>
[[nodiscard]] static bool read_data(std::span<const u8> program, u32 start, bool wide_data, T& result) {
    return read_data(program, start, wide_data + 1, false, result);
}

static void decode_rm_register(std::span<const u8> program, u32 start, Instruction& i, Instruction::Type type) {
    if (start + 1 >= program.size()) return;

    u8 a = program[start];
    u8 b = program[start + 1];

    bool no_d_or_w = (type == Lea || type == Lds || type == Les);
    u8 op = (a & 0b0011'1000) >> 3;
    bool d = a & 0b10 || no_d_or_w;
    bool w = a & 1 || no_d_or_w;
    u8 reg = (b & 0b0011'1000) >> 3;
    bool s = true;

    auto looked_reg = lookup_register(w, reg);
    COMMON_MOD_RM_DEFINITIONS;

    i.size = 2 + displacement_bytes;
    i.type = type != Invalid ? type : lookup<operations_1>(op);
    i.flags.wide = w;

    switch (mod) {
        case 0:
            if (is_direct_access) {
                i.operands[0] = looked_reg;
                i.operands[1] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
                if (type == Xchg) i.swap_operands();
            } else {
                i.operands[0] = MemoryOperand{eac};
                i.operands[1] = looked_reg;
            }
            break;
        case 1:
        case 2:
            if (displacement) {
                i.operands[0] = MemoryOperand{eac, displacement};
            } else {
                i.operands[0] = MemoryOperand{eac};
            }
            i.operands[1] = looked_reg;
            break;
        case 3:
            i.operands[0] = lookup_register(w, rm);
            i.operands[1] = looked_reg;
            break;
        default:
            assert(false);
            break;
    }

    if (!is_direct_access && d) i.swap_operands();
}

static void decode_immediate_to_rm(std::span<const u8> program, u32 start, Instruction& i, bool is_mov) {
    if (start + 1 >= program.size()) return;

    u8 a = program[start];
    u8 b = program[start + 1];

    u8 op = (b & 0b0011'1000) >> 3;
    auto type = is_mov ? Mov : lookup<operations_1>(op);
    bool is_logic = type == And || type == Or || type == Xor;

    bool s = a & 0b10 || is_logic;
    bool w = a & 1;

    bool wide_data = (is_mov || type == And || type == Or || type == Xor) ? w : !s && w;
    bool sign_extend_data = is_logic ? false : s;
    COMMON_MOD_RM_DEFINITIONS;

    u16 data = 0;
    if (read_data(program, start + 2 + displacement_bytes, wide_data ? 2 : 1, sign_extend_data, data)) return;

    i.size = 2 + displacement_bytes + (wide_data ? 2 : 1);
    i.type = type;
    i.flags.wide = w;

    if (mod == 3) {
        i.operands[0] = lookup_register(w, rm);
    } else if (is_direct_access) {
        i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[0] = MemoryOperand{eac, displacement};
    }

    i.operands[1] = data;
}

static void decode_mov_immediate_to_register(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    bool w = a & 0b1000;
    u8 reg = a & 0b111;

    u16 data = 0;
    if (read_data(program, start + 1, w, data)) return;

    i.size = w ? 3 : 2;
    i.type = Mov;
    i.flags.wide = w;

    i.operands[0] = lookup_register(w, reg);
    i.operands[1] = data;
}

static void decode_mov_memory_accumulator(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    bool to_accumulator = ~a & 0b10;
    bool w = a & 1;
    i16 address = 0;
    if (read_data(program, start + 1, w + 1, true, address)) return;

    i.size = w ? 3 : 2;
    i.type = Mov;
    i.flags.wide = w;

    i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, address};
    i.operands[1] = w ? Register::ax : Register::al;

    if (to_accumulator) i.swap_operands();
}

static void decode_mov_rm_segment_register(std::span<const u8> program, u32 start, Instruction& i) {
    if (start + 1 >= program.size()) return;

    u8 a = program[start];
    u8 b = program[start + 1];

    bool to_segment_register = a & 0b10;
    u8 segment_reg = (b & 0b1'1000) >> 3;
    bool s = true;

    COMMON_MOD_RM_DEFINITIONS;

    i.size = 2 + displacement_bytes;
    i.type = Mov;
    i.flags.wide = true;

    if (mod == 3) {
        i.operands[0] = lookup_register(true, rm);
    } else if (is_direct_access) {
        i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[0] = MemoryOperand{eac, displacement};
    }

    i.operands[1] = lookup_segment_register(segment_reg);

    if (to_segment_register) i.swap_operands();
}

static void decode_immediate_to_accumulator(std::span<const u8> program, u32 start, Instruction& i, Instruction::Type type) {
    u8 a = program[start];
    u8 op = (a & 0b0011'1000) >> 3;
    bool w = a & 1;

    u16 data = 0;
    if (read_data(program, start + 1, w, data)) return;

    i.size = w ? 3 : 2;
    i.type = type != Invalid ? type : lookup<operations_1>(op);
    i.flags.wide = w;

    i.operands[0] = w ? Register::ax : Register::al;
    i.operands[1] = data;
}

static void decode_ip_inc(std::span<const u8> program, u32 start, Instruction& i, uint8_t bitmask, lookup_function look) {
    if (start + 1 >= program.size()) return;

    u8 a = program[start];
    i8 ip_inc = (i8)program[start + 1];
    u8 lookup_i = a & bitmask;

    i.size = 2;
    i.type = look(lookup_i);
    i.flags.ip_inc = true;
    i.operands[0].set_ip_inc(ip_inc);
}

static void decode_rm(std::span<const u8> program, u32 start, Instruction& i) {
    if (start + 1 >= program.size()) return;

    u8 a = program[start];
    u8 b = program[start + 1];

    bool is_shift = (a & 0b1111'1100) == 0b1101'0000;
    u8 op = (b & 0b0011'1000) >> 3;

    auto type = Invalid;
    if (a == 0b1000'1111 && op == 0) type = Pop;
    else if ((a & ~1) == (u8)~1 && op == 0) type = Inc;
    else if ((a & ~1) == (u8)~1 && op == 1) type = Dec;
    else if (a == 0xff) type = lookup<operations_3>(op);
    else if ((a & ~1) == 0b1111'0110) type = lookup<operations_2>(op);
    else if (is_shift) type = lookup<shift_operations>(op);

    if (type == Invalid) {
#ifndef NDEBUG
        fmt::print(stderr, "decode_rm: unknown instruction {:#x} {:#x}\n", a, b);
#endif
        return;
    }

    bool v = is_shift && a & 0b10;
    bool w = a & 1 || (type == Push || type == Pop);
    bool s = true;
    bool has_data = type == Test;
    bool is_intersegment = (type == Call || type == Jmp) && (op & 1);
    COMMON_MOD_RM_DEFINITIONS;

    u16 data = 0;
    if (has_data && read_data(program, start + 2 + displacement_bytes, w, data)) return;

    i.size = 2 + displacement_bytes + (has_data ? w + 1 : 0);
    i.type = type;
    i.flags.wide = w;
    i.flags.intersegment = is_intersegment;

    if (mod == 3) {
        i.operands[0] = lookup_register(w, rm);
    } else if (is_direct_access) {
        i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[0] = MemoryOperand{eac, displacement};
    }

    if (is_shift) {
        if (v) i.operands[1] = Register::cl;
        else i.operands[1] = 1;
    }
    if (has_data) {
        i.operands[1] = data;
    }
}

static void decode_push_pop_register(std::span<const u8> program, u32 start, Instruction& i, bool is_pop) {
    u8 a = program[start];
    u8 reg = a & 0b111;

    i.size = 1;
    i.type = is_pop ? Pop : Push;
    i.flags.wide = true;
    i.operands[0] = lookup_register(true, reg);
}

static void decode_push_pop_segment_register(std::span<const u8> program, u32 start, Instruction& i, bool is_pop) {
    u8 a = program[start];
    u8 segment_reg = (a & 0b1'1000) >> 3;

    i.size = 1;
    i.type = is_pop ? Pop : Push;
    i.flags.wide = true;
    i.operands[0] = lookup_segment_register(segment_reg);
}

static void decode_xchg_register_accumulator(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    u8 reg = a & 0b111;

    i.size = 1;
    i.type = Xchg;
    i.flags.wide = true;
    i.operands[0] = Register::ax;
    i.operands[1] = lookup_register(true, reg);
}

static void decode_in_out(std::span<const u8> program, u32 start, Instruction& i, Instruction::Type type) {
    assert(type == In || type == Out);

    u8 a = program[start];
    bool fixed_port = !(a & 0b1000);
    bool w = a & 1;

    u16 data = 0;
    if (fixed_port && read_data(program, start + 1, false, data)) return;

    i.size = 1 + fixed_port;
    i.type = type;
    i.flags.wide = w;

    i.operands[0] = w ? Register::ax : Register::al;
    if (fixed_port) {
        i.operands[1] = data;
    } else {
        i.operands[1] = Register::dx;
    }

    if (type == Out) i.swap_operands();
}

static void decode_inc_dec_register(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    u8 reg = a & 0b111;

    i.size = 1;
    i.type = a & 0b1000 ? Dec : Inc;
    i.flags.wide = true;
    i.operands[0] = lookup_register(true, reg);
}

static void decode_aam_aad(std::span<const u8> program, u32 start, Instruction& i) {
    if (start + 1 >= program.size()) return;

    u8 a = program[start];
    u8 b = program[start + 1];
    if (b != 0b0000'1010) return;

    i.size = 2;
    i.type = a & 1 ? Aad : Aam;
}

static void decode_string_instruction(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    if ((a & ~0b1111) != 0b1010'0000) return;

    u8 op = (a & 0b1110) >> 1;
    bool w = a & 1;

    i.type = lookup<string_instructions>(op);
    i.flags.wide = w;
}

static void decode_direct_intersegment_call_jmp(std::span<const u8> program, u32 start, Instruction& i, Instruction::Type type) {
    u16 ip = 0;
    if (read_data(program, start + 1, true, ip)) return;
    u16 cs = 0;
    if (read_data(program, start + 3, true, cs)) return;

    i.size = 5;
    i.type = type;
    i.flags.intersegment = true;

    i.operands[0] = cs;
    i.operands[1] = ip;
}

static void decode_direct_call_jmp(std::span<const u8> program, u32 start, Instruction& i, Instruction::Type type) {
    u8 a = program[start];
    bool short_ip_inc = a & 0b10;
    u32 size = short_ip_inc ? 2 : 3;

    i16 ip_inc = 0;
    if (read_data(program, start + 1, short_ip_inc ? 1 : 2, true, ip_inc)) return;

    i.size = size;
    i.type = type;
    i.flags.short_jmp = short_ip_inc;
    i.operands[0].set_ip_inc(ip_inc);
}

static void decode_ret(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    bool has_data = !(a & 1);
    bool intersegment = a & 0b1000;

    i16 data = 0;
    if (has_data && read_data(program, start + 1, true, data)) return;

    i.size = has_data ? 3 : 1;
    i.type = Ret;
    i.flags.intersegment = intersegment;
    if (has_data) i.operands[0] = data;
}

static void decode_int(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    bool has_data = a & 1;

    u16 data = 0;
    if (has_data && read_data(program, start + 1, false, data)) return;

    i.size = has_data ? 2 : 1;
    i.type = has_data ? Int : Int3;
    if (has_data) i.operands[0] = data;
}

static void decode_esc(std::span<const u8> program, u32 start, Instruction& i) {
    u8 a = program[start];
    u8 b = program[start + 1];
    u16 esc_opcode = (a & 0b111) | (b & 0b111'000);
    bool s = true;

    COMMON_MOD_RM_DEFINITIONS;

    i.size = 2 + displacement_bytes;
    i.type = Esc;
    i.flags.wide = true;

    i.operands[0] = esc_opcode;

    if (mod == 3) {
        i.operands[1] = lookup_register(true, rm);
    } else if (is_direct_access) {
        i.operands[1] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[1] = MemoryOperand{eac, displacement};
    }
}

std::optional<Instruction> Instruction::decode_at(std::span<const u8> program, u32 start) {
    if (start >= program.size()) return {};

    Instruction i = {};
    i.address = start;
    i.size = 1;

read_after_prefix:
    u8 a = program[start];
    if ((a & 0b1111'1100) == 0b1000'1000) {
        decode_rm_register(program, start, i, Mov);
    } else if ((a & 0b1111'1110) == 0b1100'0110) {
        decode_immediate_to_rm(program, start, i, true); // MOV
    } else if ((a & 0b111'10000) == 0b1011'0000) {
        decode_mov_immediate_to_register(program, start, i);
    } else if ((a & 0b1111'1100) == 0b1010'0000) {
        decode_mov_memory_accumulator(program, start, i);
    } else if ((a & ~0b10) == 0b1000'1100) {
        decode_mov_rm_segment_register(program, start, i);
    } else if ((a & 0b1100'0100) == 0) {
        decode_rm_register(program, start, i, Invalid); // Lookup from operations_1
    } else if ((a & 0b1111'1100) == 0b1000'0000) {
        decode_immediate_to_rm(program, start, i, false); // Lookup from operations_1
    } else if ((a & 0b1100'0110) == 0b0000'0100) {
        decode_immediate_to_accumulator(program, start, i, Invalid); // Lookup from operations_1
    } else if ((a & 0b1111'0000) == 0b0111'0000) {
        decode_ip_inc(program, start, i, 0b1111, lookup<jmp_instructions>);
    } else if ((a & 0b1111'1100) == 0b1110'0000) {
        decode_ip_inc(program, start, i, 0b11, lookup<loop_instructions>);
    } else if ((a & ~1) == (u8)~1) {
        decode_rm(program, start, i); // PUSH or INC or DEC
    } else if ((a & 0b1111'1000) == 0b0101'0000) {
        decode_push_pop_register(program, start, i, false); // PUSH
    } else if ((a & 0b111'00111) == 0b110) {
        decode_push_pop_segment_register(program, start, i, false); // PUSH
    } else if (a == 0b1000'1111) {
        decode_rm(program, start, i); // POP
    } else if ((a & 0b1111'1000) == 0b0101'1000) {
        decode_push_pop_register(program, start, i, true); // POP
    } else if ((a & 0b1110'0111) == 0b111) {
        decode_push_pop_segment_register(program, start, i, true); // POP
    } else if ((a & ~1) == 0b1000'0110) {
        decode_rm_register(program, start, i, Xchg);
    } else if ((a & 0b1111'1000) == 0b1001'0000) {
        decode_xchg_register_accumulator(program, start, i);
    } else if ((a & 0b1111'0110) == 0b1110'0100) {
        decode_in_out(program, start, i, In);
    } else if ((a & 0b1111'0110) == 0b1110'0110) {
        decode_in_out(program, start, i, Out);
    } else if (a == 0b1101'0111) {
        i.type = Xlat;
    } else if (a == 0b1000'1101) {
        decode_rm_register(program, start, i, Lea);
    } else if (a == 0b1100'0101) {
        decode_rm_register(program, start, i, Lds);
    } else if (a == 0b1100'0100) {
        decode_rm_register(program, start, i, Les);
    } else if (a == 0b1001'1111) {
        i.type = Lahf;
    } else if (a == 0b1001'1110) {
        i.type = Sahf;
    } else if (a == 0b1001'1100) {
        i.type = Pushf;
    } else if (a == 0b1001'1101) {
        i.type = Popf;
    } else if ((a & 0b1111'0000) == 0b0100'0000) {
        decode_inc_dec_register(program, start, i);
    } else if (a == 0b0011'0111) {
        i.type = Aaa;
    } else if (a == 0b0010'0111) {
        i.type = Daa;
    } else if ((a & ~1) == 0b1111'0110) {
        decode_rm(program, start, i); // Lookup from operations_2
    } else if (a == 0b0011'1111) {
        i.type = Aas;
    } else if (a == 0b0010'1111) {
        i.type = Das;
    } else if ((a & ~1) == 0b1101'0100) {
        decode_aam_aad(program, start, i);
    } else if (a == 0b1001'1000) {
        i.type = Cbw;
    } else if (a == 0b1001'1001) {
        i.type = Cwd;
    } else if ((a & 0b1111'1100) == 0b1101'0000) {
        decode_rm(program, start, i); // Shift operator
    } else if ((a & ~0b11) == 0b1000'0100) {
        decode_rm_register(program, start, i, Test);
    } else if ((a & ~1) == 0b1010'1000) {
        decode_immediate_to_accumulator(program, start, i, Test);
    } else if ((a & ~1) == 0b1111'0010) {
        i.flags.rep = true;
        i.flags.rep_nz = ~a & 1;
        if (start + 1 < program.size()) {
            ++start;
            goto read_after_prefix;
        }
    } else if ((a & ~0b1111) == 0b1010'0000) {
        decode_string_instruction(program, start, i);
    } else if (a == 0b1001'1010) {
        decode_direct_intersegment_call_jmp(program, start, i, Call);
    } else if (a == 0b1110'1000) {
        decode_direct_call_jmp(program, start, i, Call);
    } else if (a == 0b1110'1010) {
        decode_direct_intersegment_call_jmp(program, start, i, Jmp);
    } else if ((a & 0b1111'1001) == 0b1110'1001) {
        decode_direct_call_jmp(program, start, i, Jmp);
    } else if ((a & ~0b1001) == 0b1100'0010) {
        decode_ret(program, start, i);
    } else if ((a & ~1) == 0b1100'1100) {
        decode_int(program, start, i);
    } else if (a == 0b1100'1110) {
        i.type = Into;
    } else if (a == 0b1100'1111) {
        i.type = Iret;
    } else if ((a & 0b1111'1000) == 0b1111'1000) {
        i.type = lookup<processor_control_instructions>(a & 0b111);
    } else if (a == 0b1111'0101) {
        i.type = Cmc;
    } else if (a == 0b1111'0100) {
        i.type = Hlt;
    } else if (a == 0b1001'1011) {
        i.type = Wait;
    } else if (a == 0b1111'0000) {
        i.flags.lock = true;
        if (start + 1 < program.size()) {
            ++start;
            goto read_after_prefix;
        }
    } else if ((a & ~0b111) == 0b1101'1000) {
        decode_esc(program, start, i);
    } else if ((a & 0b1110'0110) == 0b0010'0110) {
        i.segment_override = lookup_segment_register((a >> 3) & 0b11);
        if (start + 1 < program.size()) {
            ++start;
            goto read_after_prefix;
        }
    } else {
#ifndef NDEBUG
        fmt::print(stderr, "decode_instruction_at: unknown instruction {:#x}\n", a);
#endif
    }

    if (i.address != start) i.size += start - i.address;

    if (i.type == Invalid) return {};
    return i;
}


u32 Instruction::estimate_cycles(u32 total, FILE* out) const {
    u32 cycles = 0;
    u32 transfers = 0;
    const MemoryOperand* memory_operand = nullptr;
    bool do_eac = true;

    const auto& o1 = operands[0];
    const auto& o2 = operands[1];

    switch (type) {
        using enum Register;
        using enum Operand::Type;
        case Add:
            if (o1.type == Register && o2.type == Register) {
                cycles = 3;
            } else if (o1.type == Register && o2.type == Memory) {
                cycles = 9;
                transfers = 1;
                memory_operand = &o2.memory;
            } else if (o1.type == Memory && o2.type == Register) {
                cycles = 16;
                transfers = 2;
                memory_operand = &o1.memory;
            } else if (o1.type == Register && o2.type == Immediate) {
                cycles = 4;
            } else if (o1.type == Memory && o2.type == Immediate) {
                cycles = 17;
                transfers = 2;
                memory_operand = &o1.memory;
            }
            break;
        case Mov:
            if (o1.type == Register && o2.type == Register) {
                cycles = 2;
            } else if (o1.type == Register && o2.type == Memory) {
                if ((o1.reg == ax || o1.reg == al) && o2.memory.eac == EffectiveAddressCalculation::DirectAccess && size == 2u + flags.wide) {
                    cycles = 10;
                    do_eac = false;
                } else {
                    cycles = 8;
                }
                transfers = 1;
                memory_operand = &o2.memory;
            } else if (o1.type == Memory && o2.type == Register) {
                if ((o2.reg == ax || o2.reg == al) && o1.memory.eac == EffectiveAddressCalculation::DirectAccess && size == 2u + flags.wide) {
                    cycles = 10;
                    do_eac = false;
                } else {
                    cycles = 9;
                }
                transfers = 1;
                memory_operand = &o1.memory;
            } else if (o1.type == Register && o2.type == Immediate) {
                cycles = 4;
            } else if (o1.type == Memory && o2.type == Immediate) {
                cycles = 10;
                transfers = 1;
                memory_operand = &o1.memory;
            }
            break;
        default:
            break;
    }
    if (cycles == 0 && !memory_operand) {
#ifndef NDEBUG
        fmt::print(stderr, "cycle_estimate: unimplemented instruction {}\n", name());
#endif
        return 0;
    }

    u32 ea = 0;
    u32 transfer_penalty = 0;
    if (memory_operand) {
        auto d = memory_operand->displacement;
        if (do_eac) {
            switch (memory_operand->eac) {
                using enum EffectiveAddressCalculation;
                case DirectAccess:
                    ea = 6;
                    break;
                case si:
                case di:
                case bp:
                case bx:
                    ea = d ? 9 : 5;
                    break;
                case bp_di:
                case bx_si:
                    ea = d ? 11 : 7;
                    break;
                case bp_si:
                case bx_di:
                    ea = d ? 12 : 8;
                    break;
            }
        }
        if (transfers && (d % 2 != 0)) {
            transfer_penalty = 4 * transfers;
        }
    }
    cycles += ea + transfer_penalty;

    if (out) {
        fmt::print(out, "Clocks: +{} = {}", cycles, cycles + total);
        if (ea || transfer_penalty) {
            fmt::print(out, " ({}", cycles - ea - transfer_penalty);
            if (ea) fmt::print(out, " + {}ea", ea);
            if (transfer_penalty) fmt::print(out, " + {}p", transfer_penalty);
            fmt::print(out, ")");
        }
    }

    return cycles;
}


static fmt::format_context::iterator format_operand_to(fmt::format_context::iterator out, const Instruction& i, bool operand_index) {
    auto& o = i.operands[operand_index];
    auto& oo = i.operands[!operand_index];

    switch (o.type) {
            using enum Operand::Type;
        case None:
            break;
        case Register:
            fmt::format_to(out, "{}", lookup_register(o.reg));
            break;
        case Memory:
            if (operand_index == 0 && (oo.type == None || oo.type == Immediate || i.is_shift()) && (i.type != Call && i.type != Jmp)) {
                fmt::format_to(out, "{} ", i.flags.wide ? "word" : "byte");
            }
            if (i.segment_override) {
                fmt::format_to(out, "{}:", lookup_register(*i.segment_override));
            }
            if (o.memory.eac == EffectiveAddressCalculation::DirectAccess) {
                fmt::format_to(out, "[{}]", o.memory.displacement);
                break;
            }
            fmt::format_to(out, "[{}", lookup_effective_address_calculation(o.memory.eac));
            if (o.memory.displacement) {
                fmt::format_to(out, " {} {}", o.memory.displacement < 0 ? '-' : '+', abs(o.memory.displacement));
            }
            fmt::format_to(out, "]");
            break;
        case Immediate:
            fmt::format_to(out, "{}", o.immediate);
            break;
        case IpInc:
            i64 ip_inc = (i64)o.ip_inc + (i64)i.size;
            if (i.flags.ip_inc) fmt::format_to(out, "${:+}", ip_inc);
            else fmt::format_to(out, "{}", ip_inc + (i64)i.address);
            break;
    }

    return out;
}

fmt::format_context::iterator Instruction::format_to(fmt::format_context::iterator out) const {
    if (type == Invalid) return out;

    if (flags.rep) fmt::format_to(out, "{}", flags.rep_nz ? "repnz " : "rep ");
    if (flags.lock) fmt::format_to(out, "lock ");

    fmt::format_to(out, "{}", name());

    if (flags.intersegment) {
        if (type == Ret) {
            fmt::format_to(out, "f");
        } else if ((type == Call || type == Jmp) && operands[1].type == Operand::Type::None) {
            fmt::format_to(out, " far");
        }
    }
    if (flags.short_jmp) fmt::format_to(out, " short");
    if (is_string_manipulation()) fmt::format_to(out, "{}", flags.wide ? 'w' : 'b');

    if (operands[0].type != Operand::Type::None) {
        fmt::format_to(out, " ");
        out = format_operand_to(out, *this, 0);

        if (operands[1].type != Operand::Type::None) {
            fmt::format_to(out, "{}", !flags.intersegment ? ", " : ":");
            out = format_operand_to(out, *this, 1);
        }
    }

    return out;
}
