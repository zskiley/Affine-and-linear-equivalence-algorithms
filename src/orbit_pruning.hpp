#pragma once

#include "group/orbit_partition.hpp"
#include "group/paired_affine_group.hpp"
#include "profile.hpp"
#include "search_types.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affine {

class SolutionStore;

struct PairedFixedRightObjects {
    std::vector<group::ObjectId> domain_points;
    std::vector<group::ObjectId> domain_hyperplanes;
    std::vector<group::ObjectId> codomain_points;
    std::vector<group::ObjectId> codomain_hyperplanes;

    [[nodiscard]] bool empty() const
    {
        return domain_points.empty()
            && domain_hyperplanes.empty()
            && codomain_points.empty()
            && codomain_hyperplanes.empty();
    }
};

struct PairedStabilizerCacheKey {
    PairedFixedRightObjects fixed;

    [[nodiscard]] bool operator==(const PairedStabilizerCacheKey& other) const
    {
        return fixed.domain_points == other.fixed.domain_points
            && fixed.domain_hyperplanes == other.fixed.domain_hyperplanes
            && fixed.codomain_points == other.fixed.codomain_points
            && fixed.codomain_hyperplanes == other.fixed.codomain_hyperplanes;
    }
};

struct PairedStabilizerCacheKeyHash {
    [[nodiscard]] std::size_t operator()(const PairedStabilizerCacheKey& key) const
    {
        std::size_t hash = 1469598103934665603ull;
        auto mix = [&](std::uint64_t value) {
            hash ^= static_cast<std::size_t>(value);
            hash *= 1099511628211ull;
        };
        auto mix_vector = [&](const std::vector<group::ObjectId>& objects) {
            mix(objects.size());
            for (const group::ObjectId object : objects) {
                mix(object);
            }
        };

        mix_vector(key.fixed.domain_points);
        mix_vector(key.fixed.domain_hyperplanes);
        mix_vector(key.fixed.codomain_points);
        mix_vector(key.fixed.codomain_hyperplanes);
        return hash;
    }
};

struct PairedStabilizer {
    std::vector<group::PairedAffineGroupGenerator> generators;
    std::uint64_t group_version = 0;
};

struct PairedStabilizerCache {
    std::uint64_t group_version = std::numeric_limits<std::uint64_t>::max();
    std::unordered_map<
        PairedStabilizerCacheKey,
        PairedStabilizer,
        PairedStabilizerCacheKeyHash> entries;

    void clear_if_version_changed(std::uint64_t version)
    {
        if (group_version == version) {
            return;
        }

        entries.clear();
        group_version = version;
    }

    [[nodiscard]] const PairedStabilizer* find(
        std::uint64_t version,
        const PairedStabilizerCacheKey& key)
    {
        clear_if_version_changed(version);

        const auto found = entries.find(key);
        if (found == entries.end()) {
            return {};
        }
        return &found->second;
    }

    [[nodiscard]] const PairedStabilizer& insert_or_find(
        std::uint64_t version,
        PairedStabilizerCacheKey key,
        PairedStabilizer stabilizer)
    {
        clear_if_version_changed(version);
        const auto [inserted, _] =
            entries.emplace(std::move(key), std::move(stabilizer));
        return inserted->second;
    }
};

struct DfsContext {
    SolutionStore* solution_store = nullptr;
    const group::PairedAffineGroup* paired_group = nullptr;
    PairedStabilizerCache* paired_stabilizer_cache = nullptr;
};

[[nodiscard]] inline PairedFixedRightObjects collect_paired_fixed_right_objects(
    std::span<const BranchMove> path)
{
    PairedFixedRightObjects fixed;
    fixed.domain_points.reserve(path.size());
    fixed.domain_hyperplanes.reserve(path.size());
    fixed.codomain_points.reserve(path.size());
    fixed.codomain_hyperplanes.reserve(path.size());

    for (const BranchMove& move : path) {
        switch (move.kind) {
        case BranchKind::DomainPoint:
            fixed.domain_points.push_back(move.right_object);
            break;
        case BranchKind::DomainHyperplane:
            fixed.domain_hyperplanes.push_back(move.right_object);
            break;
        case BranchKind::CodomainPoint:
            fixed.codomain_points.push_back(move.right_object);
            break;
        case BranchKind::CodomainHyperplane:
            fixed.codomain_hyperplanes.push_back(move.right_object);
            break;
        }
    }

    return fixed;
}

enum class FixedObjectKind {
    DomainPoint,
    DomainHyperplane,
    CodomainPoint,
    CodomainHyperplane,
};

struct FixedObject {
    FixedObjectKind kind = FixedObjectKind::DomainPoint;
    group::ObjectId object = 0;
};

[[nodiscard]] inline bool pop_fixed_object(
    PairedFixedRightObjects& fixed,
    FixedObject& object)
{
    if (!fixed.codomain_hyperplanes.empty()) {
        object = FixedObject {
            .kind = FixedObjectKind::CodomainHyperplane,
            .object = fixed.codomain_hyperplanes.back(),
        };
        fixed.codomain_hyperplanes.pop_back();
        return true;
    }
    if (!fixed.codomain_points.empty()) {
        object = FixedObject {
            .kind = FixedObjectKind::CodomainPoint,
            .object = fixed.codomain_points.back(),
        };
        fixed.codomain_points.pop_back();
        return true;
    }
    if (!fixed.domain_hyperplanes.empty()) {
        object = FixedObject {
            .kind = FixedObjectKind::DomainHyperplane,
            .object = fixed.domain_hyperplanes.back(),
        };
        fixed.domain_hyperplanes.pop_back();
        return true;
    }
    if (!fixed.domain_points.empty()) {
        object = FixedObject {
            .kind = FixedObjectKind::DomainPoint,
            .object = fixed.domain_points.back(),
        };
        fixed.domain_points.pop_back();
        return true;
    }
    return false;
}

template<class ApplyGenerator>
[[nodiscard]] std::vector<group::PairedAffineGroupGenerator>
paired_object_stabilizer_generators(
    const group::PairedAffineGroupSnapshot& group_snapshot,
    std::span<const group::PairedAffineGroupGenerator> generators,
    group::ObjectId fixed_object,
    ApplyGenerator apply_generator)
{
    if (generators.empty()) {
        return {};
    }

    std::unordered_map<group::ObjectId, std::size_t> orbit_index;
    std::vector<group::ObjectId> orbit;
    std::vector<group::PairedAffineMap> transversals;
    orbit_index.emplace(fixed_object, 0);
    orbit.push_back(fixed_object);
    transversals.push_back(group::identity_paired_affine_map(
        group_snapshot.domain_dim,
        group_snapshot.codomain_dim));

    {
        profile::ScopedTimer timer(profile::counters.stabilizer_orbit_ns);
        for (std::size_t cursor = 0; cursor < orbit.size(); ++cursor) {
            const group::ObjectId object = orbit[cursor];
            const group::PairedAffineMap transversal = transversals[cursor];

            for (const group::PairedAffineGroupGenerator& generator : generators) {
                const group::ObjectId image = apply_generator(generator, object);
                if (orbit_index.find(image) != orbit_index.end()) {
                    continue;
                }

                orbit_index.emplace(image, orbit.size());
                orbit.push_back(image);
                transversals.push_back(group::compose_paired_affine_maps(
                    group::generator_map(generator),
                    transversal));
            }
        }
    }
    profile::count(profile::counters.stabilizer_orbit_size, orbit.size());

    if (orbit.size() == 1) {
        profile::count(
            profile::counters.stabilizer_reduced_generators,
            generators.size());
        return std::vector<group::PairedAffineGroupGenerator>(
            generators.begin(),
            generators.end());
    }

    std::vector<group::PairedAffineMap> schreier_generators;
    std::vector<group::PairedAffineMap> inverse_transversals;
    std::unordered_set<
        group::PairedAffineGroupKey,
        group::PairedAffineGroupKeyHash> known;

    {
        profile::ScopedTimer timer(profile::counters.stabilizer_schreier_ns);
        inverse_transversals.reserve(transversals.size());
        for (const group::PairedAffineMap& transversal : transversals) {
            inverse_transversals.push_back(
                group::inverse_paired_affine_map(transversal));
        }

        for (std::size_t object_index = 0;
             object_index < orbit.size();
             ++object_index) {
            const group::ObjectId object = orbit[object_index];
            const group::PairedAffineMap& transversal =
                transversals[object_index];
            for (const group::PairedAffineGroupGenerator& generator : generators) {
                const group::ObjectId image = apply_generator(generator, object);
                const auto found = orbit_index.find(image);
                if (found == orbit_index.end()) {
                    continue;
                }

                const group::PairedAffineMap moved =
                    group::compose_paired_affine_maps(
                        group::generator_map(generator),
                        transversal);
                if (group::same_paired_affine_map(
                        moved,
                        transversals[found->second])) {
                    continue;
                }

                const group::PairedAffineMap stabilizer_map =
                    group::compose_paired_affine_maps(
                        inverse_transversals[found->second],
                        moved);
                if (group::is_identity_paired_affine_map(stabilizer_map)) {
                    continue;
                }

                group::PairedAffineGroupKey key =
                    group::make_paired_group_key(stabilizer_map);
                if (known.find(key) != known.end()) {
                    continue;
                }

                known.insert(std::move(key));
                schreier_generators.push_back(std::move(stabilizer_map));
            }
        }
    }
    profile::count(
        profile::counters.stabilizer_raw_generators,
        schreier_generators.size());

    std::vector<group::PairedAffineGroupGenerator> result;
    result.reserve(schreier_generators.size());
    {
        profile::ScopedTimer timer(profile::counters.stabilizer_reduce_ns);
        for (const group::PairedAffineMap& map : schreier_generators) {
            group::PairedAffineGroupGenerator generator;
            if (group::make_paired_generator(map, generator)) {
                result.push_back(std::move(generator));
            }
        }
    }
    profile::count(
        profile::counters.stabilizer_reduced_generators,
        result.size());
    return result;
}

[[nodiscard]] inline PairedStabilizerCacheKey make_paired_stabilizer_cache_key(
    PairedFixedRightObjects fixed)
{
    return PairedStabilizerCacheKey {
        .fixed = std::move(fixed),
    };
}

[[nodiscard]] inline const PairedStabilizer& cached_paired_pointwise_stabilizer(
    PairedStabilizerCache& cache,
    const group::PairedAffineGroupSnapshot& group_snapshot,
    const PairedFixedRightObjects& fixed)
{
    PairedStabilizerCacheKey key =
        make_paired_stabilizer_cache_key(fixed);
    const PairedStabilizer* found = cache.find(group_snapshot.version, key);
    if (found != nullptr) {
        return *found;
    }

    profile::ScopedTimer timer(profile::counters.stabilizer_ns);
    profile::count(profile::counters.stabilizer_calls);

    PairedStabilizer stabilizer;
    stabilizer.group_version = group_snapshot.version;

    if (fixed.empty()) {
        profile::count(
            profile::counters.stabilizer_generator_checks,
            group_snapshot.generators.size());
        stabilizer.generators = group_snapshot.generators;
    } else {
        PairedFixedRightObjects parent = fixed;
        FixedObject fixed_object;
        const bool popped = pop_fixed_object(parent, fixed_object);
        (void)popped;

        const PairedStabilizer& parent_stabilizer =
            cached_paired_pointwise_stabilizer(cache, group_snapshot, parent);
        profile::count(
            profile::counters.stabilizer_generator_checks,
            parent_stabilizer.generators.size());

        switch (fixed_object.kind) {
        case FixedObjectKind::DomainPoint:
            stabilizer.generators = paired_object_stabilizer_generators(
                group_snapshot,
                parent_stabilizer.generators,
                fixed_object.object,
                [](const group::PairedAffineGroupGenerator& generator,
                   group::ObjectId object) {
                    return generator.apply_domain_point(object);
                });
            break;
        case FixedObjectKind::DomainHyperplane:
            stabilizer.generators = paired_object_stabilizer_generators(
                group_snapshot,
                parent_stabilizer.generators,
                fixed_object.object,
                [](const group::PairedAffineGroupGenerator& generator,
                   group::ObjectId object) {
                    return generator.apply_domain_hyperplane(object);
                });
            break;
        case FixedObjectKind::CodomainPoint:
            stabilizer.generators = paired_object_stabilizer_generators(
                group_snapshot,
                parent_stabilizer.generators,
                fixed_object.object,
                [](const group::PairedAffineGroupGenerator& generator,
                   group::ObjectId object) {
                    return generator.apply_codomain_point(object);
                });
            break;
        case FixedObjectKind::CodomainHyperplane:
            stabilizer.generators = paired_object_stabilizer_generators(
                group_snapshot,
                parent_stabilizer.generators,
                fixed_object.object,
                [](const group::PairedAffineGroupGenerator& generator,
                   group::ObjectId object) {
                    return generator.apply_codomain_hyperplane(object);
                });
            break;
        }
    }

    return cache.insert_or_find(
        group_snapshot.version,
        std::move(key),
        std::move(stabilizer));
}

template<class ApplyGenerator>
[[nodiscard]] group::OrbitPartition paired_orbit_partition(
    const group::PairedAffineGroupSnapshot& group_snapshot,
    const PairedStabilizer& stabilizer,
    std::span<const group::ObjectId> objects,
    ApplyGenerator apply_generator)
{
    profile::ScopedTimer timer(profile::counters.orbit_partition_ns);
    profile::count(profile::counters.orbit_partition_calls);
    std::uint64_t generator_applications = 0;

    group::OrbitPartition partition;
    partition.group_version = group_snapshot.version;
    partition.orbit_of_index.assign(objects.size(), group::OrbitPartition::no_orbit);
    partition.representatives.reserve(objects.size());

    if (objects.empty()) {
        profile::count(
            profile::counters.orbit_generator_applications,
            generator_applications);
        return partition;
    }

    std::unordered_map<group::ObjectId, std::uint32_t> local_index;
    local_index.reserve(objects.size() * 2u);
    for (std::uint32_t index = 0; index < objects.size(); ++index) {
        local_index.emplace(objects[index], index);
    }

    std::vector<group::ObjectId> queue;
    queue.reserve(objects.size());

    for (std::uint32_t index = 0; index < objects.size(); ++index) {
        if (partition.orbit_of_index[index] != group::OrbitPartition::no_orbit) {
            continue;
        }

        const std::uint32_t orbit_id = partition.orbit_count;
        ++partition.orbit_count;
        partition.representatives.push_back(objects[index]);
        partition.orbit_of_index[index] = orbit_id;

        queue.clear();
        queue.push_back(objects[index]);

        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
            const group::ObjectId object = queue[cursor];
            for (const group::PairedAffineGroupGenerator& generator :
                 stabilizer.generators) {
                ++generator_applications;
                const group::ObjectId image = apply_generator(generator, object);
                const auto found = local_index.find(image);
                if (found == local_index.end()) {
                    continue;
                }

                const std::uint32_t image_index = found->second;
                if (partition.orbit_of_index[image_index]
                    != group::OrbitPartition::no_orbit) {
                    continue;
                }

                partition.orbit_of_index[image_index] = orbit_id;
                queue.push_back(image);
            }
        }
    }

    profile::count(
        profile::counters.orbit_generator_applications,
        generator_applications);
    return partition;
}

[[nodiscard]] inline std::uint64_t search_group_version(const DfsContext* context)
{
    if (context == nullptr || context->paired_group == nullptr) {
        return 0;
    }
    return context->paired_group->version();
}

[[nodiscard]] inline std::vector<Partition::ObjectId> domain_branch_right_candidates(
    const DfsContext* context,
    std::span<const BranchMove> path,
    BranchKind kind,
    std::span<const Partition::ObjectId> right_objects)
{
    profile::ScopedTimer timer(profile::counters.domain_candidate_ns);
    profile::count(profile::counters.domain_candidate_calls);

    if (context == nullptr
        || context->paired_group == nullptr
        || context->paired_stabilizer_cache == nullptr
        || right_objects.size() <= 1) {
        return std::vector<Partition::ObjectId>(
            right_objects.begin(),
            right_objects.end());
    }

    const group::PairedAffineGroupSnapshot group_snapshot =
        context->paired_group->snapshot();
    if (group_snapshot.empty()) {
        return std::vector<Partition::ObjectId>(
            right_objects.begin(),
            right_objects.end());
    }

    const PairedFixedRightObjects fixed =
        collect_paired_fixed_right_objects(path);
    const PairedStabilizer& stabilizer = cached_paired_pointwise_stabilizer(
        *context->paired_stabilizer_cache,
        group_snapshot,
        fixed);

    group::OrbitPartition partition;
    switch (kind) {
    case BranchKind::DomainPoint:
        partition = paired_orbit_partition(
            group_snapshot,
            stabilizer,
            right_objects,
            [](const group::PairedAffineGroupGenerator& generator,
               group::ObjectId object) {
                return generator.apply_domain_point(object);
            });
        break;
    case BranchKind::DomainHyperplane:
        partition = paired_orbit_partition(
            group_snapshot,
            stabilizer,
            right_objects,
            [](const group::PairedAffineGroupGenerator& generator,
               group::ObjectId object) {
                return generator.apply_domain_hyperplane(object);
            });
        break;
    case BranchKind::CodomainPoint:
        partition = paired_orbit_partition(
            group_snapshot,
            stabilizer,
            right_objects,
            [](const group::PairedAffineGroupGenerator& generator,
               group::ObjectId object) {
                return generator.apply_codomain_point(object);
            });
        break;
    case BranchKind::CodomainHyperplane:
        partition = paired_orbit_partition(
            group_snapshot,
            stabilizer,
            right_objects,
            [](const group::PairedAffineGroupGenerator& generator,
               group::ObjectId object) {
                return generator.apply_codomain_hyperplane(object);
            });
        break;
    }

    return group::uncovered_representatives(partition, right_objects, {});
}

} // namespace affine
