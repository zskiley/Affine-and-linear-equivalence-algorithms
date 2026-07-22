#pragma once

#include "parallel_search.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace affine::api {

enum class EquivalenceKind {
    Affine,
    Linear,
};

struct EquivalenceProblem {
    std::uint32_t domain_dimension = 0;
    std::uint32_t codomain_dimension = 0;
    std::span<const std::uint32_t> left_table;
    std::span<const std::uint32_t> right_table;
};

struct SearchOptions {
    std::uint32_t threads = 0;
    EquivalenceKind kind = EquivalenceKind::Affine;
};

inline void validate_problem(const EquivalenceProblem& problem)
{
    if (problem.domain_dimension >= 32 || problem.codomain_dimension >= 32) {
        throw std::invalid_argument("dimensions must be less than 32");
    }

    const std::uint64_t table_size =
        std::uint64_t { 1 } << problem.domain_dimension;
    if (problem.left_table.size() != table_size
        || problem.right_table.size() != table_size) {
        throw std::invalid_argument(
            "truth tables must contain 2^domain_dimension values");
    }

    const std::uint64_t codomain_size =
        std::uint64_t { 1 } << problem.codomain_dimension;
    const auto out_of_range = [codomain_size](std::uint32_t value) {
        return value >= codomain_size;
    };
    if (std::any_of(
            problem.left_table.begin(), problem.left_table.end(), out_of_range)
        || std::any_of(
            problem.right_table.begin(), problem.right_table.end(), out_of_range)) {
        throw std::invalid_argument(
            "truth-table values do not fit the codomain dimension");
    }
}

[[nodiscard]] inline std::vector<Solution> find_equivalences(
    const EquivalenceProblem& problem,
    const SearchOptions& options = {})
{
    validate_problem(problem);

    std::uint32_t workers = options.threads;
    if (workers == 0) {
        workers = std::max(1u, std::thread::hardware_concurrency());
    }

    const DfsProblem dfs_problem {
        .domain_dim = problem.domain_dimension,
        .codomain_dim = problem.codomain_dimension,
        .left_function_table = problem.left_table,
        .right_function_table = problem.right_table,
    };

    WorkItem root;
    if (options.kind == EquivalenceKind::Linear) {
        // An affine map is linear exactly when it fixes zero.
        root.path = {
            BranchMove { BranchKind::DomainPoint, 0, 0 },
            BranchMove { BranchKind::CodomainPoint, 0, 0 },
        };
    }

    WorkQueue queue;
    queue.push(std::move(root));
    PruningStore pruner;
    SolutionStore solutions;

    ParallelSearchOptions parallel_options;
    parallel_options.worker_count = workers;
    parallel_options.low_watermark = 4u * workers;
    parallel_options.max_enqueue_per_split = 4u * workers;
    (void)run_parallel_search(
        dfs_problem, queue, pruner, solutions, parallel_options);

    std::vector<Solution> result = solutions.snapshot();
    for (Solution& solution : result) {
        solution.path.clear();
    }
    return result;
}

} // namespace affine::api
