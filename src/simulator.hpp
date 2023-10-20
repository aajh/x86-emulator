#pragma once

#include "common.hpp"
#include <cstdio>
#include <vector>

#include "instruction.hpp"

struct Program;

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

        bool operator==(const Flags& f) const {
            return c == f.c && p == f.p && a == f.a && z == f.z &&
                   s == f.s && o == f.o && i == f.i && d == f.d &&
                   t == f.t;
        }
        void print(FILE* out = stdout) const;
    };

    static constexpr u32 memory_size = 1 << 20;

    Intel8086() : memory(memory_size) {}
    Intel8086(const Program& program) : Intel8086() {
        load_program(program);
    }

    void load_program(const Program& program);
    error_code load_program(const char* filename);

    template<typename T = u32>
    T get(Register reg) const {
        constexpr auto is_signed = std::is_signed_v<T>;

        using R_int = std::underlying_type_t<Register>;
        auto r = static_cast<R_int>(reg);

        if (is_8bit_register(reg)) {
            using R = typename std::conditional<is_signed, i8, u8>::type;
            if (is_8bit_low_register(reg)) {
                auto r1 = r - static_cast<R_int>(al);
                return static_cast<R>(registers[r1] & 0xff);
            } else {
                auto r1 = r - static_cast<R_int>(ah);
                return static_cast<R>((registers[r1] & 0xff00) >> 8);
            }
        } else if (is_segment_register(reg)) {
            auto sr = r - static_cast<R_int>(es) + 8;
            return static_cast<T>(registers[sr]);
        } else {
            return static_cast<T>(registers[r]);
        }
    }

    template<typename T = u32>
    T get(const Operand& o) const {
        switch (o.type) {
            using enum Operand::Type;
            case None:
                fprintf(stderr, "Trying to get 'None' operand\n");
                assert(false);
                return 0;
            case Register:
                return get<T>(o.reg);
            case Immediate:
                return o.immediate;
            case Memory:
                assert(false);
                fprintf(stderr, "Unimplemented get operand 'Memory'\n");
                return 0;
            case IpInc:
                return o.ip_inc;
        }
    }

    u16 get_ip() const {
        return ip;
    }

    const Flags& get_flags() const {
        return flags;
    }

    template<typename T = u16>
    void set(Register reg, T value) {
        using R = std::underlying_type_t<Register>;
        auto r = static_cast<R>(reg);

        if (is_8bit_register(reg)) {
            if (is_8bit_low_register(reg)) {
                auto r1 = r - static_cast<R>(al);
                registers[r1] = static_cast<u16>((value & 0xff) | (registers[r1] & 0xff00));
            } else {
                auto r1 = r - static_cast<R>(ah);
                registers[r1] = static_cast<u16>(((value & 0xff) << 8) | (registers[r1] & 0xff));
            }
        } else if (is_segment_register(reg)) {
            auto sr = r - static_cast<R>(es) + 8;
            registers[sr] = static_cast<u16>(value);
        } else {
            registers[r] = static_cast<u16>(value);
        }
    }

    void set(const Operand& o, u16 value) {
        switch (o.type) {
            using enum Operand::Type;
            case None:
                fprintf(stderr, "Trying to set 'None' operand\n");
                assert(false);
                break;
            case Register:
                set(o.reg, value);
                break;
            case Immediate:
                fprintf(stderr, "Cannot modify an immediate value\n");
                break;
            case IpInc:
                fprintf(stderr, "Cannot modify an ip_inc value\n");
                break;
            case Memory:
                assert(false);
                fprintf(stderr, "Unimplemented set operand 'Memory'\n");
                break;
        }
    }

    void set(const Operand& o1, const Operand& o2) {
        switch (o2.type) {
            using enum Operand::Type;
            case None:
                fprintf(stderr, "Trying to set with 'None' operand\n");
                assert(false);
                break;
            case Register:
            case Immediate:
                set(o1, get(o2));
                break;
            case IpInc:
                fprintf(stderr, "Cannot set with an ip_inc value\n");
                break;
            case Memory:
                assert(false);
                fprintf(stderr, "Unimplemented set operand 'Memory'\n");
                break;
        }
    }

    void set(Register destination, Register source) {
        set(destination, get(source));
    }

    void print_state(FILE* out = stdout) const;
    error_code simulate(FILE* out = stdout);

#ifdef TESTING
    void assert_registers(u16 a, i16 b, u8 c, i8 d, u8 e, i8 f, bool print) const;
    void test_set_get(bool print = false);
#endif


private:
    std::array<u16, 12> registers = {};
    u16 ip = 0;
    Flags flags = {};

    std::vector<u8> memory;

    bool simulate(const Instruction& i);
    void set_flags(u16 a, u16 b, u16 result, u32 wide_result, bool is_sub);
};
