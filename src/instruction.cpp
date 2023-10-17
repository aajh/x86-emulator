#include "instruction.hpp"

#include "program.hpp"

#define COMMON_MOD_RM_DEFINITIONS\
    u8 mod = (b & 0b1100'0000) >> 6;\
    u8 rm = b & 0b111;\
    bool is_direct_address = mod == 0 && rm == 0b110;\
    auto eac = lookup_effective_address_calculation(rm);\
    u8 displacement_bytes = mod == 3 ? 0 : mod;\
    if (is_direct_address) {\
        displacement_bytes = 2;\
    }\
    i16 displacement = 0;\
    if (read_displacement(program, start + 2, displacement_bytes, s, displacement)) return;

using enum Instruction::Type;

static constexpr std::array operations_1 = {
    Add, Or, Adc, Sbb,
    And, Sub, Xor, Cmp,
};

static constexpr std::array operations_2 = {
    Test, None, Not, Neg,
    Mul, Imul, Div, Idiv,
};

static constexpr std::array operations_3 = {
    None, None, Call, None,
    Jmp, None, Push, None,
};

static constexpr std::array shift_operations = {
    Rol, Ror, Rcl, Rcr,
    Shl, Shr, None, Sar,
};

static constexpr std::array string_instructions = {
    None, None, Movs, Cmps,
    None, Stos, Lods, Scas,
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
    Cld, Std, None, None,
};

typedef Instruction::Type (*lookup_function)(u8);

template<const auto& array>
static constexpr Instruction::Type lookup(u8 i) {
    if (i >= std::size(array)) return None;
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

[[nodiscard]] static bool read_displacement(const Program& program, u32 start, u8 displacement_bytes, bool sign_extension_8_bit, i16& result) {
    assert(displacement_bytes <= 2);
    if (!displacement_bytes) return false;
    if (start + displacement_bytes > program.size) return true;

    u8 displacement_lo = program.data[start];
    u8 displacement_hi = displacement_bytes == 2 ? program.data[start + 1] : 0;

    if (displacement_bytes == 2) {
        result = (i16)(displacement_lo | (displacement_hi << 8));
    } else {
        result = sign_extension_8_bit ? (i8)displacement_lo : displacement_lo;
    }

    return false;
}

template<typename T>
[[nodiscard]] static bool read_data(const Program& program, u32 start, bool wide, T& result) {
    if (start + wide >= program.size) return true;

    u8 data_lo = program.data[start];
    u8 data_hi = wide ? program.data[start + 1] : 0;
    result = (T)(data_lo | (data_hi << 8));

    return false;
}

static void decode_rm_register(const Program& program, u32 start, Instruction& i, Instruction::Type type) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    u8 b = program.data[start + 1];

    bool no_d_or_w = (type == Lea || type == Lds || type == Les);
    u8 op = (a & 0b0011'1000) >> 3;
    bool d = a & 0b10 || no_d_or_w;
    bool w = a & 1 || no_d_or_w;
    u8 reg = (b & 0b0011'1000) >> 3;
    bool s = true;

    auto looked_reg = lookup_register(w, reg);
    COMMON_MOD_RM_DEFINITIONS;

    i.size = 2 + displacement_bytes;
    i.type = type != None ? type : lookup<operations_1>(op);
    i.flags.wide = w;

    switch (mod) {
        case 0:
            if (is_direct_address) {
                i.operands[0] = looked_reg;
                i.operands[1] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
            } else {
                i.operands[0] = MemoryOperand{eac};
                i.operands[1] = looked_reg;
                if (d) {
                    swap(i.operands[0], i.operands[1]);
                }
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
            if (d) {
                swap(i.operands[0], i.operands[1]);
            }
            break;
        case 3:
            i.operands[0] = lookup_register(w, rm);
            i.operands[1] = looked_reg;
            if (d) {
                swap(i.operands[0], i.operands[1]);
            }
            break;
        default:
            assert(false);
    }
}

static void decode_immediate_to_rm(const Program& program, u32 start, Instruction& i, bool is_mov) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    u8 b = program.data[start + 1];

    u8 op = (b & 0b0011'1000) >> 3;
    auto type = is_mov ? Mov : lookup<operations_1>(op);

    bool s = a & 0b10 || type == And || type == Or || type == Xor;
    bool w = a & 1;

    bool wide_data = (is_mov || type == And || type == Or || type == Xor) ? w : !s && w;
    COMMON_MOD_RM_DEFINITIONS;

    u16 data;
    if (read_data(program, start + 2 + displacement_bytes, wide_data, data)) return;

    i.size = 2 + displacement_bytes + (wide_data ? 2 : 1);
    i.type = type;
    i.flags.wide = w;

    if (mod == 3) {
        i.operands[0] = lookup_register(w, rm);
    } else if (is_direct_address) {
        i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[0] = MemoryOperand{eac, displacement};
    }

    i.operands[1] = data;
}

static void decode_mov_immediate_to_register(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    bool w = a & 0b1000;
    u8 reg = a & 0b111;

    u16 data;
    if (read_data(program, start + 1, w, data)) return;

    i.size = w ? 3 : 2;
    i.type = Mov;
    i.flags.wide = w;

    i.operands[0] = lookup_register(w, reg);
    i.operands[1] = data;
}

static void decode_mov_memory_accumulator(const Program& program, u32 start, Instruction& i, bool to_accumulator) {
    u8 a = program.data[start];
    bool w = a & 1;
    i16 address = 0;
    if (read_displacement(program, start + 1, w + 1, true, address)) return;

    i.size = w ? 3 : 2;
    i.type = Mov;
    i.flags.wide = w;

    i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, address};
    i.operands[1] = w ? Register::ax : Register::al;

    if (to_accumulator) swap(i.operands[0], i.operands[1]);
}

static void decode_immediate_to_accumulator(const Program& program, u32 start, Instruction& i, Instruction::Type type) {
    u8 a = program.data[start];
    u8 op = (a & 0b0011'1000) >> 3;
    bool w = a & 1;

    u16 data;
    if (read_data(program, start + 1, w, data)) return;

    i.size = w ? 3 : 2;
    i.type = type != None ? type : lookup<operations_1>(op);
    i.flags.wide = w;

    i.operands[0] = w ? Register::ax : Register::al;
    i.operands[1] = data;
}

static void decode_ip_inc(const Program& program, u32 start, Instruction& i, uint8_t bitmask, lookup_function look) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    i8 ip_inc = program.data[start + 1];

    u8 lookup_i = a & bitmask;
    // The coded jump length and the jump length in assembly differ by the size of this instruction, i.e. two bytes
    i16 adjusted_ip_inc = ip_inc + 2;

    i.size = 2;
    i.type = look(lookup_i);
    i.flags.ip_inc = true;
    i.operands[0] = adjusted_ip_inc;
}

static void decode_rm(const Program& program, u32 start, Instruction& i) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    u8 b = program.data[start + 1];

    bool is_shift = (a & 0b1111'1100) == 0b1101'0000;
    u8 op = (b & 0b0011'1000) >> 3;

    auto type = None;
    if (a == 0b1000'1111 && op == 0) type = Pop;
    else if ((a & ~1) == (u8)~1 && op == 0) type = Inc;
    else if ((a & ~1) == (u8)~1 && op == 1) type = Dec;
    else if (a == 0xff) type = lookup<operations_3>(op);
    else if ((a & ~1) == 0b1111'0110) type = lookup<operations_2>(op);
    else if (is_shift) type = lookup<shift_operations>(op);

    if (type == None) {
        fprintf(stderr, "decode_rm: unimplemented instruction 0x%X 0x%X\n", a, b);
        return;
    }

    bool v = is_shift && a & 0b10;
    bool w = a & 1 || (type == Push || type == Pop);
    bool s = true;
    bool has_data = type == Test;
    COMMON_MOD_RM_DEFINITIONS;

    u16 data = 0;
    if (has_data && read_data(program, start + 2 + displacement_bytes, w, data)) return;

    i.size = 2 + displacement_bytes + (has_data ? w + 1 : 0);
    i.type = type;
    i.flags.wide = w;

    switch (mod) {
        case 0:
        case 1:
        case 2:
            if (is_direct_address) {
                i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
            } else {
                i.operands[0] = MemoryOperand{eac, displacement};
            }
            break;
        case 3:
            i.operands[0] = lookup_register(w, rm);
            break;
        default:
            assert(false);
    }

    if (is_shift) {
        if (v) i.operands[1] = Register::cl;
        else i.operands[1] = 1;
    }
    if (has_data) {
        i.operands[1] = data;
    }
}

static void decode_push_pop_register(const Program& program, u32 start, Instruction& i, bool is_pop) {
    u8 a = program.data[start];
    u8 reg = a & 0b111;

    i.size = 1;
    i.type = is_pop ? Pop : Push;
    i.flags.wide = true;
    i.operands[0] = lookup_register(true, reg);
}

static void decode_push_pop_segment_register(const Program& program, u32 start, Instruction& i, bool is_pop) {
    u8 a = program.data[start];
    u8 segment_reg = (a & 0b1'1000) >> 3;

    i.size = 1;
    i.type = is_pop ? Pop : Push;
    i.flags.wide = true;
    i.operands[0] = lookup_segment_register(segment_reg);
}

static void decode_xchg_register_accumulator(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    u8 reg = a & 0b111;

    i.size = 1;
    i.type = Xchg;
    i.flags.wide = true;
    i.operands[0] = Register::ax;
    i.operands[1] = lookup_register(true, reg);
}

static void decode_in_out(const Program& program, u32 start, Instruction& i, Instruction::Type type) {
    assert(type == In || type == Out);

    u8 a = program.data[start];
    bool fixed_port = !(a & 0b1000);
    bool w = a & 1;

    u16 data;
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

    if (type == Out) swap(i.operands[0], i.operands[1]);
}

static void decode_inc_dec_register(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    u8 reg = a & 0b111;

    i.size = 1;
    i.type = a & 0b1000 ? Dec : Inc;
    i.flags.wide = true;
    i.operands[0] = lookup_register(true, reg);
}

static void decode_aam_aad(const Program& program, u32 start, Instruction& i) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    u8 b = program.data[start + 1];
    if (b != 0b0000'1010) return;

    i.size = 2;
    i.type = a & 1 ? Aad : Aam;
}

static void decode_string_instruction(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    if ((a & ~0b1111) != 0b1010'0000) return;

    u8 op = (a & 0b1110) >> 1;
    bool w = a & 1;

    i.size = 1 + i.flags.rep;
    i.type = lookup<string_instructions>(op);
    i.flags.wide = w;
}

static void decode_rep(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    i.flags.rep = true;
    i.flags.rep_nz = ~a & 1;

    if (start + 1 < program.size) decode_string_instruction(program, start + 1, i);
}

static void decode_ret(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    bool has_data = !(a & 1);
    bool intersegment = a & 0b1000;

    i16 data = 0;
    if (has_data && read_data(program, start + 1, true, data)) return;

    i.size = has_data ? 3 : 1;
    i.type = Ret;
    i.flags.intersegment = intersegment;
    if (has_data) i.operands[0] = data;
}

static void decode_int(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    bool has_data = a & 1;

    u16 data = 0;
    if (has_data && read_data(program, start + 1, false, data)) return;

    i.size = has_data ? 2 : 1;
    i.type = has_data ? Int : Int3;
    if (has_data) i.operands[0] = data;
}

static void decode_esc(const Program& program, u32 start, Instruction& i) {
    u8 a = program.data[start];
    u8 b = program.data[start + 1];
    u16 esc_opcode = (a & 0b111) | (b & 0b111'000);
    bool s = true;

    COMMON_MOD_RM_DEFINITIONS;

    i.size = 2 + displacement_bytes;
    i.type = Esc;
    i.flags.wide = true;

    i.operands[0] = esc_opcode;

    if (mod == 3) {
        i.operands[1] = lookup_register(true, rm);
    } else if (is_direct_address) {
        i.operands[1] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[1] = MemoryOperand{eac, displacement};
    }
}

Instruction decode_instruction_at(const Program& program, u32 start) {
    assert(program.size && program.data);
    assert(start < program.size);

    Instruction i = {};
    i.address = start;
    i.size = 1;

read_after_prefix:
    u8 a = program.data[start];
    if ((a & 0b1111'1100) == 0b1000'1000) {
        decode_rm_register(program, start, i, Mov);
    } else if ((a & 0b1111'1110) == 0b1100'0110) {
        decode_immediate_to_rm(program, start, i, true); // MOV
    } else if ((a & 0b111'10000) == 0b1011'0000) {
        decode_mov_immediate_to_register(program, start, i);
    } else if ((a & 0b1111'1110) == 0b1010'0000) {
        decode_mov_memory_accumulator(program, start, i, true);
    } else if ((a & 0b1111'1110) == 0b1010'0010) {
        decode_mov_memory_accumulator(program, start, i, false);
    } else if ((a & 0b1100'0100) == 0) {
        decode_rm_register(program, start, i, None); // Lookup from operations_1
    } else if ((a & 0b1111'1100) == 0b1000'0000) {
        decode_immediate_to_rm(program, start, i, false); // Lookup from operations_1
    } else if ((a & 0b1100'0110) == 0b0000'0100) {
        decode_immediate_to_accumulator(program, start, i, None); // Lookup from operations_1
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
        decode_rep(program, start, i);
    } else if ((a & ~0b1111) == 0b1010'0000) {
        decode_string_instruction(program, start, i);
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
        if (start + 1 < program.size) {
            ++start;
            goto read_after_prefix;
        }
    } else if ((a & ~0b111) == 0b1101'1000) {
        decode_esc(program, start, i);
    } else if ((a & 0b1110'0110) == 0b0010'0110) {
        i.segment_override = lookup_segment_register((a >> 3) & 0b11);
        if (start + 1 < program.size) {
            ++start;
            goto read_after_prefix;
        }
    } else {
        fprintf(stderr, "Unimplemented opcode 0x%X\n", a);
    }

    if (i.address != start) i.size += start - i.address;

    return i;
}


static void output_operand(FILE* out, const Instruction& i, bool operand_index) {
    auto& o = i.operands[operand_index];
    auto& oo = i.operands[!operand_index];

    switch (o.type) {
        using enum Operand::Type;
        case None:
            break;
        case Register:
            fprintf(out, "%s", lookup_register(o.reg));
            break;
        case Memory:
            if (operand_index == 0 && (oo.type == None || oo.type == Immediate || is_shift(i))) {
                fprintf(out, "%s ", i.flags.wide ? "word" : "byte");
            }
            if (i.segment_override) {
                fprintf(out, "%s:", lookup_register(*i.segment_override));
            }
            if (o.memory.eac == EffectiveAddressCalculation::DirectAccess) {
                fprintf(out, "[%d]", o.memory.displacement);
                break;
            }
            fprintf(out, "[%s", lookup_effective_address_calculation(o.memory.eac));
            if (o.memory.displacement) {
                fprintf(out, " %c %d", o.memory.displacement < 0 ? '-' : '+', abs(o.memory.displacement));
            }
            fprintf(out, "]");
            break;
        case Immediate:
            if (i.flags.ip_inc) {
                fprintf(out, "$%c%d", o.immediate < 0 ? '-' : '+', abs(o.immediate));
                break;
            }
            fprintf(out, "%d", o.immediate);
            break;
    }
}

void output_instruction_assembly(FILE* out, const Instruction& i) {
    if (i.type == None) return;

    if (i.flags.rep) {
        if (i.flags.rep_nz) fprintf(out, "repnz ");
        else fprintf(out, "rep ");
    }
    if (i.flags.lock) {
        fprintf(out, "lock ");
    }

    fprintf(out, "%s", lookup_instruction_type(i));

    if (is_string_manipulation(i)) fprintf(out, "%c", i.flags.wide ? 'w' : 'b');

    if (i.operands[0].type != Operand::Type::None) {
        fprintf(out, " ");
        output_operand(out, i, 0);

        if (i.operands[1].type != Operand::Type::None) {
            fprintf(out, ", ");
            output_operand(out, i, 1);
        }
    }

    fprintf(out, "\n");
}
