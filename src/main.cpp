#include "common.hpp"
#include <new>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/errno.h>
#include <unistd.h>

#include "instruction.hpp"

static Program read_program(FILE* input_file, std::error_code& ec) {
    Program program;

    SET_ERRNO(fseek(input_file, 0, SEEK_END));
    SET_ERRNO(fseek(input_file, 0, SEEK_END));

    const auto ftell_result = ftell(input_file);
    SET_ERRNO(ftell_result < 0);
    program.size = ftell_result;

    SET_ERRNO(fseek(input_file, 0, SEEK_SET));

    program.data = new(std::nothrow) u8[program.size];
    if (program.data == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return {};
    }

    if (fread(program.data, 1, program.size, input_file) != program.size) {
        delete[] program.data;
        if (feof(input_file)) {
            ec = Errc::EndOfFile;
        } else {
            ec = make_error_code();
        }
        return {};
    }

    return program;
}

static Program read_program(const char* filename, std::error_code& ec) {
    FILE* input_file = fopen(filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Couldn't open file %s\n", filename);
        ec = make_error_code();
        return {};
    }
    defer { fclose(input_file); };

    return read_program(input_file, ec);
}

static std::error_code disassemble_program(FILE* out, const Program& program) {
    fprintf(out, "; disassembly:\n");
    fprintf(out, "bits 16\n\n");

    u64 i = 0;
    while (i < program.size) {
        auto instruction = decode_instruction_at(program, i);
        if (instruction.type == Instruction::Type::None) {
            fflush(stdout);
            fprintf(stderr, "Unknown instruction at location %llu (first byte 0x%X)\n", i, program.data[i]);
            return Errc::UnknownInstruction;
        }

        output_instruction_assembly(out, instruction);
        i += instruction.size;
    }

    return {};
}

static std::error_code disassemble_file(FILE* out, const char* filename) {
    std::error_code ec;
    RET_EC(auto program = read_program(filename, ec));
    defer { delete[] program.data; };

    return disassemble_program(out, program);
}


static std::error_code test_disassembler(const char* filename) {
    std::error_code ec;

    RET_EC(auto program = read_program(filename, ec));
    defer { delete[] program.data; };

    std::string disassembled_filename = "/tmp/x86-sim.asm.XXXXXX";
    auto disassembled_fd = mkstemp(disassembled_filename.data());
    RET_ERRNO(disassembled_fd == -1);
    defer { close(disassembled_fd); unlink(disassembled_filename.data()); };

    auto disassembled_file = fdopen(disassembled_fd, "wb");
    RET_ERRNO(disassembled_file == nullptr);
    defer { fclose(disassembled_file); };

    printf("\nDisassembling %s to %s\n", filename, disassembled_filename.data());
    RET_IF(disassemble_program(disassembled_file, program));
    fflush(disassembled_file);


    std::string reassembled_filename = "/tmp/x86-sim_nasm.out.XXXXXX";
    auto reassembled_fd = mkstemp(reassembled_filename.data());
    RET_ERRNO(reassembled_fd == -1);
    defer { close(reassembled_fd); unlink(reassembled_filename.data()); };

    printf("Reassembling %s to %s\n", disassembled_filename.data(), reassembled_filename.data());
    {
        std::string command = "nasm -o ";
        command += reassembled_filename;
        command += " ";
        command += disassembled_filename;

        if (system(command.data())) {
            return Errc::ReassemblyError;
        }
    }


    auto reassembled_file = fdopen(reassembled_fd, "rb");
    RET_ERRNO(reassembled_file == nullptr);
    defer { fclose(reassembled_file); };

    RET_EC(auto reassembled_program = read_program(reassembled_file, ec));
    defer { delete[] reassembled_program.data; };

    if (program.size != reassembled_program.size) {
        fflush(stdout);
        fprintf(stderr, "Reassembled program has different size (%llu, original %llu)\n", reassembled_program.size, program.size);
        return Errc::ReassemblyFailed;
    }

    for (size_t i = 0; i < program.size; ++i) {
        if (program.data[i] != reassembled_program.data[i]) {
            fflush(stdout);
            fprintf(stderr, "Reassembled program differs at position %zu (0x%X, original 0x%X)\n", i, reassembled_program.data[i], program.data[i]);
            return Errc::ReassemblyFailed;
        }
    }

    return {};
}

const char test_prefix[] = "../computer_enhance/perfaware/";
const char* tests[] = {
    "part1/listing_0041_add_sub_cmp_jnz",
    "part1/listing_0042_completionist_decode",
};

static std::error_code run_tests() {
    std::string filename;
    for (auto test : tests) {
        filename = test_prefix;
        filename += test;
        RET_IF(test_disassembler(filename.data()));
    }
    return {};
}


int main(int argc, char** argv) {
    if (argc != 2) {
        auto name = argc > 0 ? argv[0] : "a";
        fprintf(stderr, "usage: %s <input_file_name> OR %s -test\n", name, name);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-test") == 0) {
        if (system("nasm --version > /dev/null")) {
            fprintf(stderr, "Tests require nasm assembler to be installed\n");
            return EXIT_FAILURE;
        }
        if (auto e = run_tests()) {
            fprintf(stderr, "Error while running tests: %s\n", e.message().data());
            return EXIT_FAILURE;
        }
    } else {
        if (auto e = disassemble_file(stdout, argv[1])) {
            fprintf(stderr, "Error while disassembling file: %s\n", e.message().data());
            return EXIT_FAILURE;
        }
    }

    return 0;
}
