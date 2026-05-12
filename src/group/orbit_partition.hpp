#pragma once

#include "affine_group.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affine::group {

struct Stabilizer {
    std::vector<AffineGroupGenerator> generators;
    std::uint64_t group_version = 0;

    [[nodiscard]] bool empty() const
    {
        return generators.empty();
    }
};

struct OrbitPartition {
    static constexpr std::uint32_t no_orbit =
        std::numeric_limits<std::uint32_t>::max();

    std::vector<ObjectId> representatives;
    std::vector<std::uint32_t> orbit_of_index;
    std::uint32_t orbit_count = 0;
    std::uint64_t group_version = 0;

    [[nodiscard]] bool empty() const
    {
        return orbit_count == 0;
    }
};

template<class ApplyGenerator>
[[nodiscard]] std::vector<AffineGroupGenerator> object_stabilizer_generators(
    std::uint32_t dim,
    std::span<const AffineGroupGenerator> generators,
    ObjectId fixed_object,
    ApplyGenerator apply_generator)
{
    if (generators.empty()) {
        return {};
    }

    std::unordered_map<ObjectId, std::size_t> orbit_index;
    std::vector<ObjectId> orbit;
    std::vector<f2::AffineMap> transversals;
    orbit_index.emplace(fixed_object, 0);
    orbit.push_back(fixed_object);
    transversals.push_back(identity_affine_map(dim));

    {
        profile::ScopedTimer timer(profile::counters.stabilizer_orbit_ns);
        for (std::size_t cursor = 0; cursor < orbit.size(); ++cursor) {
            const ObjectId object = orbit[cursor];
            const f2::AffineMap transversal = transversals[cursor];

            for (const AffineGroupGenerator& generator : generators) {
                const ObjectId image = apply_generator(generator, object);
                if (orbit_index.find(image) != orbit_index.end()) {
                    continue;
                }

                orbit_index.emplace(image, orbit.size());
                orbit.push_back(image);
                transversals.push_back(
                    compose_affine_maps(generator.map, transversal));
            }
        }
    }
    profile::count(profile::counters.stabilizer_orbit_size, orbit.size());

    if (orbit.size() == 1) {
        profile::count(
            profile::counters.stabilizer_reduced_generators,
            generators.size());
        return std::vector<AffineGroupGenerator>(generators.begin(), generators.end());
    }

    std::vector<f2::AffineMap> schreier_generators;
    std::unordered_set<SchreierSimsMapKey, SchreierSimsMapKeyHash> known;
    std::vector<f2::AffineMap> inverse_transversals;

    {
        profile::ScopedTimer timer(profile::counters.stabilizer_schreier_ns);
        inverse_transversals.reserve(transversals.size());
        for (const f2::AffineMap& transversal : transversals) {
            inverse_transversals.push_back(inverse_affine_map(transversal));
        }

        for (std::size_t object_index = 0; object_index < orbit.size(); ++object_index) {
            const ObjectId object = orbit[object_index];
            const f2::AffineMap& transversal = transversals[object_index];
            for (const AffineGroupGenerator& generator : generators) {
                const ObjectId image = apply_generator(generator, object);
                const auto found = orbit_index.find(image);
                if (found == orbit_index.end()) {
                    continue;
                }

                const f2::AffineMap moved =
                    compose_affine_maps(generator.map, transversal);
                if (same_affine_map(moved, transversals[found->second])) {
                    continue;
                }

                const f2::AffineMap stabilizer_map =
                    compose_affine_maps(inverse_transversals[found->second], moved);
                SchreierSimsMapKey key = make_schreier_sims_key(stabilizer_map);
                if (known.find(key) != known.end()) {
                    continue;
                }

                known.insert(std::move(key));
                schreier_generators.push_back(stabilizer_map);
            }
        }
    }
    profile::count(
        profile::counters.stabilizer_raw_generators,
        schreier_generators.size());

    SchreierSims reduced;
    std::vector<f2::AffineMap> strong_generators;
    {
        profile::ScopedTimer timer(profile::counters.stabilizer_reduce_ns);
        reduced.reset(dim);
        for (const f2::AffineMap& generator : schreier_generators) {
            (void)reduced.add_generator(generator);
        }
        strong_generators = reduced.strong_generators();
    }
    profile::count(
        profile::counters.stabilizer_reduced_generators,
        strong_generators.size());

    std::vector<AffineGroupGenerator> result;
    result.reserve(strong_generators.size());
    for (const f2::AffineMap& map : strong_generators) {
        AffineGroupGenerator generator;
        if (make_generator(map, generator)) {
            result.push_back(std::move(generator));
        }
    }
    return result;
}

[[nodiscard]] inline std::vector<AffineGroupGenerator> point_stabilizer_generators(
    std::uint32_t dim,
    std::span<const AffineGroupGenerator> generators,
    ObjectId fixed_point)
{
    return object_stabilizer_generators(
        dim,
        generators,
        fixed_point,
        [](const AffineGroupGenerator& generator, ObjectId object) {
            return generator.apply_point(object);
        });
}

[[nodiscard]] inline std::vector<AffineGroupGenerator> hyperplane_stabilizer_generators(
    std::uint32_t dim,
    std::span<const AffineGroupGenerator> generators,
    ObjectId fixed_hyperplane)
{
    return object_stabilizer_generators(
        dim,
        generators,
        fixed_hyperplane,
        [](const AffineGroupGenerator& generator, ObjectId object) {
            return generator.apply_hyperplane(object);
        });
}

[[nodiscard]] inline Stabilizer pointwise_stabilizer(
    const AffineGroupSnapshot& group,
    FixedObjects fixed)
{
    profile::ScopedTimer timer(profile::counters.stabilizer_ns);
    profile::count(profile::counters.stabilizer_calls);
    profile::count(profile::counters.stabilizer_generator_checks, group.generators.size());

    Stabilizer stabilizer;
    stabilizer.group_version = group.version;
    stabilizer.generators = group.generators;

    for (const ObjectId point : fixed.points) {
        stabilizer.generators = point_stabilizer_generators(
            group.dim,
            stabilizer.generators,
            point);
        if (stabilizer.generators.empty()) {
            return stabilizer;
        }
    }

    for (const ObjectId hyperplane : fixed.hyperplanes) {
        stabilizer.generators = hyperplane_stabilizer_generators(
            group.dim,
            stabilizer.generators,
            hyperplane);
        if (stabilizer.generators.empty()) {
            return stabilizer;
        }
    }

    return stabilizer;
}

template<class Apply>
[[nodiscard]] OrbitPartition orbit_partition(
    const AffineGroupSnapshot& group,
    const Stabilizer& stabilizer,
    std::span<const ObjectId> objects,
    Apply apply)
{
    profile::ScopedTimer timer(profile::counters.orbit_partition_ns);
    profile::count(profile::counters.orbit_partition_calls);
    std::uint64_t generator_applications = 0;

    OrbitPartition partition;
    partition.group_version = group.version;
    partition.orbit_of_index.assign(objects.size(), OrbitPartition::no_orbit);
    partition.representatives.reserve(objects.size());

    if (objects.empty()) {
        profile::count(
            profile::counters.orbit_generator_applications,
            generator_applications);
        return partition;
    }

    std::unordered_map<ObjectId, std::uint32_t> local_index;
    local_index.reserve(objects.size() * 2u);
    for (std::uint32_t index = 0; index < objects.size(); ++index) {
        local_index.emplace(objects[index], index);
    }

    std::vector<ObjectId> queue;
    queue.reserve(objects.size());

    for (std::uint32_t index = 0; index < objects.size(); ++index) {
        if (partition.orbit_of_index[index] != OrbitPartition::no_orbit) {
            continue;
        }

        const std::uint32_t orbit_id = partition.orbit_count;
        ++partition.orbit_count;
        partition.representatives.push_back(objects[index]);
        partition.orbit_of_index[index] = orbit_id;

        queue.clear();
        queue.push_back(objects[index]);

        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
            const ObjectId object = queue[cursor];
            for (const AffineGroupGenerator& generator : stabilizer.generators) {
                ++generator_applications;
                const ObjectId image = apply(generator, object);
                const auto found = local_index.find(image);
                if (found == local_index.end()) {
                    continue;
                }

                const std::uint32_t image_index = found->second;
                if (partition.orbit_of_index[image_index] != OrbitPartition::no_orbit) {
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

[[nodiscard]] inline OrbitPartition point_orbit_partition(
    const AffineGroupSnapshot& group,
    FixedObjects fixed,
    std::span<const ObjectId> objects)
{
    const Stabilizer stabilizer = pointwise_stabilizer(group, fixed);
    return orbit_partition(
        group,
        stabilizer,
        objects,
        [](const AffineGroupGenerator& generator, ObjectId object) {
            return generator.apply_point(object);
        });
}

[[nodiscard]] inline OrbitPartition hyperplane_orbit_partition(
    const AffineGroupSnapshot& group,
    FixedObjects fixed,
    std::span<const ObjectId> objects)
{
    const Stabilizer stabilizer = pointwise_stabilizer(group, fixed);
    return orbit_partition(
        group,
        stabilizer,
        objects,
        [](const AffineGroupGenerator& generator, ObjectId object) {
            return generator.apply_hyperplane(object);
        });
}

[[nodiscard]] inline std::vector<ObjectId> uncovered_representatives(
    const OrbitPartition& partition,
    std::span<const ObjectId> objects,
    std::span<const ObjectId> already_covered_objects)
{
    std::vector<std::uint8_t> covered_orbits(partition.orbit_count, 0);
    for (const ObjectId covered_object : already_covered_objects) {
        for (std::uint32_t index = 0; index < objects.size(); ++index) {
            if (objects[index] != covered_object) {
                continue;
            }

            const std::uint32_t orbit = partition.orbit_of_index[index];
            if (orbit != OrbitPartition::no_orbit && orbit < covered_orbits.size()) {
                covered_orbits[orbit] = 1;
            }
            break;
        }
    }

    std::vector<ObjectId> representatives;
    representatives.reserve(partition.representatives.size());
    for (std::uint32_t index = 0; index < objects.size(); ++index) {
        const std::uint32_t orbit = partition.orbit_of_index[index];
        if (orbit == OrbitPartition::no_orbit
            || orbit >= covered_orbits.size()
            || covered_orbits[orbit] != 0) {
            continue;
        }

        representatives.push_back(objects[index]);
        covered_orbits[orbit] = 1;
    }

    return representatives;
}

template<class Apply>
[[nodiscard]] std::vector<ObjectId> orbit_representatives(
    const AffineGroupSnapshot& group,
    FixedObjects fixed,
    std::span<const ObjectId> candidates,
    Apply apply)
{
    const Stabilizer stabilizer = pointwise_stabilizer(group, fixed);
    OrbitPartition partition =
        orbit_partition(group, stabilizer, candidates, std::move(apply));
    return std::move(partition.representatives);
}

[[nodiscard]] inline std::vector<ObjectId> point_representatives(
    const AffineGroupSnapshot& group,
    FixedObjects fixed,
    std::span<const ObjectId> candidates)
{
    return orbit_representatives(
        group,
        fixed,
        candidates,
        [](const AffineGroupGenerator& generator, ObjectId object) {
            return generator.apply_point(object);
        });
}

[[nodiscard]] inline std::vector<ObjectId> hyperplane_representatives(
    const AffineGroupSnapshot& group,
    FixedObjects fixed,
    std::span<const ObjectId> candidates)
{
    return orbit_representatives(
        group,
        fixed,
        candidates,
        [](const AffineGroupGenerator& generator, ObjectId object) {
            return generator.apply_hyperplane(object);
        });
}

} // namespace affine::group
