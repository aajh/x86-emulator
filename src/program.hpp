#pragma once

#include "common.hpp"
#include <cstdio>
#include <string>
#include <vector>

expected<std::vector<u8>, error_code> read_program(FILE* input_file);
expected<std::vector<u8>, error_code> read_program(const char* filename);

error_code disassemble_program(FILE* out, std::span<const u8> program, const char* filename = nullptr);
error_code disassemble_file(FILE* out, const char* filename);

expected<std::string, error_code> assemble_program_to_tmp(const char* filename);
error_code unlink_tmp_file(const std::string& tmp_filename);
