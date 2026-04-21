#pragma once

#include <cstdlib>
#if __has_include(<utility>)
#include <utility>
#endif

[[noreturn]] inline void wrapped_unreachable() noexcept 
{
#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
    std::unreachable();
#elif defined(_MSC_VER)
    __assume(false);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#else
    std::abort();
#endif
}