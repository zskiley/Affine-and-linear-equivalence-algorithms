#include "../src/equivalence_api.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
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

} // namespace

int main()
{
    test_affine_but_not_linear();
    test_linear_search_is_parallel_and_fixes_zero();
    std::cout << "equivalence API tests passed\n";
    return 0;
}
