#include "../src/dfs.cpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct TimedStats {
    std::uint64_t nodes = 0;
    std::uint64_t stops = 0;
    std::uint64_t continues = 0;
    std::uint64_t branches = 0;
    std::uint64_t pruned = 0;
    std::uint64_t no_branch_leaves = 0;
    std::uint64_t max_depth = 0;
    std::uint64_t max_stop_depth = 0;
    double refine_ms = 0.0;
};

std::vector<std::uint32_t> read_truth_table(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open " + path.string());
    }

    std::vector<std::uint32_t> table;
    std::uint32_t value = 0;
    bool in_number = false;

    char ch = 0;
    while (input.get(ch)) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isdigit(uch)) {
            value = value * 10u + static_cast<std::uint32_t>(uch - '0');
            in_number = true;
        } else if (in_number) {
            table.push_back(value);
            value = 0;
            in_number = false;
        }
    }

    if (in_number) {
        table.push_back(value);
    }

    return table;
}

bool dfs_timed(
    const affine::DfsProblem& problem,
    affine::SearchTask& task,
    affine::SolutionStore& solutions,
    const Clock::time_point deadline,
    TimedStats& stats,
    const bool stop_on_solution,
    const affine::BranchPolicy branch_policy = affine::BranchPolicy::HyperplanesFirst)
{
    if (Clock::now() >= deadline || (stop_on_solution && !solutions.empty())) {
        return false;
    }

    ++stats.nodes;
    stats.max_depth = std::max<std::uint64_t>(stats.max_depth, task.path.size());

    const affine::SearchPartitionSnapshot entry = affine::snapshot(task.partitions);
    const auto refine_start = Clock::now();
    const affine::RefineResult result = affine::refine_until_stable(
        problem.domain_dim,
        problem.codomain_dim,
        problem.left_function_table,
        problem.right_function_table,
        task.partitions,
        task.workspace,
        &solutions,
        task.path);
    const auto refine_end = Clock::now();
    stats.refine_ms += std::chrono::duration<double, std::milli>(
        refine_end - refine_start)
                           .count();

    if (result == affine::RefineResult::Stop) {
        ++stats.stops;
        stats.max_stop_depth = std::max<std::uint64_t>(stats.max_stop_depth, task.path.size());
        affine::rollback(task.partitions, entry);
        return true;
    }

    ++stats.continues;

    if (affine::is_complete(task.partitions)) {
        ++stats.stops;
        stats.max_stop_depth = std::max<std::uint64_t>(stats.max_stop_depth, task.path.size());
        affine::rollback(task.partitions, entry);
        return true;
    }

    const affine::BranchCell branch_cell =
        affine::choose_branch_cell(task.partitions, branch_policy);
    if (branch_cell.left_cell == affine::Partition::npos) {
        ++stats.stops;
        ++stats.no_branch_leaves;
        stats.max_stop_depth = std::max<std::uint64_t>(stats.max_stop_depth, task.path.size());
        affine::rollback(task.partitions, entry);
        return true;
    }

    const affine::PartitionPair& pair =
        affine::branch_pair(task.partitions, branch_cell.kind);
    const affine::Partition::ObjectId left_object =
        pair.left.objects(branch_cell.left_cell).front();
    const std::span<const affine::Partition::ObjectId> right_objects =
        pair.right.objects(branch_cell.right_cell);
    const affine::DfsContext context {
        .domain_group = &solutions.a1_group(),
    };
    std::vector<affine::Partition::ObjectId> already_branched;
    std::vector<affine::Partition::ObjectId> branch_right_objects =
        affine::domain_branch_right_candidates(
            &context,
            task.path,
            branch_cell.kind,
            right_objects);

    bool completed = true;
    std::uint64_t observed_group_version =
        affine::domain_branch_group_version(&context, branch_cell.kind);
    std::size_t branch_index = 0;
    while (branch_index < branch_right_objects.size()) {
        const affine::Partition::ObjectId right_object =
            branch_right_objects[branch_index];
        ++branch_index;

        if (Clock::now() >= deadline || (stop_on_solution && !solutions.empty())) {
            completed = false;
            break;
        }

        const affine::SearchPartitionSnapshot child = affine::snapshot(task.partitions);
        const affine::BranchMove move {
            .kind = branch_cell.kind,
            .left_object = left_object,
            .right_object = right_object,
        };

        if (affine::apply_branch_move(problem, task.partitions, move)) {
            ++stats.branches;
            task.path.push_back(move);
            if (!dfs_timed(
                    problem,
                    task,
                    solutions,
                    deadline,
                    stats,
                    stop_on_solution,
                    branch_policy)) {
                completed = false;
            }
            task.path.pop_back();
        } else {
            ++stats.pruned;
        }

        affine::rollback(task.partitions, child);
        already_branched.push_back(right_object);

        if (!completed) {
            break;
        }

        const std::uint64_t current_group_version =
            affine::domain_branch_group_version(&context, branch_cell.kind);
        if (current_group_version == observed_group_version) {
            continue;
        }

        observed_group_version = current_group_version;
        branch_right_objects = affine::domain_branch_right_candidates(
            &context,
            task.path,
            branch_cell.kind,
            right_objects,
            already_branched);
        branch_index = 0;
    }

    affine::rollback(task.partitions, entry);
    return completed;
}

void print_stats(
    std::string_view label,
    const double elapsed_ms,
    const TimedStats& stats,
    const affine::SolutionStore& solutions)
{
    std::cout << label
              << " ms=" << elapsed_ms
              << " nodes=" << stats.nodes
              << " stops=" << stats.stops
              << " continues=" << stats.continues
              << " branches=" << stats.branches
              << " pruned=" << stats.pruned
              << " no_branch_leaves=" << stats.no_branch_leaves
              << " max_depth=" << stats.max_depth
              << " max_stop_depth=" << stats.max_stop_depth
              << " solutions=" << solutions.size()
              << " a1_group=" << solutions.a1_group().size()
              << " refine_ms=" << stats.refine_ms;
}

void print_solution_depths(const affine::SolutionStore& solutions)
{
    const std::vector<affine::Solution> snapshot = solutions.snapshot();
    if (snapshot.empty()) {
        std::cout << " solution_depths=none";
        return;
    }

    std::uint64_t min_depth = snapshot.front().path.size();
    std::uint64_t max_depth = snapshot.front().path.size();
    std::uint64_t total_depth = 0;
    for (const affine::Solution& solution : snapshot) {
        const std::uint64_t depth = solution.path.size();
        min_depth = std::min(min_depth, depth);
        max_depth = std::max(max_depth, depth);
        total_depth += depth;
    }

    std::cout << " solution_depth_min=" << min_depth
              << " solution_depth_max=" << max_depth
              << " solution_depth_avg="
              << static_cast<double>(total_depth) / static_cast<double>(snapshot.size());
}

void run_plain_first_solution(
    std::string_view label,
    const affine::DfsProblem& problem,
    const int seconds)
{
    affine::SearchTask task = affine::make_search_task(problem);
    affine::SolutionStore solutions;
    TimedStats stats;

    const auto start = Clock::now();
    (void)dfs_timed(
        problem,
        task,
        solutions,
        start + std::chrono::seconds(seconds),
        stats,
        true);
    const auto end = Clock::now();

    print_stats(label, std::chrono::duration<double, std::milli>(end - start).count(), stats, solutions);
    print_solution_depths(solutions);
    std::cout << '\n';
}

void run_plain_full(
    std::string_view label,
    const affine::DfsProblem& problem,
    const int seconds,
    const affine::BranchPolicy branch_policy = affine::BranchPolicy::HyperplanesFirst)
{
    affine::SearchTask task = affine::make_search_task(problem);
    affine::SolutionStore solutions;
    TimedStats stats;

    const auto start = Clock::now();
    const bool completed = dfs_timed(
        problem,
        task,
        solutions,
        start + std::chrono::seconds(seconds),
        stats,
        false,
        branch_policy);
    const auto end = Clock::now();

    std::cout << label
              << " completed=" << completed
              << " ms=" << std::chrono::duration<double, std::milli>(end - start).count()
              << " nodes=" << stats.nodes
              << " stops=" << stats.stops
              << " continues=" << stats.continues
              << " branches=" << stats.branches
              << " pruned=" << stats.pruned
              << " no_branch_leaves=" << stats.no_branch_leaves
              << " max_depth=" << stats.max_depth
              << " max_stop_depth=" << stats.max_stop_depth
              << " solutions=" << solutions.size()
              << " a1_group=" << solutions.a1_group().size()
              << " refine_ms=" << stats.refine_ms;
    print_solution_depths(solutions);
    std::cout << '\n';
}

void run_plain_full_with_a1_group(
    std::string_view label,
    const affine::DfsProblem& problem,
    const int seconds,
    const affine::BranchPolicy branch_policy = affine::BranchPolicy::HyperplanesFirst)
{
    affine::SearchTask task = affine::make_search_task(problem);
    affine::SolutionStore solutions(
        affine::SolutionStore::Options {
            .record_a1_automorphisms = true,
        });
    TimedStats stats;

    const auto start = Clock::now();
    const bool completed = dfs_timed(
        problem,
        task,
        solutions,
        start + std::chrono::seconds(seconds),
        stats,
        false,
        branch_policy);
    const auto end = Clock::now();

    std::cout << label
              << " completed=" << completed
              << " ms=" << std::chrono::duration<double, std::milli>(end - start).count()
              << " nodes=" << stats.nodes
              << " stops=" << stats.stops
              << " continues=" << stats.continues
              << " branches=" << stats.branches
              << " pruned=" << stats.pruned
              << " no_branch_leaves=" << stats.no_branch_leaves
              << " max_depth=" << stats.max_depth
              << " max_stop_depth=" << stats.max_stop_depth
              << " solutions=" << solutions.size()
              << " a1_group=" << solutions.a1_group().size()
              << " refine_ms=" << stats.refine_ms;
    print_solution_depths(solutions);
    std::cout << '\n';
}

void run_fair_first_level(
    std::string_view label,
    const affine::DfsProblem& problem,
    const int task_slice_ms,
    const int total_seconds)
{
    affine::SearchTask root = affine::make_search_task(problem);
    affine::SolutionStore solutions;
    TimedStats stats;

    const auto start = Clock::now();
    const auto total_deadline = start + std::chrono::seconds(total_seconds);

    const affine::RefineResult root_result = affine::refine_until_stable(
        problem.domain_dim,
        problem.codomain_dim,
        problem.left_function_table,
        problem.right_function_table,
        root.partitions,
        root.workspace,
        &solutions,
        root.path);

    if (root_result == affine::RefineResult::Stop || !solutions.empty()) {
        const auto end = Clock::now();
        print_stats(label, std::chrono::duration<double, std::milli>(end - start).count(), stats, solutions);
        print_solution_depths(solutions);
        std::cout << '\n';
        return;
    }

    const affine::BranchCell branch_cell = affine::choose_branch_cell(root.partitions);
    if (branch_cell.left_cell == affine::Partition::npos) {
        const auto end = Clock::now();
        print_stats(label, std::chrono::duration<double, std::milli>(end - start).count(), stats, solutions);
        print_solution_depths(solutions);
        std::cout << '\n';
        return;
    }

    const affine::PartitionPair& pair =
        affine::branch_pair(root.partitions, branch_cell.kind);
    const affine::Partition::ObjectId left_object =
        pair.left.objects(branch_cell.left_cell).front();
    const std::span<const affine::Partition::ObjectId> right_objects =
        pair.right.objects(branch_cell.right_cell);

    std::uint32_t tasks_tried = 0;
    affine::Partition::ObjectId winning_right = affine::Partition::npos;

    for (const affine::Partition::ObjectId right_object : right_objects) {
        if (Clock::now() >= total_deadline || !solutions.empty()) {
            break;
        }

        affine::SearchTask task;
        task.partitions = root.partitions;

        const affine::BranchMove move {
            .kind = branch_cell.kind,
            .left_object = left_object,
            .right_object = right_object,
        };

        if (!affine::apply_branch_move(problem, task.partitions, move)) {
            ++stats.pruned;
            continue;
        }

        task.path.push_back(move);
        ++tasks_tried;
        ++stats.branches;

        const auto slice_deadline = std::min(
            total_deadline,
            Clock::now() + std::chrono::milliseconds(task_slice_ms));
        (void)dfs_timed(problem, task, solutions, slice_deadline, stats, true);

        if (!solutions.empty()) {
            winning_right = right_object;
            break;
        }
    }

    const auto end = Clock::now();
    std::cout << label
              << " ms=" << std::chrono::duration<double, std::milli>(end - start).count()
              << " tasks_tried=" << tasks_tried
              << " winning_right=" << winning_right
              << " nodes=" << stats.nodes
              << " stops=" << stats.stops
              << " continues=" << stats.continues
              << " branches=" << stats.branches
              << " pruned=" << stats.pruned
              << " solutions=" << solutions.size()
              << " refine_ms=" << stats.refine_ms
              << '\n';
}

void run_constrained_first_branch(
    std::string_view label,
    const affine::DfsProblem& problem,
    const affine::Partition::ObjectId requested_right_object,
    const int total_seconds,
    const bool stop_on_solution = true)
{
    affine::SearchTask task = affine::make_search_task(problem);
    affine::SolutionStore solutions;
    TimedStats stats;

    const auto start = Clock::now();
    const auto total_deadline = start + std::chrono::seconds(total_seconds);

    const affine::RefineResult root_result = affine::refine_until_stable(
        problem.domain_dim,
        problem.codomain_dim,
        problem.left_function_table,
        problem.right_function_table,
        task.partitions,
        task.workspace,
        &solutions,
        task.path);

    if (root_result == affine::RefineResult::Stop || !solutions.empty()) {
        const auto end = Clock::now();
        print_stats(label, std::chrono::duration<double, std::milli>(end - start).count(), stats, solutions);
        print_solution_depths(solutions);
        std::cout << '\n';
        return;
    }

    const affine::BranchCell branch_cell = affine::choose_branch_cell(task.partitions);
    if (branch_cell.left_cell == affine::Partition::npos) {
        const auto end = Clock::now();
        print_stats(label, std::chrono::duration<double, std::milli>(end - start).count(), stats, solutions);
        print_solution_depths(solutions);
        std::cout << '\n';
        return;
    }

    const affine::PartitionPair& pair =
        affine::branch_pair(task.partitions, branch_cell.kind);
    const affine::Partition::ObjectId left_object =
        pair.left.objects(branch_cell.left_cell).front();

    const affine::BranchMove move {
        .kind = branch_cell.kind,
        .left_object = left_object,
        .right_object = requested_right_object,
    };

    if (!affine::apply_branch_move(problem, task.partitions, move)) {
        ++stats.pruned;
    } else {
        ++stats.branches;
        task.path.push_back(move);
        const bool completed = dfs_timed(problem, task, solutions, total_deadline, stats, stop_on_solution);
        std::cout << label
                  << " first_left=" << left_object
                  << " first_right=" << requested_right_object
                  << " completed=" << completed
                  << " ms=" << std::chrono::duration<double, std::milli>(Clock::now() - start).count()
                  << " nodes=" << stats.nodes
                  << " stops=" << stats.stops
                  << " continues=" << stats.continues
                  << " branches=" << stats.branches
                  << " pruned=" << stats.pruned
                  << " no_branch_leaves=" << stats.no_branch_leaves
                  << " max_depth=" << stats.max_depth
                  << " max_stop_depth=" << stats.max_stop_depth
                  << " solutions=" << solutions.size()
                  << " refine_ms=" << stats.refine_ms;
        print_solution_depths(solutions);
        std::cout << '\n';
        return;
    }

    const auto end = Clock::now();
    std::cout << label
              << " first_left=" << left_object
              << " first_right=" << requested_right_object
              << " ms=" << std::chrono::duration<double, std::milli>(end - start).count()
              << " nodes=" << stats.nodes
              << " stops=" << stats.stops
              << " continues=" << stats.continues
              << " branches=" << stats.branches
              << " pruned=" << stats.pruned
              << " no_branch_leaves=" << stats.no_branch_leaves
              << " max_depth=" << stats.max_depth
              << " max_stop_depth=" << stats.max_stop_depth
              << " solutions=" << solutions.size()
              << " refine_ms=" << stats.refine_ms;
    print_solution_depths(solutions);
    std::cout << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path root =
            argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("apn_original_function_folders");
        const std::filesystem::path inverse8 = root / "8" / "inverse";
        const std::filesystem::path inverse18 = root / "18" / "inverse";

        const std::vector<std::uint32_t> inv8 = read_truth_table(inverse8 / "function.tt");
        const std::vector<std::uint32_t> inv8_affine = read_truth_table(inverse8 / "affine_01.tt");
        const std::vector<std::uint32_t> inv18 = read_truth_table(inverse18 / "function.tt");
        const std::vector<std::uint32_t> inv18_affine = read_truth_table(inverse18 / "affine_01.tt");

        const affine::DfsProblem self8 {
            .domain_dim = 8,
            .codomain_dim = 8,
            .left_function_table = inv8,
            .right_function_table = inv8,
        };
        const affine::DfsProblem equiv8 {
            .domain_dim = 8,
            .codomain_dim = 8,
            .left_function_table = inv8,
            .right_function_table = inv8_affine,
        };
        const affine::DfsProblem self18 {
            .domain_dim = 18,
            .codomain_dim = 18,
            .left_function_table = inv18,
            .right_function_table = inv18,
        };
        const affine::DfsProblem equiv18 {
            .domain_dim = 18,
            .codomain_dim = 18,
            .left_function_table = inv18,
            .right_function_table = inv18_affine,
        };

        if (argc >= 3 && std::string_view(argv[2]) == "branch252_full") {
            run_constrained_first_branch(
                "constrained_full inverse8_equiv",
                equiv8,
                252,
                60,
                false);
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[2]) == "full8_equiv") {
            const int seconds = argc >= 4 ? std::stoi(argv[3]) : 60;
            run_plain_full(
                "plain_full inverse8_equiv hyperplanes_first",
                equiv8,
                seconds,
                affine::BranchPolicy::HyperplanesFirst);
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[2]) == "full8_equiv_smallest_any") {
            const int seconds = argc >= 4 ? std::stoi(argv[3]) : 60;
            run_plain_full(
                "plain_full inverse8_equiv smallest_any",
                equiv8,
                seconds,
                affine::BranchPolicy::SmallestAny);
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[2]) == "full8_equiv_hyperplanes_only") {
            const int seconds = argc >= 4 ? std::stoi(argv[3]) : 60;
            run_plain_full(
                "plain_full inverse8_equiv hyperplanes_first",
                equiv8,
                seconds,
                affine::BranchPolicy::HyperplanesFirst);
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[2]) == "full8_self_compare") {
            const int seconds = argc >= 4 ? std::stoi(argv[3]) : 10;
            run_plain_full(
                "plain_full inverse8_self baseline",
                self8,
                seconds,
                affine::BranchPolicy::HyperplanesFirst);
            run_plain_full_with_a1_group(
                "plain_full inverse8_self a1_group",
                self8,
                seconds,
                affine::BranchPolicy::HyperplanesFirst);
            return 0;
        }

        run_plain_first_solution("plain_first_solution inverse8_self", self8, 10);
        run_plain_first_solution("plain_first_solution inverse8_equiv", equiv8, 10);
        run_constrained_first_branch("constrained inverse8_equiv", equiv8, 0, 20);
        run_constrained_first_branch("constrained inverse8_equiv", equiv8, 252, 20);
        run_fair_first_level("fair_first_level inverse8_equiv slice100ms", equiv8, 100, 30);
        run_plain_first_solution("plain_first_solution inverse18_self", self18, 20);
        run_plain_first_solution("plain_first_solution inverse18_equiv", equiv18, 20);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
