#include "../src/worker.hpp"

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

void test_work_queue_fifo()
{
    affine::WorkQueue queue;
    queue.push(affine::WorkItem {
        .path = { hyperplane_move(1, 2) },
    });
    queue.push(affine::WorkItem {
        .path = { hyperplane_move(3, 4) },
    });

    assert(queue.approximate_size() == 2);

    affine::WorkItem item;
    assert(queue.try_pop(item));
    assert(item.path.size() == 1);
    assert(item.path.front() == hyperplane_move(1, 2));

    assert(queue.try_pop(item));
    assert(item.path.size() == 1);
    assert(item.path.front() == hyperplane_move(3, 4));

    assert(!queue.try_pop(item));
}

void test_work_queue_pops_shallowest_depth_first()
{
    affine::WorkQueue queue;
    queue.push(affine::WorkItem {
        .path = { hyperplane_move(1, 2), hyperplane_move(3, 4) },
    });
    queue.push(affine::WorkItem {
        .path = { hyperplane_move(5, 6) },
    });

    affine::WorkItem item;
    assert(queue.try_pop(item));
    assert(item.path.size() == 1);
    assert(item.path.front() == hyperplane_move(5, 6));
    queue.complete(item);

    assert(queue.try_pop(item));
    assert(item.path.size() == 2);
    assert(item.path.front() == hyperplane_move(1, 2));
    queue.complete(item);
}

void test_work_queue_tracks_unfinished_shallower_work()
{
    affine::WorkQueue queue;
    queue.push(affine::WorkItem {
        .path = { hyperplane_move(1, 2) },
    });

    assert(queue.has_unfinished_above(2));

    affine::WorkItem item;
    assert(queue.try_pop(item));
    assert(queue.has_unfinished_above(2));
    assert(!queue.has_unfinished_above(2, item.path.size()));

    queue.complete(item);
    assert(!queue.has_unfinished_above(2));
}

void test_rebuild_from_path()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    affine::SearchTask task = affine::make_search_task(problem);
    const std::vector<affine::BranchMove> path {
        hyperplane_move(1, 2),
    };

    assert(affine::rebuild_from_path(problem, task, path) == affine::RebuildResult::Alive);
    assert(task.path == path);
    assert(task.partitions.L.left.is_singleton(task.partitions.L.left.cell_of(1)));
    assert(task.partitions.L.right.is_singleton(task.partitions.L.right.cell_of(2)));
}

void test_rebuild_rejects_conflicting_path()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    affine::SearchTask task = affine::make_search_task(problem);
    const std::vector<affine::BranchMove> path {
        hyperplane_move(1, 2),
        hyperplane_move(1, 3),
    };

    assert(affine::rebuild_from_path(problem, task, path) == affine::RebuildResult::Dead);
}

void test_serial_worker_skips_dead_prefix()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    affine::WorkQueue queue;
    const std::vector<affine::BranchMove> path {
        hyperplane_move(1, 2),
    };
    queue.push(affine::WorkItem { .path = path });

    affine::PruningStore pruner;
    pruner.add_dead_prefix(path);

    affine::SolutionStore solutions;
    affine::WorkerStats stats;
    affine::run_serial_worker(problem, queue, pruner, solutions, stats);

    assert(stats.popped == 1);
    assert(stats.skipped_dead == 1);
    assert(stats.searched == 0);
}

void test_serial_worker_searches_item()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    affine::WorkQueue queue;
    queue.push(affine::WorkItem {});

    affine::PruningStore pruner;
    affine::SolutionStore solutions;
    affine::WorkerStats stats;
    affine::DfsOptions options;
    options.stop_after_first_leaf = true;

    affine::run_serial_worker(problem, queue, pruner, solutions, stats, options);

    assert(stats.popped == 1);
    assert(stats.skipped_dead == 0);
    assert(stats.rebuild_dead == 0);
    assert(stats.searched == 1);
    assert(stats.dfs_nodes > 0);
    assert(solutions.size() == 1);
}

void test_dfs_context_refills_queue()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);
    const affine::DfsProblem problem = identity_problem(dim, table);

    affine::SearchTask task = affine::make_search_task(problem);
    affine::WorkQueue queue;
    affine::PruningStore pruner;
    affine::SolutionStore solutions;
    affine::DfsContext context {
        .queue = &queue,
        .pruner = &pruner,
        .solution_store = &solutions,
        .low_watermark = 64,
        .max_split_depth = 1,
    };

    affine::dfs(problem, task, &context);

    assert(queue.approximate_size() > 0);

    affine::WorkItem item;
    assert(queue.try_pop(item));
    assert(item.path.size() == 1);
}

} // namespace

int main()
{
    test_work_queue_fifo();
    test_work_queue_pops_shallowest_depth_first();
    test_work_queue_tracks_unfinished_shallower_work();
    test_rebuild_from_path();
    test_rebuild_rejects_conflicting_path();
    test_serial_worker_skips_dead_prefix();
    test_serial_worker_searches_item();
    test_dfs_context_refills_queue();

    std::cout << "worker tests passed\n";
    return 0;
}
