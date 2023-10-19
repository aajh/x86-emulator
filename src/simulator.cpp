#include "simulator.hpp"

#include "instruction.hpp"
#include "program.hpp"

expected<Intel8086, error_code> Intel8086::read_program_from_file(const char* filename) {
    UNWRAP(auto program, read_program(filename));
    return Intel8086(std::move(program));
}

void Intel8086::print_registers() const {
    printf("\nRegisters:\n");
    for (auto r : { ax, cx, dx, bx, sp, bp, si, di }) {
        printf("\t%s: 0x%.4hX (%hu, %hd)\n", lookup_register(r), get(r), get(r), get<i16>(r));
    }
    printf("\n");
}

error_code Intel8086::simulate() {
    test_set_get();

    if (program.size == 0) return {};

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

bool Intel8086::simulate(const Instruction& i) {
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


void Intel8086::test_assert(u16 a, i16 b, u8 c, i8 d, u8 e, i8 f, bool print) const {
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

void Intel8086::test_set_get(bool print) {
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
