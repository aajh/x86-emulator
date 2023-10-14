#pragma once

#include "common.hpp"
#include <cstdio>

enum class Register : u32 {
    ax, cx, dx, bx,
    sp, bp, si, di,
    al, cl, dl, bl,
    ah, ch, dh, bh,
};
const char* const register_names[16] = {
    "ax", "cx", "dx", "bx",
    "sp", "bp", "si", "di",
    "al", "cl", "dl", "bl",
    "ah", "ch", "dh", "bh",
};
static_assert(static_cast<size_t>(Register::bh) + 1 == ARRAY_SIZE(register_names));
static inline Register lookup_register(bool w, u8 reg) {
    assert(reg < 8);
    return static_cast<Register>(w ? reg : reg + 8);
}
static inline const char* lookup_register(Register reg) {
    auto i = static_cast<u32>(reg);
    assert(i < ARRAY_SIZE(register_names));
    return register_names[i];
}

enum class EffectiveAddressCalculation : u32 {
    bx_si, bx_di, bp_si, bp_di,
    si, di, bp, bx,
    DirectAccess,
};
const char* const effective_address_calculation_names[9] = {
    "bx + si", "bx + di", "bp + si", "bp + di",
    "si", "di", "bp", "bx",
    "",
};
static_assert(static_cast<size_t>(EffectiveAddressCalculation::DirectAccess) + 1 == ARRAY_SIZE(effective_address_calculation_names));
static inline EffectiveAddressCalculation lookup_effective_address_calculation(u8 rm) {
    assert(rm < 8);
    return static_cast<EffectiveAddressCalculation>(rm);
}
static inline const char* lookup_effective_address_calculation(EffectiveAddressCalculation eac) {
    auto i = static_cast<u32>(eac);
    assert(i < ARRAY_SIZE(effective_address_calculation_names));
    return effective_address_calculation_names[i];
}

struct MemoryOperand {
    EffectiveAddressCalculation eac = EffectiveAddressCalculation::DirectAccess;
    i32 displacement = 0;
};

struct Operand {
    enum class Type : u32 {
        None,
        Register,
        Memory,
        Immediate,
    } type = Type::None;

    union {
        Register reg;
        MemoryOperand memory;
        i32 immediate;
    };

    Operand() : type(Type::None) {}
    Operand(Register reg) : type(Type::Register), reg(reg) {}
    Operand(MemoryOperand memory) : type(Type::Memory), memory(memory) {}
    Operand(i32 immediate) : type(Type::Immediate), immediate(immediate) {}

    friend void swap(Operand& lhs, Operand& rhs) {
        using enum Type;
        Operand tmp = lhs;
        lhs.type = rhs.type;
        switch (rhs.type) {
            case None:
                break;
            case Register:
                lhs.reg = rhs.reg;
                break;
            case Memory:
                lhs.memory = rhs.memory;
                break;
            case Immediate:
                lhs.immediate = rhs.immediate;
                break;
        }
        rhs.type = tmp.type;
        switch (tmp.type) {
            case None:
                break;
            case Register:
                rhs.reg = tmp.reg;
                break;
            case Memory:
                rhs.memory = tmp.memory;
                break;
            case Immediate:
                rhs.immediate = tmp.immediate;
                break;
        }
    }
};

struct Instruction {
    enum class Type : u32 {
        None,
        Mov,

        Add,
        Sub,
        Cmp,

        Jo,
        Jno,
        Jb,
        Jnb,
        Je,
        Jnz,
        Jbe,
        Ja,
        Js,
        Jns,
        Jp,
        Jnp,
        Jl,
        Jnl,
        Jle,
        Jg,

        Loopnz,
        Loopz,
        Loop,
        Jcxz,
    };
    static constexpr auto instruction_count = static_cast<size_t>(Type::Jcxz) + 1;

    enum Flag : u32 {
        Wide = 0x1,
        IpInc = 0x2,
    };

    u32 address = 0;
    u32 size = 0;

    Type type = Type::None;
    u32 flags = 0;

    Operand operands[2] = {{}, {}};
};

const char* const instruction_type_names[Instruction::instruction_count] = {
    "", "mov", "add", "sub", "cmp",

    "jo", "jno", "jb", "jnb", "je", "jnz", "jbe", "ja",
    "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jg",

    "loopnz", "loopz", "loop", "jcxz",
};
static inline const char* lookup_instruction_type(Instruction::Type type) {
    auto i = static_cast<u32>(type);
    assert(i < Instruction::instruction_count);
    return instruction_type_names[i];
}
static inline const char* lookup_instruction_type(const Instruction& i) {
    return lookup_instruction_type(i.type);
}

Instruction decode_instruction_at(const Program& program, u64 start);
void output_instruction_assembly(FILE* out, const Instruction& instruction);