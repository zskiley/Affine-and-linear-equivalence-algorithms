#pragma once

#include "fwht.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace affine::f2 {

using Weight = std::uint32_t;
static constexpr Weight modulus = 2'147'483'647u;
static constexpr Weight inverse_two = 1'073'741'824u;

struct RadonWorkspace {
    std::vector<Weight> fourier;
};

[[nodiscard]] inline Weight add_mod(Weight left, Weight right)
{
    const Weight sum = left + right;
    return sum >= modulus ? sum - modulus : sum;
}

[[nodiscard]] inline Weight sub_mod(Weight left, Weight right)
{
    return left >= right ? left - right : left + modulus - right;
}

[[nodiscard]] inline Weight mul_mod(Weight left, Weight right)
{
    return static_cast<Weight>(
        (static_cast<std::uint64_t>(left) * static_cast<std::uint64_t>(right)) % modulus);
}

[[nodiscard]] inline Weight half_mod(Weight value)
{
    return (value >> 1u) + ((0u - (value & 1u)) & inverse_two);
}

[[nodiscard]] inline Weight pow2_mod(std::uint32_t exponent)
{
    Weight value = 1;
    for (std::uint32_t i = 0; i < exponent; ++i) {
        value = add_mod(value, value);
    }
    return value;
}

[[nodiscard]] inline Weight inverse_pow2_mod(std::uint32_t exponent)
{
    Weight value = 1;
    for (std::uint32_t i = 0; i < exponent; ++i) {
        value = mul_mod(value, inverse_two);
    }
    return value;
}

inline void fwht_mod(std::span<Weight> values)
{
    const std::size_t n = values.size();

    for (std::size_t len = 1; len < n; len <<= 1) {
        for (std::size_t base = 0; base < n; base += (len << 1)) {
            for (std::size_t offset = 0; offset < len; ++offset) {
                const Weight a = values[base + offset];
                const Weight b = values[base + offset + len];
                values[base + offset] = add_mod(a, b);
                values[base + offset + len] = sub_mod(a, b);
            }
        }
    }
}

[[nodiscard]] inline std::uint32_t point_count(std::uint32_t dim)
{
    return std::uint32_t { 1 } << dim;
}

[[nodiscard]] inline std::uint32_t hyperplane_count(std::uint32_t dim)
{
    return 2u * (point_count(dim) - 1u);
}

[[nodiscard]] inline std::uint32_t hyperplane_id(
    std::uint32_t normal,
    std::uint32_t offset)
{
    return 2u * (normal - 1u) + offset;
}

[[nodiscard]] inline std::uint32_t hyperplane_normal(std::uint32_t id)
{
    return id / 2u + 1u;
}

[[nodiscard]] inline std::uint32_t hyperplane_offset(std::uint32_t id)
{
    return id & 1u;
}

inline void radon_transform(
    std::uint32_t dim,
    std::span<const Weight> point_weights,
    std::span<Weight> hyperplane_sums,
    RadonWorkspace& workspace)
{
    const std::uint32_t points = point_count(dim);

    workspace.fourier.assign(point_weights.begin(), point_weights.end());
    fwht_mod(std::span<Weight>(workspace.fourier));

    const Weight total = workspace.fourier[0];
    for (std::uint32_t normal = 1; normal < points; ++normal) {
        const Weight slice = workspace.fourier[normal];
        hyperplane_sums[hyperplane_id(normal, 0)] = half_mod(add_mod(total, slice));
        hyperplane_sums[hyperplane_id(normal, 1)] = half_mod(sub_mod(total, slice));
    }
}

inline void radon_transform_destructive(
    std::uint32_t dim,
    std::span<Weight> point_weights,
    std::span<Weight> hyperplane_sums)
{
    const std::uint32_t points = point_count(dim);

    fwht_mod(point_weights);

    const Weight total = point_weights[0];
    for (std::uint32_t normal = 1; normal < points; ++normal) {
        const Weight slice = point_weights[normal];
        hyperplane_sums[hyperplane_id(normal, 0)] = half_mod(add_mod(total, slice));
        hyperplane_sums[hyperplane_id(normal, 1)] = half_mod(sub_mod(total, slice));
    }
}

inline void backproject(
    std::uint32_t dim,
    std::span<const Weight> hyperplane_weights,
    std::span<Weight> point_sums,
    RadonWorkspace& workspace)
{
    (void)workspace;

    const std::uint32_t points = point_count(dim);

    Weight total = 0;
    for (std::uint32_t normal = 1; normal < points; ++normal) {
        const Weight weight_zero = hyperplane_weights[hyperplane_id(normal, 0)];
        const Weight weight_one = hyperplane_weights[hyperplane_id(normal, 1)];
        total = add_mod(total, add_mod(weight_zero, weight_one));
        point_sums[normal] = sub_mod(weight_zero, weight_one);
    }
    point_sums[0] = total;

    fwht_mod(point_sums);

    for (std::uint32_t point = 0; point < points; ++point) {
        point_sums[point] = half_mod(point_sums[point]);
    }
}

} // namespace affine::f2
