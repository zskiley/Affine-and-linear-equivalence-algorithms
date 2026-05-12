#include "../src/dfs.cpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint32_t> identity_table(std::uint32_t dim)
{
    const std::uint32_t count = affine::f2::point_count(dim);
    std::vector<std::uint32_t> table(count);
    for (std::uint32_t x = 0; x < count; ++x) {
        table[x] = x;
    }
    return table;
}

affine::f2::AffineMap swap_first_two_coordinates()
{
    return affine::f2::AffineMap {
        .dim = 3,
        .translation = 0,
        .basis_images = { 2, 1, 4 },
    };
}

void test_dfs_finds_a_leaf_and_rolls_back()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);

    const affine::DfsProblem problem {
        .domain_dim = dim,
        .codomain_dim = dim,
        .left_function_table = table,
        .right_function_table = table,
    };

    affine::SearchTask task = affine::make_search_task(problem, true);
    affine::SolutionStore solutions;
    const affine::SearchPartitionSnapshot root = affine::snapshot(task.partitions);

    affine::dfs(problem, task, &solutions);

    assert(task.stats.nodes > 0);
    assert(task.stats.leaves == 1);
    assert(solutions.size() == 1);
    assert(task.path.empty());
    assert(task.partitions.P.left.snapshot() == root.p_left);
    assert(task.partitions.P.right.snapshot() == root.p_right);
    assert(task.partitions.Q.left.snapshot() == root.q_left);
    assert(task.partitions.Q.right.snapshot() == root.q_right);
    assert(task.partitions.L.left.snapshot() == root.l_left);
    assert(task.partitions.L.right.snapshot() == root.l_right);
    assert(task.partitions.R.left.snapshot() == root.r_left);
    assert(task.partitions.R.right.snapshot() == root.r_right);
}

void test_branch_conflict_is_rejected()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);

    const affine::DfsProblem problem {
        .domain_dim = dim,
        .codomain_dim = dim,
        .left_function_table = table,
        .right_function_table = table,
    };

    affine::SearchTask task = affine::make_search_task(problem);
    const affine::BranchMove first {
        .kind = affine::BranchKind::DomainPoint,
        .left_object = 0,
        .right_object = 0,
    };
    const affine::BranchMove conflicting {
        .kind = affine::BranchKind::DomainPoint,
        .left_object = 1,
        .right_object = 0,
    };

    assert(affine::apply_branch_move(problem, task.partitions, first));
    assert(!affine::apply_branch_move(problem, task.partitions, conflicting));
}

void test_hyperplane_branches_individualize_hyperplane_partitions()
{
    constexpr std::uint32_t dim = 3;
    const std::vector<std::uint32_t> table = identity_table(dim);

    const affine::DfsProblem problem {
        .domain_dim = dim,
        .codomain_dim = dim,
        .left_function_table = table,
        .right_function_table = table,
    };

    affine::SearchTask task = affine::make_search_task(problem);
    const affine::BranchMove domain_hyperplane {
        .kind = affine::BranchKind::DomainHyperplane,
        .left_object = 1,
        .right_object = 2,
    };
    const affine::BranchMove codomain_hyperplane {
        .kind = affine::BranchKind::CodomainHyperplane,
        .left_object = 3,
        .right_object = 4,
    };

    assert(affine::apply_branch_move(problem, task.partitions, domain_hyperplane));
    assert(task.partitions.L.left.is_singleton(task.partitions.L.left.cell_of(1)));
    assert(task.partitions.L.right.is_singleton(task.partitions.L.right.cell_of(2)));

    assert(affine::apply_branch_move(problem, task.partitions, codomain_hyperplane));
    assert(task.partitions.R.left.is_singleton(task.partitions.R.left.cell_of(3)));
    assert(task.partitions.R.right.is_singleton(task.partitions.R.right.cell_of(4)));
}

void test_domain_group_filters_domain_branch_candidates()
{
    affine::group::AffineGroup group;
    assert(group.add_generator(swap_first_two_coordinates()));

    affine::DfsContext context {
        .domain_group = &group,
    };

    const std::vector<std::uint32_t> candidates { 1, 2, 4 };
    const std::vector<std::uint32_t> representatives =
        affine::domain_branch_right_candidates(
            &context,
            {},
            affine::BranchKind::DomainPoint,
            candidates);

    assert(representatives.size() == 2);
    assert(representatives[0] == 1);
    assert(representatives[1] == 4);

    const std::vector<affine::BranchMove> fixed_path {
        affine::BranchMove {
            .kind = affine::BranchKind::DomainPoint,
            .left_object = 0,
            .right_object = 1,
        },
    };
    const std::vector<std::uint32_t> restricted_representatives =
        affine::domain_branch_right_candidates(
            &context,
            fixed_path,
            affine::BranchKind::DomainPoint,
            candidates);

    assert(restricted_representatives == candidates);
}

void test_covered_queued_siblings_are_marked_dead()
{
    affine::PruningStore pruner;
    affine::DfsContext context {
        .pruner = &pruner,
    };

    const std::vector<affine::BranchMove> parent_path {
        affine::BranchMove {
            .kind = affine::BranchKind::DomainHyperplane,
            .left_object = 1,
            .right_object = 2,
        },
    };
    const std::vector<std::uint32_t> queued_right_objects { 10, 20, 30 };
    const std::vector<std::uint32_t> live_right_objects { 20 };

    affine::mark_covered_queued_siblings_dead(
        &context,
        parent_path,
        affine::BranchKind::DomainPoint,
        7,
        queued_right_objects,
        live_right_objects);

    const std::vector<affine::BranchMove> dead_first {
        parent_path[0],
        affine::BranchMove {
            .kind = affine::BranchKind::DomainPoint,
            .left_object = 7,
            .right_object = 10,
        },
    };
    const std::vector<affine::BranchMove> live_middle {
        parent_path[0],
        affine::BranchMove {
            .kind = affine::BranchKind::DomainPoint,
            .left_object = 7,
            .right_object = 20,
        },
    };
    const std::vector<affine::BranchMove> dead_last {
        parent_path[0],
        affine::BranchMove {
            .kind = affine::BranchKind::DomainPoint,
            .left_object = 7,
            .right_object = 30,
        },
    };

    assert(pruner.version() == 2);
    assert(pruner.is_dead(dead_first));
    assert(!pruner.is_dead(live_middle));
    assert(pruner.is_dead(dead_last));
}

} // namespace

int main()
{
    test_dfs_finds_a_leaf_and_rolls_back();
    test_branch_conflict_is_rejected();
    test_hyperplane_branches_individualize_hyperplane_partitions();
    test_domain_group_filters_domain_branch_candidates();
    test_covered_queued_siblings_are_marked_dead();

    std::cout << "dfs skeleton tests passed\n";
    return 0;
}
