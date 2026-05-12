#include "../src/signatures_f2.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

affine::Partition make_fake_partition(std::uint32_t object_count, std::uint32_t class_count)
{
    affine::Partition partition(object_count);
    affine::Partition::RefinementScratch scratch;
    std::vector<affine::Partition::Signature> signatures(object_count);

    for (std::uint32_t object = 0; object < object_count; ++object) {
        signatures[object] = (object * 1'103'515'245u + 12'345u) % class_count;
    }

    partition.refine_cell_by_signature(partition.first_cell(), signatures, scratch);
    return partition;
}

std::vector<std::uint32_t> make_fake_function_table(
    std::uint32_t domain_dim,
    std::uint32_t codomain_dim)
{
    const std::uint32_t domain_points = affine::f2::point_count(domain_dim);
    const std::uint32_t codomain_mask = affine::f2::point_count(codomain_dim) - 1u;

    std::vector<std::uint32_t> table(domain_points);
    for (std::uint32_t x = 0; x < domain_points; ++x) {
        table[x] = ((x * 2'654'435'761u) ^ (x >> 3u) ^ (x << 5u)) & codomain_mask;
    }
    return table;
}

std::uint64_t checksum(const std::vector<affine::Partition::Signature>& values)
{
    return std::accumulate(values.begin(), values.end(), std::uint64_t { 0 });
}

void benchmark_dims(std::uint32_t domain_dim, std::uint32_t codomain_dim, int iterations)
{
    affine::Partition P = make_fake_partition(affine::f2::point_count(domain_dim), 17);
    affine::Partition Q = make_fake_partition(affine::f2::point_count(codomain_dim), 19);
    affine::Partition L = make_fake_partition(affine::f2::hyperplane_count(domain_dim), 23);
    affine::Partition R = make_fake_partition(affine::f2::hyperplane_count(codomain_dim), 29);
    std::vector<std::uint32_t> function_table = make_fake_function_table(domain_dim, codomain_dim);

    affine::f2::FunctionSignatures signatures;
    affine::f2::SignatureWorkspace workspace;

    std::uint64_t total_checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i) {
        affine::f2::compute_function_signatures(
            domain_dim,
            codomain_dim,
            function_table,
            P,
            Q,
            L,
            R,
            signatures,
            workspace);

        total_checksum += checksum(signatures.domain_points);
        total_checksum += checksum(signatures.codomain_points);
        total_checksum += checksum(signatures.domain_hyperplanes);
        total_checksum += checksum(signatures.codomain_hyperplanes);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "domain_dim=" << domain_dim
              << " codomain_dim=" << codomain_dim
              << " iterations=" << iterations
              << " all_signatures_avg_ms=" << static_cast<double>(micros) / 1000.0 / iterations
              << " checksum=" << total_checksum
              << '\n';
}

} // namespace

int main()
{
    benchmark_dims(10, 10, 10'000);
    benchmark_dims(16, 16, 1'000);
    benchmark_dims(20, 20, 100);
    benchmark_dims(22, 22, 20);
    benchmark_dims(24, 24, 5);

    return 0;
}
