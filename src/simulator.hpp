#pragma once

#include "common.hpp"

struct Program;

error_code simulate_program(const Program& program);
error_code simulate_file(const char* filename);
