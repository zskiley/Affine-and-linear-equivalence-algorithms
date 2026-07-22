#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace affine::f2 {

struct LinearBasis {
    std::uint32_t dim = 0;
    std::vector<std::uint32_t> domain_pivots;
    std::vector<std::uint32_t> image_pivots;
    std::uint32_t rank = 0;
};

inline void reset(LinearBasis& basis, std::uint32_t dim)
{
    basis.dim = dim;
    basis.domain_pivots.assign(dim, 0);
    basis.image_pivots.assign(dim, 0);
    basis.rank = 0;
}

[[nodiscard]] inline bool reduce(
    const LinearBasis& basis,
    std::uint32_t& domain_vector,
    std::uint32_t& image_vector)
{
    for (std::uint32_t bit = basis.dim; bit-- > 0;) {
        if ((domain_vector & (std::uint32_t { 1 } << bit)) == 0) {
            continue;
        }
        if (basis.domain_pivots[bit] == 0) {
            return false;
        }
        domain_vector ^= basis.domain_pivots[bit];
        image_vector ^= basis.image_pivots[bit];
    }

    return true;
}

[[nodiscard]] inline bool insert_vector(
    LinearBasis& basis,
    std::uint32_t domain_vector,
    std::uint32_t image_vector)
{
    if (reduce(basis, domain_vector, image_vector)) {
        return image_vector == 0;
    }

    const std::uint32_t pivot_bit =
        31u - static_cast<std::uint32_t>(std::countl_zero(domain_vector));
    basis.domain_pivots[pivot_bit] = domain_vector;
    basis.image_pivots[pivot_bit] = image_vector;
    ++basis.rank;
    return true;
}

[[nodiscard]] inline std::uint32_t image_of(
    const LinearBasis& basis,
    std::uint32_t domain_vector)
{
    std::uint32_t image_vector = 0;
    const bool reduced = reduce(basis, domain_vector, image_vector);
    (void)reduced;
    return image_vector;
}

inline void basis_vector_images(
    const LinearBasis& basis,
    std::span<std::uint32_t> images)
{
    for (std::uint32_t bit = 0; bit < basis.dim; ++bit) {
        images[bit] = image_of(basis, std::uint32_t { 1 } << bit);
    }
}

} // namespace affine::f2
