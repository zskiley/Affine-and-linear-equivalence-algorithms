#pragma once

#include "../affine_map.hpp"
#include "../radon_f2.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affine::group {

using ObjectId = std::uint32_t;

struct SchreierSimsMapKey {
    std::vector<std::uint32_t> words;

    [[nodiscard]] bool operator==(const SchreierSimsMapKey& other) const
    {
        return words == other.words;
    }
};

struct SchreierSimsMapKeyHash {
    [[nodiscard]] std::size_t operator()(const SchreierSimsMapKey& key) const
    {
        std::size_t hash = 1469598103934665603ull;
        for (const std::uint32_t word : key.words) {
            hash ^= word;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

[[nodiscard]] inline SchreierSimsMapKey make_schreier_sims_key(
    const f2::AffineMap& map)
{
    SchreierSimsMapKey key;
    key.words.reserve(2u + map.basis_images.size());
    key.words.push_back(map.dim);
    key.words.push_back(map.translation);
    key.words.insert(key.words.end(), map.basis_images.begin(), map.basis_images.end());
    return key;
}

[[nodiscard]] inline f2::AffineMap identity_affine_map(std::uint32_t dim)
{
    f2::AffineMap identity;
    identity.dim = dim;
    identity.translation = 0;
    identity.basis_images.resize(dim);
    for (std::uint32_t bit = 0; bit < dim; ++bit) {
        identity.basis_images[bit] = std::uint32_t { 1 } << bit;
    }
    return identity;
}

[[nodiscard]] inline bool same_affine_map(
    const f2::AffineMap& left,
    const f2::AffineMap& right)
{
    return left.dim == right.dim
        && left.translation == right.translation
        && left.basis_images == right.basis_images;
}

[[nodiscard]] inline bool is_identity_affine_map(const f2::AffineMap& map)
{
    if (map.translation != 0 || map.basis_images.size() != map.dim) {
        return false;
    }

    for (std::uint32_t bit = 0; bit < map.dim; ++bit) {
        if (map.basis_images[bit] != (std::uint32_t { 1 } << bit)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline std::uint32_t apply_linear_part(
    const f2::AffineMap& map,
    std::uint32_t vector)
{
    std::uint32_t image = 0;
    while (vector != 0) {
        const std::uint32_t bit = std::countr_zero(vector);
        image ^= map.basis_images[bit];
        vector &= vector - 1u;
    }
    return image;
}

// Returns outer(inner(x)).
[[nodiscard]] inline f2::AffineMap compose_affine_maps(
    const f2::AffineMap& outer,
    const f2::AffineMap& inner)
{
    f2::AffineMap composed;
    composed.dim = outer.dim;
    composed.translation = f2::apply(outer, inner.translation);
    composed.basis_images.resize(outer.dim);
    for (std::uint32_t bit = 0; bit < outer.dim; ++bit) {
        composed.basis_images[bit] =
            apply_linear_part(outer, inner.basis_images[bit]);
    }
    return composed;
}

[[nodiscard]] inline bool schreier_invert_linear_part(
    const f2::AffineMap& map,
    std::vector<std::uint32_t>& inverse_rows)
{
    if (map.dim == 0 || map.dim >= 32 || map.basis_images.size() != map.dim) {
        return false;
    }

    const std::uint64_t mask = (std::uint64_t { 1 } << map.dim) - 1u;
    std::vector<std::uint64_t> rows(map.dim);

    for (std::uint32_t row = 0; row < map.dim; ++row) {
        std::uint64_t left = 0;
        for (std::uint32_t col = 0; col < map.dim; ++col) {
            if (((map.basis_images[col] >> row) & 1u) != 0) {
                left |= std::uint64_t { 1 } << col;
            }
        }
        rows[row] = left | (std::uint64_t { 1 } << (map.dim + row));
    }

    for (std::uint32_t col = 0; col < map.dim; ++col) {
        std::uint32_t pivot = col;
        while (pivot < map.dim && ((rows[pivot] >> col) & 1u) == 0) {
            ++pivot;
        }
        if (pivot == map.dim) {
            return false;
        }

        std::swap(rows[col], rows[pivot]);
        for (std::uint32_t row = 0; row < map.dim; ++row) {
            if (row != col && ((rows[row] >> col) & 1u) != 0) {
                rows[row] ^= rows[col];
            }
        }
    }

    inverse_rows.resize(map.dim);
    for (std::uint32_t row = 0; row < map.dim; ++row) {
        inverse_rows[row] = static_cast<std::uint32_t>((rows[row] >> map.dim) & mask);
    }
    return true;
}

[[nodiscard]] inline std::uint32_t schreier_parity(std::uint32_t value)
{
    return static_cast<std::uint32_t>(std::popcount(value) & 1u);
}

[[nodiscard]] inline std::uint32_t apply_inverse_linear_rows(
    std::span<const std::uint32_t> inverse_rows,
    std::uint32_t vector)
{
    std::uint32_t image = 0;
    for (std::uint32_t row = 0; row < inverse_rows.size(); ++row) {
        if (schreier_parity(inverse_rows[row] & vector) != 0) {
            image |= std::uint32_t { 1 } << row;
        }
    }
    return image;
}

[[nodiscard]] inline bool invert_affine_map(
    const f2::AffineMap& map,
    f2::AffineMap& inverse)
{
    std::vector<std::uint32_t> inverse_rows;
    if (!schreier_invert_linear_part(map, inverse_rows)) {
        return false;
    }

    inverse.dim = map.dim;
    inverse.translation = apply_inverse_linear_rows(inverse_rows, map.translation);
    inverse.basis_images.resize(map.dim);
    for (std::uint32_t bit = 0; bit < map.dim; ++bit) {
        inverse.basis_images[bit] =
            apply_inverse_linear_rows(inverse_rows, std::uint32_t { 1 } << bit);
    }
    return true;
}

[[nodiscard]] inline f2::AffineMap inverse_affine_map(const f2::AffineMap& map)
{
    f2::AffineMap inverse;
    const bool inverted = invert_affine_map(map, inverse);
    (void)inverted;
    return inverse;
}

[[nodiscard]] inline std::vector<ObjectId> affine_base(std::uint32_t dim)
{
    std::vector<ObjectId> base;
    base.reserve(static_cast<std::size_t>(dim) + 1u);
    base.push_back(0);
    for (std::uint32_t bit = 0; bit < dim; ++bit) {
        base.push_back(std::uint32_t { 1 } << bit);
    }
    return base;
}

struct SchreierSimsLevel {
    ObjectId base_point = 0;
    std::vector<f2::AffineMap> strong_generators;
    std::unordered_set<SchreierSimsMapKey, SchreierSimsMapKeyHash> known_generators;
    std::unordered_map<ObjectId, f2::AffineMap> transversals;
    std::vector<ObjectId> orbit;
};

class SchreierSims {
public:
    SchreierSims() = default;

    void reset(std::uint32_t dim)
    {
        dim_ = dim;
        base_ = affine_base(dim);
        levels_.clear();
        levels_.resize(base_.size());
        for (std::size_t level = 0; level < levels_.size(); ++level) {
            levels_[level].base_point = base_[level];
        }
        compute_all_orbits();
    }

    [[nodiscard]] bool rebuild(
        std::uint32_t dim,
        std::span<const f2::AffineMap> generators)
    {
        reset(dim);

        for (const f2::AffineMap& generator : generators) {
            if (generator.dim != dim) {
                return false;
            }
            (void)add_strong_generator_direct(0, generator);

            f2::AffineMap inverse;
            if (!invert_affine_map(generator, inverse)) {
                return false;
            }
            (void)add_strong_generator_direct(0, inverse);
        }

        bool changed = true;
        while (changed) {
            compute_all_orbits();
            changed = close_once();
        }
        compute_all_orbits();
        return true;
    }

    template<class GroupSnapshot>
    [[nodiscard]] bool rebuild_from_snapshot(const GroupSnapshot& group)
    {
        std::vector<f2::AffineMap> generators;
        generators.reserve(group.generators.size());
        for (const auto& generator : group.generators) {
            generators.push_back(generator.map);
        }
        return rebuild(group.dim, generators);
    }

    [[nodiscard]] bool add_generator(const f2::AffineMap& generator)
    {
        if (levels_.empty()) {
            reset(generator.dim);
        }
        if (generator.dim != dim_ || contains(generator)) {
            return false;
        }

        return add_generator_known_new(generator);
    }

    [[nodiscard]] bool add_generator_known_new(const f2::AffineMap& generator)
    {
        if (levels_.empty()) {
            reset(generator.dim);
        }
        if (generator.dim != dim_) {
            return false;
        }

        bool changed = add_strong_generator_direct(0, generator);

        f2::AffineMap inverse;
        if (!invert_affine_map(generator, inverse)) {
            return false;
        }
        changed |= add_strong_generator_direct(0, inverse);
        if (!changed) {
            return false;
        }

        while (changed) {
            compute_all_orbits();
            changed = close_once();
        }
        compute_all_orbits();
        return true;
    }

    [[nodiscard]] bool contains(const f2::AffineMap& map) const
    {
        if (map.dim != dim_ || levels_.empty()) {
            return false;
        }

        f2::AffineMap remainder = map;
        for (std::size_t level = 0; level < levels_.size(); ++level) {
            const ObjectId image = f2::apply(remainder, base_[level]);
            const auto found = levels_[level].transversals.find(image);
            if (found == levels_[level].transversals.end()) {
                return false;
            }

            const f2::AffineMap inverse_transversal =
                inverse_affine_map(found->second);
            remainder = compose_affine_maps(inverse_transversal, remainder);
        }

        return is_identity_affine_map(remainder);
    }

    [[nodiscard]] std::span<const ObjectId> base() const
    {
        return base_;
    }

    [[nodiscard]] std::span<const SchreierSimsLevel> levels() const
    {
        return levels_;
    }

    [[nodiscard]] std::vector<std::size_t> orbit_sizes() const
    {
        std::vector<std::size_t> sizes;
        sizes.reserve(levels_.size());
        for (const SchreierSimsLevel& level : levels_) {
            sizes.push_back(level.orbit.size());
        }
        return sizes;
    }

    [[nodiscard]] std::vector<f2::AffineMap> strong_generators() const
    {
        std::vector<f2::AffineMap> generators;
        std::unordered_set<SchreierSimsMapKey, SchreierSimsMapKeyHash> known;
        for (const SchreierSimsLevel& level : levels_) {
            for (const f2::AffineMap& generator : level.strong_generators) {
                SchreierSimsMapKey key = make_schreier_sims_key(generator);
                if (known.find(key) != known.end()) {
                    continue;
                }
                known.insert(std::move(key));
                generators.push_back(generator);
            }
        }
        return generators;
    }

private:
    [[nodiscard]] bool add_strong_generator_direct(
        std::size_t level,
        const f2::AffineMap& generator)
    {
        if (level >= levels_.size() || is_identity_affine_map(generator)) {
            return false;
        }

        SchreierSimsMapKey key = make_schreier_sims_key(generator);
        if (levels_[level].known_generators.find(key)
            != levels_[level].known_generators.end()) {
            return false;
        }

        levels_[level].known_generators.insert(std::move(key));
        levels_[level].strong_generators.push_back(generator);
        return true;
    }

    [[nodiscard]] bool sift_and_add(
        std::size_t start_level,
        const f2::AffineMap& generator)
    {
        f2::AffineMap remainder = generator;
        for (std::size_t level = start_level; level < levels_.size(); ++level) {
            const ObjectId image = f2::apply(remainder, base_[level]);
            const auto found = levels_[level].transversals.find(image);
            if (found == levels_[level].transversals.end()) {
                return add_strong_generator_direct(level, remainder);
            }

            const f2::AffineMap inverse_transversal =
                inverse_affine_map(found->second);
            remainder = compose_affine_maps(inverse_transversal, remainder);
            if (is_identity_affine_map(remainder)) {
                return false;
            }
        }

        return false;
    }

    void compute_all_orbits()
    {
        for (std::size_t level = 0; level < levels_.size(); ++level) {
            compute_orbit(level);
        }
    }

    void compute_orbit(std::size_t level)
    {
        SchreierSimsLevel& data = levels_[level];
        data.transversals.clear();
        data.orbit.clear();
        const std::vector<f2::AffineMap> generators =
            suffix_strong_generators(level);

        const f2::AffineMap identity = identity_affine_map(dim_);
        data.transversals.emplace(data.base_point, identity);
        data.orbit.push_back(data.base_point);

        for (std::size_t cursor = 0; cursor < data.orbit.size(); ++cursor) {
            const ObjectId object = data.orbit[cursor];
            const f2::AffineMap& transversal = data.transversals[object];

            for (const f2::AffineMap& generator : generators) {
                const f2::AffineMap next_transversal =
                    compose_affine_maps(generator, transversal);
                const ObjectId image = f2::apply(next_transversal, data.base_point);
                if (data.transversals.find(image) != data.transversals.end()) {
                    continue;
                }

                data.transversals.emplace(image, next_transversal);
                data.orbit.push_back(image);
            }
        }
    }

    [[nodiscard]] bool close_once()
    {
        bool changed = false;
        for (std::size_t level = 0; level < levels_.size(); ++level) {
            const std::vector<f2::AffineMap> generators =
                suffix_strong_generators(level);
            const std::vector<std::pair<ObjectId, f2::AffineMap>> transversals(
                levels_[level].transversals.begin(),
                levels_[level].transversals.end());

            for (const auto& [object, transversal] : transversals) {
                (void)object;
                for (const f2::AffineMap& generator : generators) {
                    const f2::AffineMap moved =
                        compose_affine_maps(generator, transversal);
                    const ObjectId image =
                        f2::apply(moved, levels_[level].base_point);
                    const auto found = levels_[level].transversals.find(image);
                    if (found == levels_[level].transversals.end()) {
                        continue;
                    }

                    const f2::AffineMap inverse_transversal =
                        inverse_affine_map(found->second);
                    const f2::AffineMap schreier_generator =
                        compose_affine_maps(inverse_transversal, moved);
                    changed |= sift_and_add(level + 1u, schreier_generator);
                }
            }
        }
        return changed;
    }

    [[nodiscard]] std::vector<f2::AffineMap> suffix_strong_generators(
        std::size_t level) const
    {
        std::vector<f2::AffineMap> generators;
        for (std::size_t current = level; current < levels_.size(); ++current) {
            generators.insert(
                generators.end(),
                levels_[current].strong_generators.begin(),
                levels_[current].strong_generators.end());
        }
        return generators;
    }

    std::uint32_t dim_ = 0;
    std::vector<ObjectId> base_;
    std::vector<SchreierSimsLevel> levels_;
};

} // namespace affine::group
