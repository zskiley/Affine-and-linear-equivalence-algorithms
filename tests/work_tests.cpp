#include "../src/work.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

affine::BranchMove move(
    affine::BranchKind kind,
    std::uint32_t left,
    std::uint32_t right)
{
    return affine::BranchMove {
        .kind = kind,
        .left_object = left,
        .right_object = right,
    };
}

void test_branch_move_equality()
{
    const affine::BranchMove first =
        move(affine::BranchKind::DomainHyperplane, 12, 34);
    const affine::BranchMove same =
        move(affine::BranchKind::DomainHyperplane, 12, 34);
    const affine::BranchMove different_kind =
        move(affine::BranchKind::CodomainHyperplane, 12, 34);
    const affine::BranchMove different_object =
        move(affine::BranchKind::DomainHyperplane, 12, 35);

    assert(first == same);
    assert(first != different_kind);
    assert(first != different_object);
}

void test_path_prefix()
{
    const std::vector<affine::BranchMove> path {
        move(affine::BranchKind::DomainHyperplane, 1, 2),
        move(affine::BranchKind::CodomainHyperplane, 3, 4),
        move(affine::BranchKind::DomainPoint, 5, 6),
    };
    const std::vector<affine::BranchMove> prefix {
        move(affine::BranchKind::DomainHyperplane, 1, 2),
        move(affine::BranchKind::CodomainHyperplane, 3, 4),
    };
    const std::vector<affine::BranchMove> sibling {
        move(affine::BranchKind::DomainHyperplane, 1, 9),
    };
    const std::vector<affine::BranchMove> too_long {
        move(affine::BranchKind::DomainHyperplane, 1, 2),
        move(affine::BranchKind::CodomainHyperplane, 3, 4),
        move(affine::BranchKind::DomainPoint, 5, 6),
        move(affine::BranchKind::CodomainPoint, 7, 8),
    };

    assert(affine::path_has_prefix(path, prefix));
    assert(!affine::path_has_prefix(path, sibling));
    assert(!affine::path_has_prefix(path, too_long));
}

void test_pruning_store()
{
    affine::PruningStore store;

    const std::vector<affine::BranchMove> dead_parent {
        move(affine::BranchKind::DomainHyperplane, 10, 20),
        move(affine::BranchKind::DomainHyperplane, 11, 21),
    };
    const std::vector<affine::BranchMove> killed_child {
        move(affine::BranchKind::DomainHyperplane, 10, 20),
        move(affine::BranchKind::DomainHyperplane, 11, 21),
        move(affine::BranchKind::CodomainHyperplane, 12, 22),
    };
    const std::vector<affine::BranchMove> live_sibling {
        move(affine::BranchKind::DomainHyperplane, 10, 20),
        move(affine::BranchKind::DomainHyperplane, 11, 99),
    };

    assert(store.version() == 0);
    assert(!store.is_dead(dead_parent));
    assert(!store.is_dead(killed_child));

    store.add_dead_prefix(dead_parent);

    assert(store.version() == 1);
    assert(store.is_dead(dead_parent));
    assert(store.is_dead(killed_child));
    assert(!store.is_dead(live_sibling));
}

} // namespace

int main()
{
    test_branch_move_equality();
    test_path_prefix();
    test_pruning_store();

    std::cout << "work tests passed\n";
    return 0;
}
