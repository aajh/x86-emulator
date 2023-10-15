#include "common.hpp"
#include <cstdio>
#include <cstdlib>

#include "program.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <input_file_name>", argc > 0 ? argv[0] : "a");
        return EXIT_FAILURE;
    }

    if (auto e = disassemble_file(stdout, argv[1])) {
        fprintf(stderr, "Error while disassembling file: %s\n", e.message().data());
        return EXIT_FAILURE;
    }

    return 0;
}
