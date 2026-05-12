#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace affine::profile {

struct Counters {
    std::atomic<std::uint64_t> dfs_local_calls = 0;
    std::atomic<std::uint64_t> dfs_local_ns = 0;
    std::atomic<std::uint64_t> dfs_local_entry_snapshot_ns = 0;
    std::atomic<std::uint64_t> dfs_local_refine_ns = 0;
    std::atomic<std::uint64_t> dfs_local_branch_ns = 0;
    std::atomic<std::uint64_t> dfs_local_initial_candidates_ns = 0;
    std::atomic<std::uint64_t> dfs_local_recompute_ns = 0;
    std::atomic<std::uint64_t> dfs_local_enqueue_ns = 0;
    std::atomic<std::uint64_t> dfs_local_next_candidate_ns = 0;
    std::atomic<std::uint64_t> dfs_local_rollback_ns = 0;

    std::atomic<std::uint64_t> refine_calls = 0;
    std::atomic<std::uint64_t> refine_ns = 0;
    std::atomic<std::uint64_t> signature_calls = 0;
    std::atomic<std::uint64_t> signature_ns = 0;
    std::atomic<std::uint64_t> signature_resize_ns = 0;
    std::atomic<std::uint64_t> signature_weight_ns = 0;
    std::atomic<std::uint64_t> signature_zero_ns = 0;
    std::atomic<std::uint64_t> signature_backproject_ns = 0;
    std::atomic<std::uint64_t> signature_point_loop_ns = 0;
    std::atomic<std::uint64_t> signature_radon_ns = 0;
    std::atomic<std::uint64_t> signature_pack_ns = 0;
    std::atomic<std::uint64_t> pair_refine_calls = 0;
    std::atomic<std::uint64_t> pair_refine_ns = 0;
    std::atomic<std::uint64_t> partition_plan_calls = 0;
    std::atomic<std::uint64_t> partition_plan_ns = 0;
    std::atomic<std::uint64_t> partition_plan_objects = 0;
    std::atomic<std::uint64_t> partition_constant_scan_ns = 0;
    std::atomic<std::uint64_t> partition_copy_ns = 0;
    std::atomic<std::uint64_t> partition_sort_std_calls = 0;
    std::atomic<std::uint64_t> partition_sort_std_ns = 0;
    std::atomic<std::uint64_t> partition_sort_radix_calls = 0;
    std::atomic<std::uint64_t> partition_sort_radix_ns = 0;
    std::atomic<std::uint64_t> partition_sort_objects = 0;
    std::atomic<std::uint64_t> partition_run_build_ns = 0;
    std::atomic<std::uint64_t> partition_split_calls = 0;
    std::atomic<std::uint64_t> partition_split_ns = 0;
    std::atomic<std::uint64_t> partition_split_objects = 0;
    std::atomic<std::uint64_t> partition_rewrite_ns = 0;
    std::atomic<std::uint64_t> partition_split_after_reorder_ns = 0;
    std::atomic<std::uint64_t> affine_propagate_calls = 0;
    std::atomic<std::uint64_t> affine_propagate_ns = 0;

    std::atomic<std::uint64_t> publish_calls = 0;
    std::atomic<std::uint64_t> verify_ns = 0;
    std::atomic<std::uint64_t> solution_key_ns = 0;
    std::atomic<std::uint64_t> solution_lock_ns = 0;
    std::atomic<std::uint64_t> solution_group_add_ns = 0;

    std::atomic<std::uint64_t> group_add_calls = 0;
    std::atomic<std::uint64_t> group_add_prepare_ns = 0;
    std::atomic<std::uint64_t> group_add_precheck_ns = 0;
    std::atomic<std::uint64_t> group_add_lock_ns = 0;
    std::atomic<std::uint64_t> group_add_cache_hits = 0;
    std::atomic<std::uint64_t> group_add_membership_hits = 0;
    std::atomic<std::uint64_t> group_add_insertions = 0;

    std::atomic<std::uint64_t> group_snapshot_calls = 0;
    std::atomic<std::uint64_t> group_snapshot_ns = 0;
    std::atomic<std::uint64_t> group_snapshot_generators = 0;
    std::atomic<std::uint64_t> group_version_calls = 0;
    std::atomic<std::uint64_t> group_version_ns = 0;

    std::atomic<std::uint64_t> domain_candidate_calls = 0;
    std::atomic<std::uint64_t> domain_candidate_ns = 0;
    std::atomic<std::uint64_t> stabilizer_calls = 0;
    std::atomic<std::uint64_t> stabilizer_ns = 0;
    std::atomic<std::uint64_t> stabilizer_generator_checks = 0;
    std::atomic<std::uint64_t> stabilizer_orbit_ns = 0;
    std::atomic<std::uint64_t> stabilizer_schreier_ns = 0;
    std::atomic<std::uint64_t> stabilizer_reduce_ns = 0;
    std::atomic<std::uint64_t> stabilizer_orbit_size = 0;
    std::atomic<std::uint64_t> stabilizer_raw_generators = 0;
    std::atomic<std::uint64_t> stabilizer_reduced_generators = 0;
    std::atomic<std::uint64_t> orbit_partition_calls = 0;
    std::atomic<std::uint64_t> orbit_partition_ns = 0;
    std::atomic<std::uint64_t> orbit_generator_applications = 0;
};

struct Snapshot {
    std::uint64_t dfs_local_calls = 0;
    std::uint64_t dfs_local_ns = 0;
    std::uint64_t dfs_local_entry_snapshot_ns = 0;
    std::uint64_t dfs_local_refine_ns = 0;
    std::uint64_t dfs_local_branch_ns = 0;
    std::uint64_t dfs_local_initial_candidates_ns = 0;
    std::uint64_t dfs_local_recompute_ns = 0;
    std::uint64_t dfs_local_enqueue_ns = 0;
    std::uint64_t dfs_local_next_candidate_ns = 0;
    std::uint64_t dfs_local_rollback_ns = 0;
    std::uint64_t refine_calls = 0;
    std::uint64_t refine_ns = 0;
    std::uint64_t signature_calls = 0;
    std::uint64_t signature_ns = 0;
    std::uint64_t signature_resize_ns = 0;
    std::uint64_t signature_weight_ns = 0;
    std::uint64_t signature_zero_ns = 0;
    std::uint64_t signature_backproject_ns = 0;
    std::uint64_t signature_point_loop_ns = 0;
    std::uint64_t signature_radon_ns = 0;
    std::uint64_t signature_pack_ns = 0;
    std::uint64_t pair_refine_calls = 0;
    std::uint64_t pair_refine_ns = 0;
    std::uint64_t partition_plan_calls = 0;
    std::uint64_t partition_plan_ns = 0;
    std::uint64_t partition_plan_objects = 0;
    std::uint64_t partition_constant_scan_ns = 0;
    std::uint64_t partition_copy_ns = 0;
    std::uint64_t partition_sort_std_calls = 0;
    std::uint64_t partition_sort_std_ns = 0;
    std::uint64_t partition_sort_radix_calls = 0;
    std::uint64_t partition_sort_radix_ns = 0;
    std::uint64_t partition_sort_objects = 0;
    std::uint64_t partition_run_build_ns = 0;
    std::uint64_t partition_split_calls = 0;
    std::uint64_t partition_split_ns = 0;
    std::uint64_t partition_split_objects = 0;
    std::uint64_t partition_rewrite_ns = 0;
    std::uint64_t partition_split_after_reorder_ns = 0;
    std::uint64_t affine_propagate_calls = 0;
    std::uint64_t affine_propagate_ns = 0;
    std::uint64_t publish_calls = 0;
    std::uint64_t verify_ns = 0;
    std::uint64_t solution_key_ns = 0;
    std::uint64_t solution_lock_ns = 0;
    std::uint64_t solution_group_add_ns = 0;
    std::uint64_t group_add_calls = 0;
    std::uint64_t group_add_prepare_ns = 0;
    std::uint64_t group_add_precheck_ns = 0;
    std::uint64_t group_add_lock_ns = 0;
    std::uint64_t group_add_cache_hits = 0;
    std::uint64_t group_add_membership_hits = 0;
    std::uint64_t group_add_insertions = 0;
    std::uint64_t group_snapshot_calls = 0;
    std::uint64_t group_snapshot_ns = 0;
    std::uint64_t group_snapshot_generators = 0;
    std::uint64_t group_version_calls = 0;
    std::uint64_t group_version_ns = 0;
    std::uint64_t domain_candidate_calls = 0;
    std::uint64_t domain_candidate_ns = 0;
    std::uint64_t stabilizer_calls = 0;
    std::uint64_t stabilizer_ns = 0;
    std::uint64_t stabilizer_generator_checks = 0;
    std::uint64_t stabilizer_orbit_ns = 0;
    std::uint64_t stabilizer_schreier_ns = 0;
    std::uint64_t stabilizer_reduce_ns = 0;
    std::uint64_t stabilizer_orbit_size = 0;
    std::uint64_t stabilizer_raw_generators = 0;
    std::uint64_t stabilizer_reduced_generators = 0;
    std::uint64_t orbit_partition_calls = 0;
    std::uint64_t orbit_partition_ns = 0;
    std::uint64_t orbit_generator_applications = 0;
};

inline std::atomic<bool> enabled = false;
inline Counters counters;

inline void set_enabled(bool value)
{
    enabled.store(value, std::memory_order_release);
}

inline void reset()
{
    counters.dfs_local_calls.store(0, std::memory_order_relaxed);
    counters.dfs_local_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_entry_snapshot_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_refine_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_branch_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_initial_candidates_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_recompute_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_enqueue_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_next_candidate_ns.store(0, std::memory_order_relaxed);
    counters.dfs_local_rollback_ns.store(0, std::memory_order_relaxed);
    counters.refine_calls.store(0, std::memory_order_relaxed);
    counters.refine_ns.store(0, std::memory_order_relaxed);
    counters.signature_calls.store(0, std::memory_order_relaxed);
    counters.signature_ns.store(0, std::memory_order_relaxed);
    counters.signature_resize_ns.store(0, std::memory_order_relaxed);
    counters.signature_weight_ns.store(0, std::memory_order_relaxed);
    counters.signature_zero_ns.store(0, std::memory_order_relaxed);
    counters.signature_backproject_ns.store(0, std::memory_order_relaxed);
    counters.signature_point_loop_ns.store(0, std::memory_order_relaxed);
    counters.signature_radon_ns.store(0, std::memory_order_relaxed);
    counters.signature_pack_ns.store(0, std::memory_order_relaxed);
    counters.pair_refine_calls.store(0, std::memory_order_relaxed);
    counters.pair_refine_ns.store(0, std::memory_order_relaxed);
    counters.partition_plan_calls.store(0, std::memory_order_relaxed);
    counters.partition_plan_ns.store(0, std::memory_order_relaxed);
    counters.partition_plan_objects.store(0, std::memory_order_relaxed);
    counters.partition_constant_scan_ns.store(0, std::memory_order_relaxed);
    counters.partition_copy_ns.store(0, std::memory_order_relaxed);
    counters.partition_sort_std_calls.store(0, std::memory_order_relaxed);
    counters.partition_sort_std_ns.store(0, std::memory_order_relaxed);
    counters.partition_sort_radix_calls.store(0, std::memory_order_relaxed);
    counters.partition_sort_radix_ns.store(0, std::memory_order_relaxed);
    counters.partition_sort_objects.store(0, std::memory_order_relaxed);
    counters.partition_run_build_ns.store(0, std::memory_order_relaxed);
    counters.partition_split_calls.store(0, std::memory_order_relaxed);
    counters.partition_split_ns.store(0, std::memory_order_relaxed);
    counters.partition_split_objects.store(0, std::memory_order_relaxed);
    counters.partition_rewrite_ns.store(0, std::memory_order_relaxed);
    counters.partition_split_after_reorder_ns.store(0, std::memory_order_relaxed);
    counters.affine_propagate_calls.store(0, std::memory_order_relaxed);
    counters.affine_propagate_ns.store(0, std::memory_order_relaxed);
    counters.publish_calls.store(0, std::memory_order_relaxed);
    counters.verify_ns.store(0, std::memory_order_relaxed);
    counters.solution_key_ns.store(0, std::memory_order_relaxed);
    counters.solution_lock_ns.store(0, std::memory_order_relaxed);
    counters.solution_group_add_ns.store(0, std::memory_order_relaxed);
    counters.group_add_calls.store(0, std::memory_order_relaxed);
    counters.group_add_prepare_ns.store(0, std::memory_order_relaxed);
    counters.group_add_precheck_ns.store(0, std::memory_order_relaxed);
    counters.group_add_lock_ns.store(0, std::memory_order_relaxed);
    counters.group_add_cache_hits.store(0, std::memory_order_relaxed);
    counters.group_add_membership_hits.store(0, std::memory_order_relaxed);
    counters.group_add_insertions.store(0, std::memory_order_relaxed);
    counters.group_snapshot_calls.store(0, std::memory_order_relaxed);
    counters.group_snapshot_ns.store(0, std::memory_order_relaxed);
    counters.group_snapshot_generators.store(0, std::memory_order_relaxed);
    counters.group_version_calls.store(0, std::memory_order_relaxed);
    counters.group_version_ns.store(0, std::memory_order_relaxed);
    counters.domain_candidate_calls.store(0, std::memory_order_relaxed);
    counters.domain_candidate_ns.store(0, std::memory_order_relaxed);
    counters.stabilizer_calls.store(0, std::memory_order_relaxed);
    counters.stabilizer_ns.store(0, std::memory_order_relaxed);
    counters.stabilizer_generator_checks.store(0, std::memory_order_relaxed);
    counters.stabilizer_orbit_ns.store(0, std::memory_order_relaxed);
    counters.stabilizer_schreier_ns.store(0, std::memory_order_relaxed);
    counters.stabilizer_reduce_ns.store(0, std::memory_order_relaxed);
    counters.stabilizer_orbit_size.store(0, std::memory_order_relaxed);
    counters.stabilizer_raw_generators.store(0, std::memory_order_relaxed);
    counters.stabilizer_reduced_generators.store(0, std::memory_order_relaxed);
    counters.orbit_partition_calls.store(0, std::memory_order_relaxed);
    counters.orbit_partition_ns.store(0, std::memory_order_relaxed);
    counters.orbit_generator_applications.store(0, std::memory_order_relaxed);
}

[[nodiscard]] inline Snapshot snapshot()
{
    return Snapshot {
        .dfs_local_calls = counters.dfs_local_calls.load(std::memory_order_relaxed),
        .dfs_local_ns = counters.dfs_local_ns.load(std::memory_order_relaxed),
        .dfs_local_entry_snapshot_ns =
            counters.dfs_local_entry_snapshot_ns.load(std::memory_order_relaxed),
        .dfs_local_refine_ns =
            counters.dfs_local_refine_ns.load(std::memory_order_relaxed),
        .dfs_local_branch_ns =
            counters.dfs_local_branch_ns.load(std::memory_order_relaxed),
        .dfs_local_initial_candidates_ns =
            counters.dfs_local_initial_candidates_ns.load(std::memory_order_relaxed),
        .dfs_local_recompute_ns =
            counters.dfs_local_recompute_ns.load(std::memory_order_relaxed),
        .dfs_local_enqueue_ns =
            counters.dfs_local_enqueue_ns.load(std::memory_order_relaxed),
        .dfs_local_next_candidate_ns =
            counters.dfs_local_next_candidate_ns.load(std::memory_order_relaxed),
        .dfs_local_rollback_ns =
            counters.dfs_local_rollback_ns.load(std::memory_order_relaxed),
        .refine_calls = counters.refine_calls.load(std::memory_order_relaxed),
        .refine_ns = counters.refine_ns.load(std::memory_order_relaxed),
        .signature_calls = counters.signature_calls.load(std::memory_order_relaxed),
        .signature_ns = counters.signature_ns.load(std::memory_order_relaxed),
        .signature_resize_ns =
            counters.signature_resize_ns.load(std::memory_order_relaxed),
        .signature_weight_ns =
            counters.signature_weight_ns.load(std::memory_order_relaxed),
        .signature_zero_ns = counters.signature_zero_ns.load(std::memory_order_relaxed),
        .signature_backproject_ns =
            counters.signature_backproject_ns.load(std::memory_order_relaxed),
        .signature_point_loop_ns =
            counters.signature_point_loop_ns.load(std::memory_order_relaxed),
        .signature_radon_ns = counters.signature_radon_ns.load(std::memory_order_relaxed),
        .signature_pack_ns = counters.signature_pack_ns.load(std::memory_order_relaxed),
        .pair_refine_calls = counters.pair_refine_calls.load(std::memory_order_relaxed),
        .pair_refine_ns = counters.pair_refine_ns.load(std::memory_order_relaxed),
        .partition_plan_calls =
            counters.partition_plan_calls.load(std::memory_order_relaxed),
        .partition_plan_ns = counters.partition_plan_ns.load(std::memory_order_relaxed),
        .partition_plan_objects =
            counters.partition_plan_objects.load(std::memory_order_relaxed),
        .partition_constant_scan_ns =
            counters.partition_constant_scan_ns.load(std::memory_order_relaxed),
        .partition_copy_ns = counters.partition_copy_ns.load(std::memory_order_relaxed),
        .partition_sort_std_calls =
            counters.partition_sort_std_calls.load(std::memory_order_relaxed),
        .partition_sort_std_ns =
            counters.partition_sort_std_ns.load(std::memory_order_relaxed),
        .partition_sort_radix_calls =
            counters.partition_sort_radix_calls.load(std::memory_order_relaxed),
        .partition_sort_radix_ns =
            counters.partition_sort_radix_ns.load(std::memory_order_relaxed),
        .partition_sort_objects =
            counters.partition_sort_objects.load(std::memory_order_relaxed),
        .partition_run_build_ns =
            counters.partition_run_build_ns.load(std::memory_order_relaxed),
        .partition_split_calls =
            counters.partition_split_calls.load(std::memory_order_relaxed),
        .partition_split_ns = counters.partition_split_ns.load(std::memory_order_relaxed),
        .partition_split_objects =
            counters.partition_split_objects.load(std::memory_order_relaxed),
        .partition_rewrite_ns =
            counters.partition_rewrite_ns.load(std::memory_order_relaxed),
        .partition_split_after_reorder_ns =
            counters.partition_split_after_reorder_ns.load(std::memory_order_relaxed),
        .affine_propagate_calls =
            counters.affine_propagate_calls.load(std::memory_order_relaxed),
        .affine_propagate_ns =
            counters.affine_propagate_ns.load(std::memory_order_relaxed),
        .publish_calls = counters.publish_calls.load(std::memory_order_relaxed),
        .verify_ns = counters.verify_ns.load(std::memory_order_relaxed),
        .solution_key_ns = counters.solution_key_ns.load(std::memory_order_relaxed),
        .solution_lock_ns = counters.solution_lock_ns.load(std::memory_order_relaxed),
        .solution_group_add_ns =
            counters.solution_group_add_ns.load(std::memory_order_relaxed),
        .group_add_calls = counters.group_add_calls.load(std::memory_order_relaxed),
        .group_add_prepare_ns =
            counters.group_add_prepare_ns.load(std::memory_order_relaxed),
        .group_add_precheck_ns =
            counters.group_add_precheck_ns.load(std::memory_order_relaxed),
        .group_add_lock_ns = counters.group_add_lock_ns.load(std::memory_order_relaxed),
        .group_add_cache_hits =
            counters.group_add_cache_hits.load(std::memory_order_relaxed),
        .group_add_membership_hits =
            counters.group_add_membership_hits.load(std::memory_order_relaxed),
        .group_add_insertions =
            counters.group_add_insertions.load(std::memory_order_relaxed),
        .group_snapshot_calls =
            counters.group_snapshot_calls.load(std::memory_order_relaxed),
        .group_snapshot_ns = counters.group_snapshot_ns.load(std::memory_order_relaxed),
        .group_snapshot_generators =
            counters.group_snapshot_generators.load(std::memory_order_relaxed),
        .group_version_calls =
            counters.group_version_calls.load(std::memory_order_relaxed),
        .group_version_ns = counters.group_version_ns.load(std::memory_order_relaxed),
        .domain_candidate_calls =
            counters.domain_candidate_calls.load(std::memory_order_relaxed),
        .domain_candidate_ns =
            counters.domain_candidate_ns.load(std::memory_order_relaxed),
        .stabilizer_calls = counters.stabilizer_calls.load(std::memory_order_relaxed),
        .stabilizer_ns = counters.stabilizer_ns.load(std::memory_order_relaxed),
        .stabilizer_generator_checks =
            counters.stabilizer_generator_checks.load(std::memory_order_relaxed),
        .stabilizer_orbit_ns =
            counters.stabilizer_orbit_ns.load(std::memory_order_relaxed),
        .stabilizer_schreier_ns =
            counters.stabilizer_schreier_ns.load(std::memory_order_relaxed),
        .stabilizer_reduce_ns =
            counters.stabilizer_reduce_ns.load(std::memory_order_relaxed),
        .stabilizer_orbit_size =
            counters.stabilizer_orbit_size.load(std::memory_order_relaxed),
        .stabilizer_raw_generators =
            counters.stabilizer_raw_generators.load(std::memory_order_relaxed),
        .stabilizer_reduced_generators =
            counters.stabilizer_reduced_generators.load(std::memory_order_relaxed),
        .orbit_partition_calls =
            counters.orbit_partition_calls.load(std::memory_order_relaxed),
        .orbit_partition_ns = counters.orbit_partition_ns.load(std::memory_order_relaxed),
        .orbit_generator_applications =
            counters.orbit_generator_applications.load(std::memory_order_relaxed),
    };
}

class ScopedTimer {
public:
    explicit ScopedTimer(std::atomic<std::uint64_t>& target)
        : ScopedTimer(target, true)
    {
    }

    ScopedTimer(std::atomic<std::uint64_t>& target, bool active)
        : target_(
              active && enabled.load(std::memory_order_acquire) ? &target : nullptr)
    {
        if (target_ != nullptr) {
            start_ = Clock::now();
        }
    }

    ~ScopedTimer()
    {
        if (target_ == nullptr) {
            return;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start_);
        target_->fetch_add(
            static_cast<std::uint64_t>(elapsed.count()),
            std::memory_order_relaxed);
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    using Clock = std::chrono::steady_clock;

    std::atomic<std::uint64_t>* target_ = nullptr;
    Clock::time_point start_;
};

inline void count(std::atomic<std::uint64_t>& target, std::uint64_t value = 1)
{
    if (enabled.load(std::memory_order_acquire)) {
        target.fetch_add(value, std::memory_order_relaxed);
    }
}

inline void count_if(
    bool active,
    std::atomic<std::uint64_t>& target,
    std::uint64_t value = 1)
{
    if (active && enabled.load(std::memory_order_acquire)) {
        target.fetch_add(value, std::memory_order_relaxed);
    }
}

inline bool sample_hot_path(std::uint64_t mask = 1023)
{
    if (!enabled.load(std::memory_order_acquire)) {
        return false;
    }

    thread_local std::uint64_t local_counter = 0;
    return (++local_counter & mask) == 0;
}

} // namespace affine::profile
