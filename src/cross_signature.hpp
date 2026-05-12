#pragma once

#include "radon_f2.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace affine::f2 {

struct CrossSignatureWorkspace {
    RadonWorkspace domain_radon;
    RadonWorkspace codomain_radon;
    std::vector<Weight> domain_point_buffer;
    std::vector<Weight> codomain_point_buffer;
};

inline void domain_hyperplane_cross_signature(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim,
    std::span<const std::uint32_t> function_table,
    std::span<const Weight> codomain_hyperplane_weights,
    std::span<Weight> domain_hyperplane_signatures,
    CrossSignatureWorkspace& workspace)
{
    const std::uint32_t domain_points = point_count(domain_dim);
    const std::uint32_t codomain_points = point_count(codomain_dim);

    workspace.codomain_point_buffer.resize(codomain_points);
    backproject(
        codomain_dim,
        codomain_hyperplane_weights,
        workspace.codomain_point_buffer,
        workspace.codomain_radon);

    workspace.domain_point_buffer.resize(domain_points);
    for (std::uint32_t x = 0; x < domain_points; ++x) {
        workspace.domain_point_buffer[x] = workspace.codomain_point_buffer[function_table[x]];
    }

    radon_transform_destructive(
        domain_dim,
        workspace.domain_point_buffer,
        domain_hyperplane_signatures);
}

inline void codomain_hyperplane_cross_signature(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim,
    std::span<const std::uint32_t> function_table,
    std::span<const Weight> domain_hyperplane_weights,
    std::span<Weight> codomain_hyperplane_signatures,
    CrossSignatureWorkspace& workspace)
{
    const std::uint32_t domain_points = point_count(domain_dim);
    const std::uint32_t codomain_points = point_count(codomain_dim);

    workspace.domain_point_buffer.resize(domain_points);
    backproject(
        domain_dim,
        domain_hyperplane_weights,
        workspace.domain_point_buffer,
        workspace.domain_radon);

    workspace.codomain_point_buffer.assign(codomain_points, Weight { 0 });
    for (std::uint32_t x = 0; x < domain_points; ++x) {
        workspace.codomain_point_buffer[function_table[x]] = add_mod(
            workspace.codomain_point_buffer[function_table[x]],
            workspace.domain_point_buffer[x]);
    }

    radon_transform_destructive(
        codomain_dim,
        workspace.codomain_point_buffer,
        codomain_hyperplane_signatures);
}

} // namespace affine::f2
