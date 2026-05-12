#pragma once

#include "dfs.cpp"
#include "work.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

namespace affine {

enum class RebuildResult {
    Alive,
    Dead,
};

struct WorkerStats {
    std::uint64_t popped = 0;
    std::uint64_t skipped_dead = 0;
    std::uint64_t rebuild_dead = 0;
    std::uint64_t searched = 0;
    std::uint64_t dfs_nodes = 0;
    std::uint64_t dfs_leaves = 0;
    std::uint64_t dfs_pruned = 0;
    std::uint64_t work_ns = 0;
};

inline void reset_worker_task(const DfsProblem& problem, SearchTask& task)
{
    task.partitions.P.left.reset(f2::point_count(problem.domain_dim));
    task.partitions.P.right.reset(f2::point_count(problem.domain_dim));
    task.partitions.Q.left.reset(f2::point_count(problem.codomain_dim));
    task.partitions.Q.right.reset(f2::point_count(problem.codomain_dim));
    task.partitions.L.left.reset(f2::hyperplane_count(problem.domain_dim));
    task.partitions.L.right.reset(f2::hyperplane_count(problem.domain_dim));
    task.partitions.R.left.reset(f2::hyperplane_count(problem.codomain_dim));
    task.partitions.R.right.reset(f2::hyperplane_count(problem.codomain_dim));
    task.path.clear();
}

[[nodiscard]] inline RebuildResult rebuild_from_path(
    const DfsProblem& problem,
    SearchTask& task,
    std::span<const BranchMove> path,
    SolutionStore* solution_store = nullptr)
{
    reset_worker_task(problem, task);

    for (const BranchMove& move : path) {
        if (!apply_branch_move(problem, task.partitions, move)) {
            return RebuildResult::Dead;
        }
        task.path.push_back(move);
    }

    const RefineResult result = refine_until_stable(
        problem.domain_dim,
        problem.codomain_dim,
        problem.left_function_table,
        problem.right_function_table,
        task.partitions,
        task.workspace,
        solution_store,
        task.path);
    if (result == RefineResult::Stop) {
        return RebuildResult::Dead;
    }

    return RebuildResult::Alive;
}

inline void run_worker_item(
    const DfsProblem& problem,
    const WorkItem& item,
    SearchTask& task,
    WorkQueue& queue,
    PruningStore& pruner,
    SolutionStore& solutions,
    WorkerStats& stats,
    std::size_t low_watermark = 64,
    std::uint32_t max_split_depth = 4,
    std::size_t max_enqueue_per_split = 64,
    std::atomic<bool>* stop_requested = nullptr,
    std::atomic<std::uint64_t>* remaining_node_budget = nullptr,
    DomainStabilizerCache* shared_domain_stabilizer_cache = nullptr)
{
    if (stop_requested != nullptr
        && stop_requested->load(std::memory_order_acquire)) {
        return;
    }

    if (pruner.is_dead(item.path)) {
        ++stats.skipped_dead;
        return;
    }

    if (rebuild_from_path(problem, task, item.path, &solutions) == RebuildResult::Dead) {
        ++stats.rebuild_dead;
        return;
    }

    if (pruner.is_dead(task.path)) {
        ++stats.skipped_dead;
        return;
    }

    DfsContext context {
        .queue = &queue,
        .pruner = &pruner,
        .solution_store = &solutions,
        .low_watermark = low_watermark,
        .max_split_depth = max_split_depth,
        .max_enqueue_per_split = max_enqueue_per_split,
        .active_work_depth = item.path.size(),
        .domain_group = &solutions.a1_group(),
        .domain_stabilizer_cache = shared_domain_stabilizer_cache == nullptr
            ? &task.domain_stabilizer_cache
            : shared_domain_stabilizer_cache,
        .stop_requested = stop_requested,
        .remaining_node_budget = remaining_node_budget,
    };

    const DfsStats before = task.stats;
    dfs(problem, task, &context);
    ++stats.searched;
    stats.dfs_nodes += task.stats.nodes - before.nodes;
    stats.dfs_leaves += task.stats.leaves - before.leaves;
    stats.dfs_pruned += task.stats.pruned - before.pruned;
}

inline void run_serial_worker(
    const DfsProblem& problem,
    WorkQueue& queue,
    PruningStore& pruner,
    SolutionStore& solutions,
    WorkerStats& stats,
    DfsOptions options = {})
{
    SearchTask task = make_search_task(problem, options);
    WorkItem item;

    while (queue.try_pop(item)) {
        ++stats.popped;
        run_worker_item(problem, item, task, queue, pruner, solutions, stats);
        queue.complete(item);
    }
}

} // namespace affine
