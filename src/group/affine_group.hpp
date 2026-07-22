#pragma once

#include "../affine_map.hpp"
#include "../profile.hpp"
#include "../radon_f2.hpp"
#include "schreier_sims.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affine::group {

using ObjectId = std::uint32_t;

struct FixedObjects {
    std::span<const ObjectId> points;
    std::span<const ObjectId> hyperplanes;
};

struct AffineGroupKey {
    std::vector<std::uint32_t> words;

    [[nodiscard]] bool operator==(const AffineGroupKey& other) const
    {
        return words == other.words;
    }
};

struct AffineGroupKeyHash {
    [[nodiscard]] std::size_t operator()(const AffineGroupKey& key) const
    {
        std::size_t hash = 1469598103934665603ull;
        for (const std::uint32_t word : key.words) {
            hash ^= word;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

[[nodiscard]] inline AffineGroupKey make_key(const f2::AffineMap& map)
{
    AffineGroupKey key;
    key.words.reserve(2u + map.basis_images.size());
    key.words.push_back(map.dim);
    key.words.push_back(map.translation);
    key.words.insert(key.words.end(), map.basis_images.begin(), map.basis_images.end());
    return key;
}

[[nodiscard]] inline bool invert_linear_part(
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

[[nodiscard]] inline std::uint32_t parity(std::uint32_t value)
{
    return static_cast<std::uint32_t>(std::popcount(value) & 1u);
}

struct AffineGroupGenerator {
    f2::AffineMap map;
    std::vector<std::uint32_t> inverse_rows;

    [[nodiscard]] ObjectId apply_point(ObjectId point) const
    {
        return f2::apply(map, point);
    }

    [[nodiscard]] std::uint32_t apply_dual(std::uint32_t normal) const
    {
        std::uint32_t image = 0;
        for (std::uint32_t bit = 0; bit < map.dim; ++bit) {
            if ((normal & (std::uint32_t { 1 } << bit)) != 0) {
                image ^= inverse_rows[bit];
            }
        }
        return image;
    }

    [[nodiscard]] ObjectId apply_hyperplane(ObjectId hyperplane) const
    {
        const std::uint32_t normal = f2::hyperplane_normal(hyperplane);
        const std::uint32_t offset = f2::hyperplane_offset(hyperplane);
        const std::uint32_t image_normal = apply_dual(normal);
        const std::uint32_t image_offset =
            offset ^ parity(image_normal & map.translation);
        return f2::hyperplane_id(image_normal, image_offset);
    }
};

[[nodiscard]] inline bool make_generator(
    const f2::AffineMap& map,
    AffineGroupGenerator& generator)
{
    generator.map = map;
    return invert_linear_part(map, generator.inverse_rows);
}

struct AffineGroupSnapshot {
    std::vector<AffineGroupGenerator> generators;
    std::uint64_t version = 0;
    std::uint32_t dim = 0;

    [[nodiscard]] bool empty() const
    {
        return generators.empty();
    }
};

class AffineGroup {
public:
    [[nodiscard]] bool add_generator(const f2::AffineMap& map)
    {
        profile::count(profile::counters.group_add_calls);
        AffineGroupKey key = make_key(map);
        if (generated_cache_contains(key)) {
            profile::count(profile::counters.group_add_cache_hits);
            return false;
        }

        AffineGroupGenerator generator;
        {
            profile::ScopedTimer timer(profile::counters.group_add_prepare_ns);
            if (!make_generator(map, generator)) {
                return false;
            }
        }

        {
            profile::ScopedTimer timer(profile::counters.group_add_precheck_ns);
            const std::shared_ptr<const SchreierSims> membership =
                std::atomic_load_explicit(
                    &published_membership_, std::memory_order_acquire);
            if (membership != nullptr && membership->contains(map)) {
                profile::count(profile::counters.group_add_membership_hits);
                remember_generated(key);
                return false;
            }
        }

        if (generated_cache_contains(key)) {
            profile::count(profile::counters.group_add_cache_hits);
            return false;
        }

        {
            profile::ScopedTimer timer(profile::counters.group_add_lock_ns);
            std::unique_lock<std::shared_mutex> lock(mutex_);
            if (dim_ != unset_dim && dim_ != map.dim) {
                return false;
            }
            if (dim_ == unset_dim) {
                dim_ = map.dim;
                schreier_sims_.reset(map.dim);
            }
            if (known_.find(key) != known_.end()) {
                remember_generated(key);
                return false;
            }
            if (schreier_sims_.contains(map)) {
                profile::count(profile::counters.group_add_membership_hits);
                remember_generated(key);
                return false;
            }
            if (!schreier_sims_.add_generator_known_new(map)) {
                return false;
            }

            generators_.push_back(std::move(generator));
            known_.insert(key);
            remember_generated(key);

            const std::uint64_t new_version =
                version_.load(std::memory_order_relaxed) + 1u;
            publish_read_models_locked(new_version);
            version_.store(new_version, std::memory_order_release);
            profile::count(profile::counters.group_add_insertions);
        }
        return true;
    }

    [[nodiscard]] AffineGroupSnapshot snapshot() const
    {
        profile::ScopedTimer timer(profile::counters.group_snapshot_ns);
        const std::shared_ptr<const AffineGroupSnapshot> published =
            std::atomic_load_explicit(
                &published_snapshot_, std::memory_order_acquire);
        if (published == nullptr) {
            profile::count(profile::counters.group_snapshot_calls);
            return {};
        }

        profile::count(profile::counters.group_snapshot_calls);
        profile::count(
            profile::counters.group_snapshot_generators,
            published->generators.size());
        return *published;
    }

    [[nodiscard]] std::uint64_t version() const
    {
        profile::ScopedTimer timer(profile::counters.group_version_ns);
        profile::count(profile::counters.group_version_calls);
        return version_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return generators_.size();
    }

private:
    [[nodiscard]] bool generated_cache_contains(const AffineGroupKey& key) const
    {
        std::shared_lock<std::shared_mutex> lock(generated_cache_mutex_);
        return generated_members_.find(key) != generated_members_.end();
    }

    void remember_generated(const AffineGroupKey& key)
    {
        std::unique_lock<std::shared_mutex> lock(generated_cache_mutex_);
        generated_members_.insert(key);
    }

    [[nodiscard]] std::shared_ptr<const AffineGroupSnapshot>
    make_snapshot_locked(std::uint64_t version) const
    {
        auto snapshot = std::make_shared<AffineGroupSnapshot>();
        snapshot->version = version;
        snapshot->dim = dim_ == unset_dim ? 0 : dim_;
        if (dim_ == unset_dim) {
            return snapshot;
        }

        const std::vector<f2::AffineMap> strong_maps =
            schreier_sims_.strong_generators();
        snapshot->generators.reserve(strong_maps.size());
        for (const f2::AffineMap& map : strong_maps) {
            AffineGroupGenerator generator;
            if (make_generator(map, generator)) {
                snapshot->generators.push_back(std::move(generator));
            }
        }
        return snapshot;
    }

    void publish_read_models_locked(std::uint64_t version) const
    {
        std::shared_ptr<const AffineGroupSnapshot> snapshot =
            make_snapshot_locked(version);
        std::atomic_store_explicit(
            &published_snapshot_, std::move(snapshot), std::memory_order_release);
        std::shared_ptr<const SchreierSims> membership =
            dim_ == unset_dim
                ? nullptr
                : std::make_shared<SchreierSims>(schreier_sims_);
        std::atomic_store_explicit(
            &published_membership_,
            std::move(membership),
            std::memory_order_release);
    }

    mutable std::shared_mutex mutex_;
    mutable std::shared_mutex generated_cache_mutex_;
    std::vector<AffineGroupGenerator> generators_;
    std::unordered_set<AffineGroupKey, AffineGroupKeyHash> known_;
    std::unordered_set<AffineGroupKey, AffineGroupKeyHash> generated_members_;
    SchreierSims schreier_sims_;
    mutable std::shared_ptr<const AffineGroupSnapshot> published_snapshot_;
    mutable std::shared_ptr<const SchreierSims> published_membership_;
    std::atomic<std::uint64_t> version_ = 0;
    static constexpr std::uint32_t unset_dim = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t dim_ = unset_dim;
};

[[nodiscard]] inline bool fixes_all(
    const AffineGroupGenerator& generator,
    FixedObjects fixed)
{
    for (const ObjectId point : fixed.points) {
        if (generator.apply_point(point) != point) {
            return false;
        }
    }

    for (const ObjectId hyperplane : fixed.hyperplanes) {
        if (generator.apply_hyperplane(hyperplane) != hyperplane) {
            return false;
        }
    }

    return true;
}

} // namespace affine::group
