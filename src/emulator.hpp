#pragma once

#include "common.hpp"
#include <cstdio>
#include <vector>
#include <fmt/core.h>

#include "instruction.hpp"

class Intel8086 {
    using enum Register;
public:
    struct Flags {
        bool c : 1; // Carry
        bool p : 1; // Parity
        bool a : 1; // Auxiliary carry
        bool z : 1; // Zero
        bool s : 1; // Sign
        bool o : 1; // Overflow
        bool i : 1; // Interrupt-enable
        bool d : 1; // Direction
        bool t : 1; // Trap

        bool operator==(const Flags& f) const = default;
        explicit operator bool() const {
            return *this != Flags();
        }
    };

    static constexpr u32 memory_size = 1 << 16;

    Intel8086() : memory(memory_size) {
        set(sp, 0xffff);
    }
    Intel8086(std::span<const u8> program) : Intel8086() {
        load_program(program);
    }

    void load_program(std::span<const u8> program);
    error_code load_program(const char* filename);

    error_code dump_memory(const char* filename);

    template<typename T = u16>
    T get(Register reg) const {
        constexpr auto is_signed = std::is_signed_v<T>;

        using R_int = std::underlying_type_t<Register>;
        auto r = static_cast<R_int>(reg);

        if (is_8bit_register(reg)) {
            using R = typename std::conditional<is_signed, i8, u8>::type;
            if (is_8bit_low_register(reg)) {
                auto r1 = r - static_cast<R_int>(Register::al);
                return static_cast<R>(registers[r1] & 0xff);
            } else {
                auto r1 = r - static_cast<R_int>(Register::ah);
                return static_cast<R>((registers[r1] & 0xff00) >> 8);
            }
        } else if (is_segment_register(reg)) {
            auto sr = r - static_cast<R_int>(Register::es) + 8;
            return static_cast<T>(registers[sr]);
        } else {
            return static_cast<T>(registers[r]);
        }
    }

    template<typename T = u16>
    T get(const Operand& o, bool wide_memory = false) const {
        switch (o.type) {
            using enum Operand::Type;
            case None:
                fmt::print(stderr, "Trying to get 'None' operand\n");
                assert(false);
                return 0;
            case Register:
                return get<T>(o.reg);
            case Immediate:
                return o.immediate;
            case Memory: {
                auto address = calculate_address(o.memory);
                u16 value = memory[address];
                if (wide_memory) value |= memory[address + 1] << 8;
                return value;
            }
            case IpInc:
                return o.ip_inc;
        }
        assert(false);
        return 0;
    }

    u16 calculate_address(const MemoryOperand& mo) const;
    u16 get_ip() const { return ip; }
    const Flags& get_flags() const { return flags; }

    template<typename T = u16>
    void set(Register reg, T value) {
        using R = std::underlying_type_t<Register>;
        auto r = static_cast<R>(reg);

        if (is_8bit_register(reg)) {
            if (is_8bit_low_register(reg)) {
                auto r1 = r - static_cast<R>(Register::al);
                registers[r1] = static_cast<u16>((value & 0xff) | (registers[r1] & 0xff00));
            } else {
                auto r1 = r - static_cast<R>(Register::ah);
                registers[r1] = static_cast<u16>(((value & 0xff) << 8) | (registers[r1] & 0xff));
            }
        } else if (is_segment_register(reg)) {
            auto sr = r - static_cast<R>(Register::es) + 8;
            registers[sr] = static_cast<u16>(value);
        } else {
            registers[r] = static_cast<u16>(value);
        }
    }

    void set(const Operand& o, u16 value, bool wide_memory) {
        switch (o.type) {
            using enum Operand::Type;
            case None:
                fmt::print(stderr, "Trying to set 'None' operand\n");
                assert(false);
                break;
            case Register:
                set(o.reg, value);
                break;
            case Immediate:
                fmt::print(stderr, "Cannot modify an immediate value\n");
                break;
            case IpInc:
                fmt::print(stderr, "Cannot modify an ip_inc value\n");
                break;
            case Memory:
                auto address = calculate_address(o.memory);
                memory[address] = value & 0xff;
                if (wide_memory) memory[address + 1] = (value & 0xff00) >> 8;
                break;
        }
    }

    void set(const Operand& o1, const Operand& o2, bool wide_memory) {
        set(o1, get(o2, wide_memory), wide_memory);
    }

    void set(Register destination, Register source) {
        set(destination, get(source));
    }

    void print_state(FILE* out = stdout) const;
    error_code run(bool estimate_cycles = false);

#ifdef TESTING
    void assert_registers(u16 a, i16 b, u8 c, i8 d, u8 e, i8 f, bool print) const;
    void test_set_get(bool print = false);
#endif


private:
    std::array<u16, 12> registers = {};
    u16 ip = 0;
    Flags flags = {};

    std::vector<u8> memory;

    bool execute(const Instruction& i, bool estimate_cycles, u32& cycles);
    void set_flags(u16 a, u16 b, u16 result, u32 wide_result, bool is_sub);
    void push(u16 value, bool wide = true);
    u16 pop(bool wide = true);
};

template <> struct fmt::formatter<Intel8086::Flags> {
    constexpr format_parse_context::iterator parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    format_context::iterator format(const Intel8086::Flags& f, format_context& ctx) {
        auto out = ctx.out();
        if (f.c) format_to(out, "C");
        if (f.p) format_to(out, "P");
        if (f.a) format_to(out, "A");
        if (f.z) format_to(out, "Z");
        if (f.s) format_to(out, "S");
        if (f.o) format_to(out, "O");
        if (f.i) format_to(out, "I");
        if (f.d) format_to(out, "D");
        if (f.t) format_to(out, "T");
        return out;
    }
};
