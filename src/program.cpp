#include "program.hpp"
#include <cstdlib>
#include <unistd.h>
#include <fmt/core.h>

#include "instruction.hpp"

expected<std::vector<u8>, error_code> read_program(FILE* input_file) {
    if (fseek(input_file, 0, SEEK_END)) return make_unexpected_errno();

    const auto ftell_result = ftell(input_file);
    if (ftell_result < 0) return make_unexpected_errno();
    if (fseek(input_file, 0, SEEK_SET)) return make_unexpected_errno();

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
        fmt::print(stderr, "Couldn't open file {}\n", filename);
        return make_unexpected_errno();
    }
    DEFER { fclose(input_file); };

    return read_program(input_file);
}

error_code disassemble_program(FILE* out, std::span<const u8> program, const char* filename, bool estimate_cycles) {
    if (filename != nullptr) fmt::print(out, "; {} disassembly:\n", filename);
    fmt::print(out, "bits 16\n\n");

    u32 cycles = 0;
    u32 i = 0;
    while (i < program.size()) {
        auto instruction = Instruction::decode_at(program, i);
        if (!instruction) {
            fflush(stdout);
            fmt::print(stderr, "Unknown instruction at location {} (first byte {:#x})\n", i, program[i]);
            return Errc::UnknownInstruction;
        }

        fmt::print(out, "{}", *instruction);
        if (estimate_cycles) {
            fmt::print(out, " ; ");
            cycles += instruction->estimate_cycles(cycles, out);
        }
        fmt::print(out, "\n");

        i += instruction->size;
    }

    return {};
}

error_code disassemble_file(FILE* out, const char* filename, bool estimate_cycles) {
    UNWRAP_BARE(auto program, read_program(filename));
    return disassemble_program(out, program, filename, estimate_cycles);
}

expected<std::string, error_code> assemble_program_to_tmp(const char* filename) {
    std::string assembled_filename = "/tmp/x86-emulator.nasm.out.XXXXXX";
    auto assembled_fd = mkstemp(assembled_filename.data());
    if (assembled_fd == -1) return make_unexpected_errno();
    if (close(assembled_fd)) {
        auto e = make_unexpected_errno();
        unlink(assembled_filename.data());
        return e;
    }

    fmt::print("Assembling {} to {}\n", filename, assembled_filename);
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
    if (unlink(tmp_filename.data())) return make_error_code_errno();
    return {};
}
