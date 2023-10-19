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
                case InvalidOutputFile:
                    return "invalid output file";
                case InvalidExpectedOutputFile:
                    return "invalid expected output file";
                case SimulatingError:
                    return "simulating error";
            }
            return "(unrecognized error)";
        };
    } err_category;
}

error_code make_error_code(Errc e) {
    return {static_cast<int>(e), err_category};
}
