#include "simulator.hpp"

#include "instruction.hpp"
#include "program.hpp"

expected<Intel8086, error_code> Intel8086::read_program_from_file(const char* filename) {
    UNWRAP(auto program, read_program(filename));
    return Intel8086(std::move(program));
}

void Intel8086::print_registers(FILE* out) const {
    fprintf(out, "Registers:\n");
    for (auto r : { ax, bx, cx, dx, sp, bp, si, di }) {
        fprintf(out, "\t%s: 0x%.4hX (%hu)\n", lookup_register(r), get(r), get(r));
    }
}

error_code Intel8086::simulate(FILE* out) {
    if (program.size == 0) return {};

    DEFER { print_registers(out); };
    while (ip < program.size) {
        UNWRAP_OR(auto instruction, decode_instruction_at(program, ip)) {
            fflush(out);
            fprintf(stderr, "Unknown instruction at location %u (first byte 0x%X)\n", ip, program.data[ip]);
            return Errc::UnknownInstruction;
        }

        if (simulate(instruction)) break;
    }
    return {};
}

bool Intel8086::simulate(const Instruction& i) {
    switch (i.type) {
            using enum Instruction::Type;
            using enum Operand::Type;
        case Mov:
            if (i.operands[0].type == Register && i.operands[1].type == Immediate) {
                set(i.operands[0].reg, i.operands[1].immediate);
                ip += i.size;
                break;
            } else if (i.operands[0].type == Register && i.operands[1].type == Register) {
                set(i.operands[0].reg, i.operands[1].reg);
                ip += i.size;
                break;
            }
        default:
            fprintf(stderr, "Unimplemented instruction %s\n", i.name());
            return true;
    }
    return false;
}


#ifdef TESTING
void Intel8086::assert_registers(u16 a, i16 b, u8 c, i8 d, u8 e, i8 f, bool print) const {
    if (print) {
        printf("ax: 0x%hX 0x%hX, al: 0x%hX 0x%hX, ah: 0x%hX 0x%hX\n", get<u16>(ax), get<i16>(ax), get(al), get<i16>(al), get(ah), get<i16>(ah));
        printf("ax: %u %d, al: %u %d, ah: %u %d\n\n", get<u16>(ax), get<i16>(ax), get(al), get<i16>(al), get(ah), get<i16>(ah));
    }
    test_assert(get<u16>(ax), a);
    test_assert(get<i16>(ax), b);
    test_assert(get<u16>(al), c);
    test_assert(get<i16>(al), d);
    test_assert(get<u16>(ah), e);
    test_assert(get<i16>(ah), f);
}

void Intel8086::test_set_get(bool print) {
    set(ax, 0);
    set(al, 42);
    assert_registers(42, 42, 42, 42, 0, 0, print);
    set(ah, 42);
    assert_registers(0x2a2a, 0x2a2a, 42, 42, 42, 42, print);

    set(ax, 0xffff);
    assert_registers(0xffff, -1, 255, -1, 255, -1, print);

    set(ax, 0);
    set(al, 0xff);
    assert_registers(0xff, 0xff, 255, -1, 0, 0, print);
    set(ah, 0xff);
    assert_registers(0xffff, -1, 255, -1, 255, -1, print);

    set(ax, 0);
    set(al, -128);
    assert_registers(128, 128, 128, -128, 0, 0, print);
    set(ah, -128);
    assert_registers(0x8080, -32640, 128, -128, 128, -128, print);

    set(ax, 0);
    assert_registers(0, 0, 0, 0, 0, 0, print);
}
#endif
