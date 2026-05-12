#include "../src/parallel_search.hpp"
#include "../src/profile.hpp"

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

bool g_profile_enabled = false;
int g_argc = 0;
char** g_argv = nullptr;

bool has_flag(int argc, char** argv, std::string_view flag)
{
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == flag) {
            return true;
        }
    }
    return false;
}

std::uint64_t flag_value_u64(
    int argc,
    char** argv,
    std::string_view prefix,
    std::uint64_t fallback = 0)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (!argument.starts_with(prefix)) {
            continue;
        }

        const std::string_view value = argument.substr(prefix.size());
        return static_cast<std::uint64_t>(std::stoull(std::string(value)));
    }
    return fallback;
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

void print_depth_counts(
    std::string_view label,
    const std::vector<std::uint64_t>& counts)
{
    std::cout << ' ' << label << '=';
    bool first = true;
    for (std::size_t depth = 0; depth < counts.size(); ++depth) {
        if (counts[depth] == 0) {
            continue;
        }
        if (!first) {
            std::cout << ',';
        }
        std::cout << depth << ':' << counts[depth];
        first = false;
    }
    if (first) {
        std::cout << "none";
    }
}

void print_worker_counts(
    const std::vector<affine::WorkerStats>& worker_stats)
{
    std::cout << " worker_nodes=";
    for (std::size_t worker = 0; worker < worker_stats.size(); ++worker) {
        if (worker != 0) {
            std::cout << ',';
        }
        std::cout << worker << ':' << worker_stats[worker].dfs_nodes;
    }

    std::cout << " worker_popped=";
    for (std::size_t worker = 0; worker < worker_stats.size(); ++worker) {
        if (worker != 0) {
            std::cout << ',';
        }
        std::cout << worker << ':' << worker_stats[worker].popped;
    }

    std::cout << " worker_work_ms=";
    for (std::size_t worker = 0; worker < worker_stats.size(); ++worker) {
        if (worker != 0) {
            std::cout << ',';
        }
        std::cout << worker << ':'
                  << static_cast<double>(worker_stats[worker].work_ns)
                / 1'000'000.0;
    }
}

[[nodiscard]] double ms(std::uint64_t ns)
{
    return static_cast<double>(ns) / 1'000'000.0;
}

void print_profile(const affine::profile::Snapshot& profile)
{
    std::cout << " profile_dfs_local_calls=" << profile.dfs_local_calls
              << " profile_dfs_local_ms=" << ms(profile.dfs_local_ns)
              << " profile_dfs_entry_snapshot_ms="
              << ms(profile.dfs_local_entry_snapshot_ns)
              << " profile_dfs_refine_ms=" << ms(profile.dfs_local_refine_ns)
              << " profile_dfs_branch_ms=" << ms(profile.dfs_local_branch_ns)
              << " profile_dfs_initial_candidates_ms="
              << ms(profile.dfs_local_initial_candidates_ns)
              << " profile_dfs_recompute_ms="
              << ms(profile.dfs_local_recompute_ns)
              << " profile_dfs_enqueue_ms=" << ms(profile.dfs_local_enqueue_ns)
              << " profile_dfs_next_candidate_ms="
              << ms(profile.dfs_local_next_candidate_ns)
              << " profile_dfs_rollback_ms=" << ms(profile.dfs_local_rollback_ns)
              << " profile_refine_calls=" << profile.refine_calls
              << " profile_refine_ms=" << ms(profile.refine_ns)
              << " profile_signature_calls=" << profile.signature_calls
              << " profile_signature_ms=" << ms(profile.signature_ns)
              << " profile_signature_resize_ms="
              << ms(profile.signature_resize_ns)
              << " profile_signature_weight_ms="
              << ms(profile.signature_weight_ns)
              << " profile_signature_zero_ms=" << ms(profile.signature_zero_ns)
              << " profile_signature_backproject_ms="
              << ms(profile.signature_backproject_ns)
              << " profile_signature_point_loop_ms="
              << ms(profile.signature_point_loop_ns)
              << " profile_signature_radon_ms=" << ms(profile.signature_radon_ns)
              << " profile_signature_pack_ms=" << ms(profile.signature_pack_ns)
              << " profile_pair_refine_calls=" << profile.pair_refine_calls
              << " profile_pair_refine_ms=" << ms(profile.pair_refine_ns)
              << " profile_partition_plan_sample_calls="
              << profile.partition_plan_calls
              << " profile_partition_plan_sample_ms=" << ms(profile.partition_plan_ns)
              << " profile_partition_plan_sample_objects="
              << profile.partition_plan_objects
              << " profile_partition_constant_scan_sample_ms="
              << ms(profile.partition_constant_scan_ns)
              << " profile_partition_copy_sample_ms="
              << ms(profile.partition_copy_ns)
              << " profile_partition_sort_std_sample_calls="
              << profile.partition_sort_std_calls
              << " profile_partition_sort_std_sample_ms="
              << ms(profile.partition_sort_std_ns)
              << " profile_partition_sort_radix_sample_calls="
              << profile.partition_sort_radix_calls
              << " profile_partition_sort_radix_sample_ms="
              << ms(profile.partition_sort_radix_ns)
              << " profile_partition_sort_sample_objects="
              << profile.partition_sort_objects
              << " profile_partition_run_build_sample_ms="
              << ms(profile.partition_run_build_ns)
              << " profile_partition_split_sample_calls="
              << profile.partition_split_calls
              << " profile_partition_split_sample_ms="
              << ms(profile.partition_split_ns)
              << " profile_partition_split_sample_objects="
              << profile.partition_split_objects
              << " profile_partition_rewrite_sample_ms="
              << ms(profile.partition_rewrite_ns)
              << " profile_partition_split_after_reorder_sample_ms="
              << ms(profile.partition_split_after_reorder_ns)
              << " profile_affine_propagate_calls="
              << profile.affine_propagate_calls
              << " profile_affine_propagate_ms="
              << ms(profile.affine_propagate_ns)
              << " profile_publish_calls=" << profile.publish_calls
              << " profile_verify_ms=" << ms(profile.verify_ns)
              << " profile_solution_key_ms=" << ms(profile.solution_key_ns)
              << " profile_solution_lock_ms=" << ms(profile.solution_lock_ns)
              << " profile_solution_group_add_ms="
              << ms(profile.solution_group_add_ns)
              << " profile_group_add_calls=" << profile.group_add_calls
              << " profile_group_add_prepare_ms="
              << ms(profile.group_add_prepare_ns)
              << " profile_group_add_precheck_ms="
              << ms(profile.group_add_precheck_ns)
              << " profile_group_add_lock_ms=" << ms(profile.group_add_lock_ns)
              << " profile_group_add_cache_hits="
              << profile.group_add_cache_hits
              << " profile_group_add_membership_hits="
              << profile.group_add_membership_hits
              << " profile_group_add_insertions="
              << profile.group_add_insertions
              << " profile_snapshot_calls=" << profile.group_snapshot_calls
              << " profile_snapshot_ms=" << ms(profile.group_snapshot_ns)
              << " profile_snapshot_generators="
              << profile.group_snapshot_generators
              << " profile_version_calls=" << profile.group_version_calls
              << " profile_version_ms=" << ms(profile.group_version_ns)
              << " profile_candidate_calls=" << profile.domain_candidate_calls
              << " profile_candidate_ms=" << ms(profile.domain_candidate_ns)
              << " profile_stabilizer_calls=" << profile.stabilizer_calls
              << " profile_stabilizer_ms=" << ms(profile.stabilizer_ns)
              << " profile_stabilizer_checks="
              << profile.stabilizer_generator_checks
              << " profile_stabilizer_orbit_ms="
              << ms(profile.stabilizer_orbit_ns)
              << " profile_stabilizer_schreier_ms="
              << ms(profile.stabilizer_schreier_ns)
              << " profile_stabilizer_reduce_ms="
              << ms(profile.stabilizer_reduce_ns)
              << " profile_stabilizer_orbit_size="
              << profile.stabilizer_orbit_size
              << " profile_stabilizer_raw_generators="
              << profile.stabilizer_raw_generators
              << " profile_stabilizer_reduced_generators="
              << profile.stabilizer_reduced_generators
              << " profile_orbit_calls=" << profile.orbit_partition_calls
              << " profile_orbit_ms=" << ms(profile.orbit_partition_ns)
              << " profile_orbit_generator_apps="
              << profile.orbit_generator_applications;
}

void run_case(
    std::string_view label,
    std::uint32_t dim,
    const std::filesystem::path& left_path,
    const std::filesystem::path& right_path,
    std::uint32_t worker_count,
    bool record_a1_automorphisms = false,
    std::size_t low_watermark = 0,
    std::uint32_t max_split_depth = 4,
    std::size_t max_enqueue_per_split = 0,
    bool count_verified_only = false)
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

    affine::WorkQueue queue;
    queue.push(affine::WorkItem {});

    affine::PruningStore pruner;
    affine::SolutionStore solutions(
        affine::SolutionStore::Options {
            .record_a1_automorphisms = record_a1_automorphisms,
            .count_verified_only = count_verified_only,
        });
    affine::ParallelSearchOptions options;
    options.worker_count = worker_count;
    options.low_watermark = low_watermark == 0 ? 4u * worker_count : low_watermark;
    options.max_split_depth = max_split_depth;
    options.max_enqueue_per_split =
        max_enqueue_per_split == 0 ? 4u * worker_count : max_enqueue_per_split;
    options.max_dfs_nodes = flag_value_u64(
        g_argc,
        g_argv,
        "max_nodes=");

    affine::profile::set_enabled(false);
    if (g_profile_enabled) {
        affine::profile::reset();
        affine::profile::set_enabled(true);
    }

    const auto start = std::chrono::steady_clock::now();
    const affine::ParallelSearchResult result =
        affine::run_parallel_search(problem, queue, pruner, solutions, options);
    const auto end = std::chrono::steady_clock::now();
    const affine::profile::Snapshot profile_snapshot =
        g_profile_enabled ? affine::profile::snapshot() : affine::profile::Snapshot {};
    affine::profile::set_enabled(false);

    const affine::WorkerStats stats = affine::total_worker_stats(result);
    const affine::WorkQueueStats queue_stats = queue.stats();

    std::cout << "parallel_case=" << label
              << " dim=" << dim
              << " workers=" << worker_count
              << " low_watermark=" << options.low_watermark
              << " max_split_depth=" << options.max_split_depth
              << " max_enqueue_per_split=" << options.max_enqueue_per_split
              << " max_dfs_nodes=" << options.max_dfs_nodes
              << " ms=" << std::chrono::duration<double, std::milli>(end - start).count()
              << " popped=" << stats.popped
              << " searched=" << stats.searched
              << " rebuild_dead=" << stats.rebuild_dead
              << " skipped_dead=" << stats.skipped_dead
              << " dfs_nodes=" << stats.dfs_nodes
              << " dfs_leaves=" << stats.dfs_leaves
              << " solutions=" << solutions.size()
              << " a1_group=" << solutions.a1_group().size()
              << " record_a1=" << record_a1_automorphisms
              << " count_only=" << count_verified_only
              << " dead_prefixes=" << pruner.version()
              << " remaining_queue=" << queue.approximate_size();
    print_depth_counts("pushed_depths", queue_stats.pushed_by_depth);
    print_depth_counts("popped_depths", queue_stats.popped_by_depth);
    print_worker_counts(result.worker_stats);
    if (g_profile_enabled) {
        print_profile(profile_snapshot);
    }
    std::cout << '\n';
}

void run_inverse8_equiv(
    const std::filesystem::path& root,
    std::uint32_t worker_count)
{
    const std::filesystem::path folder = root / "8" / "inverse";
    run_case(
        "inverse8_equiv",
        8,
        folder / "function.tt",
        folder / "affine_01.tt",
        worker_count);
}

void run_inverse8_self_auto(
    const std::filesystem::path& root,
    std::uint32_t worker_count,
    std::size_t low_watermark = 0,
    std::uint32_t max_split_depth = 4,
    std::size_t max_enqueue_per_split = 0,
    bool count_verified_only = false)
{
    const std::filesystem::path folder = root / "8" / "inverse";
    run_case(
        "inverse8_self_auto",
        8,
        folder / "function.tt",
        folder / "function.tt",
        worker_count,
        true,
        low_watermark,
        max_split_depth,
        max_enqueue_per_split,
        count_verified_only);
}

void run_inverse8_self_auto_sweep(const std::filesystem::path& root)
{
    for (const std::uint32_t worker_count : { 1u, 2u, 4u, 8u, 16u }) {
        run_inverse8_self_auto(root, worker_count);
    }
}

void run_inverse_equiv(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t worker_count)
{
    const std::filesystem::path folder = root / std::to_string(dim) / "inverse";
    run_case(
        "inverse_affine01",
        dim,
        folder / "function.tt",
        folder / "affine_01.tt",
        worker_count);
}

void run_inverse_self_auto(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t worker_count,
    std::size_t low_watermark = 0,
    std::uint32_t max_split_depth = 4,
    std::size_t max_enqueue_per_split = 0,
    bool count_verified_only = false,
    bool record_a1_automorphisms = true)
{
    const std::filesystem::path folder = root / std::to_string(dim) / "inverse";
    run_case(
        "inverse_self_auto",
        dim,
        folder / "function.tt",
        folder / "function.tt",
        worker_count,
        record_a1_automorphisms,
        low_watermark,
        max_split_depth,
        max_enqueue_per_split,
        count_verified_only);
}

void run_apn01_equiv(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t worker_count)
{
    const std::filesystem::path folder = apn_folder(root, dim, 1);
    run_case(
        "apn01_affine01",
        dim,
        folder / "function.tt",
        folder / "affine_01.tt",
        worker_count);
}

void run_apn01_self_auto(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t worker_count,
    std::size_t low_watermark = 0,
    std::uint32_t max_split_depth = 4,
    std::size_t max_enqueue_per_split = 0,
    bool count_verified_only = false)
{
    const std::filesystem::path folder = apn_folder(root, dim, 1);
    run_case(
        "apn01_self_auto",
        dim,
        folder / "function.tt",
        folder / "function.tt",
        worker_count,
        true,
        low_watermark,
        max_split_depth,
        max_enqueue_per_split,
        count_verified_only);
}

void run_random_self_auto(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t worker_count,
    std::size_t low_watermark = 0,
    std::uint32_t max_split_depth = 4,
    std::size_t max_enqueue_per_split = 0,
    bool count_verified_only = false)
{
    const std::filesystem::path folder =
        root / std::to_string(dim) / "random_permutation";
    run_case(
        "random_self_auto",
        dim,
        folder / "function.tt",
        folder / "function.tt",
        worker_count,
        true,
        low_watermark,
        max_split_depth,
        max_enqueue_per_split,
        count_verified_only);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        g_argc = argc;
        g_argv = argv;

        const std::filesystem::path root =
            argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("apn_original_function_folders");
        g_profile_enabled = has_flag(argc, argv, "profile");

        if (argc >= 5 && std::string_view(argv[2]) == "apn") {
            run_apn01_equiv(
                root,
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                static_cast<std::uint32_t>(std::stoul(argv[4])));
            return 0;
        }
        if (argc >= 5 && std::string_view(argv[2]) == "apn_self") {
            run_apn01_self_auto(
                root,
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argc >= 6 ? static_cast<std::size_t>(std::stoull(argv[5])) : 0u,
                argc >= 7 ? static_cast<std::uint32_t>(std::stoul(argv[6])) : 4u,
                argc >= 8 ? static_cast<std::size_t>(std::stoull(argv[7])) : 0u,
                has_flag(argc, argv, "count_only"));
            return 0;
        }
        if (argc >= 5 && std::string_view(argv[2]) == "random_self") {
            run_random_self_auto(
                root,
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argc >= 6 ? static_cast<std::size_t>(std::stoull(argv[5])) : 0u,
                argc >= 7 ? static_cast<std::uint32_t>(std::stoul(argv[6])) : 4u,
                argc >= 8 ? static_cast<std::size_t>(std::stoull(argv[7])) : 0u,
                has_flag(argc, argv, "count_only"));
            return 0;
        }
        if (argc >= 5 && std::string_view(argv[2]) == "inverse") {
            run_inverse_equiv(
                root,
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                static_cast<std::uint32_t>(std::stoul(argv[4])));
            return 0;
        }
        if (argc >= 5 && std::string_view(argv[2]) == "inverse_self") {
            run_inverse_self_auto(
                root,
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argc >= 6 ? static_cast<std::size_t>(std::stoull(argv[5])) : 0u,
                argc >= 7 ? static_cast<std::uint32_t>(std::stoul(argv[6])) : 4u,
                argc >= 8 ? static_cast<std::size_t>(std::stoull(argv[7])) : 0u,
                has_flag(argc, argv, "count_only"),
                !has_flag(argc, argv, "no_group"));
            return 0;
        }
        if (argc >= 3 && std::string_view(argv[2]) == "inverse8_self_sweep") {
            run_inverse8_self_auto_sweep(root);
            return 0;
        }

        if (argc >= 3) {
            run_inverse8_equiv(root, static_cast<std::uint32_t>(std::stoul(argv[2])));
            return 0;
        }

        for (const std::uint32_t worker_count : { 1u, 2u, 4u, 8u, 16u }) {
            run_inverse8_equiv(root, worker_count);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
