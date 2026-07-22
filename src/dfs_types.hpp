#pragma once

#include "orbit_pruning.hpp"
#include "refinement.hpp"
#include "search_types.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace affine {

struct DfsProblem {
    std::uint32_t domain_dim = 0;
    std::uint32_t codomain_dim = 0;
    std::span<const std::uint32_t> left_function_table;
    std::span<const std::uint32_t> right_function_table;
};

struct DfsStats {
    std::uint64_t nodes = 0;
    std::uint64_t pruned = 0;
    std::uint64_t leaves = 0;
    std::uint64_t restarts = 0;
};

enum class BranchPolicy {
    SmallestAny,
    HyperplanesFirst,
    HyperplanesOnly,
    DomainPointFirst,
    CodomainPointFirst,
    DomainHyperplaneFirst,
    CodomainHyperplaneFirst,
};

struct DfsOptions {
    BranchPolicy branch_policy = BranchPolicy::HyperplanesFirst;
    bool stop_after_first_leaf = false;
    EquivalenceMode mode = EquivalenceMode::Affine;
};

struct SearchTask {
    SearchPartitions partitions;
    RefinementWorkspace workspace;
    std::vector<BranchMove> path;
    DfsStats stats;
    DfsOptions options;
    PairedStabilizerCache paired_stabilizer_cache;
};

struct BranchCell {
    BranchKind kind = BranchKind::DomainPoint;
    Partition::CellId left_cell = Partition::npos;
    Partition::CellId right_cell = Partition::npos;
    Partition::Index size = std::numeric_limits<Partition::Index>::max();
};

[[nodiscard]] inline SearchTask make_search_task(
    const DfsProblem& problem,
    DfsOptions options = {})
{
    SearchTask task;
    task.partitions = make_initial_partitions(
        problem.domain_dim,
        problem.codomain_dim,
        options.mode);
    task.options = options;
    return task;
}

[[nodiscard]] inline SearchTask make_search_task(
    const DfsProblem& problem,
    bool stop_after_first_leaf)
{
    return make_search_task(
        problem,
        DfsOptions {
            .stop_after_first_leaf = stop_after_first_leaf,
        });
}

} // namespace affine
