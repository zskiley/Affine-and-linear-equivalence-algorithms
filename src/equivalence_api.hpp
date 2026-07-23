#pragma once

#include "group/paired_affine_group.hpp"
#include "solution_store.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

namespace affine::api {

// Selects affine equivalence by default. Use Linear to require both maps to
// have zero translation.
struct SearchOptions {
    EquivalenceMode mode = EquivalenceMode::Affine;
};

// Produces equivalences one at a time without first storing the complete list.
// An equivalence consists of a domain map A and a codomain map B satisfying
//
//     B(left_function_table[x]) = right_function_table[A(x)].
//
// The group search is completed before this object is returned. Calling next()
// then expands the resulting paired generators only as far as needed.
class EquivalenceGenerator {
public:
    EquivalenceGenerator() = default;

    // For a successful equivalence or self-equivalence search, returns paired
    // generators of the target function's complete self-equivalence group.
    // The set is not guaranteed to be minimal. A failed equivalence search
    // returns an empty span. The span remains valid through calls to next(),
    // but not after this object is moved, assigned, or destroyed.
    [[nodiscard]] std::span<const group::PairedAffineMap>
    paired_group_generators() const & noexcept;

    // Returns the identity for a self-equivalence search, one witness for an
    // equivalence search, or nullptr if the functions are not equivalent.
    // Together with paired_group_generators(), the witness compactly
    // represents every equivalence. The pointer has the same lifetime rules as
    // the span returned by paired_group_generators().
    [[nodiscard]] const group::PairedAffineMap* witness() const & noexcept;

    // Returns the next equivalence. Returns std::nullopt after every
    // equivalence has been produced; later calls also return std::nullopt.
    [[nodiscard]] std::optional<Solution> next();

private:
    EquivalenceGenerator(
        std::uint32_t domain_dimension,
        std::uint32_t codomain_dimension,
        std::vector<group::PairedAffineMap> generators,
        group::PairedAffineMap representative);

    std::vector<group::PairedAffineMap> generators_;
    std::deque<group::PairedAffineMap> pending_;
    std::unordered_set<
        group::PairedAffineGroupKey,
        group::PairedAffineGroupKeyHash> seen_;
    std::optional<group::PairedAffineMap> representative_;

    friend EquivalenceGenerator generate_self_equivalences(
        std::uint32_t domain_dimension,
        std::uint32_t codomain_dimension,
        std::span<const std::uint32_t> function_table,
        SearchOptions options);
    friend EquivalenceGenerator generate_equivalences(
        std::uint32_t domain_dimension,
        std::uint32_t codomain_dimension,
        std::span<const std::uint32_t> left_function_table,
        std::span<const std::uint32_t> right_function_table,
        SearchOptions options);
};

// Convenience overload for functions from F_2^n to F_2^n. The dimension n is
// inferred from the length of function_table.
[[nodiscard]] EquivalenceGenerator generate_self_equivalences(
    std::span<const std::uint32_t> function_table,
    SearchOptions options = {});

// Finds the paired self-equivalence group of a function from F_2^n to F_2^m
// and returns a generator that produces every group element, including the
// identity. The table must contain 2^n values in the range [0, 2^m).
[[nodiscard]] EquivalenceGenerator generate_self_equivalences(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> function_table,
    SearchOptions options = {});

// Convenience overload for two functions from F_2^n to F_2^n. The dimension n
// is inferred from the table length.
[[nodiscard]] EquivalenceGenerator generate_equivalences(
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    SearchOptions options = {});

// Finds one equivalence between two functions from F_2^n to F_2^m and the
// self-equivalence group of the right function. The returned generator
// produces every equivalence by combining that witness with the group. If the
// functions are not equivalent, its first call to next() returns std::nullopt.
[[nodiscard]] EquivalenceGenerator generate_equivalences(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    SearchOptions options = {});

} // namespace affine::api
