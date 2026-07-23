#include "equivalence_api.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

void check(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

affine::Solution as_solution(const affine::group::PairedAffineMap& map)
{
    return affine::Solution {
        .domain_map = map.domain_map,
        .codomain_map = map.codomain_map,
        .path = {},
    };
}

void check_compact_representation(
    const affine::api::EquivalenceGenerator& generator,
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right,
    affine::EquivalenceMode mode)
{
    const affine::group::PairedAffineMap* witness = generator.witness();
    check(witness != nullptr, "equivalent functions should have a witness");
    check(
        affine::verify_solution(left, right, as_solution(*witness), mode),
        "compact witness is invalid");

    for (const affine::group::PairedAffineMap& group_generator :
         generator.paired_group_generators()) {
        check(
            affine::verify_solution(
                right,
                right,
                as_solution(group_generator),
                mode),
            "compact group generator is invalid");
    }
}

std::size_t check_all(
    affine::api::EquivalenceGenerator generator,
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right,
    affine::EquivalenceMode mode)
{
    std::unordered_set<
        affine::SolutionKey,
        affine::SolutionKeyHash> seen;
    std::size_t count = 0;
    while (std::optional<affine::Solution> solution = generator.next()) {
        check(
            affine::verify_solution(left, right, *solution, mode),
            "generator returned an invalid equivalence");
        check(
            seen.insert(affine::make_solution_key(*solution)).second,
            "generator returned a duplicate equivalence");
        ++count;
    }
    check(!generator.next().has_value(), "exhausted generator produced a value");
    return count;
}

} // namespace

int main()
{
    const std::vector<std::uint32_t> identity { 0, 1, 2, 3 };
    const std::vector<std::uint32_t> constant_zero { 0, 0, 0, 0 };
    const std::vector<std::uint32_t> translated_identity { 1, 0, 3, 2 };
    const std::vector<std::uint32_t> embedding_1_to_2 { 0, 1 };
    const std::vector<std::uint32_t> translated_embedding_1_to_2 { 1, 0 };
    const std::vector<std::uint32_t> projection_2_to_1 { 0, 1, 0, 1 };

    auto identity_self =
        affine::api::generate_self_equivalences(identity);
    check_compact_representation(
        identity_self,
        identity,
        identity,
        affine::EquivalenceMode::Affine);
    check(
        affine::group::is_identity_paired_affine_map(
            *identity_self.witness()),
        "self-equivalence witness should be the identity");
    const std::size_t identity_generator_count =
        identity_self.paired_group_generators().size();
    const affine::group::PairedAffineMap* identity_generator_data =
        identity_self.paired_group_generators().data();
    const affine::group::PairedAffineMap* identity_witness =
        identity_self.witness();
    check(
        check_all(
            identity_self,
            identity,
            identity,
            affine::EquivalenceMode::Affine)
            == 24,
        "two-bit identity should have 24 affine self-equivalences");
    while (identity_self.next().has_value()) {
    }
    check(
        identity_self.paired_group_generators().size()
            == identity_generator_count,
        "iteration changed the compact generator set");
    check(
        identity_self.paired_group_generators().data()
            == identity_generator_data,
        "iteration invalidated the compact generator span");
    check(
        identity_self.witness() == identity_witness,
        "iteration invalidated the compact witness");

    check(
        check_all(
            affine::api::generate_self_equivalences(
                identity,
                { .mode = affine::EquivalenceMode::Linear }),
            identity,
            identity,
            affine::EquivalenceMode::Linear)
            == 6,
        "two-bit identity should have 6 linear self-equivalences");

    check(
        check_all(
            affine::api::generate_self_equivalences(constant_zero),
            constant_zero,
            constant_zero,
            affine::EquivalenceMode::Affine)
            == 144,
        "two-bit zero function should have 144 affine self-equivalences");

    auto translated_equivalences =
        affine::api::generate_equivalences(identity, translated_identity);
    check_compact_representation(
        translated_equivalences,
        identity,
        translated_identity,
        affine::EquivalenceMode::Affine);
    check(
        check_all(
            translated_equivalences,
            identity,
            translated_identity,
            affine::EquivalenceMode::Affine)
            == 24,
        "translated identity should have 24 affine equivalences");

    auto no_linear_equivalence = affine::api::generate_equivalences(
        identity,
        translated_identity,
        { .mode = affine::EquivalenceMode::Linear });
    check(
        no_linear_equivalence.witness() == nullptr,
        "non-equivalent functions should not have a witness");
    check(
        no_linear_equivalence.paired_group_generators().empty(),
        "non-equivalent functions should not expose group generators");
    check(
        check_all(
            no_linear_equivalence,
            identity,
            translated_identity,
            affine::EquivalenceMode::Linear)
            == 0,
        "translated identity should have no linear equivalence");

    check(
        check_all(
            affine::api::generate_self_equivalences(
                1,
                2,
                embedding_1_to_2),
            embedding_1_to_2,
            embedding_1_to_2,
            affine::EquivalenceMode::Affine)
            == 4,
        "1-to-2 embedding should have 4 affine self-equivalences");

    check(
        check_all(
            affine::api::generate_equivalences(
                1,
                2,
                embedding_1_to_2,
                translated_embedding_1_to_2),
            embedding_1_to_2,
            translated_embedding_1_to_2,
            affine::EquivalenceMode::Affine)
            == 4,
        "1-to-2 translated embedding should have 4 affine equivalences");

    check(
        check_all(
            affine::api::generate_self_equivalences(
                1,
                2,
                embedding_1_to_2,
                { .mode = affine::EquivalenceMode::Linear }),
            embedding_1_to_2,
            embedding_1_to_2,
            affine::EquivalenceMode::Linear)
            == 2,
        "1-to-2 embedding should have 2 linear self-equivalences");

    check(
        check_all(
            affine::api::generate_self_equivalences(
                2,
                1,
                projection_2_to_1),
            projection_2_to_1,
            projection_2_to_1,
            affine::EquivalenceMode::Affine)
            == 8,
        "2-to-1 projection should have 8 affine self-equivalences");
}
