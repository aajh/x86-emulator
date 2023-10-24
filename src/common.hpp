// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#pragma once

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <optional>
#include <span>
#include <system_error>
#include <type_traits>

#if __has_include("expected")
#include <expected>
using std::expected;
using std::unexpected;
#else
#include <tl/expected.hpp>
using tl::expected;
using tl::unexpected;
#endif

using std::error_code;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct DeferDummy {};
template <class F> struct Deferrer { F f; ~Deferrer() { f(); } }; // NOLINT(cppcoreguidelines-special-member-functions)
template <class F> Deferrer<F> operator*(DeferDummy, F f) { return {f}; }
#define DEFER_IMPL_(LINE) zz_defer##LINE
#define DEFER_IMPL(LINE) DEFER_IMPL_(LINE)
#define DEFER auto DEFER_IMPL(__LINE__) = DeferDummy{} *[&]() // NOLINT(bugprone-macro-parentheses)

#define RET_IF(x) if (auto e = (x)) { return e; }

#define OPEN_PAREN (
#define CLOSE_PAREN )

#define UNWRAP_WRAPPED_VAR(LINE) zz_variable_wrapped_##LINE
#define UNWRAP_BASE_IMPL(variable_decl, expression, LINE, error_wrapper_start, error_wrapper_end)\
    auto UNWRAP_WRAPPED_VAR(LINE) = expression;\
    if (!UNWRAP_WRAPPED_VAR(LINE)) return error_wrapper_start UNWRAP_WRAPPED_VAR(LINE).error() error_wrapper_end; /* NOLINT(bugprone-macro-parentheses) */\
    variable_decl = std::move(*UNWRAP_WRAPPED_VAR(LINE)); /* NOLINT(bugprone-macro-parentheses) */

#define UNWRAP_IMPL(v, e, LINE) UNWRAP_BASE_IMPL(v, e, LINE, unexpected OPEN_PAREN, CLOSE_PAREN)
#define UNWRAP(variable_decl, expression) UNWRAP_IMPL(variable_decl, expression, __LINE__)

#define UNWRAP_BARE_IMPL(v, e, LINE) UNWRAP_BASE_IMPL(v, e, LINE, , )
#define UNWRAP_BARE(variable_decl, expression) UNWRAP_BARE_IMPL(variable_decl, expression, __LINE__)

static inline error_code make_error_code_errno(int errno_value = errno) {
    return std::make_error_code(std::errc(errno_value));
}
static inline unexpected<error_code> make_unexpected_errno(int errno_value = errno) {
    return unexpected(std::make_error_code(std::errc(errno_value)));
}
static inline unexpected<error_code> make_unexpected(std::errc e) {
    return unexpected(std::make_error_code(e));
}

enum class Errc {
    EndOfFile = 1,
    UnknownInstruction,
    ReassemblyError,
    InvalidExpectedOutputFile,
    EmulationError,
};
namespace std {
    template<> struct is_error_code_enum<Errc> : true_type {};
}
error_code make_error_code(Errc);

#ifdef TESTING
#ifndef NDEBUG
#define test_assert(a, b) assert((a) == (b))
#else
#define test_assert(a, b) if ((a) != (b)) exit(EXIT_FAILURE);
#endif
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
