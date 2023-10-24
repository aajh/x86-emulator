#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <unistd.h>

#include "program.hpp"
#include "emulator.hpp"

static expected<std::string, error_code> read_file(const std::string& filename) {
    auto file = fopen(filename.data(), "rb");
    if (file == nullptr) return make_unexpected_errno();

    if (fseek(file, 0, SEEK_END)) return make_unexpected_errno();

    const auto ftell_result = ftell(file);
    if (ftell_result < 0) return make_unexpected_errno();
    const size_t size = ftell_result;

    if (fseek(file, 0, SEEK_SET)) return make_unexpected_errno();

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

static error_code test_disassembler(const std::string& filename) {
    UNWRAP_BARE(auto program, read_program(filename.data()));

    std::string disassembled_filename = "/tmp/x86-emulator.asm.XXXXXX";
    auto disassembled_fd = mkstemp(disassembled_filename.data());
    if (disassembled_fd == -1) return make_error_code_errno();
    DEFER { close(disassembled_fd); unlink(disassembled_filename.data()); };

    auto disassembled_file = fdopen(disassembled_fd, "wb");
    if (disassembled_file == nullptr) return make_error_code_errno();
    DEFER { fclose(disassembled_file); };

    printf("Disassembling %s to %s\n", filename.data(), disassembled_filename.data());
    RET_IF(disassemble_program(disassembled_file, program));
    fflush(disassembled_file);


    UNWRAP_BARE(auto reassembled_filename, assemble_program_to_tmp(disassembled_filename.data()));
    DEFER { (void)unlink_tmp_file(reassembled_filename); };

    UNWRAP_BARE(auto reassembled_program, read_program(reassembled_filename.data()));

    if (program.size() != reassembled_program.size()) {
        fflush(stdout);
        fprintf(stderr, "Reassembled program has different size (%lu, original %lu)\n", reassembled_program.size(), program.size());
        return Errc::ReassemblyError;
    }

    for (i32 i = 0; i < (i32)program.size(); ++i) {
        if (program[i] != reassembled_program[i]) {
            fflush(stdout);
            fprintf(stderr, "Reassembled program differs at position %d (0x%x, original 0x%x)\n", i, reassembled_program[i], program[i]);

            fprintf(stderr, "Reassembled bytes around the location are:");
            constexpr i32 window_size = 5;
            for (i32 j = -window_size; j < window_size + 1; ++j) {
                if (i + j < 0 || i + j >= (i32)reassembled_program.size()) continue;
                fprintf(stderr, " 0x%X", reassembled_program[i + j]);
            }
            fprintf(stderr, "\n");

            fprintf(stderr, "Original bytes around the location are:   ");
            for (i32 j = -window_size; j < window_size + 1; ++j) {
                if (i + j < 0 || i + j >= (i32)program.size()) continue;
                fprintf(stderr, " 0x%X", program[i + j]);
            }
            fprintf(stderr, "\n");

            return Errc::ReassemblyError;
        }
    }

    return {};
}

static error_code test_emulator(const std::string& program_filename, const std::string& expected_filename) {
    printf("Emulating program %s\n", program_filename.data());

    Intel8086 x86;
    RET_IF(x86.load_program(program_filename.data()));
    RET_IF(x86.run());

    UNWRAP_BARE(auto expected_output, read_file(expected_filename));

    constexpr std::string_view register_line = "Final registers:";
    auto search_i = expected_output.find(register_line);
    if (search_i == std::string::npos) {
        fflush(stdout);
        fprintf(stderr, "Didn't find the register line in the expected output file %s\n", expected_filename.data());
        return Errc::InvalidExpectedOutputFile;
    }
    search_i += register_line.size();

    error_code ret = {};
    auto expected_line_end_i = search_i;
    while (expected_line_end_i < expected_output.size()) {
        auto expected_line = read_line({ expected_output.data() + expected_line_end_i, expected_output.size() - expected_line_end_i }, true);

        constexpr std::string_view flags = "flags:";
        if (expected_line.s.starts_with(flags)) {
            auto expected_flags_string = expected_line.s.size() > flags.size() ? expected_line.s.substr(flags.size() + 1) : std::string_view{};

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
                return Errc::EmulationError;
            }
            break;
        }

        constexpr std::string_view output_template = "XX: 0x";
        if (expected_line.s.size() < output_template.size() + 4) break;

        constexpr int hex_base = 16;
        auto expected_value = strtol(expected_line.s.data() + output_template.size(), nullptr, hex_base);
        if (expected_value < 0 || expected_value > std::numeric_limits<u16>::max()) {
            fflush(stdout);
            fprintf(stderr, "Register value parsing failed on line ");
            fprintf(stderr, "%.*s\n", (int)expected_line.s.size(), expected_line.s.data());
            return Errc::InvalidExpectedOutputFile;
        }

        const std::string expected_reg(expected_line.s.substr(0, 2));
        u16 value = x86.get_ip();

        if (expected_reg != "ip") {
            auto reg = lookup_register(expected_reg.data());
            if (!reg) {
                fflush(stdout);
                fprintf(stderr, "Unknown register %s on line ", expected_reg.data());
                fprintf(stderr, "%.*s\n", (int)expected_line.s.size(), expected_line.s.data());
                return Errc::InvalidExpectedOutputFile;
            }

            value = x86.get(*reg);
        }

        if ((u16)expected_value != value) {
            fflush(stdout);
            fprintf(stderr, "Register %s has unexpected value 0x%.4x (expected 0x%.4hx)\n", expected_reg.data(), value, (u16)expected_value);
            ret = Errc::EmulationError;
        }

        expected_line_end_i += expected_line.length;
    }

    return ret;
}

static error_code assemble_and_test_disassembler(const std::string& filename) {
    UNWRAP_BARE(auto assembled_filename, assemble_program_to_tmp(filename.data()));
    DEFER { (void)unlink_tmp_file(assembled_filename); };
    return test_disassembler(assembled_filename);
}

static error_code assemble_and_test_emulator(const std::string& filename) {
    UNWRAP_BARE(auto assembled_filename, assemble_program_to_tmp(filename.data()));
    DEFER { (void)unlink_tmp_file(assembled_filename); };
    return test_emulator(assembled_filename, filename + ".txt");
}

static constexpr std::string_view test_prefix = "../tests/";
static constexpr std::string_view ce_test_prefix = "../computer_enhance/perfaware/";

static constexpr std::array disassembly_tests = {
    "direct_jmp_call_within_segment.asm",
};
static constexpr std::array ce_disassembly_tests = {
    "part1/listing_0040_challenge_movs",
    "part1/listing_0041_add_sub_cmp_jnz",
    "part1/listing_0042_completionist_decode",
};

static constexpr std::array emulator_tests = {
    "short_memory.asm",
    "function_call.asm",
    "recursive_call.asm",
};
static constexpr std::array ce_emulator_tests = {
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
        RET_IF(assemble_and_test_disassembler(filename));
    }
    for (auto test : ce_disassembly_tests) {
        filename = ce_test_prefix;
        filename += test;
        printf("\n");
        RET_IF(test_disassembler(filename));
    }

    printf("\nRunning emulator tests\n");
    {
        Intel8086 x86;
        x86.test_set_get();
    }
    for (auto test : emulator_tests) {
        filename = test_prefix;
        filename += test;
        RET_IF(assemble_and_test_emulator(filename));
    }
    for (auto test : ce_emulator_tests) {
        filename = ce_test_prefix;
        filename += test;
        RET_IF(test_emulator(filename, filename + ".txt"));
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
