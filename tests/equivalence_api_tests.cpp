#include "../src/equivalence_api.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <span>
#include <unordered_set>
#include <vector>

namespace {

std::vector<affine::Solution> search(
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right,
    affine::api::EquivalenceKind kind,
    std::uint32_t threads)
{
    return affine::api::find_equivalences(
        affine::api::EquivalenceProblem {
            .domain_dimension = 2,
            .codomain_dimension = 2,
            .left_table = left,
            .right_table = right,
        },
        affine::api::SearchOptions {
            .threads = threads,
            .kind = kind,
        });
}

std::vector<std::vector<std::uint32_t>> solution_keys(
    const std::vector<affine::Solution>& solutions)
{
    std::vector<std::vector<std::uint32_t>> keys;
    for (const affine::Solution& solution : solutions) {
        keys.push_back(affine::make_solution_key(solution).words);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

std::size_t generated_order(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const affine::Solution> generators)
{
    affine::Solution identity {
        .domain_map = affine::group::identity_affine_map(domain_dimension),
        .codomain_map = affine::group::identity_affine_map(codomain_dimension),
    };
    std::vector<affine::Solution> closure { std::move(identity) };
    std::unordered_set<affine::SolutionKey, affine::SolutionKeyHash> known;
    known.insert(affine::make_solution_key(closure.front()));

    for (std::size_t cursor = 0; cursor < closure.size(); ++cursor) {
        for (const affine::Solution& generator : generators) {
            affine::Solution product {
                .domain_map = affine::group::compose_affine_maps(
                    generator.domain_map, closure[cursor].domain_map),
                .codomain_map = affine::group::compose_affine_maps(
                    generator.codomain_map, closure[cursor].codomain_map),
            };
            affine::SolutionKey key = affine::make_solution_key(product);
            if (known.insert(std::move(key)).second) {
                closure.push_back(std::move(product));
            }
        }
    }
    return closure.size();
}

affine::api::SelfEquivalenceGroup self_group(
    const std::vector<std::uint32_t>& table,
    affine::api::EquivalenceKind kind,
    std::uint32_t threads)
{
    return affine::api::find_self_equivalence_group(
        affine::api::EquivalenceProblem {
            .domain_dimension = 2,
            .codomain_dimension = 2,
            .left_table = table,
            .right_table = table,
        },
        affine::api::SearchOptions {
            .threads = threads,
            .kind = kind,
        });
}

void test_affine_but_not_linear()
{
    const std::vector<std::uint32_t> identity { 0, 1, 2, 3 };
    const std::vector<std::uint32_t> translated { 1, 0, 3, 2 };

    assert(!search(
        identity,
        translated,
        affine::api::EquivalenceKind::Affine,
        2).empty());
    assert(search(
        identity,
        translated,
        affine::api::EquivalenceKind::Linear,
        2).empty());
}

void test_linear_search_is_parallel_and_fixes_zero()
{
    const std::vector<std::uint32_t> identity { 0, 1, 2, 3 };
    const std::vector<affine::Solution> single = search(
        identity,
        identity,
        affine::api::EquivalenceKind::Linear,
        1);
    const std::vector<affine::Solution> parallel = search(
        identity,
        identity,
        affine::api::EquivalenceKind::Linear,
        4);

    assert(!single.empty());
    assert(solution_keys(single) == solution_keys(parallel));
    for (const affine::Solution& solution : parallel) {
        assert(solution.domain_map.translation == 0);
        assert(solution.codomain_map.translation == 0);
    }
}

void test_self_equivalence_group_generators()
{
    const std::vector<std::uint32_t> identity { 0, 1, 2, 3 };
    const affine::api::SelfEquivalenceGroup affine_single = self_group(
        identity, affine::api::EquivalenceKind::Affine, 1);
    const affine::api::SelfEquivalenceGroup affine_parallel = self_group(
        identity, affine::api::EquivalenceKind::Affine, 4);

    assert(affine_single.order == 24);
    assert(solution_keys(affine_single.generators)
        == solution_keys(affine_parallel.generators));
    assert(affine_single.generators.size() < affine_single.order);
    assert(generated_order(2, 2, affine_single.generators) == affine_single.order);

    const affine::api::SelfEquivalenceGroup linear = self_group(
        identity, affine::api::EquivalenceKind::Linear, 2);
    assert(linear.order == 6);
    assert(generated_order(2, 2, linear.generators) == linear.order);

    // The full pair group matters: for a constant function, the domain and
    // codomain components vary independently.
    const std::vector<std::uint32_t> constant { 0, 0, 0, 0 };
    const affine::api::SelfEquivalenceGroup constant_linear = self_group(
        constant, affine::api::EquivalenceKind::Linear, 2);
    assert(constant_linear.order == 36);
    assert(generated_order(2, 2, constant_linear.generators)
        == constant_linear.order);

}

} // namespace

int main()
{
    test_affine_but_not_linear();
    test_linear_search_is_parallel_and_fixes_zero();
    test_self_equivalence_group_generators();
    std::cout << "equivalence API tests passed\n";
    return 0;
}
