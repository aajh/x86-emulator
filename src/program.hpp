#pragma once

#include "common.hpp"
#include <cstdio>

struct Program {
    u8* data = nullptr;
    u32 size = 0;
};

expected<Program, error_code> read_program(FILE* input_file);
expected<Program, error_code> read_program(const char* filename);

error_code disassemble_program(FILE* out, const Program& program, const char* filename = nullptr);
error_code disassemble_file(FILE* out, const char* filename);
