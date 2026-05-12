#pragma once

#include <cstdint>
#include <span>

namespace affine {

template <class Integer>
inline void fwht(std::span<Integer> values)
{
    const std::size_t n = values.size();

    for (std::size_t len = 1; len < n; len <<= 1) {
        for (std::size_t base = 0; base < n; base += (len << 1)) {
            for (std::size_t offset = 0; offset < len; ++offset) {
                const Integer a = values[base + offset];
                const Integer b = values[base + offset + len];
                values[base + offset] = a + b;
                values[base + offset + len] = a - b;
            }
        }
    }
}

} // namespace affine
