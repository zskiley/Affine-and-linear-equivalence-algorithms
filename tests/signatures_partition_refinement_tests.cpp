#include "../src/signatures_f2.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

affine::Partition make_ordered_partition(std::uint32_t object_count, std::uint32_t class_count)
{
    affine::Partition partition(object_count);
    affine::Partition::RefinementScratch scratch;
    std::vector<affine::Partition::Signature> seed_signatures(object_count);

    for (std::uint32_t object = 0; object < object_count; ++object) {
        seed_signatures[object] = (object * 2'654'435'761u + 97u) % class_count;
    }

    partition.refine_cell_by_signature(partition.first_cell(), seed_signatures, scratch);
    return partition;
}

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

bool same_shape(const affine::Partition& left, const affine::Partition& right)
{
    return cell_sizes(left) == cell_sizes(right);
}

bool refine_partition_all_cells(
    affine::Partition& partition,
    std::span<const affine::Partition::Signature> signatures)
{
    bool changed = false;
    affine::Partition::RefinementScratch scratch;

    for (affine::Partition::CellId cell = partition.first_cell();
         cell != affine::Partition::npos;) {
        const affine::Partition::CellId next = partition.next_cell(cell);
        changed |= partition.refine_cell_by_signature(cell, signatures, scratch);
        cell = next;
    }

    return changed;
}

bool refine_pair_all_cells(
    affine::Partition& left,
    affine::Partition& right,
    std::span<const affine::Partition::Signature> left_signatures,
    std::span<const affine::Partition::Signature> right_signatures)
{
    const bool left_changed = refine_partition_all_cells(left, left_signatures);
    const bool right_changed = refine_partition_all_cells(right, right_signatures);

    assert(left.check_invariants());
    assert(right.check_invariants());
    assert(same_shape(left, right));

    return left_changed || right_changed;
}

void test_signatures_refine_ordered_partitions()
{
    constexpr std::uint32_t domain_dim = 6;
    constexpr std::uint32_t codomain_dim = 5;

    const std::uint32_t domain_points = affine::f2::point_count(domain_dim);
    const std::uint32_t codomain_points = affine::f2::point_count(codomain_dim);
    const std::uint32_t domain_hyperplanes = affine::f2::hyperplane_count(domain_dim);
    const std::uint32_t codomain_hyperplanes = affine::f2::hyperplane_count(codomain_dim);

    affine::Partition PF = make_ordered_partition(domain_points, 5);
    affine::Partition PG = make_ordered_partition(domain_points, 5);
    affine::Partition QF = make_ordered_partition(codomain_points, 4);
    affine::Partition QG = make_ordered_partition(codomain_points, 4);
    affine::Partition LF = make_ordered_partition(domain_hyperplanes, 7);
    affine::Partition LG = make_ordered_partition(domain_hyperplanes, 7);
    affine::Partition RF = make_ordered_partition(codomain_hyperplanes, 6);
    affine::Partition RG = make_ordered_partition(codomain_hyperplanes, 6);

    const auto pf_snapshot = PF.snapshot();
    const auto pg_snapshot = PG.snapshot();
    const auto qf_snapshot = QF.snapshot();
    const auto qg_snapshot = QG.snapshot();
    const auto lf_snapshot = LF.snapshot();
    const auto lg_snapshot = LG.snapshot();
    const auto rf_snapshot = RF.snapshot();
    const auto rg_snapshot = RG.snapshot();

    const std::vector<std::uint32_t> function_table =
        make_function_table(domain_dim, codomain_dim);

    affine::f2::FunctionSignatures sigF;
    affine::f2::FunctionSignatures sigG;
    affine::f2::SignatureWorkspace workspaceF;
    affine::f2::SignatureWorkspace workspaceG;

    affine::f2::compute_function_signatures(
        domain_dim,
        codomain_dim,
        function_table,
        PF,
        QF,
        LF,
        RF,
        sigF,
        workspaceF);

    affine::f2::compute_function_signatures(
        domain_dim,
        codomain_dim,
        function_table,
        PG,
        QG,
        LG,
        RG,
        sigG,
        workspaceG);

    bool changed = false;
    changed |= refine_pair_all_cells(PF, PG, sigF.domain_points, sigG.domain_points);
    changed |= refine_pair_all_cells(QF, QG, sigF.codomain_points, sigG.codomain_points);
    changed |= refine_pair_all_cells(LF, LG, sigF.domain_hyperplanes, sigG.domain_hyperplanes);
    changed |= refine_pair_all_cells(RF, RG, sigF.codomain_hyperplanes, sigG.codomain_hyperplanes);

    assert(changed);
    assert(PF.check_invariants());
    assert(PG.check_invariants());
    assert(QF.check_invariants());
    assert(QG.check_invariants());
    assert(LF.check_invariants());
    assert(LG.check_invariants());
    assert(RF.check_invariants());
    assert(RG.check_invariants());
    assert(same_shape(PF, PG));
    assert(same_shape(QF, QG));
    assert(same_shape(LF, LG));
    assert(same_shape(RF, RG));

    PF.rollback(pf_snapshot);
    PG.rollback(pg_snapshot);
    QF.rollback(qf_snapshot);
    QG.rollback(qg_snapshot);
    LF.rollback(lf_snapshot);
    LG.rollback(lg_snapshot);
    RF.rollback(rf_snapshot);
    RG.rollback(rg_snapshot);

    assert(same_shape(PF, PG));
    assert(same_shape(QF, QG));
    assert(same_shape(LF, LG));
    assert(same_shape(RF, RG));
}

} // namespace

int main()
{
    test_signatures_refine_ordered_partitions();
    std::cout << "signature partition refinement tests passed\n";
    return 0;
}
