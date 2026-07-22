#pragma once

#include "dfs_types.hpp"
#include "partition_pair.hpp"
#include "refinement.hpp"
#include "search_types.hpp"

#include <cstdint>

namespace affine {

[[nodiscard]] inline bool is_discrete(const Partition& partition)
{
    for (Partition::CellId cell = partition.first_cell();
         cell != Partition::npos;
         cell = partition.next_cell(cell)) {
        if (partition.cell(cell).size() != 1) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool is_complete(const SearchPartitions& partitions)
{
    return is_discrete(partitions.P.left)
        && is_discrete(partitions.P.right)
        && is_discrete(partitions.Q.left)
        && is_discrete(partitions.Q.right);
}

[[nodiscard]] inline bool objects_are_in_corresponding_cells(
    const PartitionPair& pair,
    Partition::ObjectId left_object,
    Partition::ObjectId right_object)
{
    const Partition::CellId target_left = pair.left.cell_of(left_object);
    const Partition::CellId target_right = pair.right.cell_of(right_object);

    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();
    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        if (left_cell == target_left || right_cell == target_right) {
            return left_cell == target_left && right_cell == target_right;
        }
        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }

    return false;
}

[[nodiscard]] inline PartitionPair& branch_pair(
    SearchPartitions& partitions,
    BranchKind kind)
{
    switch (kind) {
    case BranchKind::DomainPoint:
        return partitions.P;
    case BranchKind::CodomainPoint:
        return partitions.Q;
    case BranchKind::DomainHyperplane:
        return partitions.L;
    case BranchKind::CodomainHyperplane:
        return partitions.R;
    }

    return partitions.P;
}

[[nodiscard]] inline const PartitionPair& branch_pair(
    const SearchPartitions& partitions,
    BranchKind kind)
{
    switch (kind) {
    case BranchKind::DomainPoint:
        return partitions.P;
    case BranchKind::CodomainPoint:
        return partitions.Q;
    case BranchKind::DomainHyperplane:
        return partitions.L;
    case BranchKind::CodomainHyperplane:
        return partitions.R;
    }

    return partitions.P;
}

[[nodiscard]] inline bool individualize_branch_pair(
    PartitionPair& pair,
    Partition::ObjectId left_object,
    Partition::ObjectId right_object)
{
    if (!objects_are_in_corresponding_cells(pair, left_object, right_object)) {
        return false;
    }

    pair.left.individualize(left_object);
    pair.right.individualize(right_object);
    return true;
}

[[nodiscard]] inline bool apply_branch_move(
    const DfsProblem& problem,
    SearchPartitions& partitions,
    const BranchMove& move,
    EquivalenceMode mode = EquivalenceMode::Affine)
{
    if (move.kind == BranchKind::DomainPoint) {
        if (!objects_are_in_corresponding_cells(
                partitions.P,
                move.left_object,
                move.right_object)) {
            return false;
        }

        const Partition::ObjectId left_value =
            problem.left_function_table[move.left_object];
        const Partition::ObjectId right_value =
            problem.right_function_table[move.right_object];
        if (!objects_are_in_corresponding_cells(partitions.Q, left_value, right_value)) {
            return false;
        }

        partitions.P.left.individualize(move.left_object);
        partitions.P.right.individualize(move.right_object);
        partitions.Q.left.individualize(left_value);
        partitions.Q.right.individualize(right_value);
        return true;
    }

    if (mode == EquivalenceMode::Linear
        && (move.kind == BranchKind::DomainHyperplane
            || move.kind == BranchKind::CodomainHyperplane)) {
        PartitionPair& pair = branch_pair(partitions, move.kind);
        const Partition::ObjectId paired_left = move.left_object ^ 1u;
        const Partition::ObjectId paired_right = move.right_object ^ 1u;
        if (!objects_are_in_corresponding_cells(
                pair,
                move.left_object,
                move.right_object)
            || !objects_are_in_corresponding_cells(
                pair,
                paired_left,
                paired_right)) {
            return false;
        }

        pair.left.individualize(move.left_object);
        pair.right.individualize(move.right_object);
        pair.left.individualize(paired_left);
        pair.right.individualize(paired_right);
        return true;
    }

    return individualize_branch_pair(
        branch_pair(partitions, move.kind),
        move.left_object,
        move.right_object);
}

[[nodiscard]] inline BranchCell choose_branch_cell_from_pair(
    const PartitionPair& pair,
    BranchKind kind)
{
    BranchCell best;
    best.kind = kind;

    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();
    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        const Partition::Index size = pair.left.cell(left_cell).size();
        if (size > 1 && size == pair.right.cell(right_cell).size() && size < best.size) {
            best.left_cell = left_cell;
            best.right_cell = right_cell;
            best.size = size;
        }
        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }

    return best;
}

[[nodiscard]] inline std::uint32_t branch_priority(BranchKind kind)
{
    switch (kind) {
    case BranchKind::DomainPoint:
        return 0;
    case BranchKind::CodomainPoint:
        return 1;
    case BranchKind::DomainHyperplane:
        return 2;
    case BranchKind::CodomainHyperplane:
        return 3;
    }

    return 4;
}

[[nodiscard]] inline bool is_better_branch_cell(
    const BranchCell& candidate,
    const BranchCell& best)
{
    if (candidate.left_cell == Partition::npos) {
        return false;
    }
    if (best.left_cell == Partition::npos) {
        return true;
    }
    if (candidate.size != best.size) {
        return candidate.size < best.size;
    }
    return branch_priority(candidate.kind) < branch_priority(best.kind);
}

[[nodiscard]] inline BranchCell choose_branch_cell(const SearchPartitions& partitions)
{
    BranchCell best;

    for (const BranchCell candidate : {
             choose_branch_cell_from_pair(partitions.P, BranchKind::DomainPoint),
             choose_branch_cell_from_pair(partitions.Q, BranchKind::CodomainPoint),
             choose_branch_cell_from_pair(partitions.L, BranchKind::DomainHyperplane),
             choose_branch_cell_from_pair(partitions.R, BranchKind::CodomainHyperplane),
         }) {
        if (is_better_branch_cell(candidate, best)) {
            best = candidate;
        }
    }

    return best;
}

[[nodiscard]] inline BranchCell choose_hyperplane_branch_cell(
    const SearchPartitions& partitions)
{
    BranchCell best;
    for (const BranchCell candidate : {
             choose_branch_cell_from_pair(partitions.L, BranchKind::DomainHyperplane),
             choose_branch_cell_from_pair(partitions.R, BranchKind::CodomainHyperplane),
         }) {
        if (is_better_branch_cell(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}

[[nodiscard]] inline BranchCell choose_point_branch_cell(
    const SearchPartitions& partitions)
{
    BranchCell best;
    for (const BranchCell candidate : {
             choose_branch_cell_from_pair(partitions.P, BranchKind::DomainPoint),
             choose_branch_cell_from_pair(partitions.Q, BranchKind::CodomainPoint),
         }) {
        if (is_better_branch_cell(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}

[[nodiscard]] inline BranchCell choose_kind_first_branch_cell(
    const SearchPartitions& partitions,
    BranchKind preferred_kind)
{
    const BranchCell preferred =
        choose_branch_cell_from_pair(branch_pair(partitions, preferred_kind), preferred_kind);
    if (preferred.left_cell != Partition::npos) {
        return preferred;
    }
    return choose_branch_cell(partitions);
}

[[nodiscard]] inline BranchCell choose_branch_cell(
    const SearchPartitions& partitions,
    BranchPolicy policy)
{
    if (policy == BranchPolicy::HyperplanesOnly) {
        return choose_hyperplane_branch_cell(partitions);
    }

    if (policy == BranchPolicy::HyperplanesFirst) {
        BranchCell hyperplane_cell = choose_hyperplane_branch_cell(partitions);
        if (hyperplane_cell.left_cell != Partition::npos) {
            return hyperplane_cell;
        }
        return choose_point_branch_cell(partitions);
    }

    if (policy == BranchPolicy::DomainPointFirst) {
        return choose_kind_first_branch_cell(partitions, BranchKind::DomainPoint);
    }

    if (policy == BranchPolicy::CodomainPointFirst) {
        return choose_kind_first_branch_cell(partitions, BranchKind::CodomainPoint);
    }

    if (policy == BranchPolicy::DomainHyperplaneFirst) {
        return choose_kind_first_branch_cell(
            partitions,
            BranchKind::DomainHyperplane);
    }

    if (policy == BranchPolicy::CodomainHyperplaneFirst) {
        return choose_kind_first_branch_cell(
            partitions,
            BranchKind::CodomainHyperplane);
    }

    return choose_branch_cell(partitions);
}

} // namespace affine
