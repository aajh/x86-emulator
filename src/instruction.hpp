#pragma once

#include "common.hpp"
#include <cstdio>
#include <cstring>
#include <fmt/core.h>

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
static constexpr const char* lookup_register(Register reg) {
    auto i = static_cast<u32>(reg);
    assert(i < std::size(register_names));
    return register_names[i];
}
static constexpr std::optional<Register> lookup_register(const char* reg) {
    for (std::underlying_type_t<Register> i = 0; i < register_names.size(); ++i) {
        if (strcmp(register_names[i], reg) == 0) return static_cast<Register>(i);
    }
    return {};
}
static constexpr bool is_8bit_register(Register reg) {
    using T = std::underlying_type_t<Register>;
    auto r = static_cast<T>(reg);
    return static_cast<T>(Register::al) <= r && r <= static_cast<T>(Register::bh);
}
static constexpr bool is_8bit_low_register(Register reg) {
    using T = std::underlying_type_t<Register>;
    auto r = static_cast<T>(reg);
    return static_cast<T>(Register::al) <= r && r <= static_cast<T>(Register::bl);
}
static constexpr bool is_8bit_high_register(Register reg) {
    using T = std::underlying_type_t<Register>;
    auto r = static_cast<T>(reg);
    return static_cast<T>(Register::ah) <= r && r <= static_cast<T>(Register::bh);
}
static constexpr bool is_segment_register(Register reg) {
    using T = std::underlying_type_t<Register>;
    auto r = static_cast<T>(reg);
    return static_cast<T>(Register::es) <= r && r <= static_cast<T>(Register::ds);
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

static constexpr const char* lookup_effective_address_calculation(EffectiveAddressCalculation eac) {
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
        IpInc,
    } type = Type::None;

    union {
        Register reg;
        MemoryOperand memory;
        u16 immediate;
        i16 ip_inc;
    };

    Operand() {} // NOLINT(cppcoreguidelines-pro-type-member-init)
    Operand(Register reg) : type(Type::Register), reg(reg) {}
    Operand(MemoryOperand memory) : type(Type::Memory), memory(memory) {}
    Operand(u16 immediate) : type(Type::Immediate), immediate(immediate) {}

    void set_ip_inc(i16 ii) {
        type = Type::IpInc;
        ip_inc = ii;
    }

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
            case IpInc:
                lhs.ip_inc = rhs.ip_inc;
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
            case IpInc:
                rhs.ip_inc = tmp.ip_inc;
                break;
        }
    }
};

struct Instruction {
    enum class Type : u32 {
        Invalid,

        Mov, Push, Pop, Xchg, In, Out,
        Xlat, Lea, Lds, Les,
        Lahf, Sahf, Pushf, Popf,

        Add, Adc, Inc, Aaa, Daa,
        Sub, Sbb, Dec, Neg,
        Cmp, Aas, Das,
        Mul, Imul, Aam,
        Div, Idiv, Aad,
        Cbw, Cwd,

        Not,
        Shl, Shr, Sar, Rol,
        Ror, Rcl, Rcr,
        And, Test, Or, Xor,

        Movs, Cmps, Scas, Lods, Stos,

        Call, Jmp, Ret,

        Jo, Jno, Jb, Jnb, Je, Jnz, Jbe, Ja,
        Js, Jns, Jp, Jnp, Jl, Jnl, Jle, Jg,

        Loop, Loopz, Loopnz, Jcxz,

        Int, Int3, Into, Iret,

        Clc, Cmc, Stc, Cld, Std, Cli, Sti, Hlt, Wait, Esc,
    };
    static constexpr auto instruction_count = static_cast<size_t>(Type::Esc) + 1;
    static constexpr std::array instruction_type_names = {
        "UNKNOWN_INSTRUCTION",

        "mov", "push", "pop", "xchg", "in", "out",
        "xlat", "lea", "lds", "les",
        "lahf", "sahf", "pushf", "popf",

        "add", "adc", "inc", "aaa", "daa",
        "sub", "sbb", "dec", "neg",
        "cmp", "aas", "das",
        "mul", "imul", "aam",
        "div", "idiv", "aad",
        "cbw", "cwd",

        "not",
        "shl", "shr", "sar", "rol",
        "ror", "rcl", "rcr",
        "and", "test", "or", "xor",

        "movs", "cmps", "scas", "lods", "stos",

        "call", "jmp", "ret",

        "jo", "jno", "jb", "jnb", "je", "jnz", "jbe", "ja",
        "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jg",

        "loop", "loopz", "loopnz", "jcxz",

        "int", "int3", "into", "iret",

        "clc", "cmc", "stc", "cld", "std", "cli", "sti", "hlt", "wait", "esc",
    };
    static_assert(instruction_type_names.size() == instruction_count);

    struct Flags {
        bool wide : 1;
        bool ip_inc : 1;
        bool rep : 1;
        bool rep_nz : 1;
        bool intersegment : 1;
        bool lock : 1;
        bool short_jmp : 1;
    };

    u32 address = 0;
    u32 size = 0;

    Type type = Type::Invalid;
    Flags flags = {};
    std::optional<Register> segment_override;

    std::array<Operand, 2> operands = {};

    static std::optional<Instruction> decode_at(std::span<const u8> program, u32 start);

    static constexpr const char* lookup_type(Type type) {
        auto i = static_cast<std::underlying_type_t<Type>>(type);
        assert(i < std::size(instruction_type_names));
        return instruction_type_names[i];
    }

    const char* name() const {
        return lookup_type(type);
    }

    bool is_shift() const {
        using T = std::underlying_type_t<Type>;
        auto t = static_cast<T>(type);
        return static_cast<T>(Type::Shl) <= t && t <= static_cast<T>(Type::Rcr);
    }

    bool is_string_manipulation() const {
        using T = std::underlying_type_t<Type>;
        auto t = static_cast<T>(type);
        return static_cast<T>(Type::Movs) <= t && t <= static_cast<T>(Type::Stos);
    }

    void swap_operands() {
        swap(operands[0], operands[1]);
    }

    u32 estimate_cycles(u32 total = 0, FILE* out = nullptr) const;
    fmt::format_context::iterator format_to(fmt::format_context::iterator out) const;
};

template <> struct fmt::formatter<Instruction> {
    constexpr format_parse_context::iterator parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    format_context::iterator format(const Instruction& i, format_context& ctx) {
        return i.format_to(ctx.out());
    }
};
