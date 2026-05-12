#include "../src/signatures_f2.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
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

bool refine_partition_all_cells(
    affine::Partition& partition,
    std::span<const affine::Partition::Signature> signatures,
    affine::Partition::RefinementScratch& scratch)
{
    bool changed = false;

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
    std::span<const affine::Partition::Signature> right_signatures,
    affine::Partition::RefinementScratch& left_scratch,
    affine::Partition::RefinementScratch& right_scratch)
{
    const bool left_changed = refine_partition_all_cells(left, left_signatures, left_scratch);
    const bool right_changed = refine_partition_all_cells(right, right_signatures, right_scratch);
    return left_changed || right_changed;
}

std::uint64_t checksum_partition(const affine::Partition& partition)
{
    std::uint64_t checksum = 0;
    std::uint64_t cell_index = 1;
    for (affine::Partition::CellId cell = partition.first_cell();
         cell != affine::Partition::npos;
         cell = partition.next_cell(cell)) {
        checksum += cell_index * partition.cell(cell).size();
        ++cell_index;
    }
    return checksum;
}

void benchmark_dims(std::uint32_t domain_dim, std::uint32_t codomain_dim, int iterations)
{
    affine::Partition PF = make_ordered_partition(affine::f2::point_count(domain_dim), 17);
    affine::Partition PG = make_ordered_partition(affine::f2::point_count(domain_dim), 17);
    affine::Partition QF = make_ordered_partition(affine::f2::point_count(codomain_dim), 19);
    affine::Partition QG = make_ordered_partition(affine::f2::point_count(codomain_dim), 19);
    affine::Partition LF = make_ordered_partition(affine::f2::hyperplane_count(domain_dim), 23);
    affine::Partition LG = make_ordered_partition(affine::f2::hyperplane_count(domain_dim), 23);
    affine::Partition RF = make_ordered_partition(affine::f2::hyperplane_count(codomain_dim), 29);
    affine::Partition RG = make_ordered_partition(affine::f2::hyperplane_count(codomain_dim), 29);

    const auto pf_snapshot = PF.snapshot();
    const auto pg_snapshot = PG.snapshot();
    const auto qf_snapshot = QF.snapshot();
    const auto qg_snapshot = QG.snapshot();
    const auto lf_snapshot = LF.snapshot();
    const auto lg_snapshot = LG.snapshot();
    const auto rf_snapshot = RF.snapshot();
    const auto rg_snapshot = RG.snapshot();

    std::vector<std::uint32_t> function_table = make_function_table(domain_dim, codomain_dim);

    affine::f2::FunctionSignatures sigF;
    affine::f2::FunctionSignatures sigG;
    affine::f2::SignatureWorkspace workspaceF;
    affine::f2::SignatureWorkspace workspaceG;
    affine::Partition::RefinementScratch scratch_left;
    affine::Partition::RefinementScratch scratch_right;

    std::uint64_t total_checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i) {
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

        refine_pair_all_cells(PF, PG, sigF.domain_points, sigG.domain_points, scratch_left, scratch_right);
        refine_pair_all_cells(QF, QG, sigF.codomain_points, sigG.codomain_points, scratch_left, scratch_right);
        refine_pair_all_cells(LF, LG, sigF.domain_hyperplanes, sigG.domain_hyperplanes, scratch_left, scratch_right);
        refine_pair_all_cells(RF, RG, sigF.codomain_hyperplanes, sigG.codomain_hyperplanes, scratch_left, scratch_right);

        total_checksum += checksum_partition(PF);
        total_checksum += checksum_partition(QF);
        total_checksum += checksum_partition(LF);
        total_checksum += checksum_partition(RF);

        PF.rollback(pf_snapshot);
        PG.rollback(pg_snapshot);
        QF.rollback(qf_snapshot);
        QG.rollback(qg_snapshot);
        LF.rollback(lf_snapshot);
        LG.rollback(lg_snapshot);
        RF.rollback(rf_snapshot);
        RG.rollback(rg_snapshot);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "domain_dim=" << domain_dim
              << " codomain_dim=" << codomain_dim
              << " iterations=" << iterations
              << " signatures_plus_refine_avg_ms=" << static_cast<double>(micros) / 1000.0 / iterations
              << " checksum=" << total_checksum
              << '\n';
}

} // namespace

int main()
{
    benchmark_dims(10, 10, 1'000);
    benchmark_dims(16, 16, 100);
    benchmark_dims(20, 20, 10);
    benchmark_dims(22, 22, 3);

    return 0;
}
