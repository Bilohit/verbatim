#pragma once
#include <cstdint>

namespace vb
{
// Integer direction bias (docs/04-seed-grammar.md section 3). u in 0..65535, e in -3..3.
inline std::uint32_t bias (std::uint32_t u, int e)
{
    if (e > 0)
    {
        const std::uint64_t v0 = 65535u - u;
        std::uint64_t v = v0;
        for (int i = 0; i < e; ++i)
            v = (v * v0) / 65535u;
        return 65535u - static_cast<std::uint32_t> (v);
    }
    if (e < 0)
    {
        const std::uint64_t u0 = u;
        std::uint64_t x = u0;
        for (int i = 0; i < -e; ++i)
            x = (x * u0) / 65535u;
        return static_cast<std::uint32_t> (x);
    }
    return u;
}
} // namespace vb
