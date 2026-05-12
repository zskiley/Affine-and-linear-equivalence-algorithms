#include "../src/refinement.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint32_t> make_function_table(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim)
{
    const std::uint32_t domain_points = affine::f2::point_count(domain_dim);
    const std::uint32_t codomain_mask = affine::f2::point_count(codomain_dim) - 1u;

    std::vector<std::uint32_t> table(domain_points);
    for (std::uint32_t x = 0; x < domain_points; ++x) {
        table[x] = ((x * 13u) ^ (x >> 1u) ^ (x << 3u) ^ 7u) & codomain_mask;
    }
    return table;
}

std::vector<std::uint32_t> cell_sizes(const affine::Partition& partition)
{
    std::vector<std::uint32_t> sizes;
    for (affine::Partition::CellId cell = partition.first_cell();
         cell != affine::Partition::npos;
         cell = partition.next_cell(cell)) {
        sizes.push_back(partition.cell(cell).size());
    }
    return sizes;
}

void assert_initial_partition_shapes(
    const affine::SearchPartitions& partitions,
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim)
{
    assert(cell_sizes(partitions.P.left) ==
        std::vector<std::uint32_t> { affine::f2::point_count(domain_dim) });
    assert(cell_sizes(partitions.P.right) ==
        std::vector<std::uint32_t> { affine::f2::point_count(domain_dim) });
    assert(cell_sizes(partitions.Q.left) ==
        std::vector<std::uint32_t> { affine::f2::point_count(codomain_dim) });
    assert(cell_sizes(partitions.Q.right) ==
        std::vector<std::uint32_t> { affine::f2::point_count(codomain_dim) });
    assert(cell_sizes(partitions.L.left) ==
        std::vector<std::uint32_t> { affine::f2::hyperplane_count(domain_dim) });
    assert(cell_sizes(partitions.L.right) ==
        std::vector<std::uint32_t> { affine::f2::hyperplane_count(domain_dim) });
    assert(cell_sizes(partitions.R.left) ==
        std::vector<std::uint32_t> { affine::f2::hyperplane_count(codomain_dim) });
    assert(cell_sizes(partitions.R.right) ==
        std::vector<std::uint32_t> { affine::f2::hyperplane_count(codomain_dim) });
}

void assert_all_invariants(const affine::SearchPartitions& partitions)
{
    assert(partitions.P.left.check_invariants());
    assert(partitions.P.right.check_invariants());
    assert(partitions.Q.left.check_invariants());
    assert(partitions.Q.right.check_invariants());
    assert(partitions.L.left.check_invariants());
    assert(partitions.L.right.check_invariants());
    assert(partitions.R.left.check_invariants());
    assert(partitions.R.right.check_invariants());
}

void test_pair_refinement_inconsistent_rolls_back()
{
    affine::PartitionPair pair(4);
    affine::PairRefinementScratch scratch;

    const auto left_snapshot = pair.left.snapshot();
    const auto right_snapshot = pair.right.snapshot();

    const std::vector<affine::Partition::Signature> left_signatures = { 0, 0, 1, 1 };
    const std::vector<affine::Partition::Signature> right_signatures = { 0, 1, 1, 1 };

    const affine::RefineStep status = affine::refine_pair_by_signature(
        pair,
        left_signatures,
        right_signatures,
        scratch);

    assert(status == affine::RefineStep::Inconsistent);
    assert(pair.left.snapshot() == left_snapshot);
    assert(pair.right.snapshot() == right_snapshot);
    assert(pair.left.check_invariants());
    assert(pair.right.check_invariants());
    assert(cell_sizes(pair.left) == std::vector<std::uint32_t> { 4 });
    assert(cell_sizes(pair.right) == std::vector<std::uint32_t> { 4 });
}

void test_search_partitions_full_rollback_after_refinement()
{
    constexpr std::uint32_t domain_dim = 6;
    constexpr std::uint32_t codomain_dim = 6;

    affine::SearchPartitions partitions =
        affine::make_initial_partitions(domain_dim, codomain_dim);
    const affine::SearchPartitionSnapshot root = affine::snapshot(partitions);
    affine::RefinementWorkspace workspace;
    const std::vector<std::uint32_t> function_table =
        make_function_table(domain_dim, codomain_dim);

    for (const std::uint32_t object : { 0u, 1u, 2u }) {
        partitions.P.left.individualize(object);
        partitions.P.right.individualize(object);
    }

    const affine::RefineResult result = affine::refine_until_stable(
        domain_dim,
        codomain_dim,
        function_table,
        function_table,
        partitions,
        workspace);

    assert(result == affine::RefineResult::Continue);
    assert_all_invariants(partitions);
    assert(cell_sizes(partitions.P.left)
        != std::vector<std::uint32_t> { affine::f2::point_count(domain_dim) });

    affine::rollback(partitions, root);
    assert_all_invariants(partitions);
    assert_initial_partition_shapes(partitions, domain_dim, codomain_dim);
}

void test_search_partitions_full_rollback_after_inconsistent_branch()
{
    constexpr std::uint32_t domain_dim = 5;
    constexpr std::uint32_t codomain_dim = 5;

    affine::SearchPartitions partitions =
        affine::make_initial_partitions(domain_dim, codomain_dim);
    const affine::SearchPartitionSnapshot root = affine::snapshot(partitions);
    affine::RefinementWorkspace workspace;
    const std::vector<std::uint32_t> function_table =
        make_function_table(domain_dim, codomain_dim);

    partitions.P.left.individualize(0);

    const affine::RefineResult result = affine::refine_until_stable(
        domain_dim,
        codomain_dim,
        function_table,
        function_table,
        partitions,
        workspace);

    assert(result == affine::RefineResult::Stop);
    assert_all_invariants(partitions);
    const std::vector<std::uint32_t> expected_left_p_sizes { 1, 31 };
    assert(cell_sizes(partitions.P.left) == expected_left_p_sizes);

    affine::rollback(partitions, root);
    assert_all_invariants(partitions);
    assert_initial_partition_shapes(partitions, domain_dim, codomain_dim);
}

void test_refine_until_stable_matching_functions()
{
    constexpr std::uint32_t domain_dim = 5;
    constexpr std::uint32_t codomain_dim = 4;

    affine::SearchPartitions partitions =
        affine::make_initial_partitions(domain_dim, codomain_dim);
    affine::RefinementWorkspace workspace;
    const std::vector<std::uint32_t> function_table =
        make_function_table(domain_dim, codomain_dim);

    const affine::RefineResult result = affine::refine_until_stable(
        domain_dim,
        codomain_dim,
        function_table,
        function_table,
        partitions,
        workspace);

    assert(result == affine::RefineResult::Continue);
    assert_all_invariants(partitions);
    assert(affine::same_shape(partitions.P.left, partitions.P.right));
    assert(affine::same_shape(partitions.Q.left, partitions.Q.right));
    assert(affine::same_shape(partitions.L.left, partitions.L.right));
    assert(affine::same_shape(partitions.R.left, partitions.R.right));
}

void test_determined_domain_map_is_individualized()
{
    constexpr std::uint32_t domain_dim = 3;
    constexpr std::uint32_t codomain_dim = 3;

    affine::SearchPartitions partitions =
        affine::make_initial_partitions(domain_dim, codomain_dim);
    affine::RefinementWorkspace workspace;
    const std::vector<std::uint32_t> function_table =
        make_function_table(domain_dim, codomain_dim);

    for (const std::uint32_t object : { 0u, 1u, 2u, 4u }) {
        partitions.P.left.individualize(object);
        partitions.P.right.individualize(object);
    }

    const affine::RefineResult result = affine::refine_until_stable(
        domain_dim,
        codomain_dim,
        function_table,
        function_table,
        partitions,
        workspace);

    assert(
        result == affine::RefineResult::Continue
        || result == affine::RefineResult::Stop);
    assert_all_invariants(partitions);
    assert(cell_sizes(partitions.P.left) == std::vector<std::uint32_t>(
        affine::f2::point_count(domain_dim),
        1));
    assert(cell_sizes(partitions.P.right) == std::vector<std::uint32_t>(
        affine::f2::point_count(domain_dim),
        1));
}

void test_refinement_publishes_determined_solution()
{
    constexpr std::uint32_t dim = 3;

    affine::SearchPartitions partitions =
        affine::make_initial_partitions(dim, dim);
    affine::RefinementWorkspace workspace;
    affine::SolutionStore solutions;
    const std::vector<std::uint32_t> function_table =
        make_function_table(dim, dim);

    for (const std::uint32_t object : { 0u, 1u, 2u, 4u }) {
        partitions.P.left.individualize(object);
        partitions.P.right.individualize(object);
        partitions.Q.left.individualize(object);
        partitions.Q.right.individualize(object);
    }

    const affine::RefineResult result = affine::refine_until_stable(
        dim,
        dim,
        function_table,
        function_table,
        partitions,
        workspace,
        &solutions);

    assert(result == affine::RefineResult::Stop);
    assert(solutions.size() == 1);
    assert(affine::verify_solution(
        function_table,
        function_table,
        solutions.snapshot().front()));
}

} // namespace

int main()
{
    test_pair_refinement_inconsistent_rolls_back();
    test_search_partitions_full_rollback_after_refinement();
    test_search_partitions_full_rollback_after_inconsistent_branch();
    test_refine_until_stable_matching_functions();
    test_determined_domain_map_is_individualized();
    test_refinement_publishes_determined_solution();

    std::cout << "refinement tests passed\n";
    return 0;
}
