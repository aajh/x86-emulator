#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "program.hpp"
#include "simulator.hpp"

int print_instructions_for_help(const char* name) {
    fprintf(stderr, "%s: type '%s --help' for help.\n", name, name);
    return EXIT_FAILURE;
}

int main(int argc, char** argv) {
    auto name = argc > 0 ? argv[0] : "x86-sim";
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("usage: %s  [options] <input_program_file_name>\n", name);
        printf(" -s, --simulate   \tSimulate the program\n");
        printf(" -d, --disassemble\tDisassemble the program\n");
        return EXIT_SUCCESS;
    }
    if (argc != 3) {
        return print_instructions_for_help(name);
    }

    auto option = argv[1];
    auto filename = argv[2];

    if (strcmp(option, "-s") == 0 || strcmp(option, "--simulate") == 0) {
        auto x86 = Intel8086::read_program_from_file(filename);
        if (!x86) {
            fprintf(stderr, "Error while reading file %s: %s\n", filename, x86.error().message().data());
            return EXIT_FAILURE;
        }
        x86->test_set_get(); // TODO: Move to test
        if (auto e = x86->simulate()) {
            fprintf(stderr, "Error while simulating file %s: %s\n", filename, e.message().data());
            return EXIT_FAILURE;
        }
    } else if (strcmp(option, "-d") == 0 || strcmp(option, "--disassemble") == 0) {
        if (auto e = disassemble_file(stdout, filename)) {
            fprintf(stderr, "Error while disassembling file %s: %s\n", filename, e.message().data());
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "%s: option %s is unknown\n", name, option);
        return print_instructions_for_help(name);
    }

    return 0;
}
