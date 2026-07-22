#include "branching.hpp"
#include "dfs.hpp"
#include "profile.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace affine {

namespace {

class DfsLocalNodeTimer {
public:
    DfsLocalNodeTimer()
        : active_(profile::enabled.load(std::memory_order_acquire))
    {
        if (!active_) {
            return;
        }

        profile::counters.dfs_local_calls.fetch_add(1, std::memory_order_relaxed);
        running_ = true;
        start_ = Clock::now();
    }

    ~DfsLocalNodeTimer()
    {
        if (!active_) {
            return;
        }

        pause();
        profile::counters.dfs_local_ns.fetch_add(
            elapsed_ns_,
            std::memory_order_relaxed);
    }

    void pause()
    {
        if (!active_ || !running_) {
            return;
        }

        elapsed_ns_ += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now() - start_)
                .count());
        running_ = false;
    }

    void resume()
    {
        if (!active_ || running_) {
            return;
        }

        running_ = true;
        start_ = Clock::now();
    }

    DfsLocalNodeTimer(const DfsLocalNodeTimer&) = delete;
    DfsLocalNodeTimer& operator=(const DfsLocalNodeTimer&) = delete;

private:
    using Clock = std::chrono::steady_clock;

    bool active_ = false;
    bool running_ = false;
    std::uint64_t elapsed_ns_ = 0;
    Clock::time_point start_;
};

enum class DfsResult {
    Complete,
    Restart,
};

[[nodiscard]] bool should_restart_search(
    const SearchTask& task,
    const DfsContext* context,
    std::uint64_t observed_group_version)
{
    return !task.options.stop_after_first_leaf
        && search_group_version(context) != observed_group_version;
}

[[nodiscard]] bool should_stop_after_first_solution(
    const SearchTask& task,
    const DfsContext* context)
{
    return task.options.stop_after_first_leaf
        && context != nullptr
        && context->solution_store != nullptr
        && !context->solution_store->empty();
}

[[nodiscard]] DfsResult dfs_impl(
    const DfsProblem& problem,
    SearchTask& task,
    DfsContext* context)
{
    if (should_stop_after_first_solution(task, context)) {
        return DfsResult::Complete;
    }

    const std::uint64_t entry_group_version = search_group_version(context);
    ++task.stats.nodes;
    DfsLocalNodeTimer local_node_timer;

    const SearchPartitionSnapshot entry = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_entry_snapshot_ns);
        return snapshot(task.partitions);
    }();

    auto rollback_entry = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_rollback_ns);
        rollback(task.partitions, entry);
    };

    const RefineResult result = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_refine_ns);
        return refine_until_stable(
            problem.domain_dim,
            problem.codomain_dim,
            problem.left_function_table,
            problem.right_function_table,
            task.partitions,
            task.workspace,
            context == nullptr ? nullptr : context->solution_store,
            task.path,
            task.options.mode);
    }();

    if (result == RefineResult::Stop) {
        ++task.stats.leaves;
        rollback_entry();
        return should_restart_search(task, context, entry_group_version)
            ? DfsResult::Restart
            : DfsResult::Complete;
    }

    if (is_complete(task.partitions)) {
        ++task.stats.leaves;
        rollback_entry();
        return DfsResult::Complete;
    }

    const BranchCell branch_cell = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_branch_ns);
        return choose_branch_cell(task.partitions, task.options.branch_policy);
    }();
    if (branch_cell.left_cell == Partition::npos) {
        ++task.stats.leaves;
        rollback_entry();
        return DfsResult::Complete;
    }

    const PartitionPair& pair = branch_pair(task.partitions, branch_cell.kind);
    const Partition::ObjectId left_object =
        pair.left.objects(branch_cell.left_cell).front();
    const std::span<const Partition::ObjectId> right_objects =
        pair.right.objects(branch_cell.right_cell);
    std::vector<Partition::ObjectId> branch_right_objects;
    {
        profile::ScopedTimer timer(
            profile::counters.dfs_local_initial_candidates_ns);
        branch_right_objects = domain_branch_right_candidates(
            context,
            task.path,
            branch_cell.kind,
            right_objects);
    }

    auto search_child = [&](Partition::ObjectId right_object) {
        if (should_stop_after_first_solution(task, context)) {
            return DfsResult::Complete;
        }

        const SearchPartitionSnapshot child = snapshot(task.partitions);
        const BranchMove move {
            .kind = branch_cell.kind,
            .left_object = left_object,
            .right_object = right_object,
        };

        if (apply_branch_move(
                problem,
                task.partitions,
                move,
                task.options.mode)) {
            task.path.push_back(move);
            const DfsResult child_result = dfs_impl(problem, task, context);
            task.path.pop_back();
            if (child_result == DfsResult::Restart) {
                rollback(task.partitions, child);
                return DfsResult::Restart;
            }
        } else {
            ++task.stats.pruned;
        }

        rollback(task.partitions, child);
        return DfsResult::Complete;
    };

    for (std::size_t branch_index = 0; branch_index < branch_right_objects.size();) {
        Partition::ObjectId right_object = 0;
        [&] {
            profile::ScopedTimer timer(
                profile::counters.dfs_local_next_candidate_ns);
            right_object = branch_right_objects[branch_index];
            ++branch_index;
        }();

        local_node_timer.pause();
        const DfsResult child_result = search_child(right_object);
        local_node_timer.resume();
        if (child_result == DfsResult::Restart) {
            rollback_entry();
            return DfsResult::Restart;
        }

        if (should_stop_after_first_solution(task, context)) {
            break;
        }
    }

    rollback_entry();
    return DfsResult::Complete;
}

} // namespace

void dfs(const DfsProblem& problem, SearchTask& task, DfsContext* context)
{
    const SearchPartitionSnapshot restart_entry = snapshot(task.partitions);
    const std::size_t restart_path_size = task.path.size();

    while (true) {
        const DfsResult result = dfs_impl(problem, task, context);
        if (result == DfsResult::Complete) {
            return;
        }

        ++task.stats.restarts;
        rollback(task.partitions, restart_entry);
        task.path.resize(restart_path_size);

        if (should_stop_after_first_solution(task, context)) {
            return;
        }
    }
}

void dfs(
    const DfsProblem& problem,
    SearchTask& task,
    SolutionStore* solution_store)
{
    DfsContext context;
    context.solution_store = solution_store;
    context.paired_group = solution_store == nullptr
        ? nullptr
        : &solution_store->paired_group();
    context.paired_stabilizer_cache = &task.paired_stabilizer_cache;
    dfs(problem, task, &context);
}

} // namespace affine
