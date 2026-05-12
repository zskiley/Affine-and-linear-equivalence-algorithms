#include "../src/refinement.hpp"

#include <chrono>
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

std::uint64_t checksum_pair(const affine::PartitionPair& pair)
{
    std::uint64_t checksum = 0;
    std::uint64_t cell_index = 1;
    for (affine::Partition::CellId cell = pair.left.first_cell();
         cell != affine::Partition::npos;
         cell = pair.left.next_cell(cell)) {
        checksum += cell_index * pair.left.cell(cell).size();
        ++cell_index;
    }
    return checksum;
}

void benchmark_dims(std::uint32_t domain_dim, std::uint32_t codomain_dim, int iterations)
{
    const std::vector<std::uint32_t> function_table =
        make_function_table(domain_dim, codomain_dim);

    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i) {
        affine::SearchPartitions partitions =
            affine::make_initial_partitions(domain_dim, codomain_dim);
        affine::RefinementWorkspace workspace;

        const affine::RefineResult result = affine::refine_until_stable(
            domain_dim,
            codomain_dim,
            function_table,
            function_table,
            partitions,
            workspace);

        (void)result;

        checksum += checksum_pair(partitions.P);
        checksum += checksum_pair(partitions.Q);
        checksum += checksum_pair(partitions.L);
        checksum += checksum_pair(partitions.R);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "domain_dim=" << domain_dim
              << " codomain_dim=" << codomain_dim
              << " iterations=" << iterations
              << " refine_until_stable_avg_ms=" << static_cast<double>(micros) / 1000.0 / iterations
              << " checksum=" << checksum
              << '\n';
}

} // namespace

int main()
{
    benchmark_dims(8, 8, 100);
    benchmark_dims(10, 10, 50);
    benchmark_dims(12, 12, 10);
    benchmark_dims(14, 14, 3);

    return 0;
}
