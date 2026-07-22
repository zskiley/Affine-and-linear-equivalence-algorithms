#include "../src/dfs.hpp"
#include "../src/profile.hpp"

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

struct CliOptions {
    std::string command;
    affine::EquivalenceMode mode = affine::EquivalenceMode::Affine;
    affine::BranchPolicy branch_policy = affine::BranchPolicy::HyperplanesOnly;
    std::filesystem::path function_path;
    std::filesystem::path left_path;
    std::filesystem::path right_path;
    bool profile = false;
};

struct RunResult {
    affine::DfsStats stats;
    std::size_t solutions = 0;
    std::size_t a1_generators = 0;
    std::size_t a2_generators = 0;
    double elapsed_ms = 0.0;
};

[[nodiscard]] double ms_between(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

[[nodiscard]] std::vector<std::uint32_t> read_truth_table(
    const std::filesystem::path& path)
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

    if (table.empty()) {
        throw std::runtime_error("truth table is empty: " + path.string());
    }
    return table;
}

[[nodiscard]] bool is_power_of_two(std::size_t value)
{
    return value != 0 && (value & (value - 1u)) == 0;
}

[[nodiscard]] std::uint32_t log2_exact(std::size_t value)
{
    if (!is_power_of_two(value)) {
        throw std::runtime_error("truth table length must be a power of two");
    }

    std::uint32_t result = 0;
    while (value > 1) {
        value >>= 1u;
        ++result;
    }
    return result;
}

void validate_n_to_n_table(
    const std::vector<std::uint32_t>& table,
    std::uint32_t dim,
    std::string_view name)
{
    if (dim >= 32) {
        throw std::runtime_error("dimensions >= 32 are not supported");
    }

    const std::uint32_t limit = std::uint32_t { 1 } << dim;
    for (const std::uint32_t value : table) {
        if (value >= limit) {
            throw std::runtime_error(
                std::string(name) + " contains value outside F_2^n");
        }
    }
}

[[nodiscard]] affine::EquivalenceMode parse_mode(std::string_view value)
{
    if (value == "affine") {
        return affine::EquivalenceMode::Affine;
    }
    if (value == "linear") {
        return affine::EquivalenceMode::Linear;
    }
    throw std::runtime_error("unknown mode: " + std::string(value));
}

[[nodiscard]] const char* mode_name(affine::EquivalenceMode mode)
{
    return mode == affine::EquivalenceMode::Linear ? "linear" : "affine";
}

[[nodiscard]] affine::BranchPolicy parse_branch_policy(std::string_view value)
{
    if (value == "smallest_any") {
        return affine::BranchPolicy::SmallestAny;
    }
    if (value == "hyperplanes_first") {
        return affine::BranchPolicy::HyperplanesFirst;
    }
    if (value == "hyperplanes_only") {
        return affine::BranchPolicy::HyperplanesOnly;
    }
    if (value == "domain_point_first") {
        return affine::BranchPolicy::DomainPointFirst;
    }
    if (value == "codomain_point_first") {
        return affine::BranchPolicy::CodomainPointFirst;
    }
    if (value == "domain_hyperplane_first") {
        return affine::BranchPolicy::DomainHyperplaneFirst;
    }
    if (value == "codomain_hyperplane_first") {
        return affine::BranchPolicy::CodomainHyperplaneFirst;
    }
    throw std::runtime_error("unknown branch policy: " + std::string(value));
}

[[nodiscard]] const char* branch_policy_name(affine::BranchPolicy policy)
{
    switch (policy) {
    case affine::BranchPolicy::SmallestAny:
        return "smallest_any";
    case affine::BranchPolicy::HyperplanesFirst:
        return "hyperplanes_first";
    case affine::BranchPolicy::HyperplanesOnly:
        return "hyperplanes_only";
    case affine::BranchPolicy::DomainPointFirst:
        return "domain_point_first";
    case affine::BranchPolicy::CodomainPointFirst:
        return "codomain_point_first";
    case affine::BranchPolicy::DomainHyperplaneFirst:
        return "domain_hyperplane_first";
    case affine::BranchPolicy::CodomainHyperplaneFirst:
        return "codomain_hyperplane_first";
    }
    return "unknown";
}

[[nodiscard]] RunResult run_dfs(
    std::uint32_t dim,
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right,
    const CliOptions& options,
    affine::SolutionStore& solutions,
    bool stop_after_first_leaf)
{
    const affine::DfsProblem problem {
        .domain_dim = dim,
        .codomain_dim = dim,
        .left_function_table = left,
        .right_function_table = right,
    };
    affine::SearchTask task = affine::make_search_task(
        problem,
        affine::DfsOptions {
            .branch_policy = options.branch_policy,
            .stop_after_first_leaf = stop_after_first_leaf,
            .mode = options.mode,
        });

    const auto start = Clock::now();
    affine::dfs(problem, task, &solutions);
    const auto end = Clock::now();

    return RunResult {
        .stats = task.stats,
        .solutions = solutions.size(),
        .a1_generators = solutions.a1_group().size(),
        .a2_generators = solutions.a2_group().size(),
        .elapsed_ms = ms_between(start, end),
    };
}

void seed_from_target_self_group(
    const affine::SolutionStore& target_self,
    affine::SolutionStore& equivalence_solutions)
{
    for (const affine::group::AffineGroupGenerator& generator :
         target_self.a1_group().snapshot().generators) {
        (void)equivalence_solutions.a1_group().add_generator(generator.map);
    }
    for (const affine::group::AffineGroupGenerator& generator :
         target_self.a2_group().snapshot().generators) {
        (void)equivalence_solutions.a2_group().add_generator(generator.map);
    }
    for (const affine::group::PairedAffineGroupGenerator& generator :
         target_self.paired_group().snapshot().generators) {
        (void)equivalence_solutions.paired_group().add_generator(
            generator.domain.map,
            generator.codomain.map);
    }
}

[[noreturn]] void usage()
{
    std::cerr
        << "Usage:\n"
        << "  affine_equiv self --function F.tt [--mode affine|linear]\n"
        << "  affine_equiv equiv --left F.tt --right G.tt [--mode affine|linear]\n"
        << "\n\n"
        << "  affine_equiv seeded-equiv --left F.tt --right G.tt\n"
        << "               [--mode affine|linear]\n\n"
        << "Options:\n"
        << "  --branch POLICY    hyperplanes_only, hyperplanes_first, smallest_any,\n"
        << "                     domain_point_first, codomain_point_first,\n"
        << "                     domain_hyperplane_first, codomain_hyperplane_first\n"
        << "  --profile          print selected profiling counters\n"
        << "  --help             show this help text\n";
    throw std::runtime_error("invalid command line");
}

[[nodiscard]] CliOptions parse_args(int argc, char** argv)
{
    if (argc < 2) {
        usage();
    }

    CliOptions options;
    options.command = argv[1];
    if (options.command == "--help" || options.command == "-h") {
        usage();
    }
    if (options.command != "self"
        && options.command != "equiv"
        && options.command != "seeded-equiv") {
        usage();
    }

    for (int index = 2; index < argc; ++index) {
        const std::string_view arg = argv[index];
        auto require_value = [&](std::string_view option) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error("missing value for " + std::string(option));
            }
            ++index;
            return argv[index];
        };

        if (arg == "--mode") {
            options.mode = parse_mode(require_value(arg));
        } else if (arg == "--branch") {
            options.branch_policy = parse_branch_policy(require_value(arg));
        } else if (arg == "--function") {
            options.function_path = require_value(arg);
        } else if (arg == "--left") {
            options.left_path = require_value(arg);
        } else if (arg == "--right") {
            options.right_path = require_value(arg);
        } else if (arg == "--profile") {
            options.profile = true;
        } else if (arg == "--help" || arg == "-h") {
            usage();
        } else {
            throw std::runtime_error("unknown option: " + std::string(arg));
        }
    }

    if (options.command == "self" && options.function_path.empty()) {
        throw std::runtime_error("self requires --function");
    }
    if ((options.command == "equiv" || options.command == "seeded-equiv")
        && (options.left_path.empty() || options.right_path.empty())) {
        throw std::runtime_error(options.command + " requires --left and --right");
    }
    return options;
}

void print_result(std::string_view prefix, const RunResult& result)
{
    std::cout << prefix << "elapsed_ms=" << result.elapsed_ms << '\n';
    std::cout << prefix << "nodes=" << result.stats.nodes << '\n';
    std::cout << prefix << "leaves=" << result.stats.leaves << '\n';
    std::cout << prefix << "pruned=" << result.stats.pruned << '\n';
    std::cout << prefix << "restarts=" << result.stats.restarts << '\n';
    std::cout << prefix << "solutions=" << result.solutions << '\n';
    std::cout << prefix << "a1_generators=" << result.a1_generators << '\n';
    std::cout << prefix << "a2_generators=" << result.a2_generators << '\n';
}

void print_profile()
{
    const affine::profile::Snapshot snapshot = affine::profile::snapshot();
    auto ms = [](std::uint64_t ns) {
        return static_cast<double>(ns) / 1'000'000.0;
    };

    std::cout << "profile_refine_calls=" << snapshot.refine_calls << '\n';
    std::cout << "profile_refine_ms=" << ms(snapshot.refine_ns) << '\n';
    std::cout << "profile_signature_calls=" << snapshot.signature_calls << '\n';
    std::cout << "profile_signature_ms=" << ms(snapshot.signature_ns) << '\n';
    std::cout << "profile_pair_refine_calls=" << snapshot.pair_refine_calls << '\n';
    std::cout << "profile_affine_propagate_ms="
              << ms(snapshot.affine_propagate_ns) << '\n';
    std::cout << "profile_domain_candidate_calls="
              << snapshot.domain_candidate_calls << '\n';
    std::cout << "profile_stabilizer_calls=" << snapshot.stabilizer_calls << '\n';
    std::cout << "profile_orbit_partition_calls="
              << snapshot.orbit_partition_calls << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const CliOptions options = parse_args(argc, argv);

        affine::profile::reset();
        affine::profile::set_enabled(options.profile);

        if (options.command == "self") {
            std::vector<std::uint32_t> table =
                read_truth_table(options.function_path);
            const std::uint32_t dim = log2_exact(table.size());
            validate_n_to_n_table(table, dim, "function");

            affine::SolutionStore solutions(
                affine::SolutionStore::Options {
                    .record_a1_automorphisms = true,
                });
            const RunResult result =
                run_dfs(dim, table, table, options, solutions, false);

            std::cout << "task=self\n";
            std::cout << "mode=" << mode_name(options.mode) << '\n';
            std::cout << "branch_policy="
                      << branch_policy_name(options.branch_policy) << '\n';
            std::cout << "dimension=" << dim << '\n';
            print_result("", result);
        } else {
            std::vector<std::uint32_t> left = read_truth_table(options.left_path);
            std::vector<std::uint32_t> right = read_truth_table(options.right_path);
            const std::uint32_t dim = log2_exact(left.size());
            if (right.size() != left.size()) {
                throw std::runtime_error("left and right tables have different sizes");
            }
            validate_n_to_n_table(left, dim, "left table");
            validate_n_to_n_table(right, dim, "right table");

            const bool use_seeded_equivalence =
                options.command == "seeded-equiv";

            affine::SolutionStore solutions;
            if (use_seeded_equivalence) {
                affine::SolutionStore target_self(
                    affine::SolutionStore::Options {
                        .record_a1_automorphisms = true,
                    });
                const RunResult target_result =
                    run_dfs(dim, right, right, options, target_self, false);
                seed_from_target_self_group(target_self, solutions);
                print_result("target_self_", target_result);
            }

            const RunResult result =
                run_dfs(dim, left, right, options, solutions, true);
            std::cout << "task="
                      << (use_seeded_equivalence
                              ? "seeded-equiv"
                              : "equiv")
                      << '\n';
            std::cout << "mode=" << mode_name(options.mode) << '\n';
            std::cout << "branch_policy="
                      << branch_policy_name(options.branch_policy) << '\n';
            std::cout << "dimension=" << dim << '\n';
            std::cout << "found=" << (result.solutions != 0 ? 1 : 0) << '\n';
            print_result("", result);
        }

        affine::profile::set_enabled(false);
        if (options.profile) {
            print_profile();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
