#pragma once

#include "worker.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace affine {

struct ParallelSearchOptions {
    std::uint32_t worker_count = 1;
    std::size_t low_watermark = 64;
    std::uint32_t max_split_depth = 4;
    std::size_t max_enqueue_per_split = 64;
    std::uint64_t max_dfs_nodes = 0;
    DfsOptions dfs_options;
};

struct ParallelSearchResult {
    std::vector<WorkerStats> worker_stats;
};

inline void run_parallel_worker_loop(
    const DfsProblem& problem,
    WorkQueue& queue,
    PruningStore& pruner,
    SolutionStore& solutions,
    const ParallelSearchOptions& options,
    DomainStabilizerCache& domain_stabilizer_cache,
    std::atomic<bool>& done,
    std::atomic<std::uint64_t>* remaining_node_budget,
    WorkerStats& stats)
{
    SearchTask task = make_search_task(problem, options.dfs_options);

    while (true) {
        WorkItem item;
        if (queue.wait_pop(item, done)) {
            ++stats.popped;
            const auto work_start = std::chrono::steady_clock::now();
            run_worker_item(
                problem,
                item,
                task,
                queue,
                pruner,
                solutions,
                stats,
                options.low_watermark,
                options.max_split_depth,
                options.max_enqueue_per_split,
                &done,
                remaining_node_budget,
                &domain_stabilizer_cache);
            const auto work_end = std::chrono::steady_clock::now();
            stats.work_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    work_end - work_start)
                    .count());
            queue.complete(item);
            continue;
        }

        break;
    }
}

[[nodiscard]] inline ParallelSearchResult run_parallel_search(
    const DfsProblem& problem,
    WorkQueue& queue,
    PruningStore& pruner,
    SolutionStore& solutions,
    ParallelSearchOptions options = {})
{
    if (options.worker_count == 0) {
        options.worker_count = 1;
    }

    ParallelSearchResult result;
    result.worker_stats.resize(options.worker_count);

    std::atomic<bool> done = false;
    std::atomic<std::uint64_t> remaining_node_budget = options.max_dfs_nodes;
    std::atomic<std::uint64_t>* remaining_node_budget_ptr =
        options.max_dfs_nodes == 0 ? nullptr : &remaining_node_budget;
    DomainStabilizerCache domain_stabilizer_cache;
    std::vector<std::thread> threads;
    threads.reserve(options.worker_count);

    for (std::uint32_t worker = 0; worker < options.worker_count; ++worker) {
        threads.emplace_back(
            [&problem,
             &queue,
             &pruner,
             &solutions,
             &options,
             &domain_stabilizer_cache,
             &done,
             remaining_node_budget_ptr,
             &stats = result.worker_stats[worker]] {
                run_parallel_worker_loop(
                    problem,
                    queue,
                    pruner,
                    solutions,
                    options,
                    domain_stabilizer_cache,
                    done,
                    remaining_node_budget_ptr,
                    stats);
            });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    return result;
}

[[nodiscard]] inline WorkerStats total_worker_stats(
    const ParallelSearchResult& result)
{
    WorkerStats total;
    for (const WorkerStats& stats : result.worker_stats) {
        total.popped += stats.popped;
        total.skipped_dead += stats.skipped_dead;
        total.rebuild_dead += stats.rebuild_dead;
        total.searched += stats.searched;
        total.dfs_nodes += stats.dfs_nodes;
        total.dfs_leaves += stats.dfs_leaves;
        total.dfs_pruned += stats.dfs_pruned;
        total.work_ns += stats.work_ns;
    }
    return total;
}

} // namespace affine
