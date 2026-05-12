#pragma once

#include "affine_map.hpp"
#include "partition.hpp"
#include "partition_pair.hpp"
#include "profile.hpp"
#include "signatures_f2.hpp"
#include "solution_store.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace affine {

enum class RefineResult : std::uint8_t {
    Continue,
    Stop,
};

enum class RefineStep : std::uint8_t {
    Unchanged,
    Changed,
    Inconsistent,
    Solved,
};

[[nodiscard]] inline bool changed(RefineStep status)
{
    return status == RefineStep::Changed;
}

struct SearchPartitions {
    PartitionPair P;
    PartitionPair Q;
    PartitionPair L;
    PartitionPair R;
};

struct SearchPartitionSnapshot {
    Partition::Snapshot p_left;
    Partition::Snapshot p_right;
    Partition::Snapshot q_left;
    Partition::Snapshot q_right;
    Partition::Snapshot l_left;
    Partition::Snapshot l_right;
    Partition::Snapshot r_left;
    Partition::Snapshot r_right;
};

struct PairRefinementScratch {
    Partition::RefinementScratch left;
    Partition::RefinementScratch right;
};

struct RefinementWorkspace {
    f2::FunctionSignatures left_signatures;
    f2::FunctionSignatures right_signatures;
    f2::SignatureWorkspace left_signature_workspace;
    f2::SignatureWorkspace right_signature_workspace;
    f2::AffineMap domain_map;
    f2::AffineMap codomain_map;
    f2::AffineFitWorkspace domain_map_workspace;
    f2::AffineFitWorkspace codomain_map_workspace;
    PairRefinementScratch pair_scratch;
};

[[nodiscard]] inline SearchPartitions make_initial_partitions(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim)
{
    SearchPartitions partitions;
    partitions.P = PartitionPair(f2::point_count(domain_dim));
    partitions.Q = PartitionPair(f2::point_count(codomain_dim));
    partitions.L = PartitionPair(f2::hyperplane_count(domain_dim));
    partitions.R = PartitionPair(f2::hyperplane_count(codomain_dim));
    return partitions;
}

[[nodiscard]] inline SearchPartitionSnapshot snapshot(const SearchPartitions& partitions)
{
    return SearchPartitionSnapshot {
        .p_left = partitions.P.left.snapshot(),
        .p_right = partitions.P.right.snapshot(),
        .q_left = partitions.Q.left.snapshot(),
        .q_right = partitions.Q.right.snapshot(),
        .l_left = partitions.L.left.snapshot(),
        .l_right = partitions.L.right.snapshot(),
        .r_left = partitions.R.left.snapshot(),
        .r_right = partitions.R.right.snapshot(),
    };
}

inline void rollback(SearchPartitions& partitions, const SearchPartitionSnapshot& snapshot)
{
    partitions.P.left.rollback(snapshot.p_left);
    partitions.P.right.rollback(snapshot.p_right);
    partitions.Q.left.rollback(snapshot.q_left);
    partitions.Q.right.rollback(snapshot.q_right);
    partitions.L.left.rollback(snapshot.l_left);
    partitions.L.right.rollback(snapshot.l_right);
    partitions.R.left.rollback(snapshot.r_left);
    partitions.R.right.rollback(snapshot.r_right);
}

[[nodiscard]] inline bool same_shape(const Partition& left, const Partition& right)
{
    Partition::CellId left_cell = left.first_cell();
    Partition::CellId right_cell = right.first_cell();

    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        if (left.cell(left_cell).size() != right.cell(right_cell).size()) {
            return false;
        }
        left_cell = left.next_cell(left_cell);
        right_cell = right.next_cell(right_cell);
    }

    return left_cell == Partition::npos && right_cell == Partition::npos;
}

[[nodiscard]] inline bool same_refinement_shape(
    const Partition::RefinementScratch& left,
    const Partition::RefinementScratch& right)
{
    return left.run_signatures == right.run_signatures
        && left.run_sizes == right.run_sizes;
}

[[nodiscard]] inline bool has_mismatched_singleton_pairs(const PartitionPair& pair)
{
    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();

    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        const Partition::Cell& left = pair.left.cell(left_cell);
        const Partition::Cell& right = pair.right.cell(right_cell);
        if (left.size() == 1 && right.size() == 1) {
            if (pair.left.objects(left_cell).front()
                != pair.right.objects(right_cell).front()) {
                return true;
            }
        }
        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }

    return false;
}

inline RefineStep refine_pair_by_signature(
    PartitionPair& pair,
    std::span<const Partition::Signature> left_signatures,
    std::span<const Partition::Signature> right_signatures,
    PairRefinementScratch& scratch)
{
    profile::ScopedTimer timer(profile::counters.pair_refine_ns);
    profile::count(profile::counters.pair_refine_calls);

    const auto left_snapshot = pair.left.snapshot();
    const auto right_snapshot = pair.right.snapshot();

    bool any_changed = false;

    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();

    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        if (pair.left.cell(left_cell).size() != pair.right.cell(right_cell).size()) {
            pair.left.rollback(left_snapshot);
            pair.right.rollback(right_snapshot);
            return RefineStep::Inconsistent;
        }

        const Partition::CellId next_left = pair.left.next_cell(left_cell);
        const Partition::CellId next_right = pair.right.next_cell(right_cell);

        const bool left_changed = pair.left.build_refinement_plan(
            left_cell,
            left_signatures,
            scratch.left);
        const bool right_changed = pair.right.build_refinement_plan(
            right_cell,
            right_signatures,
            scratch.right);

        if (!same_refinement_shape(scratch.left, scratch.right)) {
            pair.left.rollback(left_snapshot);
            pair.right.rollback(right_snapshot);
            return RefineStep::Inconsistent;
        }

        if (left_changed != right_changed) {
            pair.left.rollback(left_snapshot);
            pair.right.rollback(right_snapshot);
            return RefineStep::Inconsistent;
        }

        if (left_changed) {
            pair.left.split_cell_by_ordered_objects(
                left_cell,
                scratch.left.ordered_objects,
                scratch.left.run_sizes);
            pair.right.split_cell_by_ordered_objects(
                right_cell,
                scratch.right.ordered_objects,
                scratch.right.run_sizes);
            any_changed = true;
        }

        left_cell = next_left;
        right_cell = next_right;
    }

    if (left_cell != Partition::npos || right_cell != Partition::npos) {
        pair.left.rollback(left_snapshot);
        pair.right.rollback(right_snapshot);
        return RefineStep::Inconsistent;
    }

    return any_changed ? RefineStep::Changed : RefineStep::Unchanged;
}

inline RefineStep individualize_pair_by_affine_map(
    PartitionPair& pair,
    const f2::AffineMap& map,
    f2::AffineFitWorkspace& workspace)
{
    const auto left_snapshot = pair.left.snapshot();
    const auto right_snapshot = pair.right.snapshot();

    if (!f2::affine_map_respects_partition(map, pair, workspace)) {
        return RefineStep::Inconsistent;
    }

    bool any_changed = false;
    const std::size_t object_count = pair.left.object_count();
    for (std::size_t object = 0; object < object_count; ++object) {
        const Partition::ObjectId left_object =
            static_cast<Partition::ObjectId>(object);
        const Partition::ObjectId right_object =
            f2::apply(map, left_object);

        if (right_object >= pair.right.object_count()) {
            pair.left.rollback(left_snapshot);
            pair.right.rollback(right_snapshot);
            return RefineStep::Inconsistent;
        }

        if (!pair.left.is_singleton(pair.left.cell_of(left_object))
            || !pair.right.is_singleton(pair.right.cell_of(right_object))) {
            any_changed = true;
        }

        pair.left.individualize(left_object);
        pair.right.individualize(right_object);
    }

    if (!same_shape(pair.left, pair.right)) {
        pair.left.rollback(left_snapshot);
        pair.right.rollback(right_snapshot);
        return RefineStep::Inconsistent;
    }

    return any_changed ? RefineStep::Changed : RefineStep::Unchanged;
}

inline RefineStep propagate_determined_affine_maps(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim,
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    SearchPartitions& partitions,
    RefinementWorkspace& workspace,
    SolutionStore* solution_store,
    std::span<const BranchMove> path)
{
    profile::ScopedTimer timer(profile::counters.affine_propagate_ns);
    profile::count(profile::counters.affine_propagate_calls);

    bool any_changed = false;

    const f2::AffineFitStatus domain_status = f2::fit_affine_map(
        domain_dim,
        partitions.P,
        workspace.domain_map,
        workspace.domain_map_workspace);
    if (domain_status == f2::AffineFitStatus::Inconsistent) {
        return RefineStep::Inconsistent;
    }
    if (domain_status == f2::AffineFitStatus::Determined) {
        const RefineStep status = individualize_pair_by_affine_map(
            partitions.P,
            workspace.domain_map,
            workspace.domain_map_workspace);
        if (status == RefineStep::Inconsistent) {
            return RefineStep::Inconsistent;
        }
        any_changed |= changed(status);
    }

    const bool domain_determined =
        domain_status == f2::AffineFitStatus::Determined;

    const f2::AffineFitStatus codomain_status = f2::fit_affine_map(
        codomain_dim,
        partitions.Q,
        workspace.codomain_map,
        workspace.codomain_map_workspace);
    if (codomain_status == f2::AffineFitStatus::Inconsistent) {
        return RefineStep::Inconsistent;
    }
    if (codomain_status == f2::AffineFitStatus::Determined) {
        const RefineStep status = individualize_pair_by_affine_map(
            partitions.Q,
            workspace.codomain_map,
            workspace.codomain_map_workspace);
        if (status == RefineStep::Inconsistent) {
            return RefineStep::Inconsistent;
        }
        any_changed |= changed(status);
    }

    const bool codomain_determined =
        codomain_status == f2::AffineFitStatus::Determined;

    if (domain_determined && codomain_determined) {
        Solution solution {
            .domain_map = workspace.domain_map,
            .codomain_map = workspace.codomain_map,
            .path = std::vector<BranchMove>(path.begin(), path.end()),
        };

        if (solution_store != nullptr) {
            (void)solution_store->publish(
                left_function_table,
                right_function_table,
                std::move(solution));
        }
        return RefineStep::Solved;
    }

    return any_changed ? RefineStep::Changed : RefineStep::Unchanged;
}

inline RefineResult refine_until_stable(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim,
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    SearchPartitions& partitions,
    RefinementWorkspace& workspace,
    SolutionStore* solution_store = nullptr,
    std::span<const BranchMove> path = {})
{
    profile::ScopedTimer timer(profile::counters.refine_ns);
    profile::count(profile::counters.refine_calls);

    const SearchPartitionSnapshot start = snapshot(partitions);

    while (true) {
        const RefineStep early_affine_status = propagate_determined_affine_maps(
            domain_dim,
            codomain_dim,
            left_function_table,
            right_function_table,
            partitions,
            workspace,
            solution_store,
            path);
        if (early_affine_status == RefineStep::Inconsistent) {
            rollback(partitions, start);
            return RefineResult::Stop;
        }
        if (early_affine_status == RefineStep::Solved) {
            return RefineResult::Stop;
        }
        if (changed(early_affine_status)) {
            continue;
        }

        f2::compute_function_signatures(
            domain_dim,
            codomain_dim,
            left_function_table,
            partitions.P.left,
            partitions.Q.left,
            partitions.L.left,
            partitions.R.left,
            workspace.left_signatures,
            workspace.left_signature_workspace);

        f2::compute_function_signatures(
            domain_dim,
            codomain_dim,
            right_function_table,
            partitions.P.right,
            partitions.Q.right,
            partitions.L.right,
            partitions.R.right,
            workspace.right_signatures,
            workspace.right_signature_workspace);

        bool round_changed = false;

        const bool hyperplane_first =
            has_mismatched_singleton_pairs(partitions.P)
            || has_mismatched_singleton_pairs(partitions.Q);

        if (hyperplane_first) {
            const RefineStep l_status = refine_pair_by_signature(
                partitions.L,
                workspace.left_signatures.domain_hyperplanes,
                workspace.right_signatures.domain_hyperplanes,
                workspace.pair_scratch);
            if (l_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(l_status);

            const RefineStep r_status = refine_pair_by_signature(
                partitions.R,
                workspace.left_signatures.codomain_hyperplanes,
                workspace.right_signatures.codomain_hyperplanes,
                workspace.pair_scratch);
            if (r_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(r_status);

            const RefineStep p_status = refine_pair_by_signature(
                partitions.P,
                workspace.left_signatures.domain_points,
                workspace.right_signatures.domain_points,
                workspace.pair_scratch);
            if (p_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(p_status);

            const RefineStep q_status = refine_pair_by_signature(
                partitions.Q,
                workspace.left_signatures.codomain_points,
                workspace.right_signatures.codomain_points,
                workspace.pair_scratch);
            if (q_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(q_status);
        } else {
            const RefineStep p_status = refine_pair_by_signature(
                partitions.P,
                workspace.left_signatures.domain_points,
                workspace.right_signatures.domain_points,
                workspace.pair_scratch);
            if (p_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(p_status);

            const RefineStep q_status = refine_pair_by_signature(
                partitions.Q,
                workspace.left_signatures.codomain_points,
                workspace.right_signatures.codomain_points,
                workspace.pair_scratch);
            if (q_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(q_status);

            const RefineStep l_status = refine_pair_by_signature(
                partitions.L,
                workspace.left_signatures.domain_hyperplanes,
                workspace.right_signatures.domain_hyperplanes,
                workspace.pair_scratch);
            if (l_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(l_status);

            const RefineStep r_status = refine_pair_by_signature(
                partitions.R,
                workspace.left_signatures.codomain_hyperplanes,
                workspace.right_signatures.codomain_hyperplanes,
                workspace.pair_scratch);
            if (r_status == RefineStep::Inconsistent) {
                rollback(partitions, start);
                return RefineResult::Stop;
            }
            round_changed |= changed(r_status);
        }

        const RefineStep affine_status = propagate_determined_affine_maps(
            domain_dim,
            codomain_dim,
            left_function_table,
            right_function_table,
            partitions,
            workspace,
            solution_store,
            path);
        if (affine_status == RefineStep::Inconsistent) {
            rollback(partitions, start);
            return RefineResult::Stop;
        }
        if (affine_status == RefineStep::Solved) {
            return RefineResult::Stop;
        }
        round_changed |= changed(affine_status);

        if (!round_changed) {
            return RefineResult::Continue;
        }
    }
}

} // namespace affine
