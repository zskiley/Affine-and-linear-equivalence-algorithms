#include "../src/parallel_search.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint32_t> identity_table(std::uint32_t dim)
{
    const std::uint32_t count = affine::f2::point_count(dim);
    std::vector<std::uint32_t> table(count);
    for (std::uint32_t x = 0; x < count; ++x) {
        table[x] = x;
    }
    return table;
}

affine::DfsProblem identity_problem(
    std::uint32_t dim,
    const std::vector<std::uint32_t>& table)
{
    return affine::DfsProblem {
        .domain_dim = dim,
        .codomain_dim = dim,
        .left_function_table = table,
        .right_function_table = table,
    };
}

affine::BranchMove hyperplane_move(std::uint32_t left, std::uint32_t right)
{
    return affine::BranchMove {
        .kind = affine::BranchKind::DomainHyperplane,
        .left_object = left,
        .right_object = right,
    };
}

using CanonicalSolutions = std::vector<std::vector<std::uint32_t>>;

struct SearchOutcome {
    CanonicalSolutions solutions;
    affine::WorkerStats stats;
};

SearchOutcome run_identity_search(
    std::uint32_t dim,
    std::uint32_t worker_count,
    std::uint64_t max_dfs_nodes = 0)
{
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);
    affine::WorkQueue queue;
    queue.push(affine::WorkItem {});
    affine::PruningStore pruner;
    affine::SolutionStore solutions;
    affine::ParallelSearchOptions options;
    options.worker_count = worker_count;
    options.max_dfs_nodes = max_dfs_nodes;

    const affine::ParallelSearchResult result =
        affine::run_parallel_search(problem, queue, pruner, solutions, options);

    CanonicalSolutions canonical;
    for (const affine::Solution& solution : solutions.snapshot()) {
        canonical.push_back(affine::make_solution_key(solution).words);
    }
    std::sort(canonical.begin(), canonical.end());
    return SearchOutcome {
        .solutions = std::move(canonical),
        .stats = affine::total_worker_stats(result),
    };
}

void test_parallel_search_skips_dead_item()
{
    constexpr std::uint32_t dim = 2;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    const std::vector<affine::BranchMove> dead_path {
        hyperplane_move(1, 2),
    };

    affine::WorkQueue queue;
    queue.push(affine::WorkItem { .path = dead_path });

    affine::PruningStore pruner;
    pruner.add_dead_prefix(dead_path);

    affine::SolutionStore solutions;
    affine::ParallelSearchOptions options;
    options.worker_count = 2;

    const affine::ParallelSearchResult result =
        affine::run_parallel_search(problem, queue, pruner, solutions, options);
    const affine::WorkerStats stats = affine::total_worker_stats(result);

    assert(stats.popped == 1);
    assert(stats.skipped_dead == 1);
    assert(stats.searched == 0);
    assert(solutions.empty());
    assert(queue.approximate_size() == 0);
}

void test_parallel_search_runs_small_root_item()
{
    constexpr std::uint32_t dim = 2;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    affine::WorkQueue queue;
    queue.push(affine::WorkItem {});

    affine::PruningStore pruner;
    affine::SolutionStore solutions;
    affine::ParallelSearchOptions options;
    options.worker_count = 4;
    options.low_watermark = 8;
    options.max_split_depth = 2;

    const affine::ParallelSearchResult result =
        affine::run_parallel_search(problem, queue, pruner, solutions, options);
    const affine::WorkerStats stats = affine::total_worker_stats(result);

    assert(stats.popped > 0);
    assert(stats.searched > 0);
    assert(stats.dfs_nodes > 0);
    assert(!solutions.empty());
    assert(queue.approximate_size() == 0);
}

void test_parallel_matches_serial_and_cancels_cleanly()
{
    const SearchOutcome serial = run_identity_search(3, 1);
    const SearchOutcome parallel = run_identity_search(3, 4);
    assert(!serial.solutions.empty());
    assert(serial.solutions == parallel.solutions);

    const SearchOutcome limited = run_identity_search(3, 4, 1);
    assert(limited.stats.dfs_nodes == 1);
}

} // namespace

int main()
{
    test_parallel_search_skips_dead_item();
    test_parallel_search_runs_small_root_item();
    test_parallel_matches_serial_and_cancels_cleanly();

    std::cout << "parallel search tests passed\n";
    return 0;
}
