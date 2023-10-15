#pragma once

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <system_error>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

#ifndef defer
struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define VARGS_(_10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define VARGS(...) VARGS_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define RET_IF(x) if (auto e = (x)) { return e; }
#define RET_EC(x) x; if (ec) return ec;
#define RET_ERRNO(x) if (x) { return make_error_code(); }

#define SET_ERRNO_2(x, ec) if (x) { ec = make_error_code(); return {}; }
#define SET_ERRNO_1(x) SET_ERRNO_2(x, ec)
#define SET_ERRNO(...) CONCAT(SET_ERRNO_, VARGS(__VA_ARGS__))(__VA_ARGS__)

static inline std::error_code make_error_code(int errno_value = errno) {
    return std::make_error_code(std::errc(errno_value));
}

enum class Errc {
    EndOfFile = 1,
    UnknownInstruction,
    ReassemblyError,
    ReassemblyFailed,
};
namespace std {
    template<> struct is_error_code_enum<Errc> : true_type {};
}
std::error_code make_error_code(Errc);

struct Program {
    u64 size = 0;
    u8* data = nullptr;
};
