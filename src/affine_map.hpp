#pragma once

#include "linear_algebra.hpp"
#include "partition_pair.hpp"

#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace affine::f2 {

struct AffineMap {
    std::uint32_t dim = 0;
    std::uint32_t translation = 0;
    std::vector<std::uint32_t> basis_images;
};

enum class AffineFitStatus : std::uint8_t {
    Inconsistent,
    Underdetermined,
    Determined,
};

struct AffineFitWorkspace {
    LinearBasis basis;
    std::vector<std::uint32_t> singleton_left;
    std::vector<std::uint32_t> singleton_right;
    std::vector<Partition::CellId> left_cell_order;
    std::vector<Partition::CellId> right_cell_order;
};

[[nodiscard]] inline std::uint32_t apply(
    const AffineMap& map,
    std::uint32_t point)
{
    std::uint32_t image = map.translation;
    while (point != 0) {
        const std::uint32_t bit = std::countr_zero(point);
        image ^= map.basis_images[bit];
        point &= point - 1u;
    }
    return image;
}

inline void collect_singleton_pairs(
    const PartitionPair& pair,
    AffineFitWorkspace& workspace)
{
    workspace.singleton_left.clear();
    workspace.singleton_right.clear();

    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();
    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        if (pair.left.cell(left_cell).size() == 1
            && pair.right.cell(right_cell).size() == 1) {
            workspace.singleton_left.push_back(pair.left.objects(left_cell).front());
            workspace.singleton_right.push_back(pair.right.objects(right_cell).front());
        }
        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }
}

[[nodiscard]] inline AffineFitStatus fit_affine_map(
    std::uint32_t dim,
    const PartitionPair& point_pair,
    AffineMap& map,
    AffineFitWorkspace& workspace)
{
    collect_singleton_pairs(point_pair, workspace);
    if (workspace.singleton_left.empty()) {
        return AffineFitStatus::Underdetermined;
    }

    reset(workspace.basis, dim);

    const std::uint32_t anchor_left = workspace.singleton_left.front();
    const std::uint32_t anchor_right = workspace.singleton_right.front();

    for (std::size_t i = 1; i < workspace.singleton_left.size(); ++i) {
        const std::uint32_t domain_difference =
            workspace.singleton_left[i] ^ anchor_left;
        const std::uint32_t image_difference =
            workspace.singleton_right[i] ^ anchor_right;
        if (!insert_vector(workspace.basis, domain_difference, image_difference)) {
            return AffineFitStatus::Inconsistent;
        }
    }

    if (workspace.basis.rank < dim) {
        return AffineFitStatus::Underdetermined;
    }

    map.dim = dim;
    map.translation = 0;
    map.basis_images.resize(dim);
    basis_vector_images(workspace.basis, map.basis_images);
    map.translation = anchor_right ^ apply(map, anchor_left);

    for (std::size_t i = 0; i < workspace.singleton_left.size(); ++i) {
        if (apply(map, workspace.singleton_left[i]) != workspace.singleton_right[i]) {
            return AffineFitStatus::Inconsistent;
        }
    }

    return AffineFitStatus::Determined;
}

[[nodiscard]] inline AffineFitStatus fit_linear_map(
    std::uint32_t dim,
    const PartitionPair& point_pair,
    AffineMap& map,
    AffineFitWorkspace& workspace)
{
    collect_singleton_pairs(point_pair, workspace);
    if (workspace.singleton_left.empty()) {
        return AffineFitStatus::Underdetermined;
    }

    reset(workspace.basis, dim);

    for (std::size_t i = 0; i < workspace.singleton_left.size(); ++i) {
        const std::uint32_t domain_vector = workspace.singleton_left[i];
        const std::uint32_t image_vector = workspace.singleton_right[i];
        if (domain_vector == 0) {
            if (image_vector != 0) {
                return AffineFitStatus::Inconsistent;
            }
            continue;
        }

        if (!insert_vector(workspace.basis, domain_vector, image_vector)) {
            return AffineFitStatus::Inconsistent;
        }
    }

    if (workspace.basis.rank < dim) {
        return AffineFitStatus::Underdetermined;
    }

    map.dim = dim;
    map.translation = 0;
    map.basis_images.resize(dim);
    basis_vector_images(workspace.basis, map.basis_images);

    for (std::size_t i = 0; i < workspace.singleton_left.size(); ++i) {
        if (apply(map, workspace.singleton_left[i]) != workspace.singleton_right[i]) {
            return AffineFitStatus::Inconsistent;
        }
    }

    return AffineFitStatus::Determined;
}

[[nodiscard]] inline bool affine_map_respects_partition(
    const AffineMap& map,
    const PartitionPair& pair,
    AffineFitWorkspace& workspace)
{
    if (pair.left.object_count() != pair.right.object_count()) {
        return false;
    }

    workspace.left_cell_order.assign(pair.left.object_count(), Partition::npos);
    workspace.right_cell_order.assign(pair.right.object_count(), Partition::npos);

    Partition::CellId order = 0;
    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();
    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        if (pair.left.cell(left_cell).size() != pair.right.cell(right_cell).size()) {
            return false;
        }

        if (left_cell >= workspace.left_cell_order.size()) {
            workspace.left_cell_order.resize(
                static_cast<std::size_t>(left_cell) + 1u,
                Partition::npos);
        }
        if (right_cell >= workspace.right_cell_order.size()) {
            workspace.right_cell_order.resize(
                static_cast<std::size_t>(right_cell) + 1u,
                Partition::npos);
        }

        workspace.left_cell_order[left_cell] = order;
        workspace.right_cell_order[right_cell] = order;
        ++order;

        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }

    if (left_cell != Partition::npos || right_cell != Partition::npos) {
        return false;
    }

    for (Partition::ObjectId object = 0; object < pair.left.object_count(); ++object) {
        const Partition::ObjectId image = apply(map, object);
        if (image >= pair.right.object_count()) {
            return false;
        }

        const Partition::CellId target_left = pair.left.cell_of(object);
        const Partition::CellId target_right = pair.right.cell_of(image);
        if (target_left >= workspace.left_cell_order.size()
            || target_right >= workspace.right_cell_order.size()
            || workspace.left_cell_order[target_left] == Partition::npos
            || workspace.left_cell_order[target_left]
                != workspace.right_cell_order[target_right]) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline bool affine_map_respects_partition(
    const AffineMap& map,
    const PartitionPair& pair)
{
    AffineFitWorkspace workspace;
    return affine_map_respects_partition(map, pair, workspace);
}

} // namespace affine::f2
