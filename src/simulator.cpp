#include "simulator.hpp"
#include <algorithm>
#include <cstring>

#include "instruction.hpp"
#include "program.hpp"

#ifdef TESTING
constexpr bool verbose_execution = false;
#else
constexpr bool verbose_execution = false;
#endif

#define UNIMPLEMENTED_INSTRUCTION\
    fflush(stdout);\
    fprintf(stderr, "Unimplemented instruction %s\n", i.name());\
    return true;

#define UNIMPLEMENTED_SHORT\
    if (!i.flags.wide) {\
        fflush(stdout);\
        fprintf(stderr, "Unimplemented short version of instruction %s\n", i.name());\
        return true;\
    }

#define TWO_OPERANDS_REQUIRED\
    if (ocount != 2) {\
        fflush(stdout);\
        fprintf(stderr, "Instruction %s requires two operands\n", i.name());\
        return true;\
    }

void Intel8086::Flags::print(FILE* out) const {
    if (c) fprintf(out, "C");
    if (p) fprintf(out, "P");
    if (a) fprintf(out, "A");
    if (z) fprintf(out, "Z");
    if (s) fprintf(out, "S");
    if (o) fprintf(out, "O");
    if (i) fprintf(out, "I");
    if (d) fprintf(out, "D");
    if (t) fprintf(out, "T");
}

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

void Intel8086::print_state(FILE* out) const {
    constexpr int padding = 8;
    fprintf(out, "Registers:\n");
    for (auto r : { ax, bx, cx, dx, sp, bp, si, di, es, cs, ss, ds }) {
        if (get(r) == 0) continue;
        fprintf(out, "%*s: 0x%.4x (%u)\n", padding, lookup_register(r), get(r), get(r));
    }
    fprintf(out, "%*s: ", padding, "flags");
    flags.print(out);
    fprintf(out, "\n");
}

error_code Intel8086::simulate(FILE* out) {
    DEFER { if (out) print_state(out); };
    while (true) {
        UNWRAP_OR(auto instruction, Instruction::decode_at({ memory.data(), (u32)memory.size() }, ip)) {
            if (out) fflush(out);
            fprintf(stderr, "Unknown instruction at location %u (first byte 0x%x)\n", ip, memory[ip]);
            return Errc::UnknownInstruction;
        }

        if (simulate(instruction)) break;
    }
    return {};
}

bool Intel8086::simulate(const Instruction& i) {
    using enum Instruction::Type;
    using enum Operand::Type;

    if constexpr (verbose_execution) i.print_assembly();

    const auto& o1 = i.operands[0];
    const auto& o2 = i.operands[1];

    i32 ocount = o1.type != None;
    if (ocount == 1 && o2.type != None) ++ocount;

    switch (i.type) {
        case Mov:
            TWO_OPERANDS_REQUIRED;
            if (o1.type == Register && (o2.type == Register || o2.type == Immediate)) {
                set(o1, o2);
            } else {
                UNIMPLEMENTED_INSTRUCTION;
            }
            break;
        case Add:
            TWO_OPERANDS_REQUIRED;
            UNIMPLEMENTED_SHORT;
            if (o1.type == Register && (o2.type == Register || o2.type == Immediate)) {
                u16 a = get(o1);
                u16 b = get(o2);

                u32 wide_result = a + b;
                u16 result = wide_result & 0xffff;

                set(o1, result);
                set_flags(a, b, result, wide_result, false);
            } else {
                UNIMPLEMENTED_INSTRUCTION;
            }
            break;
        case Sub:
        case Cmp:
            TWO_OPERANDS_REQUIRED;
            UNIMPLEMENTED_SHORT;
            if (o1.type == Register && (o2.type == Register || o2.type == Immediate)) {
                u16 a = get(o1);
                u16 b = get(o2);

                u32 wide_result = a - b;
                u16 result = wide_result & 0xffff;

                if (i.type == Sub) set(o1, result);
                set_flags(a, b, result, wide_result, true);
            } else {
                UNIMPLEMENTED_INSTRUCTION;
            }
            break;
        case Hlt:
            return true;
        default:
            UNIMPLEMENTED_INSTRUCTION;
    }

    ip += i.size;
    return false;
}

void Intel8086::set_flags(u16 a, u16 b, u16 result, u32 wide_result, bool is_sub) {
    if (is_sub) b = -b;

    bool same_sign = (a & (1 << 15)) == (b & (1 << 15));
    bool a_signed = a & (1 << 15);
    bool result_signed = result & (1 << 15);

    u16 low_byte = result & 0xff;

    // Parity check from Hacker's Delight section 5-2
    u16 lb_parity = low_byte ^ (low_byte >> 1);
    lb_parity ^= lb_parity >> 2;
    lb_parity ^= lb_parity >> 4;
    lb_parity ^= lb_parity >> 8;
    lb_parity = !(lb_parity & 1);

    bool aux_carry = (a & 0xf0 + b & 0xf0) > ((a + b) & 0xf0);
    bool aux_borrow = is_sub && ((a + b) & 0xf) > (a & 0xf + b & 0xf);

    flags.c = wide_result > std::numeric_limits<decltype(result)>::max();
    flags.p = lb_parity;
    flags.a = aux_carry || aux_borrow; // TODO: Test this more
    flags.z = result == 0;
    flags.s = result & (1 << 15);
    flags.o = same_sign && (a_signed != result_signed);

    if constexpr (verbose_execution) {
        printf("    Flags: ");
        flags.print();
        printf(" 0x%.4x 0x%.8x 0x%.4x 0x%.4x\n", result, wide_result, a, b);
    }
}


#ifdef TESTING
void Intel8086::assert_registers(u16 a, i16 b, u8 c, i8 d, u8 e, i8 f, bool print) const {
    if (print) {
        printf("ax: 0x%hx 0x%hx, al: 0x%hx 0x%hx, ah: 0x%hx 0x%hx\n", get<u16>(ax), get<i16>(ax), get<u16>(al), get<i16>(al), get<u16>(ah), get<i16>(ah));
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
