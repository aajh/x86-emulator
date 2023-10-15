#include "common.hpp"

namespace {
    struct ErrCategory : std::error_category {
        const char* name() const noexcept override {
            return "x64-sim";
        };
        std::string message(int ev) const override {
            using enum Errc;
            switch (static_cast<Errc>(ev)) {
                case EndOfFile:
                    return "end of file";
                case UnknownInstruction:
                    return "unknown instruction";
                case ReassemblyError:
                    return "reassembly error";
                case ReassemblyFailed:
                    return "reassembly failed";
            }
            return "(unrecognized error)";
        };
    } err_category;
}

std::error_code make_error_code(Errc e) {
    return {static_cast<int>(e), err_category};
}
