#include "program.hpp"
#include <new>

#include "instruction.hpp"

expected<Program, error_code> read_program(FILE* input_file) {
    Program program;

    RET_ERRNO(fseek(input_file, 0, SEEK_END));

    const auto ftell_result = ftell(input_file);
    RET_ERRNO(ftell_result < 0);
    program.size = ftell_result;

    RET_ERRNO(fseek(input_file, 0, SEEK_SET));

    program.data = new(std::nothrow) u8[program.size];
    if (program.data == nullptr) return make_unexpected(std::errc::not_enough_memory);

    if (fread(program.data, 1, program.size, input_file) != program.size) {
        delete[] program.data;
        if (feof(input_file)) return unexpected(Errc::EndOfFile);
        return make_unexpected_errno();
    }

    return program;
}

expected<Program, error_code> read_program(const char* filename) {
    FILE* input_file = fopen(filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Couldn't open file %s\n", filename);
        return make_unexpected_errno();
    }
    DEFER { fclose(input_file); };

    return read_program(input_file);
}

error_code disassemble_program(FILE* out, const Program& program, const char* filename) {
    if (filename != nullptr) fprintf(out, "; %s disassembly:\n", filename);
    fprintf(out, "bits 16\n\n");

    u32 i = 0;
    while (i < program.size) {
        UNWRAP_OR(auto instruction, decode_instruction_at(program, i)) {
            fflush(stdout);
            fprintf(stderr, "Unknown instruction at location %u (first byte 0x%X)\n", i, program.data[i]);
            return Errc::UnknownInstruction;
        }

        output_instruction_assembly(out, instruction);
        i += instruction.size;
    }

    return {};
}

error_code disassemble_file(FILE* out, const char* filename) {
    UNWRAP_BARE(auto program, read_program(filename));
    DEFER { delete[] program.data; };

    return disassemble_program(out, program, filename);
}

