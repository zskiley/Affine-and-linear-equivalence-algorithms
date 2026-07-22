#pragma once

#include "dfs_types.hpp"
#include "solution_store.hpp"

namespace affine {

void dfs(const DfsProblem& problem, SearchTask& task, DfsContext* context);

void dfs(
    const DfsProblem& problem,
    SearchTask& task,
    SolutionStore* solution_store = nullptr);

} // namespace affine
