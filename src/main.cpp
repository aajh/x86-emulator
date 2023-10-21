#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "program.hpp"
#include "simulator.hpp"

static int print_instructions_for_help(const char* name) {
    fprintf(stderr, "%s: type '%s --help' for help.\n", name, name);
    return EXIT_FAILURE;
}

static int print_requires_parameter(const char* name, const char* option) {
    fprintf(stderr, "%s: option %s: requires parameter\n", name, option);
    return print_instructions_for_help(name);
}

enum class Option {
    None,
    Disassemble,
    Execute,
};

int main(int argc, char** argv) {
    using enum Option;

    auto name = argc > 0 ? argv[0] : "x86-sim";
    auto option = None;
    std::string filename;
    bool dump_memory = false;

    for (i32 i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: %s  [options...]\n", name);
            printf(" -d, --disassemble <program>\tDisassemble the program\n");
            printf(" -e, --execute <program>    \tExecute the program\n");
            printf(" -D, --dump                 \tDump the memory after executing the program\n");
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--disassemble") == 0) {
            option = Disassemble;
            if (i + 1 >= argc || argv[i + 1][0] == '-') return print_requires_parameter(name, argv[i]);
            ++i;
            filename = argv[i];
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--execute") == 0) {
            option = Execute;
            if (i + 1 >= argc || argv[i + 1][0] == '-') return print_requires_parameter(name, argv[i]);
            ++i;
            filename = argv[i];
        } else if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--dump") == 0) {
            dump_memory = true;
        } else {
            fprintf(stderr, "%s: option %s: is unknown\n", name, argv[i]);
            return print_instructions_for_help(name);
        }
    }

    bool assemble = filename.ends_with(".asm");
    if (assemble) {
        printf("; ");
        auto assembled_filename = assemble_program_to_tmp(filename.data());
        if (!assembled_filename) {
            auto e = assembled_filename.error();
            fprintf(stderr, "Error while assembling file %s with nasm: %s\n", filename.data(), e.message().data());
            return EXIT_FAILURE;
        }
        filename = *assembled_filename;
    }
    DEFER { if (assemble) unlink_tmp_file(filename); };

    switch (option) {
        case Disassemble:
            if (auto e = disassemble_file(stdout, filename.data())) {
                fprintf(stderr, "Error while disassembling file %s: %s\n", filename.data(), e.message().data());
                return EXIT_FAILURE;
            }
            break;
        case Execute: {
            Intel8086 x86;
            if (auto e = x86.load_program(filename.data())) {
                fprintf(stderr, "Error while reading file %s: %s\n", filename.data(), e.message().data());
                return EXIT_FAILURE;
            }
            if (auto e = x86.run()) {
                fprintf(stderr, "Error while executing file %s: %s\n", filename.data(), e.message().data());
                return EXIT_FAILURE;
            }
            if (dump_memory) {
                if (auto e = x86.dump_memory("x86-sim.memory.data")) {
                    fprintf(stderr, "Error while dumping the memory: %s\n", e.message().data());
                    return EXIT_FAILURE;
                }
            }
            break;
        }
        case None:
            return print_instructions_for_help(name);
    }

    return 0;
}
