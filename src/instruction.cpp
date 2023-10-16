#include "instruction.hpp"

#include "program.hpp"

using enum Instruction::Type;

static constexpr std::array arithmetic_operations = {
    Add, None, None, None,
    None, Sub, None, Cmp,
};

static constexpr std::array jmp_instructions = {
    Jo, Jno, Jb, Jnb, Je, Jnz, Jbe, Ja,
    Js, Jns, Jp, Jnp, Jl, Jnl, Jle, Jg,
};

static constexpr std::array loop_instructions = {
    Loopnz, Loopz, Loop, Jcxz,
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

[[nodiscard]] static bool read_displacement(const Program& program, u32 start, u8 displacement_bytes, bool sign_extension, i32& result) {
    assert(displacement_bytes <= 2);
    if (!displacement_bytes) return false;
    if (start + displacement_bytes > program.size) return true;

    u8 displacement_lo = program.data[start];
    u8 displacement_hi = displacement_bytes == 2 ? program.data[start + 1] : 0;

    if (displacement_bytes == 2) {
        result = displacement_lo | (displacement_hi << 8);
    } else {
        result = sign_extension ? (i8)displacement_lo : displacement_lo;
    }

    return false;
}

[[nodiscard]] static bool read_data(const Program& program, u32 start, bool wide, u16& result) {
    if (start + wide >= program.size) return true;

    u8 data_lo = program.data[start];
    u8 data_hi = wide ? program.data[start + 1] : 0;
    result = data_lo | (data_hi << 8);

    return false;
}

static void decode_rm_with_register(const Program& program, u32 start, Instruction& i) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    u8 b = program.data[start + 1];

    u8 op = (a & 0b00111000) >> 3;
    bool d = a & 0b10;
    bool w = a & 1;
    u8 mod = (b & 0b11000000) >> 6;
    u8 reg = (b & 0b00111000) >> 3;
    u8 rm = b & 0b111;

    bool is_direct_address = mod == 0 && rm == 0b110;
    auto looked_reg = lookup_register(w, reg);
    auto eac = lookup_effective_address_calculation(rm);

    u8 displacement_bytes = 0;
    if (mod == 1) {
        displacement_bytes = 1;
    } else if (mod == 2 || is_direct_address) {
        displacement_bytes = 2;
    }

    i32 displacement = 0;
    if (read_displacement(program, start + 2, displacement_bytes, true, displacement)) return;

    i.size = 2 + displacement_bytes;
    if (w) {
        i.flags |= Instruction::Flag::Wide;
    }
    if (i.type != Mov) {
        i.type = lookup<arithmetic_operations>(op);
    }

    switch (mod) {
        case 0: {
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
        }
        case 1:
        case 2: {
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
        }
        case 3: {
            i.operands[0] = lookup_register(w, rm);
            i.operands[1] = looked_reg;
            if (d) {
                swap(i.operands[0], i.operands[1]);
            }
            break;
        }
    }
}

static void decode_immediate_to_rm(const Program& program, u32 start, Instruction& i) {
    if (start + 1 >= program.size) return;

    u8 a = program.data[start];
    u8 b = program.data[start + 1];

    bool s = a & 0b10;
    bool w = a & 1;
    u8 mod = (b & 0b11000000) >> 6;
    u8 op = (b & 0b00111000) >> 3;
    u8 rm = b & 0b111;

    bool is_direct_address = mod == 0 && rm == 0b110;
    bool wide_data = i.type == Mov ? w : !s && w;
    auto eac = lookup_effective_address_calculation(rm);

    u8 displacement_bytes = mod == 3 ? 0 : mod;
    if (is_direct_address) {
        displacement_bytes = 2;
    }

    i32 displacement = 0;
    if (read_displacement(program, start + 2, displacement_bytes, s, displacement)) return;

    u16 data;
    if (read_data(program, start + 2 + displacement_bytes, wide_data, data)) return;

    i.size = 2 + displacement_bytes + (wide_data ? 2 : 1);
    if (w) {
        i.flags |= Instruction::Wide;
    }
    if (i.type != Mov) {
        i.type = lookup<arithmetic_operations>(op);
    }

    if (mod == 3) {
        i.operands[0] = lookup_register(w, rm);
    } else if (is_direct_address) {
        i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, displacement};
    } else {
        i.operands[0] = MemoryOperand{eac, displacement};
    }

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
    i.flags |= Instruction::IpInc;
    i.operands[0] = adjusted_ip_inc;
}

Instruction decode_instruction_at(const Program& program, u32 start) {
    assert(program.size && program.data);
    assert(start < program.size);

    Instruction i = {};
    i.address = start;

    u8 a = program.data[start];
    if ((a & 0b11111100) == 0b10001000) {
        // MOV: Registry/memory to/from registry
        i.type = Mov;
        decode_rm_with_register(program, start, i);
    } else if ((a & 0b11111110) == 0b11000110) {
        // MOV: Immediate to register/memory
        i.type = Mov;
        decode_immediate_to_rm(program, start, i);
    } else if ((a & 0b11110000) == 0b10110000) {
        // MOV: Immediate to register
        bool w = a & 0b1000;
        u8 reg = a & 0b111;

        u16 data;
        if (read_data(program, start + 1, w, data)) return i;

        i.type = Mov;
        i.size = w ? 3 : 2;
        if (w) {
            i.flags |= Instruction::Flag::Wide;
        }

        i.operands[0] = lookup_register(w, reg);
        i.operands[1] = data;
    } else if ((a & 0b11111110) == 0b10100000) {
        // MOV: Memory to accumulator
        bool w = a & 1;
        i32 address = 0;
        if (read_displacement(program, start + 1, w + 1, true, address)) return i;

        i.type = Mov;
        i.size = w ? 3 : 2;
        if (w) {
            i.flags |= Instruction::Flag::Wide;
        }

        i.operands[0] = w ? Register::ax : Register::al;
        i.operands[1] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, address};
    } else if ((a & 0b11111110) == 0b10100010) {
        // MOV: Accumulator to memory
        bool w = a & 1;
        i32 address;
        if (read_displacement(program, start + 1, w + 1, true, address)) return i;

        i.type = Mov;
        i.size = w ? 3 : 2;
        if (w) {
            i.flags |= Instruction::Flag::Wide;
        }

        i.operands[0] = MemoryOperand{EffectiveAddressCalculation::DirectAccess, address};
        i.operands[1] = w ? Register::ax : Register::al;
    } else if ((a & 0b11000100) == 0) {
        // Arithmetic
        decode_rm_with_register(program, start, i);
    } else if ((a & 0b11111100) == 0b10000000) {
        // Arithmetic
        decode_immediate_to_rm(program, start, i);
    } else if ((a & 0b11000110) == 0b00000100) {
        // Arithmetic: Immediate to accumulator
        u8 op = (a & 0b00111000) >> 3;
        bool w = a & 1;

        u16 data;
        if (read_data(program, start + 1, w, data)) return i;

        i.type = lookup<arithmetic_operations>(op);
        i.size = w ? 3 : 2;
        if (w) {
            i.flags |= Instruction::Flag::Wide;
        }

        i.operands[0] = w ? Register::ax : Register::al;
        i.operands[1] = data;
    } else if ((a & 0b11110000) == 0x70) {
        decode_ip_inc(program, start, i, 0b1111, lookup<jmp_instructions>);
    } else if ((a & 0b11111100) == 0b11100000) {
        decode_ip_inc(program, start, i, 0b11, lookup<loop_instructions>);
    } else if (a == 0xff) {
        // PUSH: Register/memory
        if (start + 1 >= program.size) return i;

        u8 b = program.data[start + 1];
        if ((b & 0b00111000) >> 3 != 0b110) return i;

        u8 mod = (b & 0b11000000) >> 6;
        u8 rm = b & 0b111;

        bool is_direct_address = mod == 0 && rm == 0b110;
        auto eac = lookup_effective_address_calculation(rm);

        u8 displacement_bytes = 0;
        if (mod == 1) {
            displacement_bytes = 1;
        } else if (mod == 2 || is_direct_address) {
            displacement_bytes = 2;
        }

        i32 displacement = 0;
        if (read_displacement(program, start + 2, displacement_bytes, true, displacement)) return i;

        i.size = 2 + displacement_bytes;
        i.type = Push;
        i.flags |= Instruction::Wide;

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
                i.operands[0] = lookup_register(true, rm);
                break;
        }
    } else if ((a & 0b11111000) == 0b01010000) {
        // PUSH: Register
        u8 reg = a & 0b111;

        i.size = 1;
        i.type = Push;
        i.flags |= Instruction::Wide;
        i.operands[0] = lookup_register(true, reg);
    } else if ((a & 0b11100111) == 0b110) {
        // PUSH: Segment register
        u8 segment_reg = (a & 0b11000) >> 3;

        i.size = 1;
        i.type = Push;
        i.flags |= Instruction::Wide;
        i.operands[0] = lookup_segment_register(segment_reg);
    }

    if (i.size == 0) i.type = None;

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
            if (operand_index == 0 && i.operands[1].type == None) {
                fprintf(out, "%s ", i.flags & Instruction::Wide ? "word" : "byte");
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
            if (i.flags & Instruction::IpInc) {
                fprintf(out, "$%c%d", o.immediate < 0 ? '-' : '+', abs(o.immediate));
                break;
            }
            if (operand_index == 1 && oo.type != Register) {
                fprintf(out, "%s ", i.flags & Instruction::Wide ? "word" : "byte");
            }
            fprintf(out, "%d", o.immediate);
            break;
    }
}

void output_instruction_assembly(FILE* out, const Instruction& i) {
    if (i.type == None) return;

    fprintf(out, "%s", lookup_instruction_type(i));

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
