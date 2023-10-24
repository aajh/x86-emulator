#include "emulator.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

#include "instruction.hpp"
#include "program.hpp"

#ifdef TESTING
constexpr bool verbose_execution = false;
#else
constexpr bool verbose_execution = true;
#endif

#define UNIMPLEMENTED_INSTRUCTION\
    do {\
        fflush(stdout);\
        fprintf(stderr, "\nUnimplemented instruction %s\n", i.name());\
        return true;\
    } while (false)

#define UNIMPLEMENTED_SHORT\
    if (!i.flags.wide) {\
        fflush(stdout);\
        fprintf(stderr, "\nUnimplemented short version of instruction %s\n", i.name());\
        return true;\
    }

#define ONE_OPERAND_REQUIRED\
    if (ocount == 0) {\
        fflush(stdout);\
        fprintf(stderr, "\nInstruction %s requires at least one operand\n", i.name());\
        return true;\
    }

#define TWO_OPERANDS_REQUIRED\
    if (ocount != 2) {\
        fflush(stdout);\
        fprintf(stderr, "\nInstruction %s requires two operands\n", i.name());\
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

// Normally not used x86 op code
constexpr u8 inserted_halt_instruction = 0xf;

void Intel8086::load_program(std::span<const u8> program) {
    auto size = std::min(program.size(), memory.size());
    memcpy(memory.data(), program.data(), size);
    if (memory.size() > size) memory[size] = inserted_halt_instruction;
}

error_code Intel8086::load_program(const char* filename) {
    UNWRAP_BARE(auto program, read_program(filename));
    load_program(program);
    return {};
}

error_code Intel8086::dump_memory(const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Couldn't open file %s\n", filename);
        return make_error_code_errno();
    }
    DEFER { fclose(file); };

    RET_BARE_ERRNO(fwrite(memory.data(), 1, memory.size(), file) != memory.size());

    return {};
}

void Intel8086::print_state(FILE* out) const {
    constexpr int padding = 8;

    fprintf(out, "\nFinal registers:\n");
    for (auto r : { ax, bx, cx, dx, sp, bp, si, di, es, cs, ss, ds }) {
        if (get(r) == 0) continue;
        fprintf(out, "%*s: 0x%.4x (%u)\n", padding, lookup_register(r), get(r), get(r));
    }
    if (ip) fprintf(out, "%*s: 0x%.4x (%u)\n", padding, "ip", ip, ip);

    if (flags) {
        fprintf(out, "%*s: ", padding, "flags");
        flags.print(out);
    }

    fprintf(out, "\n");
}

u16 Intel8086::calculate_address(const MemoryOperand& mo) const {
    switch (mo.eac) {
        using E = EffectiveAddressCalculation;
        case E::bx_si:
            return get(bx) + get(si) + mo.displacement;
        case E::bx_di:
            return get(bx) + get(di) + mo.displacement;
        case E::bp_si:
            return get(bp) + get(si) + mo.displacement;
        case E::bp_di:
            return get(bp) + get(di) + mo.displacement;
        case E::si:
            return get(si) + mo.displacement;
        case E::di:
            return get(di) + mo.displacement;
        case E::bp:
            return get(bp) + mo.displacement;
        case E::bx:
            return get(bx) + mo.displacement;
        case E::DirectAccess:
            return mo.displacement;
    }
    assert(false);
    return 0;
}

error_code Intel8086::run(bool estimate_cycles) {
    DEFER { if constexpr (verbose_execution) print_state(); };

    u32 cycles = 0;
    while (true) {
        if (memory[ip] == inserted_halt_instruction) break;

        auto instruction = Instruction::decode_at({ memory.data(), (u32)memory.size() }, ip);
        if (!instruction) {
            fflush(stdout);
            fprintf(stderr, "Unknown instruction at location %u (first byte 0x%x)\n", ip, memory[ip]);
            return Errc::UnknownInstruction;
        }

        if (execute(*instruction, estimate_cycles, cycles)) break;
    }

    return {};
}

bool Intel8086::execute(const Instruction& i, bool estimate_cycles, u32& cycles) {
    using enum Instruction::Type;
    using enum Operand::Type;

    if constexpr (verbose_execution) {
        i.print_assembly();
        if (estimate_cycles) {
            printf(" ; ");
            cycles += i.estimate_cycles(cycles, stdout);
        }
    }
    DEFER { if constexpr (verbose_execution) printf("\n"); };

    const auto& o1 = i.operands[0];
    const auto& o2 = i.operands[1];

    i32 ocount = o1.type != None;
    if (ocount == 1 && o2.type != None) ++ocount;

    ip += i.size;

    switch (i.type) {
        case Mov:
            TWO_OPERANDS_REQUIRED;
            set(o1, o2, i.flags.wide);
            break;
        case Add: {
            TWO_OPERANDS_REQUIRED;
            UNIMPLEMENTED_SHORT;

            u16 a = get(o1, true);
            u16 b = get(o2, true);

            u32 wide_result = a + b;
            u16 result = wide_result & 0xffff;

            set(o1, result, true);
            set_flags(a, b, result, wide_result, false);

            break;
        }
        case Sub:
        case Cmp: {
            TWO_OPERANDS_REQUIRED;
            UNIMPLEMENTED_SHORT;

            u16 a = get(o1, true);
            u16 b = get(o2, true);

            u32 wide_result = a - b;
            u16 result = wide_result & 0xffff;

            if (i.type == Sub) set(o1, result, true);
            set_flags(a, b, result, wide_result, true);

            break;
        }
        case Call:
            ONE_OPERAND_REQUIRED;
            if (o1.type != IpInc) UNIMPLEMENTED_INSTRUCTION;
            push(get(ip));
            ip += o1.ip_inc;
            break;
        case Ret:
            if (i.flags.intersegment) UNIMPLEMENTED_INSTRUCTION;
            ip = pop();
            if (o1.type == Immediate) set(sp, get(sp) + o1.immediate);
            break;
        case Jb:
            ONE_OPERAND_REQUIRED;
            if (flags.c) ip += get<i16>(o1);
            break;
        case Je:
            ONE_OPERAND_REQUIRED;
            if (flags.z) ip += get<i16>(o1);
            break;
        case Jnz:
            ONE_OPERAND_REQUIRED;
            if (!flags.z) ip += get<i16>(o1);
            break;
        case Jp:
            ONE_OPERAND_REQUIRED;
            if (flags.p) ip += get<i16>(o1);
            break;
        case Loop:
            ONE_OPERAND_REQUIRED;
            set(cx, get(cx) - 1);
            if (get(cx) != 0) ip += get<i16>(o1);
            break;
        case Loopz:
            ONE_OPERAND_REQUIRED;
            set(cx, get(cx) - 1);
            if (get(cx) != 0 && flags.z) ip += get<i16>(o1);
            break;
        case Loopnz:
            ONE_OPERAND_REQUIRED;
            set(cx, get(cx) - 1);
            if (get(cx) != 0 && !flags.z) ip += get<i16>(o1);
            break;
        case Hlt:
            return true;
        default:
            UNIMPLEMENTED_INSTRUCTION;
    }

    return false;
}

void Intel8086::set_flags(u16 a, u16 b, u16 result, u32 wide_result, bool is_sub) {
    if constexpr (verbose_execution) {
        printf(" ; Flags: ");
        flags.print();
        printf("->");
    }

    bool a_signed = a & (1 << 15);
    bool b_signed = (is_sub ? -b : b) & (1 << 15);
    bool result_signed = result & (1 << 15);

    bool aux_carry = (u32)(a & 0xf) + (u32)(b & 0xf) > 0xf;
    bool aux_borrow =  (i32)(a & 0xf) - (i32)(b & 0xf) < 0;

    bool argument_same_sign = a_signed == b_signed;

    flags.c = wide_result > std::numeric_limits<decltype(result)>::max();
    flags.p = std::popcount<u8>(result & 0xff) % 2 == 0;
    flags.a = is_sub ? aux_borrow : aux_carry;
    flags.z = result == 0;
    flags.s = result & (1 << 15);
    flags.o = argument_same_sign && (a_signed != result_signed);

    if constexpr (verbose_execution) {
        flags.print();
    }
}

void Intel8086::push(u16 value, bool wide) {
    set(sp, get(sp) - 2);
    u32 s = get(sp);
    memory[s] = value & 0xff;
    if (wide) memory[s + 1] = value >> 8;
}

u16 Intel8086::pop(bool wide) {
    u32 s = get(sp);
    u16 value = memory[s] | (wide ? (memory[s + 1] << 8) : 0);
    set(sp, get(s) + 2);
    return value;
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
