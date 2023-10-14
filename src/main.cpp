#include "common.hpp"
#include <new>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/errno.h>
#include <unistd.h>

#include "instruction.hpp"

[[noreturn]] void print_errno_and_exit(const char* what = nullptr) {
    fflush(stdout);
    fprintf(stderr, "%s: %s\n", what ? what : "error", strerror(errno));
    exit(EXIT_FAILURE);
}

Program read_program(FILE* input_file) {
    Program program;

    if (fseek(input_file, 0, SEEK_END)) {
        print_errno_and_exit("fseek");
    }
    const auto ftell_result = ftell(input_file);
    if (ftell_result < 0) {
        print_errno_and_exit("ftell");
    }
    program.size = ftell_result;
    if (fseek(input_file, 0, SEEK_SET)) {
        print_errno_and_exit("fseek");
    }

    program.data = new(std::nothrow) u8[program.size];
    if (program.data == nullptr) {
        fprintf(stderr, "allocation failed\n");
        exit(EXIT_FAILURE);
    }

    if (fread(program.data, 1, program.size, input_file) != program.size) {
        delete[] program.data;
        print_errno_and_exit("fread");
    }

    return program;
}

Program read_program(const char* filename) {

    FILE* input_file = fopen(filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Couldn't open file ");
        print_errno_and_exit(filename);
    }
    defer { fclose(input_file); };

    return read_program(input_file);
}

void disassemble_program(FILE* out, const Program& program) {
    fprintf(out, "; disassembly:\n");
    fprintf(out, "bits 16\n\n");

    u64 i = 0;
    while (i < program.size) {
        auto instruction = decode_instruction_at(program, i);
        if (instruction.type == Instruction::Type::None) {
            fprintf(stderr, "Unknown instruction at location %llu (first byte 0x%X)\n", i, program.data[i]);
            exit(EXIT_FAILURE);
        }

        output_instruction_assembly(out, instruction);
        i += instruction.size;
    }
}

void disassemble_file(FILE* out, const char* filename) {
    auto program = read_program(filename);
    defer { delete[] program.data; };

    disassemble_program(out, program);
}

void test(const char* filename) {
    auto program = read_program(filename);
    defer { delete[] program.data; };

    std::string disassembled_filename = "/tmp/x86-sim.asm.XXXXXX";
    auto disassembled_fd = mkstemp(disassembled_filename.data());
    if (disassembled_fd == -1) {
        print_errno_and_exit("mkstemp");
    }
    defer { close(disassembled_fd); };

    auto disassembled_file = fdopen(disassembled_fd, "wb");
    if (disassembled_file == nullptr) {
        print_errno_and_exit("fdopen");
    }
    defer { fclose(disassembled_file); };

    printf("\nDisassembling %s to %s\n", filename, disassembled_filename.data());
    disassemble_program(disassembled_file, program);
    fflush(disassembled_file);


    std::string reassembled_filename = "/tmp/x86-sim_nasm.out.XXXXXX";
    auto reassembled_fd = mkstemp(reassembled_filename.data());
    if (reassembled_fd == -1) {
        print_errno_and_exit("mkstemp");
    }
    defer { close(reassembled_fd); };

    printf("Reassembling %s to %s\n", disassembled_filename.data(), reassembled_filename.data());
    {
        std::string command = "nasm -o ";
        command += reassembled_filename;
        command += " ";
        command += disassembled_filename;

        printf("Running shell command: %s\n", command.data());
        if (system(command.data())) {
            fprintf(stderr, "Reassembly failed\n");
            exit(EXIT_FAILURE);
        }
    }

    auto reassembled_file = fdopen(reassembled_fd, "rb");
    if (reassembled_file == nullptr) {
        print_errno_and_exit("fdopen");
    }
    defer { fclose(reassembled_file); };

    auto reassembled_program = read_program(reassembled_file);
    defer { delete[] reassembled_program.data; };

    if (program.size != reassembled_program.size) {
        fflush(stdout);
        fprintf(stderr, "Reassembled program has different size (%llu, original %llu)\n", reassembled_program.size, program.size);
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < program.size; ++i) {
        if (program.data[i] != reassembled_program.data[i]) {
            fflush(stdout);
            fprintf(stderr, "Reassembled program differs at position %zu (0x%X, original 0x%X)\n", i, reassembled_program.data[i], program.data[i]);
            exit(EXIT_FAILURE);
        }
    }

    printf("Test passed for %s\n", filename);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        auto name = argc > 0 ? argv[0] : "a";
        fprintf(stderr, "usage: %s <input_file_name> OR %s -test\n", name, name);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-test") == 0) {
        if (system("nasm --version")) {
            fprintf(stderr, "Tests require nasm assembler to be installed\n");
            return EXIT_FAILURE;
        }

        test("../computer_enhance/perfaware/part1/listing_0041_add_sub_cmp_jnz");
    } else {
        disassemble_file(stdout, argv[1]);
    }

    return 0;
}