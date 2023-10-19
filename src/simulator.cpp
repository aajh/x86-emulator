#include "simulator.hpp"
#include <algorithm>
#include <cstring>

#include "instruction.hpp"
#include "program.hpp"

constexpr u8 halt_instruction = 0xf4;

void Intel8086::load_program(const Program& program) {
    auto size = std::min((size_t)program.size, memory.size());
    memcpy(memory.data(), program.data, size);
    if (memory.size() > size) memory[size] = halt_instruction;
}

error_code Intel8086::load_program(const char* filename) {
    UNWRAP_BARE(auto program, read_program(filename));
    DEFER { delete[] program.data; };
    load_program(program);
    return {};
}

void Intel8086::print_registers(FILE* out) const {
    fprintf(out, "Registers:\n");
    for (auto r : { ax, bx, cx, dx, sp, bp, si, di, es, cs, ss, ds }) {
        fprintf(out, "\t%s: 0x%.4hX (%hu)\n", lookup_register(r), get(r), get(r));
    }
}

error_code Intel8086::simulate(FILE* out) {
    DEFER { if (out) print_registers(out); };
    while (true) {
        UNWRAP_OR(auto instruction, decode_instruction_at({ memory.data(), (u32)memory.size() }, ip)) {
            if (out) fflush(out);
            fprintf(stderr, "Unknown instruction at location %u (first byte 0x%X)\n", ip, memory[ip]);
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
            } else if (i.operands[0].type == Register && i.operands[1].type == Register) {
                set(i.operands[0].reg, i.operands[1].reg);
            } else {
                fprintf(stderr, "Unimplemented instruction mov\n");
                return true;
            }
            break;
        case Hlt:
            return true;
        default:
            fprintf(stderr, "Unimplemented instruction %s\n", i.name());
            return true;
    }

    ip += i.size;
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
