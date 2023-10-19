#include "simulator.hpp"

#include "instruction.hpp"
#include "program.hpp"

using enum Register;

struct Intel8086 {
    std::array<u16, 12> registers = {};
    u16 ip = 0;

    template<typename T = u16>
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

    void print_registers() const {
        printf("\nRegisters:\n");
        for (auto r : { ax, cx, dx, bx, sp, bp, si, di }) {
            printf("\t%s: 0x%.4hX (%hu, %hd)\n", lookup_register(r), get(r), get(r), get<i16>(r));
        }
        printf("\n");
    }

    bool simulate(const Instruction& i) {
        switch (i.type) {
            using enum Instruction::Type;
            using enum Operand::Type;
            case Mov:
                if (i.operands[0].type == Register && i.operands[1].type == Immediate) {
                    set(i.operands[0].reg, i.operands[1].immediate);
                    ip += i.size;
                    break;
                }
            default:
                fprintf(stderr, "Unimplemented instruction %s\n", i.name());
                return true;
        }
        return false;
    }

    error_code simulate(const Program& program) {
        DEFER { print_registers(); };
        while (ip < program.size) {
            UNWRAP_OR(auto instruction, decode_instruction_at(program, ip)) {
                fflush(stdout);
                fprintf(stderr, "Unknown instruction at location %u (first byte 0x%X)\n", ip, program.data[ip]);
                return Errc::UnknownInstruction;
            }

            if (simulate(instruction)) break;
        }
        return {};
    }


    void test_assert(u16 a, i16 b, u8 c, i8 d, u8 e, i8 f, bool print) const {
        if (print) {
            printf("ax: 0x%hX 0x%hX, al: 0x%hX 0x%hX, ah: 0x%hX 0x%hX\n", get<u16>(ax), get<i16>(ax), get(al), get<i16>(al), get(ah), get<i16>(ah));
            printf("ax: %u %d, al: %u %d, ah: %u %d\n\n", get<u16>(ax), get<i16>(ax), get(al), get<i16>(al), get(ah), get<i16>(ah));
        }
        assert(get<u16>(ax) == a);
        assert(get<i16>(ax) == b);
        assert(get<u16>(al) == c);
        assert(get<i16>(al) == d);
        assert(get<u16>(ah) == e);
        assert(get<i16>(ah) == f);
    }

    void test_set_get(bool print = false) {
        set(ax, 0);
        set(al, 42);
        test_assert(42, 42, 42, 42, 0, 0, print);
        set(ah, 42);
        test_assert(0x2a2a, 0x2a2a, 42, 42, 42, 42, print);

        set(ax, 0xffff);
        test_assert(0xffff, -1, 255, -1, 255, -1, print);

        set(ax, 0);
        set(al, 0xff);
        test_assert(0xff, 0xff, 255, -1, 0, 0, print);
        set(ah, 0xff);
        test_assert(0xffff, -1, 255, -1, 255, -1, print);

        set(ax, 0);
        set(al, -128);
        test_assert(128, 128, 128, -128, 0, 0, print);
        set(ah, -128);
        test_assert(0x8080, -32640, 128, -128, 128, -128, print);

        set(ax, 0);
        test_assert(0, 0, 0, 0, 0, 0, print);
    }
};

error_code simulate_program(const Program& program) {
    Intel8086 x86;
    x86.test_set_get();
    return x86.simulate(program);
}

error_code simulate_file(const char* filename) {
    UNWRAP_BARE(auto program, read_program(filename));
    DEFER { delete[] program.data; };

    return simulate_program(program);
}
