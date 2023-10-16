#pragma once

#include "common.hpp"
#include <cstdio>

enum class Register : u32 {
    ax, cx, dx, bx,
    sp, bp, si, di,
    al, cl, dl, bl,
    ah, ch, dh, bh,
    es, cs, ss, ds,
};
inline constexpr std::array register_names = {
    "ax", "cx", "dx", "bx",
    "sp", "bp", "si", "di",
    "al", "cl", "dl", "bl",
    "ah", "ch", "dh", "bh",
    "es", "cs", "ss", "ds",
};
static_assert(register_names.size() == static_cast<size_t>(Register::ds) + 1);
static inline const char* lookup_register(Register reg) {
    auto i = static_cast<u32>(reg);
    assert(i < std::size(register_names));
    return register_names[i];
}

enum class EffectiveAddressCalculation : u32 {
    bx_si, bx_di, bp_si, bp_di,
    si, di, bp, bx,
    DirectAccess,
};
inline constexpr std::array effective_address_calculation_names = {
    "bx + si", "bx + di", "bp + si", "bp + di",
    "si", "di", "bp", "bx",
    "DIRECT_ACCESS",
};
static_assert(effective_address_calculation_names.size() == static_cast<size_t>(EffectiveAddressCalculation::DirectAccess) + 1);

static inline const char* lookup_effective_address_calculation(EffectiveAddressCalculation eac) {
    auto i = static_cast<u32>(eac);
    assert(i < std::size(effective_address_calculation_names));
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

        Mov, Push, Pop,

        Add, Sub, Cmp,

        Jo, Jno, Jb, Jnb, Je, Jnz, Jbe, Ja,
        Js, Jns, Jp, Jnp, Jl, Jnl, Jle, Jg,

        Loopnz, Loopz, Loop, Jcxz,
    };
    static constexpr auto instruction_count = static_cast<size_t>(Type::Jcxz) + 1;
    static constexpr std::array instruction_type_names = {
        "UNKNOWN_INSTRUCTION",

        "mov", "push", "pop",

        "add", "sub", "cmp",

        "jo", "jno", "jb", "jnb", "je", "jnz", "jbe", "ja",
        "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jg",

        "loopnz", "loopz", "loop", "jcxz",
    };
    static_assert(instruction_type_names.size() == instruction_count);

    enum Flag : u32 {
        Wide = 0x1,
        IpInc = 0x2,
    };

    u32 address = 0;
    u32 size = 0;

    Type type = Type::None;
    u32 flags = 0;

    std::array<Operand, 2> operands = {};
};

static inline const char* lookup_instruction_type(Instruction::Type type) {
    auto i = static_cast<u32>(type);
    assert(i < std::size(Instruction::instruction_type_names));
    return Instruction::instruction_type_names[i];
}
static inline const char* lookup_instruction_type(const Instruction& i) {
    return lookup_instruction_type(i.type);
}

struct Program;

Instruction decode_instruction_at(const Program& program, u32 start);
void output_instruction_assembly(FILE* out, const Instruction& instruction);
