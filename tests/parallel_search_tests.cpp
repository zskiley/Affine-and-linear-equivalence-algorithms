#include "../src/parallel_search.hpp"

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

} // namespace

int main()
{
    test_parallel_search_skips_dead_item();
    test_parallel_search_runs_small_root_item();

    std::cout << "parallel search tests passed\n";
    return 0;
}
