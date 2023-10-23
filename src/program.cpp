#include "program.hpp"
#include <cstdlib>
#include <unistd.h>

#include "instruction.hpp"

expected<std::vector<u8>, error_code> read_program(FILE* input_file) {
    RET_ERRNO(fseek(input_file, 0, SEEK_END));

    const auto ftell_result = ftell(input_file);
    RET_ERRNO(ftell_result < 0);
    RET_ERRNO(fseek(input_file, 0, SEEK_SET));

    std::vector<u8> program(ftell_result);

    if (fread(program.data(), 1, program.size(), input_file) != program.size()) {
        if (feof(input_file)) return unexpected(Errc::EndOfFile);
        return make_unexpected_errno();
    }

    return program;
}

expected<std::vector<u8>, error_code> read_program(const char* filename) {
    FILE* input_file = fopen(filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Couldn't open file %s\n", filename);
        return make_unexpected_errno();
    }
    DEFER { fclose(input_file); };

    return read_program(input_file);
}

error_code disassemble_program(FILE* out, std::span<const u8> program, const char* filename) {
    if (filename != nullptr) fprintf(out, "; %s disassembly:\n", filename);
    fprintf(out, "bits 16\n\n");

    u32 i = 0;
    while (i < program.size()) {
        UNWRAP_OR(auto instruction, Instruction::decode_at(program, i)) {
            fflush(stdout);
            fprintf(stderr, "Unknown instruction at location %u (first byte 0x%x)\n", i, program[i]);
            return Errc::UnknownInstruction;
        }

        instruction.print_assembly(out);
        fprintf(out, "\n");
        i += instruction.size;
    }

    return {};
}

error_code disassemble_file(FILE* out, const char* filename) {
    UNWRAP_BARE(auto program, read_program(filename));
    return disassemble_program(out, program, filename);
}

expected<std::string, error_code> assemble_program_to_tmp(const char* filename) {
    std::string assembled_filename = "/tmp/x86-emulator.nasm.out.XXXXXX";
    auto assembled_fd = mkstemp(assembled_filename.data());
    RET_ERRNO(assembled_fd == -1);
    if (close(assembled_fd)) {
        auto e = make_unexpected_errno();
        unlink(assembled_filename.data());
        return e;
    }

    printf("Assembling %s to %s\n", filename, assembled_filename.data());
    {
        std::string command = "nasm -o ";
        command += assembled_filename;
        command += " ";
        command += filename;

        if (system(command.data())) {
            unlink(assembled_filename.data());
            return unexpected(Errc::ReassemblyError);
        }
    }

    return assembled_filename;
}

error_code unlink_tmp_file(const std::string& tmp_filename) {
    RET_BARE_ERRNO(unlink(tmp_filename.data()));
    return {};
}
