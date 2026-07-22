#pragma once

#include "affine_map.hpp"
#include "group/affine_group.hpp"
#include "group/paired_affine_group.hpp"
#include "profile.hpp"
#include "search_types.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affine {

struct Solution {
    f2::AffineMap domain_map;
    f2::AffineMap codomain_map;
    std::vector<BranchMove> path;
};

[[nodiscard]] inline bool map_is_well_formed(const f2::AffineMap& map)
{
    if (map.dim >= 32 || map.basis_images.size() != map.dim) {
        return false;
    }

    const std::uint32_t point_count = std::uint32_t { 1 } << map.dim;
    if (map.translation >= point_count) {
        return false;
    }

    for (const std::uint32_t image : map.basis_images) {
        if (image >= point_count) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline bool verify_solution(
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    const Solution& solution,
    EquivalenceMode mode = EquivalenceMode::Affine)
{
    if (!map_is_well_formed(solution.domain_map)
        || !map_is_well_formed(solution.codomain_map)) {
        return false;
    }

    if (mode == EquivalenceMode::Linear
        && (solution.domain_map.translation != 0
            || solution.codomain_map.translation != 0)) {
        return false;
    }

    const std::uint32_t domain_point_count =
        std::uint32_t { 1 } << solution.domain_map.dim;
    const std::uint32_t codomain_point_count =
        std::uint32_t { 1 } << solution.codomain_map.dim;

    if (left_function_table.size() != domain_point_count
        || right_function_table.size() != domain_point_count) {
        return false;
    }

    for (std::uint32_t x = 0; x < domain_point_count; ++x) {
        const std::uint32_t left_value = left_function_table[x];
        if (left_value >= codomain_point_count) {
            return false;
        }

        const std::uint32_t mapped_x = f2::apply(solution.domain_map, x);
        if (mapped_x >= right_function_table.size()) {
            return false;
        }

        const std::uint32_t right_value = right_function_table[mapped_x];
        if (right_value >= codomain_point_count) {
            return false;
        }

        if (f2::apply(solution.codomain_map, left_value) != right_value) {
            return false;
        }
    }

    return true;
}

struct SolutionKey {
    std::vector<std::uint32_t> words;

    [[nodiscard]] bool operator==(const SolutionKey& other) const
    {
        return words == other.words;
    }
};

struct SolutionKeyHash {
    [[nodiscard]] std::size_t operator()(const SolutionKey& key) const
    {
        std::size_t hash = 1469598103934665603ull;
        for (const std::uint32_t word : key.words) {
            hash ^= word;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

inline void append_map_key(std::vector<std::uint32_t>& words, const f2::AffineMap& map)
{
    words.push_back(map.dim);
    words.push_back(map.translation);
    words.insert(words.end(), map.basis_images.begin(), map.basis_images.end());
}

[[nodiscard]] inline SolutionKey make_solution_key(const Solution& solution)
{
    SolutionKey key;
    key.words.reserve(
        4u + solution.domain_map.basis_images.size()
        + solution.codomain_map.basis_images.size());
    append_map_key(key.words, solution.domain_map);
    append_map_key(key.words, solution.codomain_map);
    return key;
}

class SolutionStore {
public:
    struct Options {
        bool record_a1_automorphisms = false;
    };

    SolutionStore() = default;

    explicit SolutionStore(Options options)
        : options_(options)
    {
    }

    [[nodiscard]] bool publish(
        std::span<const std::uint32_t> left_function_table,
        std::span<const std::uint32_t> right_function_table,
        Solution solution,
        EquivalenceMode mode = EquivalenceMode::Affine)
    {
        profile::count(profile::counters.publish_calls);

        bool verified = false;
        {
            profile::ScopedTimer timer(profile::counters.verify_ns);
            verified = verify_solution(
                left_function_table,
                right_function_table,
                solution,
                mode);
        }
        if (!verified) {
            return false;
        }

        SolutionKey key;
        {
            profile::ScopedTimer timer(profile::counters.solution_key_ns);
            key = make_solution_key(solution);
        }
        const f2::AffineMap a1_map = solution.domain_map;
        const f2::AffineMap a2_map = solution.codomain_map;

        {
            profile::ScopedTimer timer(profile::counters.solution_lock_ns);
            std::lock_guard<std::mutex> lock(mutex_);
            if (known_.find(key) != known_.end()) {
                return false;
            }

            solutions_.push_back(std::move(solution));
            known_.insert(std::move(key));
        }

        if (options_.record_a1_automorphisms) {
            profile::ScopedTimer timer(profile::counters.solution_group_add_ns);
            (void)a1_group_.add_generator(a1_map);
            (void)a2_group_.add_generator(a2_map);
            (void)paired_group_.add_generator(a1_map, a2_map);
        }
        return true;
    }

    [[nodiscard]] group::AffineGroup& a1_group()
    {
        return a1_group_;
    }

    [[nodiscard]] const group::AffineGroup& a1_group() const
    {
        return a1_group_;
    }

    [[nodiscard]] group::AffineGroup& a2_group()
    {
        return a2_group_;
    }

    [[nodiscard]] const group::AffineGroup& a2_group() const
    {
        return a2_group_;
    }

    [[nodiscard]] group::PairedAffineGroup& paired_group()
    {
        return paired_group_;
    }

    [[nodiscard]] const group::PairedAffineGroup& paired_group() const
    {
        return paired_group_;
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return solutions_.size();
    }

    [[nodiscard]] bool empty() const
    {
        return size() == 0;
    }

private:
    Options options_;
    mutable std::mutex mutex_;
    std::vector<Solution> solutions_;
    std::unordered_set<SolutionKey, SolutionKeyHash> known_;
    group::AffineGroup a1_group_;
    group::AffineGroup a2_group_;
    group::PairedAffineGroup paired_group_;
};

} // namespace affine
