#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/core.h>

#include "program.hpp"
#include "emulator.hpp"

static int print_instructions_for_help(const char* name) {
    fmt::print(stderr, "{0}: type '{0} --help ' for help.\n", name);
    return EXIT_FAILURE;
}

static int print_requires_parameter(const char* name, const char* option) {
    fmt::print(stderr, "{}: option {}: requires parameter\n", name, option);
    return print_instructions_for_help(name);
}

enum class Option {
    None,
    Disassemble,
    Execute,
};

int main(int argc, char** argv) {
    using enum Option;

    auto name = argc > 0 ? argv[0] : "x86-emulator";
    auto option = None;
    std::string filename;
    bool dump_memory = false;
    bool estimate_cycles = false;

    for (i32 i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            fmt::print("usage: {}  [options...]\n", name);
            fmt::print(" -d, --disassemble <program>\tDisassemble the program\n");
            fmt::print(" -e, --execute <program>    \tExecute the program\n");
            fmt::print(" -D, --dump                 \tDump the memory after executing the program\n");
            fmt::print(" -C, --estimate-cycles      \tEstimate the number of cycles that instructions take\n");
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
        } else if (strcmp(argv[i], "-C") == 0 || strcmp(argv[i], "--estimate-cycles") == 0) {
            estimate_cycles = true;
        } else {
            fmt::print(stderr, "{}: option {}: is unknown\n", name, argv[i]);
            return print_instructions_for_help(name);
        }
    }

    bool assemble = filename.ends_with(".asm");
    if (assemble) {
        fmt::print("; ");
        auto assembled_filename = assemble_program_to_tmp(filename.data());
        if (!assembled_filename) {
            auto e = assembled_filename.error();
            fmt::print(stderr, "Error while assembling file {} with nasm: {}\n", filename, e.message());
            return EXIT_FAILURE;
        }
        filename = *assembled_filename;
    }
    DEFER { if (assemble) (void)unlink_tmp_file(filename); };

    switch (option) {
        case Disassemble:
            if (auto e = disassemble_file(stdout, filename.data(), estimate_cycles)) {
                fmt::print(stderr, "Error while disassembling file {}: {}\n", filename, e.message());
                return EXIT_FAILURE;
            }
            break;
        case Execute: {
            Intel8086 x86;
            if (auto e = x86.load_program(filename.data())) {
                fmt::print(stderr, "Error while reading file {}: {}\n", filename, e.message());
                return EXIT_FAILURE;
            }
            if (auto e = x86.run(estimate_cycles)) {
                fmt::print(stderr, "Error while executing file {}: {}\n", filename, e.message());
                return EXIT_FAILURE;
            }
            if (dump_memory) {
                if (auto e = x86.dump_memory("x86-emulator.memory.data")) {
                    fmt::print(stderr, "Error while dumping the memory: {}\n", e.message());
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
