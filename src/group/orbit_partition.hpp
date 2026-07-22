#pragma once

#include "affine_group.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace affine::group {

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

} // namespace affine::group
