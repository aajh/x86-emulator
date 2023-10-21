#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unistd.h>

#include "program.hpp"
#include "simulator.hpp"

static expected<std::string, error_code> read_file(const std::string& filename) {
    auto file = fopen(filename.data(), "rb");
    RET_ERRNO(file == nullptr);

    RET_ERRNO(fseek(file, 0, SEEK_END));

    const auto ftell_result = ftell(file);
    RET_ERRNO(ftell_result < 0);
    const size_t size = ftell_result;

    RET_ERRNO(fseek(file, 0, SEEK_SET));

    std::string content(size, '\0');
    if (fread(content.data(), 1, content.size(), file) != content.size()) {
        if (feof(file)) return unexpected(Errc::EndOfFile);
        return make_unexpected_errno();
    }

    return content;
}

struct ReadLineResult {
    std::string_view s;
    size_t length;
};
static ReadLineResult read_line(const std::string_view& string, bool skip_whitespace = false) {
    if (string.size() == 0) return {};

    auto start = string.find_first_not_of(skip_whitespace ? "\r\n\t\v " : "\r\n");
    if (start == std::string::npos) return { {}, string.size() };

    auto line_end_i = string.find_first_of("\r\n", start);
    if (line_end_i == std::string::npos) return {
        { string.data() + start, string.size() - start },
        string.size(),
    };

    return {
        { string.data() + start, line_end_i - start },
        line_end_i,
    };
}

static expected<std::string, error_code> assemble(const std::string& filename) {
    std::string assembled_filename = "/tmp/x86-sim_nasm.out.XXXXXX";
    auto assembled_fd = mkstemp(assembled_filename.data());
    RET_ERRNO(assembled_fd == -1);
    if (close(assembled_fd)) {
        auto e = make_unexpected_errno();
        unlink(assembled_filename.data());
        return e;
    }

    printf("Assembling %s to %s\n", filename.data(), assembled_filename.data());
    {
        std::string command = "nasm -o ";
        command += assembled_filename;
        command += " ";
        command += filename;

        if (system(command.data())) {
            unlink(assembled_filename.data());
            return unexpected(Errc::ReassemblyError);
        }
    }

    return assembled_filename;
}

static error_code test_disassembler(const std::string& filename) {
    UNWRAP_BARE(auto program, read_program(filename.data()));
    DEFER { delete[] program.data; };

    std::string disassembled_filename = "/tmp/x86-sim.asm.XXXXXX";
    auto disassembled_fd = mkstemp(disassembled_filename.data());
    RET_BARE_ERRNO(disassembled_fd == -1);
    DEFER { close(disassembled_fd); unlink(disassembled_filename.data()); };

    auto disassembled_file = fdopen(disassembled_fd, "wb");
    RET_BARE_ERRNO(disassembled_file == nullptr);
    DEFER { fclose(disassembled_file); };

    printf("Disassembling %s to %s\n", filename.data(), disassembled_filename.data());
    RET_IF(disassemble_program(disassembled_file, program));
    fflush(disassembled_file);


    std::string reassembled_filename = "/tmp/x86-sim_nasm.out.XXXXXX";
    auto reassembled_fd = mkstemp(reassembled_filename.data());
    RET_BARE_ERRNO(reassembled_fd == -1);
    DEFER { close(reassembled_fd); unlink(reassembled_filename.data()); };

    printf("Reassembling %s to %s\n", disassembled_filename.data(), reassembled_filename.data());
    {
        std::string command = "nasm -o ";
        command += reassembled_filename;
        command += " ";
        command += disassembled_filename;

        if (system(command.data())) return Errc::ReassemblyError;
    }


    auto reassembled_file = fdopen(reassembled_fd, "rb");
    RET_BARE_ERRNO(reassembled_file == nullptr);
    DEFER { fclose(reassembled_file); };

    UNWRAP_BARE(auto reassembled_program, read_program(reassembled_file));
    DEFER { delete[] reassembled_program.data; };

    if (program.size != reassembled_program.size) {
        fflush(stdout);
        fprintf(stderr, "Reassembled program has different size (%u, original %u)\n", reassembled_program.size, program.size);
        return Errc::ReassemblyFailed;
    }

    for (size_t i = 0; i < program.size; ++i) {
        if (program.data[i] != reassembled_program.data[i]) {
            fflush(stdout);
            fprintf(stderr, "Reassembled program differs at position %zu (0x%x, original 0x%x)\n", i, reassembled_program.data[i], program.data[i]);

            fprintf(stderr, "Reassembled bytes around the location are:");
            for (i32 j = -5; j < 6; ++j) {
                if (i + j < 0 || i + j >= reassembled_program.size) continue;
                fprintf(stderr, " 0x%X", reassembled_program.data[i + j]);
            }
            fprintf(stderr, "\n");

            fprintf(stderr, "Original bytes around the location are:   ");
            for (i32 j = -5; j < 6; ++j) {
                if (i + j < 0 || i + j >= program.size) continue;
                fprintf(stderr, " 0x%X", program.data[i + j]);
            }
            fprintf(stderr, "\n");

            return Errc::ReassemblyFailed;
        }
    }

    return {};
}

static error_code assemble_and_test_disassembler(const std::string& filename) {
    UNWRAP_BARE(auto assembled_filename, assemble(filename));
    DEFER { unlink(assembled_filename.data()); };
    return test_disassembler(assembled_filename);
}

static error_code test_simulator(const std::string& program_filename, const std::string& expected_filename) {
    printf("Simulating program %s\n", program_filename.data());

    Intel8086 x86;
    RET_IF(x86.load_program(program_filename.data()));
    RET_IF(x86.simulate(nullptr));

    UNWRAP_BARE(auto expected_output, read_file(expected_filename));

    const char register_line[] = "Final registers:";
    auto search_i = expected_output.find(register_line);
    if (search_i == std::string::npos) {
        fflush(stdout);
        fprintf(stderr, "Didn't find the register line in the expected output file %s\n", expected_filename.data());
        return Errc::InvalidExpectedOutputFile;
    }
    search_i += std::size(register_line) - 1;

    error_code ret = {};
    auto expected_line_end_i = search_i;
    while (expected_line_end_i < expected_output.size()) {
        auto expected_line = read_line({ expected_output.data() + expected_line_end_i, expected_output.size() - expected_line_end_i }, true);

        const char flags[] = "flags:";
        if (expected_line.s.starts_with(flags)) {
            auto expected_flags_string = expected_line.s.size() > std::size(flags) ? expected_line.s.substr(std::size(flags)) : std::string_view{};

            Intel8086::Flags expected_flags = {};
            for (auto f : expected_flags_string) {
                switch (f) {
                    case 'C':
                        expected_flags.c = true;
                        break;
                    case 'P':
                        expected_flags.p = true;
                        break;
                    case 'A':
                        expected_flags.a = true;
                        break;
                    case 'Z':
                        expected_flags.z = true;
                        break;
                    case 'S':
                        expected_flags.s = true;
                        break;
                    case 'O':
                        expected_flags.o = true;
                        break;
                    case 'I':
                        expected_flags.i = true;
                        break;
                    case 'D':
                        expected_flags.d = true;
                        break;
                    case 'T':
                        expected_flags.t = true;
                        break;
                    default:
                        fflush(stdout);
                        fprintf(stderr, "Unknown flag '%c' in the expected output file.\n", f);
                        return Errc::InvalidExpectedOutputFile;
                }
            }

            if (expected_flags != x86.get_flags()) {
                fflush(stdout);
                fprintf(stderr, "Flags do not match: has '");
                x86.get_flags().print(stderr);
                fprintf(stderr, "' expected '");
                expected_flags.print(stderr);
                fprintf(stderr, "'\n");
                return Errc::SimulatingError;
            }
            break;
        }

        const char output_template[] = "XX: 0x";
        if (expected_line.s.size() < std::size(output_template) - 1 + 4) break;

        auto expected_value = strtol(expected_line.s.data() + std::size(output_template) - 1, nullptr, 16);
        if (expected_value < 0 || expected_value > std::numeric_limits<u16>::max()) {
            fflush(stdout);
            fprintf(stderr, "Register value parsing failed on line ");
            fprintf(stderr, "%.*s\n", (int)expected_line.s.size(), expected_line.s.data());
            return Errc::InvalidExpectedOutputFile;
        }

        char expected_reg[3] = { expected_line.s[0], expected_line.s[1], '\0' };
        u16 value = x86.get_ip();

        if (strcmp(expected_reg, "ip") != 0) {
            UNWRAP_OR(auto reg, lookup_register(expected_reg)) {
                fflush(stdout);
                fprintf(stderr, "Unknown register %s on line ", expected_reg);
                fprintf(stderr, "%.*s\n", (int)expected_line.s.size(), expected_line.s.data());
                return Errc::InvalidExpectedOutputFile;
            }

            value = x86.get(reg);
        }

        if ((u16)expected_value != value) {
            fflush(stdout);
            fprintf(stderr, "Register %s has unexpected value 0x%.4x (expected 0x%.4hx)\n", expected_reg, value, (u16)expected_value);
            ret = Errc::SimulatingError;
        }

        expected_line_end_i += expected_line.length;
    }

    return ret;
}

static error_code assemble_and_test_simulator(const std::string& filename) {
    UNWRAP_BARE(auto assembled_filename, assemble(filename));
    DEFER { unlink(assembled_filename.data()); };
    return test_simulator(assembled_filename, filename + ".txt");
}

static const char test_prefix[] = "../tests/";
static const char ce_test_prefix[] = "../computer_enhance/perfaware/";

static constexpr std::array disassembly_tests = {
    "direct_jmp_call_within_segment.asm",
};
static constexpr std::array ce_disassembly_tests = {
    "part1/listing_0040_challenge_movs",
    "part1/listing_0041_add_sub_cmp_jnz",
    "part1/listing_0042_completionist_decode",
};

static constexpr std::array simulator_tests = {
    "short_memory.asm",
};
static constexpr std::array ce_simulator_tests = {
    "part1/listing_0043_immediate_movs",
    "part1/listing_0044_register_movs",
    "part1/listing_0045_challenge_register_movs",
    "part1/listing_0046_add_sub_cmp",
    "part1/listing_0047_challenge_flags",
    "part1/listing_0049_conditional_jumps",
    "part1/listing_0050_challenge_jumps",
    "part1/listing_0051_memory_mov",
    "part1/listing_0052_memory_add_loop",
    "part1/listing_0053_add_loop_challenge",
};

static error_code run_tests() {
    std::string filename;

    for (auto test : disassembly_tests) {
        filename = test_prefix;
        filename += test;
        printf("\n");
        RET_IF(assemble_and_test_disassembler(filename.data()));
    }
    for (auto test : ce_disassembly_tests) {
        filename = ce_test_prefix;
        filename += test;
        printf("\n");
        RET_IF(test_disassembler(filename.data()));
    }

    printf("\nRunning simulator tests\n");
    {
        Intel8086 x86;
        x86.test_set_get();
    }
    for (auto test : simulator_tests) {
        filename = test_prefix;
        filename += test;
        RET_IF(assemble_and_test_simulator(filename));
    }
    for (auto test : ce_simulator_tests) {
        filename = ce_test_prefix;
        filename += test;
        RET_IF(test_simulator(filename, filename + ".txt"));
    }

    return {};
}

int main() {
    if (system("nasm --version > /dev/null")) {
        fprintf(stderr, "Tests require nasm assembler to be installed\n");
        return EXIT_FAILURE;
    }

    if (auto e = run_tests()) {
        fprintf(stderr, "Error while running tests: %s\n", e.message().data());
        return EXIT_FAILURE;
    }

    printf("\nAll tests passed\n");

    return 0;
}
