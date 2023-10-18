#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "program.hpp"

static error_code test_disassembler(const char* filename) {
    UNWRAP_BARE(auto program, read_program(filename));
    DEFER { delete[] program.data; };

    std::string disassembled_filename = "/tmp/x86-sim.asm.XXXXXX";
    auto disassembled_fd = mkstemp(disassembled_filename.data());
    RET_BARE_ERRNO(disassembled_fd == -1);
    DEFER { close(disassembled_fd); unlink(disassembled_filename.data()); };

    auto disassembled_file = fdopen(disassembled_fd, "wb");
    RET_BARE_ERRNO(disassembled_file == nullptr);
    DEFER { fclose(disassembled_file); };

    printf("Disassembling %s to %s\n", filename, disassembled_filename.data());
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
            fprintf(stderr, "Reassembled program differs at position %zu (0x%X, original 0x%X)\n", i, reassembled_program.data[i], program.data[i]);

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

static error_code assemble_and_test_disassembler(const char* filename) {
    std::string assembled_filename = "/tmp/x86-sim_nasm.out.XXXXXX";
    auto assembled_fd = mkstemp(assembled_filename.data());
    RET_BARE_ERRNO(assembled_fd == -1);
    DEFER { close(assembled_fd); unlink(assembled_filename.data()); };

    printf("Assembling %s to %s\n", filename, assembled_filename.data());
    {
        std::string command = "nasm -o ";
        command += assembled_filename;
        command += " ";
        command += filename;

        if (system(command.data())) return Errc::ReassemblyError;
    }

    return test_disassembler(assembled_filename.data());
}

static const char test_prefix[] = "../tests/";
static constexpr std::array tests = {
    "direct_jmp_call_within_segment.asm",
};

static const char ce_test_prefix[] = "../computer_enhance/perfaware/";
static constexpr std::array ce_tests = {
    "part1/listing_0040_challenge_movs",
    "part1/listing_0041_add_sub_cmp_jnz",
    "part1/listing_0042_completionist_decode",
};

static error_code run_tests() {
    std::string filename;
    for (auto test : tests) {
        filename = test_prefix;
        filename += test;
        printf("\n");
        RET_IF(assemble_and_test_disassembler(filename.data()));
    }
    for (auto test : ce_tests) {
        filename = ce_test_prefix;
        filename += test;
        printf("\n");
        RET_IF(test_disassembler(filename.data()));
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

    return 0;
}
