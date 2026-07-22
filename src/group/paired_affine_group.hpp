#pragma once

#include "affine_group.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affine::group {

struct PairedAffineMap {
    f2::AffineMap domain_map;
    f2::AffineMap codomain_map;
};

struct PairedAffineGroupGenerator {
    AffineGroupGenerator domain;
    AffineGroupGenerator codomain;

    [[nodiscard]] ObjectId apply_domain_point(ObjectId point) const
    {
        return domain.apply_point(point);
    }

    [[nodiscard]] ObjectId apply_domain_hyperplane(ObjectId hyperplane) const
    {
        return domain.apply_hyperplane(hyperplane);
    }

    [[nodiscard]] ObjectId apply_codomain_point(ObjectId point) const
    {
        return codomain.apply_point(point);
    }

    [[nodiscard]] ObjectId apply_codomain_hyperplane(ObjectId hyperplane) const
    {
        return codomain.apply_hyperplane(hyperplane);
    }
};

struct PairedAffineGroupSnapshot {
    std::vector<PairedAffineGroupGenerator> generators;
    std::uint64_t version = 0;
    std::uint32_t domain_dim = 0;
    std::uint32_t codomain_dim = 0;

    [[nodiscard]] bool empty() const
    {
        return generators.empty();
    }
};

struct PairedAffineGroupKey {
    std::vector<std::uint32_t> words;

    [[nodiscard]] bool operator==(const PairedAffineGroupKey& other) const
    {
        return words == other.words;
    }
};

struct PairedAffineGroupKeyHash {
    [[nodiscard]] std::size_t operator()(const PairedAffineGroupKey& key) const
    {
        std::size_t hash = 1469598103934665603ull;
        for (const std::uint32_t word : key.words) {
            hash ^= word;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

inline void append_paired_group_key_map(
    std::vector<std::uint32_t>& words,
    const f2::AffineMap& map)
{
    words.push_back(map.dim);
    words.push_back(map.translation);
    words.insert(words.end(), map.basis_images.begin(), map.basis_images.end());
}

[[nodiscard]] inline PairedAffineGroupKey make_paired_group_key(
    const PairedAffineMap& map)
{
    PairedAffineGroupKey key;
    key.words.reserve(
        4u + map.domain_map.basis_images.size()
        + map.codomain_map.basis_images.size());
    append_paired_group_key_map(key.words, map.domain_map);
    append_paired_group_key_map(key.words, map.codomain_map);
    return key;
}

[[nodiscard]] inline bool make_paired_generator(
    const PairedAffineMap& map,
    PairedAffineGroupGenerator& generator)
{
    return make_generator(map.domain_map, generator.domain)
        && make_generator(map.codomain_map, generator.codomain);
}

[[nodiscard]] inline PairedAffineMap identity_paired_affine_map(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim)
{
    return PairedAffineMap {
        .domain_map = identity_affine_map(domain_dim),
        .codomain_map = identity_affine_map(codomain_dim),
    };
}

[[nodiscard]] inline bool same_paired_affine_map(
    const PairedAffineMap& left,
    const PairedAffineMap& right)
{
    return same_affine_map(left.domain_map, right.domain_map)
        && same_affine_map(left.codomain_map, right.codomain_map);
}

[[nodiscard]] inline bool is_identity_paired_affine_map(
    const PairedAffineMap& map)
{
    return is_identity_affine_map(map.domain_map)
        && is_identity_affine_map(map.codomain_map);
}

// Returns outer(inner(x)) on both coordinates.
[[nodiscard]] inline PairedAffineMap compose_paired_affine_maps(
    const PairedAffineMap& outer,
    const PairedAffineMap& inner)
{
    return PairedAffineMap {
        .domain_map = compose_affine_maps(outer.domain_map, inner.domain_map),
        .codomain_map = compose_affine_maps(
            outer.codomain_map,
            inner.codomain_map),
    };
}

[[nodiscard]] inline PairedAffineMap inverse_paired_affine_map(
    const PairedAffineMap& map)
{
    return PairedAffineMap {
        .domain_map = inverse_affine_map(map.domain_map),
        .codomain_map = inverse_affine_map(map.codomain_map),
    };
}

[[nodiscard]] inline PairedAffineMap generator_map(
    const PairedAffineGroupGenerator& generator)
{
    return PairedAffineMap {
        .domain_map = generator.domain.map,
        .codomain_map = generator.codomain.map,
    };
}

class PairedAffineGroup {
public:
    [[nodiscard]] bool add_generator(
        const f2::AffineMap& domain_map,
        const f2::AffineMap& codomain_map)
    {
        return add_generator(PairedAffineMap {
            .domain_map = domain_map,
            .codomain_map = codomain_map,
        });
    }

    [[nodiscard]] bool add_generator(const PairedAffineMap& map)
    {
        if (is_identity_paired_affine_map(map)) {
            return false;
        }

        PairedAffineGroupKey key = make_paired_group_key(map);
        if (known_contains(key)) {
            return false;
        }

        PairedAffineGroupGenerator generator;
        if (!make_paired_generator(map, generator)) {
            return false;
        }

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            if (domain_dim_ != unset_dim && domain_dim_ != map.domain_map.dim) {
                return false;
            }
            if (codomain_dim_ != unset_dim && codomain_dim_ != map.codomain_map.dim) {
                return false;
            }
            if (known_.find(key) != known_.end()) {
                return false;
            }

            if (domain_dim_ == unset_dim) {
                domain_dim_ = map.domain_map.dim;
            }
            if (codomain_dim_ == unset_dim) {
                codomain_dim_ = map.codomain_map.dim;
            }

            generators_.push_back(std::move(generator));
            known_.insert(std::move(key));

            const std::uint64_t new_version =
                version_.load(std::memory_order_relaxed) + 1u;
            publish_snapshot_locked(new_version);
            version_.store(new_version, std::memory_order_release);
        }
        return true;
    }

    [[nodiscard]] PairedAffineGroupSnapshot snapshot() const
    {
        const std::shared_ptr<const PairedAffineGroupSnapshot> published =
            published_snapshot_.load(std::memory_order_acquire);
        if (published == nullptr) {
            return {};
        }
        return *published;
    }

    [[nodiscard]] std::uint64_t version() const
    {
        return version_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return generators_.size();
    }

private:
    [[nodiscard]] bool known_contains(const PairedAffineGroupKey& key) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return known_.find(key) != known_.end();
    }

    void publish_snapshot_locked(std::uint64_t version) const
    {
        auto snapshot = std::make_shared<PairedAffineGroupSnapshot>();
        snapshot->version = version;
        snapshot->domain_dim = domain_dim_ == unset_dim ? 0 : domain_dim_;
        snapshot->codomain_dim = codomain_dim_ == unset_dim ? 0 : codomain_dim_;
        snapshot->generators = generators_;
        published_snapshot_.store(std::move(snapshot), std::memory_order_release);
    }

    mutable std::shared_mutex mutex_;
    std::vector<PairedAffineGroupGenerator> generators_;
    std::unordered_set<PairedAffineGroupKey, PairedAffineGroupKeyHash> known_;
    mutable std::atomic<std::shared_ptr<const PairedAffineGroupSnapshot>>
        published_snapshot_;
    std::atomic<std::uint64_t> version_ = 0;
    static constexpr std::uint32_t unset_dim =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t domain_dim_ = unset_dim;
    std::uint32_t codomain_dim_ = unset_dim;
};

} // namespace affine::group
