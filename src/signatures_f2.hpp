#pragma once

#include "partition.hpp"
#include "profile.hpp"
#include "radon_f2.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace affine::f2 {

struct FunctionSignatures {
    std::vector<Partition::Signature> domain_points;
    std::vector<Partition::Signature> codomain_points;
    std::vector<Partition::Signature> domain_hyperplanes;
    std::vector<Partition::Signature> codomain_hyperplanes;
};

struct SignatureWorkspace {
    RadonWorkspace domain_radon;
    RadonWorkspace codomain_radon;

    std::vector<Weight> domain_point_weights;
    std::vector<Weight> codomain_point_weights;
    std::vector<Weight> domain_hyperplane_weights;
    std::vector<Weight> codomain_hyperplane_weights;

    std::vector<Weight> domain_points_from_hyperplanes;
    std::vector<Weight> codomain_points_from_hyperplanes;

    std::vector<Weight> domain_hyperplane_direct;
    std::vector<Weight> domain_hyperplane_cross;
    std::vector<Weight> codomain_hyperplane_direct;
    std::vector<Weight> codomain_hyperplane_cross;

    std::vector<Weight> domain_point_temp;
    std::vector<Weight> codomain_point_temp;
    std::vector<Weight> codomain_point_preimage_weights;
    std::vector<Weight> cell_weights;
};

[[nodiscard]] inline Partition::Signature pack_pair_signature(Weight first, Weight second)
{
    return (static_cast<Partition::Signature>(static_cast<std::uint32_t>(first)) << 32)
        | static_cast<std::uint32_t>(second);
}

[[nodiscard]] inline Weight hash_cell_index(std::uint32_t cell_index)
{
    std::uint32_t value = cell_index + 0x9e3779b9u;
    value ^= value >> 16u;
    value *= 0x85ebca6bu;
    value ^= value >> 13u;
    value *= 0xc2b2ae35u;
    value ^= value >> 16u;
    return 1u + (value % (modulus - 1u));
}

inline void build_partition_weights(
    const Partition& partition,
    std::span<Weight> weights)
{
    std::uint32_t cell_index = 1;
    for (Partition::CellId cell = partition.first_cell();
         cell != Partition::npos;
         cell = partition.next_cell(cell)) {
        const Weight weight = hash_cell_index(cell_index);
        for (const Partition::ObjectId object : partition.objects(cell)) {
            weights[object] = weight;
        }
        ++cell_index;
    }
}

inline void build_partition_weights_by_object(
    const Partition& partition,
    std::span<Weight> weights,
    std::vector<Weight>& cell_weights)
{
    cell_weights.clear();

    std::uint32_t cell_index = 1;
    for (Partition::CellId cell = partition.first_cell();
         cell != Partition::npos;
         cell = partition.next_cell(cell)) {
        if (cell >= cell_weights.size()) {
            cell_weights.resize(static_cast<std::size_t>(cell) + 1u);
        }
        cell_weights[cell] = hash_cell_index(cell_index);
        ++cell_index;
    }

    for (Partition::ObjectId object = 0; object < partition.object_count(); ++object) {
        weights[object] = cell_weights[partition.cell_of(object)];
    }
}

inline void compute_function_signatures(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim,
    std::span<const std::uint32_t> function_table,
    const Partition& domain_point_partition,
    const Partition& codomain_point_partition,
    const Partition& domain_hyperplane_partition,
    const Partition& codomain_hyperplane_partition,
    FunctionSignatures& signatures,
    SignatureWorkspace& workspace)
{
    profile::ScopedTimer timer(profile::counters.signature_ns);
    profile::count(profile::counters.signature_calls);

    const std::uint32_t domain_points = point_count(domain_dim);
    const std::uint32_t codomain_points = point_count(codomain_dim);
    const std::uint32_t domain_hyperplanes = hyperplane_count(domain_dim);
    const std::uint32_t codomain_hyperplanes = hyperplane_count(codomain_dim);

    {
        profile::ScopedTimer resize_timer(profile::counters.signature_resize_ns);
        workspace.domain_point_weights.resize(domain_points);
        workspace.codomain_point_weights.resize(codomain_points);
        workspace.domain_hyperplane_weights.resize(domain_hyperplanes);
        workspace.codomain_hyperplane_weights.resize(codomain_hyperplanes);

        workspace.domain_points_from_hyperplanes.resize(domain_points);
        workspace.codomain_points_from_hyperplanes.resize(codomain_points);
        workspace.domain_hyperplane_direct.resize(domain_hyperplanes);
        workspace.domain_hyperplane_cross.resize(domain_hyperplanes);
        workspace.codomain_hyperplane_direct.resize(codomain_hyperplanes);
        workspace.codomain_hyperplane_cross.resize(codomain_hyperplanes);
        workspace.domain_point_temp.resize(domain_points);

        signatures.domain_points.resize(domain_points);
        signatures.codomain_points.resize(codomain_points);
        signatures.domain_hyperplanes.resize(domain_hyperplanes);
        signatures.codomain_hyperplanes.resize(codomain_hyperplanes);
    }

    {
        profile::ScopedTimer weight_timer(profile::counters.signature_weight_ns);
        build_partition_weights_by_object(
            domain_point_partition,
            workspace.domain_point_weights,
            workspace.cell_weights);
        build_partition_weights_by_object(
            codomain_point_partition,
            workspace.codomain_point_weights,
            workspace.cell_weights);
        build_partition_weights_by_object(
            domain_hyperplane_partition,
            workspace.domain_hyperplane_weights,
            workspace.cell_weights);
        build_partition_weights_by_object(
            codomain_hyperplane_partition,
            workspace.codomain_hyperplane_weights,
            workspace.cell_weights);
    }

    {
        profile::ScopedTimer zero_timer(profile::counters.signature_zero_ns);
        workspace.codomain_point_temp.assign(codomain_points, Weight { 0 });
        workspace.codomain_point_preimage_weights.assign(codomain_points, Weight { 0 });
    }

    {
        profile::ScopedTimer backproject_timer(
            profile::counters.signature_backproject_ns);
        backproject(
            domain_dim,
            workspace.domain_hyperplane_weights,
            workspace.domain_points_from_hyperplanes,
            workspace.domain_radon);

        backproject(
            codomain_dim,
            workspace.codomain_hyperplane_weights,
            workspace.codomain_points_from_hyperplanes,
            workspace.codomain_radon);
    }

    {
        profile::ScopedTimer point_loop_timer(
            profile::counters.signature_point_loop_ns);
        for (std::uint32_t x = 0; x < domain_points; ++x) {
            const std::uint32_t y = function_table[x];
            signatures.domain_points[x] = pack_pair_signature(
                workspace.domain_points_from_hyperplanes[x],
                workspace.codomain_point_weights[y]);
            workspace.codomain_point_preimage_weights[y] = add_mod(
                workspace.codomain_point_preimage_weights[y],
                workspace.domain_point_weights[x]);
            workspace.domain_point_temp[x] =
                workspace.codomain_points_from_hyperplanes[y];
            workspace.codomain_point_temp[y] = add_mod(
                workspace.codomain_point_temp[y],
                workspace.domain_points_from_hyperplanes[x]);
        }

        for (std::uint32_t y = 0; y < codomain_points; ++y) {
            signatures.codomain_points[y] = pack_pair_signature(
                workspace.codomain_points_from_hyperplanes[y],
                workspace.codomain_point_preimage_weights[y]);
        }
    }

    {
        profile::ScopedTimer radon_timer(profile::counters.signature_radon_ns);
        radon_transform_destructive(
            domain_dim,
            workspace.domain_point_weights,
            workspace.domain_hyperplane_direct);

        radon_transform_destructive(
            codomain_dim,
            workspace.codomain_point_weights,
            workspace.codomain_hyperplane_direct);

        radon_transform_destructive(
            domain_dim,
            workspace.domain_point_temp,
            workspace.domain_hyperplane_cross);

        radon_transform_destructive(
            codomain_dim,
            workspace.codomain_point_temp,
            workspace.codomain_hyperplane_cross);
    }

    {
        profile::ScopedTimer pack_timer(profile::counters.signature_pack_ns);
        for (std::uint32_t h = 0; h < domain_hyperplanes; ++h) {
            signatures.domain_hyperplanes[h] = pack_pair_signature(
                workspace.domain_hyperplane_direct[h],
                workspace.domain_hyperplane_cross[h]);
        }
        for (std::uint32_t h = 0; h < codomain_hyperplanes; ++h) {
            signatures.codomain_hyperplanes[h] = pack_pair_signature(
                workspace.codomain_hyperplane_direct[h],
                workspace.codomain_hyperplane_cross[h]);
        }
    }
}

} // namespace affine::f2
