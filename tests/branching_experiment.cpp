#include "../src/dfs.cpp"

#include <array>
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

struct ProbeStats {
    std::uint64_t nodes = 0;
    std::uint64_t stops = 0;
    std::uint64_t continues = 0;
    std::uint64_t branches = 0;
    std::uint64_t pruned = 0;
    std::uint64_t no_branch_leaves = 0;
    std::uint64_t max_depth = 0;
    bool completed = true;
    std::array<std::uint64_t, 4> selected {};
    std::array<std::uint64_t, 4> child_moves {};
    std::array<std::uint64_t, 4> selected_size_sum {};
    std::array<std::uint32_t, 4> selected_size_max {};
};

std::size_t kind_index(affine::BranchKind kind)
{
    switch (kind) {
    case affine::BranchKind::DomainPoint:
        return 0;
    case affine::BranchKind::CodomainPoint:
        return 1;
    case affine::BranchKind::DomainHyperplane:
        return 2;
    case affine::BranchKind::CodomainHyperplane:
        return 3;
    }
    return 0;
}

const char* kind_name(affine::BranchKind kind)
{
    switch (kind) {
    case affine::BranchKind::DomainPoint:
        return "P";
    case affine::BranchKind::CodomainPoint:
        return "Q";
    case affine::BranchKind::DomainHyperplane:
        return "L";
    case affine::BranchKind::CodomainHyperplane:
        return "R";
    }
    return "?";
}

const char* kind_name(std::size_t index)
{
    static constexpr std::array<const char*, 4> names { "P", "Q", "L", "R" };
    return names[index];
}

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

bool probe_dfs(
    const affine::DfsProblem& problem,
    affine::SearchTask& task,
    affine::SolutionStore& solutions,
    ProbeStats& stats,
    const Clock::time_point deadline,
    std::uint64_t node_limit)
{
    if (Clock::now() >= deadline || stats.nodes >= node_limit) {
        stats.completed = false;
        return false;
    }

    ++stats.nodes;
    if (task.path.size() > stats.max_depth) {
        stats.max_depth = task.path.size();
    }

    const affine::SearchPartitionSnapshot entry = affine::snapshot(task.partitions);
    const affine::RefineResult result = affine::refine_until_stable(
        problem.domain_dim,
        problem.codomain_dim,
        problem.left_function_table,
        problem.right_function_table,
        task.partitions,
        task.workspace,
        &solutions,
        task.path);

    if (result == affine::RefineResult::Stop) {
        ++stats.stops;
        affine::rollback(task.partitions, entry);
        return true;
    }

    ++stats.continues;

    if (affine::is_complete(task.partitions)) {
        ++stats.stops;
        affine::rollback(task.partitions, entry);
        return true;
    }

    const affine::BranchCell branch_cell =
        affine::choose_branch_cell(task.partitions, task.options.branch_policy);
    if (branch_cell.left_cell == affine::Partition::npos) {
        ++stats.stops;
        ++stats.no_branch_leaves;
        affine::rollback(task.partitions, entry);
        return true;
    }

    const std::size_t selected_index = kind_index(branch_cell.kind);
    ++stats.selected[selected_index];
    stats.selected_size_sum[selected_index] += branch_cell.size;
    if (branch_cell.size > stats.selected_size_max[selected_index]) {
        stats.selected_size_max[selected_index] = branch_cell.size;
    }

    const affine::PartitionPair& pair =
        affine::branch_pair(task.partitions, branch_cell.kind);
    const affine::Partition::ObjectId left_object =
        pair.left.objects(branch_cell.left_cell).front();
    const std::span<const affine::Partition::ObjectId> right_objects =
        pair.right.objects(branch_cell.right_cell);

    bool completed = true;
    for (const affine::Partition::ObjectId right_object : right_objects) {
        if (Clock::now() >= deadline || stats.nodes >= node_limit) {
            stats.completed = false;
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
            ++stats.child_moves[selected_index];
            task.path.push_back(move);
            if (!probe_dfs(problem, task, solutions, stats, deadline, node_limit)) {
                completed = false;
            }
            task.path.pop_back();
        } else {
            ++stats.pruned;
        }

        affine::rollback(task.partitions, child);

        if (!completed) {
            break;
        }
    }

    affine::rollback(task.partitions, entry);
    return completed;
}

std::filesystem::path apn_folder(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t class_index)
{
    static constexpr std::uint32_t source_stride = 11;
    const std::uint32_t source_index = 1u + (class_index - 1u) * source_stride;

    std::string folder = "apn_";
    if (class_index < 10) {
        folder += '0';
    }
    folder += std::to_string(class_index);
    folder += "_f_";
    folder += std::to_string(source_index);
    return root / std::to_string(dim) / folder;
}

void print_distribution(const ProbeStats& stats)
{
    std::cout << " selected=";
    for (std::size_t i = 0; i < stats.selected.size(); ++i) {
        const double avg_size =
            stats.selected[i] == 0
                ? 0.0
                : static_cast<double>(stats.selected_size_sum[i])
                    / static_cast<double>(stats.selected[i]);
        std::cout << kind_name(i) << ':' << stats.selected[i]
                  << "/moves=" << stats.child_moves[i]
                  << "/avg_size=" << avg_size
                  << "/max_size=" << stats.selected_size_max[i];
        if (i + 1 != stats.selected.size()) {
            std::cout << ',';
        }
    }
}

void run_case(
    std::uint32_t dim,
    std::string_view label,
    const std::filesystem::path& left_path,
    const std::filesystem::path& right_path,
    int seconds,
    std::uint64_t node_limit)
{
    const std::vector<std::uint32_t> left = read_truth_table(left_path);
    const std::vector<std::uint32_t> right = read_truth_table(right_path);
    const std::uint32_t expected_size = affine::f2::point_count(dim);
    if (left.size() != expected_size || right.size() != expected_size) {
        throw std::runtime_error("truth-table size mismatch for dim " + std::to_string(dim));
    }

    const affine::DfsProblem problem {
        .domain_dim = dim,
        .codomain_dim = dim,
        .left_function_table = left,
        .right_function_table = right,
    };
    affine::SearchTask task = affine::make_search_task(problem);
    affine::SolutionStore solutions;
    ProbeStats stats;

    const auto start = Clock::now();
    (void)probe_dfs(
        problem,
        task,
        solutions,
        stats,
        start + std::chrono::seconds(seconds),
        node_limit);
    const auto end = Clock::now();

    std::cout << "case=" << label
              << " dim=" << dim
              << " completed=" << stats.completed
              << " ms=" << std::chrono::duration<double, std::milli>(end - start).count()
              << " nodes=" << stats.nodes
              << " stops=" << stats.stops
              << " continues=" << stats.continues
              << " branches=" << stats.branches
              << " no_branch_leaves=" << stats.no_branch_leaves
              << " max_depth=" << stats.max_depth
              << " solutions=" << solutions.size();
    print_distribution(stats);
    std::cout << '\n';
}

void run_representative_cases(const std::filesystem::path& root)
{
    for (const std::uint32_t dim : { 8u, 10u, 12u, 14u }) {
        const std::filesystem::path folder = apn_folder(root, dim, 1);
        run_case(
            dim,
            "apn01_affine01",
            folder / "function.tt",
            folder / "affine_01.tt",
            10,
            200000);
        run_case(
            dim,
            "apn01_self",
            folder / "function.tt",
            folder / "function.tt",
            10,
            200000);
    }

    run_case(
        8,
        "inverse8_affine01",
        root / "8" / "inverse" / "function.tt",
        root / "8" / "inverse" / "affine_01.tt",
        20,
        500000);
    run_case(
        8,
        "inverse8_self",
        root / "8" / "inverse" / "function.tt",
        root / "8" / "inverse" / "function.tt",
        20,
        500000);
    run_case(
        18,
        "inverse18_affine01",
        root / "18" / "inverse" / "function.tt",
        root / "18" / "inverse" / "affine_01.tt",
        20,
        5000);
    run_case(
        18,
        "inverse18_self",
        root / "18" / "inverse" / "function.tt",
        root / "18" / "inverse" / "function.tt",
        20,
        5000);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path root =
            argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("apn_original_function_folders");
        run_representative_cases(root);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
