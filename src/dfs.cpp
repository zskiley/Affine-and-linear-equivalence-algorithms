#include "refinement.hpp"
#include "group/orbit_partition.hpp"
#include "profile.hpp"
#include "search_types.hpp"
#include "work.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <utility>
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

} // namespace

struct DfsProblem {
    std::uint32_t domain_dim = 0;
    std::uint32_t codomain_dim = 0;
    std::span<const std::uint32_t> left_function_table;
    std::span<const std::uint32_t> right_function_table;
};

struct DfsStats {
    std::uint64_t nodes = 0;
    std::uint64_t pruned = 0;
    std::uint64_t leaves = 0;
};

enum class BranchPolicy {
    SmallestAny,
    HyperplanesFirst,
};

struct DfsOptions {
    BranchPolicy branch_policy = BranchPolicy::HyperplanesFirst;
    bool stop_after_first_leaf = false;
};

struct DomainStabilizerCacheKey {
    std::uint64_t group_version = 0;
    std::vector<group::ObjectId> points;
    std::vector<group::ObjectId> hyperplanes;

    [[nodiscard]] bool operator==(const DomainStabilizerCacheKey& other) const
    {
        return group_version == other.group_version
            && points == other.points
            && hyperplanes == other.hyperplanes;
    }
};

struct DomainStabilizerCacheKeyHash {
    [[nodiscard]] std::size_t operator()(const DomainStabilizerCacheKey& key) const
    {
        std::size_t hash = 1469598103934665603ull;
        auto mix = [&](std::uint64_t value) {
            hash ^= static_cast<std::size_t>(value);
            hash *= 1099511628211ull;
        };

        mix(key.group_version);
        mix(key.points.size());
        for (const group::ObjectId point : key.points) {
            mix(point);
        }
        mix(key.hyperplanes.size());
        for (const group::ObjectId hyperplane : key.hyperplanes) {
            mix(hyperplane);
        }
        return hash;
    }
};

struct DomainStabilizerCache {
    std::uint64_t group_version = std::numeric_limits<std::uint64_t>::max();
    std::mutex mutex;
    std::unordered_map<
        DomainStabilizerCacheKey,
        std::shared_ptr<const group::Stabilizer>,
        DomainStabilizerCacheKeyHash> entries;

    DomainStabilizerCache() = default;
    DomainStabilizerCache(const DomainStabilizerCache&) = delete;
    DomainStabilizerCache& operator=(const DomainStabilizerCache&) = delete;

    DomainStabilizerCache(DomainStabilizerCache&& other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex);
        group_version = other.group_version;
        entries = std::move(other.entries);
    }

    DomainStabilizerCache& operator=(DomainStabilizerCache&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        std::scoped_lock lock(mutex, other.mutex);
        group_version = other.group_version;
        entries = std::move(other.entries);
        return *this;
    }

    void clear_if_version_changed_locked(std::uint64_t version)
    {
        if (group_version == version) {
            return;
        }
        if (group_version != std::numeric_limits<std::uint64_t>::max()
            && version < group_version) {
            return;
        }

        entries.clear();
        group_version = version;
    }

    [[nodiscard]] std::shared_ptr<const group::Stabilizer> find(
        std::uint64_t version,
        const DomainStabilizerCacheKey& key)
    {
        std::lock_guard<std::mutex> lock(mutex);
        clear_if_version_changed_locked(version);

        const auto found = entries.find(key);
        if (found == entries.end()) {
            return {};
        }
        return found->second;
    }

    [[nodiscard]] std::shared_ptr<const group::Stabilizer> insert_or_find(
        std::uint64_t version,
        DomainStabilizerCacheKey key,
        group::Stabilizer stabilizer)
    {
        auto stabilizer_ptr =
            std::make_shared<const group::Stabilizer>(std::move(stabilizer));

        std::lock_guard<std::mutex> lock(mutex);
        if (group_version != version) {
            return stabilizer_ptr;
        }

        const auto [inserted, _] =
            entries.emplace(std::move(key), stabilizer_ptr);
        return inserted->second;
    }
};

struct SearchTask {
    SearchPartitions partitions;
    RefinementWorkspace workspace;
    std::vector<BranchMove> path;
    DfsStats stats;
    DfsOptions options;
    DomainStabilizerCache domain_stabilizer_cache;
};

struct DfsContext {
    WorkQueue* queue = nullptr;
    PruningStore* pruner = nullptr;
    SolutionStore* solution_store = nullptr;
    std::size_t low_watermark = 64;
    // At and beyond this depth, a node only donates one sibling at a time.
    std::uint32_t max_split_depth = 4;
    std::size_t max_enqueue_per_split = 64;
    std::size_t active_work_depth = WorkQueue::no_active_depth;
    const group::AffineGroup* domain_group = nullptr;
    DomainStabilizerCache* domain_stabilizer_cache = nullptr;
    std::atomic<bool>* stop_requested = nullptr;
    std::atomic<std::uint64_t>* remaining_node_budget = nullptr;
};

struct BranchCell {
    BranchKind kind = BranchKind::DomainPoint;
    Partition::CellId left_cell = Partition::npos;
    Partition::CellId right_cell = Partition::npos;
    Partition::Index size = std::numeric_limits<Partition::Index>::max();
};

[[nodiscard]] SearchTask make_search_task(
    const DfsProblem& problem,
    DfsOptions options = {})
{
    SearchTask task;
    task.partitions = make_initial_partitions(problem.domain_dim, problem.codomain_dim);
    task.options = options;
    return task;
}

[[nodiscard]] SearchTask make_search_task(
    const DfsProblem& problem,
    bool stop_after_first_leaf)
{
    DfsOptions options;
    options.stop_after_first_leaf = stop_after_first_leaf;
    SearchTask task = make_search_task(problem, options);
    return task;
}

[[nodiscard]] bool is_discrete(const Partition& partition)
{
    for (Partition::CellId cell = partition.first_cell();
         cell != Partition::npos;
         cell = partition.next_cell(cell)) {
        if (partition.cell(cell).size() != 1) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_complete(const SearchPartitions& partitions)
{
    return is_discrete(partitions.P.left)
        && is_discrete(partitions.P.right)
        && is_discrete(partitions.Q.left)
        && is_discrete(partitions.Q.right);
}

[[nodiscard]] bool objects_are_in_corresponding_cells(
    const PartitionPair& pair,
    Partition::ObjectId left_object,
    Partition::ObjectId right_object)
{
    const Partition::CellId target_left = pair.left.cell_of(left_object);
    const Partition::CellId target_right = pair.right.cell_of(right_object);

    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();
    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        if (left_cell == target_left || right_cell == target_right) {
            return left_cell == target_left && right_cell == target_right;
        }
        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }

    return false;
}

[[nodiscard]] PartitionPair& branch_pair(SearchPartitions& partitions, BranchKind kind)
{
    switch (kind) {
    case BranchKind::DomainPoint:
        return partitions.P;
    case BranchKind::CodomainPoint:
        return partitions.Q;
    case BranchKind::DomainHyperplane:
        return partitions.L;
    case BranchKind::CodomainHyperplane:
        return partitions.R;
    }

    return partitions.P;
}

[[nodiscard]] const PartitionPair& branch_pair(
    const SearchPartitions& partitions,
    BranchKind kind)
{
    switch (kind) {
    case BranchKind::DomainPoint:
        return partitions.P;
    case BranchKind::CodomainPoint:
        return partitions.Q;
    case BranchKind::DomainHyperplane:
        return partitions.L;
    case BranchKind::CodomainHyperplane:
        return partitions.R;
    }

    return partitions.P;
}

[[nodiscard]] bool individualize_branch_pair(
    PartitionPair& pair,
    Partition::ObjectId left_object,
    Partition::ObjectId right_object)
{
    if (!objects_are_in_corresponding_cells(pair, left_object, right_object)) {
        return false;
    }

    pair.left.individualize(left_object);
    pair.right.individualize(right_object);
    return true;
}

[[nodiscard]] bool apply_branch_move(
    const DfsProblem& problem,
    SearchPartitions& partitions,
    const BranchMove& move)
{
    if (move.kind == BranchKind::DomainPoint) {
        if (!objects_are_in_corresponding_cells(
                partitions.P,
                move.left_object,
                move.right_object)) {
            return false;
        }

        const Partition::ObjectId left_value =
            problem.left_function_table[move.left_object];
        const Partition::ObjectId right_value =
            problem.right_function_table[move.right_object];
        if (!objects_are_in_corresponding_cells(partitions.Q, left_value, right_value)) {
            return false;
        }

        partitions.P.left.individualize(move.left_object);
        partitions.P.right.individualize(move.right_object);
        partitions.Q.left.individualize(left_value);
        partitions.Q.right.individualize(right_value);
        return true;
    }

    return individualize_branch_pair(
        branch_pair(partitions, move.kind),
        move.left_object,
        move.right_object);
}

[[nodiscard]] BranchCell choose_branch_cell_from_pair(
    const PartitionPair& pair,
    BranchKind kind)
{
    BranchCell best;
    best.kind = kind;

    Partition::CellId left_cell = pair.left.first_cell();
    Partition::CellId right_cell = pair.right.first_cell();
    while (left_cell != Partition::npos && right_cell != Partition::npos) {
        const Partition::Index size = pair.left.cell(left_cell).size();
        if (size > 1 && size == pair.right.cell(right_cell).size() && size < best.size) {
            best.left_cell = left_cell;
            best.right_cell = right_cell;
            best.size = size;
        }
        left_cell = pair.left.next_cell(left_cell);
        right_cell = pair.right.next_cell(right_cell);
    }

    return best;
}

[[nodiscard]] std::uint32_t branch_priority(BranchKind kind)
{
    switch (kind) {
    case BranchKind::DomainPoint:
        return 0;
    case BranchKind::CodomainPoint:
        return 1;
    case BranchKind::DomainHyperplane:
        return 2;
    case BranchKind::CodomainHyperplane:
        return 3;
    }

    return 4;
}

[[nodiscard]] bool is_better_branch_cell(
    const BranchCell& candidate,
    const BranchCell& best)
{
    if (candidate.left_cell == Partition::npos) {
        return false;
    }
    if (best.left_cell == Partition::npos) {
        return true;
    }
    if (candidate.size != best.size) {
        return candidate.size < best.size;
    }
    return branch_priority(candidate.kind) < branch_priority(best.kind);
}

[[nodiscard]] BranchCell choose_branch_cell(const SearchPartitions& partitions)
{
    BranchCell best;

    for (const BranchCell candidate : {
             choose_branch_cell_from_pair(partitions.P, BranchKind::DomainPoint),
             choose_branch_cell_from_pair(partitions.Q, BranchKind::CodomainPoint),
             choose_branch_cell_from_pair(partitions.L, BranchKind::DomainHyperplane),
             choose_branch_cell_from_pair(partitions.R, BranchKind::CodomainHyperplane),
         }) {
        if (is_better_branch_cell(candidate, best)) {
            best = candidate;
        }
    }

    return best;
}

[[nodiscard]] BranchCell choose_hyperplane_branch_cell(
    const SearchPartitions& partitions)
{
    BranchCell best;
    for (const BranchCell candidate : {
             choose_branch_cell_from_pair(partitions.L, BranchKind::DomainHyperplane),
             choose_branch_cell_from_pair(partitions.R, BranchKind::CodomainHyperplane),
         }) {
        if (is_better_branch_cell(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}

[[nodiscard]] BranchCell choose_point_branch_cell(
    const SearchPartitions& partitions)
{
    BranchCell best;
    for (const BranchCell candidate : {
             choose_branch_cell_from_pair(partitions.P, BranchKind::DomainPoint),
             choose_branch_cell_from_pair(partitions.Q, BranchKind::CodomainPoint),
         }) {
        if (is_better_branch_cell(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}

[[nodiscard]] BranchCell choose_branch_cell(
    const SearchPartitions& partitions,
    BranchPolicy policy)
{
    if (policy == BranchPolicy::HyperplanesFirst) {
        BranchCell hyperplane_cell = choose_hyperplane_branch_cell(partitions);
        if (hyperplane_cell.left_cell != Partition::npos) {
            return hyperplane_cell;
        }
        return choose_point_branch_cell(partitions);
    }

    return choose_branch_cell(partitions);
}

[[nodiscard]] bool path_is_dead(const DfsContext* context, std::span<const BranchMove> path)
{
    return context != nullptr
        && context->pruner != nullptr
        && context->pruner->is_dead(path);
}

[[nodiscard]] bool should_stop(const DfsContext* context)
{
    return context != nullptr
        && context->stop_requested != nullptr
        && context->stop_requested->load(std::memory_order_acquire);
}

[[nodiscard]] bool consume_node_budget(DfsContext* context)
{
    if (context == nullptr || context->remaining_node_budget == nullptr) {
        return true;
    }

    std::uint64_t remaining =
        context->remaining_node_budget->load(std::memory_order_acquire);
    while (remaining != 0) {
        if (context->remaining_node_budget->compare_exchange_weak(
                remaining,
                remaining - 1u,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return true;
        }
    }

    if (context->stop_requested != nullptr) {
        context->stop_requested->store(true, std::memory_order_release);
    }
    return false;
}

[[nodiscard]] bool should_refill_queue(
    const DfsContext* context,
    const SearchTask& task,
    std::size_t branch_count)
{
    return context != nullptr
        && context->queue != nullptr
        && branch_count > 1
        && !task.options.stop_after_first_leaf
        && context->max_enqueue_per_split > 0;
}

[[nodiscard]] std::size_t depth_enqueue_budget(
    const DfsContext* context,
    const SearchTask& task)
{
    if (context == nullptr || context->max_enqueue_per_split == 0) {
        return 0;
    }

    if (task.path.size() >= context->max_split_depth) {
        return 1;
    }

    std::size_t budget = context->max_enqueue_per_split;
    for (std::size_t depth = 0; depth < task.path.size() && budget > 1; ++depth) {
        budget /= 2u;
    }
    return std::max<std::size_t>(1u, budget);
}

[[nodiscard]] std::size_t depth_refill_target(
    const DfsContext* context,
    const SearchTask& task)
{
    if (context == nullptr || context->low_watermark == 0) {
        return 0;
    }

    std::size_t target = context->low_watermark;
    const std::size_t floor = std::max<std::size_t>(1u, context->low_watermark / 4u);
    for (std::size_t depth = 0; depth < task.path.size() && target > floor; ++depth) {
        target = std::max(floor, target / 2u);
    }
    return target;
}

[[nodiscard]] std::size_t queue_refill_budget(
    const DfsContext* context,
    const SearchTask& task,
    std::size_t branch_count)
{
    if (!should_refill_queue(context, task, branch_count)) {
        return 0;
    }

    if (context->queue->has_unfinished_above(
            task.path.size(),
            context->active_work_depth)) {
        return 0;
    }

    const std::size_t queue_size = context->queue->approximate_size();
    const std::size_t refill_target = depth_refill_target(context, task);
    if (queue_size >= refill_target) {
        return 0;
    }

    return std::min(
        refill_target - queue_size,
        depth_enqueue_budget(context, task));
}

struct DomainFixedRightObjects {
    std::vector<group::ObjectId> points;
    std::vector<group::ObjectId> hyperplanes;

    [[nodiscard]] group::FixedObjects view() const
    {
        return group::FixedObjects {
            .points = points,
            .hyperplanes = hyperplanes,
        };
    }
};

[[nodiscard]] DomainFixedRightObjects collect_domain_fixed_right_objects(
    std::span<const BranchMove> path)
{
    DomainFixedRightObjects fixed;
    fixed.points.reserve(path.size());
    fixed.hyperplanes.reserve(path.size());

    for (const BranchMove& move : path) {
        if (move.kind == BranchKind::DomainPoint) {
            fixed.points.push_back(move.right_object);
        } else if (move.kind == BranchKind::DomainHyperplane) {
            fixed.hyperplanes.push_back(move.right_object);
        }
    }

    return fixed;
}

[[nodiscard]] DomainStabilizerCacheKey make_domain_stabilizer_cache_key(
    std::uint64_t group_version,
    const DomainFixedRightObjects& fixed)
{
    return DomainStabilizerCacheKey {
        .group_version = group_version,
        .points = fixed.points,
        .hyperplanes = fixed.hyperplanes,
    };
}

[[nodiscard]] std::shared_ptr<const group::Stabilizer> cached_pointwise_stabilizer(
    DomainStabilizerCache& cache,
    const group::AffineGroupSnapshot& group_snapshot,
    const DomainFixedRightObjects& fixed)
{
    DomainStabilizerCacheKey key =
        make_domain_stabilizer_cache_key(group_snapshot.version, fixed);
    std::shared_ptr<const group::Stabilizer> found =
        cache.find(group_snapshot.version, key);
    if (found) {
        return found;
    }

    profile::ScopedTimer timer(profile::counters.stabilizer_ns);
    profile::count(profile::counters.stabilizer_calls);

    group::Stabilizer stabilizer;
    stabilizer.group_version = group_snapshot.version;
    if (fixed.points.empty() && fixed.hyperplanes.empty()) {
        profile::count(
            profile::counters.stabilizer_generator_checks,
            group_snapshot.generators.size());
        stabilizer.generators = group_snapshot.generators;
    } else {
        DomainFixedRightObjects parent = fixed;
        group::ObjectId fixed_object = 0;
        bool fixed_hyperplane = false;

        if (!parent.hyperplanes.empty()) {
            fixed_object = parent.hyperplanes.back();
            parent.hyperplanes.pop_back();
            fixed_hyperplane = true;
        } else {
            fixed_object = parent.points.back();
            parent.points.pop_back();
        }

        const std::shared_ptr<const group::Stabilizer> parent_stabilizer =
            cached_pointwise_stabilizer(cache, group_snapshot, parent);
        profile::count(
            profile::counters.stabilizer_generator_checks,
            parent_stabilizer->generators.size());

        if (fixed_hyperplane) {
            stabilizer.generators = group::hyperplane_stabilizer_generators(
                group_snapshot.dim,
                parent_stabilizer->generators,
                fixed_object);
        } else {
            stabilizer.generators = group::point_stabilizer_generators(
                group_snapshot.dim,
                parent_stabilizer->generators,
                fixed_object);
        }
    }

    return cache.insert_or_find(
        group_snapshot.version,
        std::move(key),
        std::move(stabilizer));
}

[[nodiscard]] bool domain_branch_can_use_group(BranchKind kind)
{
    return kind == BranchKind::DomainPoint
        || kind == BranchKind::DomainHyperplane;
}

[[nodiscard]] std::uint64_t domain_branch_group_version(
    const DfsContext* context,
    BranchKind kind)
{
    if (context == nullptr
        || context->domain_group == nullptr
        || !domain_branch_can_use_group(kind)) {
        return 0;
    }

    return context->domain_group->version();
}

[[nodiscard]] bool contains_object(
    std::span<const Partition::ObjectId> objects,
    Partition::ObjectId target)
{
    for (const Partition::ObjectId object : objects) {
        if (object == target) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<Partition::ObjectId> without_already_branched(
    std::span<const Partition::ObjectId> right_objects,
    std::span<const Partition::ObjectId> already_branched)
{
    if (already_branched.empty()) {
        return std::vector<Partition::ObjectId>(
            right_objects.begin(),
            right_objects.end());
    }

    std::vector<Partition::ObjectId> unbranched;
    unbranched.reserve(right_objects.size());
    for (const Partition::ObjectId right_object : right_objects) {
        if (!contains_object(already_branched, right_object)) {
            unbranched.push_back(right_object);
        }
    }
    return unbranched;
}

[[nodiscard]] std::vector<Partition::ObjectId> domain_branch_right_candidates(
    const DfsContext* context,
    std::span<const BranchMove> path,
    BranchKind kind,
    std::span<const Partition::ObjectId> right_objects,
    std::span<const Partition::ObjectId> already_branched = {})
{
    profile::ScopedTimer timer(profile::counters.domain_candidate_ns);
    profile::count(profile::counters.domain_candidate_calls);

    if (context == nullptr
        || context->domain_group == nullptr
        || right_objects.size() <= 1
        || !domain_branch_can_use_group(kind)) {
        return without_already_branched(right_objects, already_branched);
    }

    const group::AffineGroupSnapshot group_snapshot =
        context->domain_group->snapshot();
    if (group_snapshot.empty()) {
        return without_already_branched(right_objects, already_branched);
    }

    const DomainFixedRightObjects fixed =
        collect_domain_fixed_right_objects(path);

    std::shared_ptr<const group::Stabilizer> cached_stabilizer;
    group::Stabilizer local_stabilizer;
    const group::Stabilizer* stabilizer = nullptr;
    if (context->domain_stabilizer_cache != nullptr) {
        cached_stabilizer = cached_pointwise_stabilizer(
            *context->domain_stabilizer_cache,
            group_snapshot,
            fixed);
        stabilizer = cached_stabilizer.get();
    } else {
        local_stabilizer = group::pointwise_stabilizer(group_snapshot, fixed.view());
        stabilizer = &local_stabilizer;
    }

    if (kind == BranchKind::DomainPoint) {
        const group::OrbitPartition partition = group::orbit_partition(
            group_snapshot,
            *stabilizer,
            right_objects,
            [](const group::AffineGroupGenerator& generator, group::ObjectId object) {
                return generator.apply_point(object);
            });
        return group::uncovered_representatives(
            partition,
            right_objects,
            already_branched);
    }

    const group::OrbitPartition partition = group::orbit_partition(
        group_snapshot,
        *stabilizer,
        right_objects,
        [](const group::AffineGroupGenerator& generator, group::ObjectId object) {
            return generator.apply_hyperplane(object);
        });
    return group::uncovered_representatives(
        partition,
        right_objects,
        already_branched);
}

inline void push_work(
    DfsContext* context,
    std::span<const BranchMove> parent_path,
    const BranchMove& move)
{
    if (context == nullptr || context->queue == nullptr) {
        return;
    }

    WorkItem item;
    item.path.reserve(parent_path.size() + 1u);
    item.path.insert(item.path.end(), parent_path.begin(), parent_path.end());
    item.path.push_back(move);

    context->queue->push(std::move(item));
}

inline void mark_covered_queued_siblings_dead(
    DfsContext* context,
    std::span<const BranchMove> parent_path,
    BranchKind kind,
    Partition::ObjectId left_object,
    std::span<const Partition::ObjectId> queued_right_objects,
    std::span<const Partition::ObjectId> live_right_objects)
{
    if (context == nullptr || context->pruner == nullptr) {
        return;
    }

    for (const Partition::ObjectId right_object : queued_right_objects) {
        if (contains_object(live_right_objects, right_object)) {
            continue;
        }

        std::vector<BranchMove> dead_path;
        dead_path.reserve(parent_path.size() + 1u);
        dead_path.insert(dead_path.end(), parent_path.begin(), parent_path.end());
        dead_path.push_back(BranchMove {
            .kind = kind,
            .left_object = left_object,
            .right_object = right_object,
        });
        context->pruner->add_dead_prefix(std::move(dead_path));
    }
}

void dfs(const DfsProblem& problem, SearchTask& task, DfsContext* context)
{
    if ((task.options.stop_after_first_leaf && task.stats.leaves != 0)
        || should_stop(context)
        || path_is_dead(context, task.path)
        || !consume_node_budget(context)) {
        return;
    }

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
            task.path);
    }();

    if (result == RefineResult::Stop) {
        ++task.stats.leaves;
        rollback_entry();
        return;
    }

    if (is_complete(task.partitions)) {
        ++task.stats.leaves;
        rollback_entry();
        return;
    }

    const BranchCell branch_cell = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_branch_ns);
        return choose_branch_cell(task.partitions, task.options.branch_policy);
    }();
    if (branch_cell.left_cell == Partition::npos) {
        ++task.stats.leaves;
        rollback_entry();
        return;
    }

    const PartitionPair& pair = branch_pair(task.partitions, branch_cell.kind);
    const Partition::ObjectId left_object =
        pair.left.objects(branch_cell.left_cell).front();
    const std::span<const Partition::ObjectId> right_objects =
        pair.right.objects(branch_cell.right_cell);
    std::vector<Partition::ObjectId> already_branched;
    std::vector<Partition::ObjectId> branch_right_objects;
    std::uint64_t observed_group_version = 0;
    {
        profile::ScopedTimer timer(
            profile::counters.dfs_local_initial_candidates_ns);
        branch_right_objects = domain_branch_right_candidates(
            context,
            task.path,
            branch_cell.kind,
            right_objects);
        observed_group_version =
            domain_branch_group_version(context, branch_cell.kind);
    }
    std::vector<Partition::ObjectId> queued_right_objects;

    auto search_child = [&](Partition::ObjectId right_object) {
        if ((task.options.stop_after_first_leaf && task.stats.leaves != 0)
            || should_stop(context)) {
            return;
        }

        const SearchPartitionSnapshot child = snapshot(task.partitions);
        const BranchMove move {
            .kind = branch_cell.kind,
            .left_object = left_object,
            .right_object = right_object,
        };

        if (apply_branch_move(problem, task.partitions, move)) {
            task.path.push_back(move);
            if (!path_is_dead(context, task.path)) {
                dfs(problem, task, context);
            }
            task.path.pop_back();
        } else {
            ++task.stats.pruned;
        }

        rollback(task.partitions, child);
        already_branched.push_back(right_object);
    };

    auto is_unhandled_locally = [&](Partition::ObjectId right_object) {
        return !contains_object(already_branched, right_object)
            && !contains_object(queued_right_objects, right_object);
    };

    auto next_local_candidate = [&](Partition::ObjectId& right_object) {
        for (const Partition::ObjectId candidate : branch_right_objects) {
            if (is_unhandled_locally(candidate)) {
                right_object = candidate;
                return true;
            }
        }
        return false;
    };

    auto recompute_if_group_changed = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_recompute_ns);
        const std::uint64_t current_group_version =
            domain_branch_group_version(context, branch_cell.kind);
        if (current_group_version != observed_group_version) {
            observed_group_version = current_group_version;
            branch_right_objects = domain_branch_right_candidates(
                context,
                task.path,
                branch_cell.kind,
                right_objects,
                already_branched);
            mark_covered_queued_siblings_dead(
                context,
                task.path,
                branch_cell.kind,
                left_object,
                queued_right_objects,
                branch_right_objects);
        }
    };

    auto enqueue_more_work = [&] {
        profile::ScopedTimer timer(profile::counters.dfs_local_enqueue_ns);
        std::size_t budget = queue_refill_budget(
            context,
            task,
            branch_right_objects.size());
        if (budget == 0) {
            return;
        }

        Partition::ObjectId reserved_local = 0;
        const bool has_reserved_local = next_local_candidate(reserved_local);

        for (const Partition::ObjectId right_object : branch_right_objects) {
            if (budget == 0) {
                break;
            }
            if (!is_unhandled_locally(right_object)) {
                continue;
            }
            if (has_reserved_local && right_object == reserved_local) {
                continue;
            }

            queued_right_objects.push_back(right_object);
            const BranchMove move {
                .kind = branch_cell.kind,
                .left_object = left_object,
                .right_object = right_object,
            };
            push_work(context, task.path, move);
            --budget;
        }
    };

    while (true) {
        if (should_stop(context)) {
            break;
        }

        recompute_if_group_changed();
        enqueue_more_work();

        Partition::ObjectId right_object = 0;
        const bool has_next_candidate = [&] {
            profile::ScopedTimer timer(
                profile::counters.dfs_local_next_candidate_ns);
            return next_local_candidate(right_object);
        }();
        if (!has_next_candidate) {
            break;
        }

        local_node_timer.pause();
        search_child(right_object);
        local_node_timer.resume();

        if ((task.options.stop_after_first_leaf && task.stats.leaves != 0)
            || should_stop(context)) {
            break;
        }
    }

    rollback_entry();
}

void dfs(
    const DfsProblem& problem,
    SearchTask& task,
    SolutionStore* solution_store = nullptr,
    const group::AffineGroup* domain_group = nullptr)
{
    DfsContext context;
    context.solution_store = solution_store;
    context.domain_group = domain_group != nullptr
        ? domain_group
        : (solution_store == nullptr ? nullptr : &solution_store->a1_group());
    context.domain_stabilizer_cache = &task.domain_stabilizer_cache;
    dfs(problem, task, &context);
}

} // namespace affine
