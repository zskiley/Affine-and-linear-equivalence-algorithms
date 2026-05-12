#include "../src/cross_signature.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

using Weight = affine::f2::Weight;

std::int64_t checksum(const std::vector<Weight>& values)
{
    return std::accumulate(values.begin(), values.end(), std::int64_t { 0 });
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

void fill_fake_weights(std::vector<Weight>& weights, std::uint32_t seed)
{
    for (std::uint32_t i = 0; i < weights.size(); ++i) {
        weights[i] = (i * 1'103'515'245u + seed) % 31u;
    }
}

void benchmark_dims(std::uint32_t domain_dim, std::uint32_t codomain_dim, int iterations)
{
    const std::uint32_t domain_hyperplanes = affine::f2::hyperplane_count(domain_dim);
    const std::uint32_t codomain_hyperplanes = affine::f2::hyperplane_count(codomain_dim);

    std::vector<std::uint32_t> function_table = make_fake_function_table(domain_dim, codomain_dim);
    std::vector<Weight> domain_hyperplane_weights(domain_hyperplanes);
    std::vector<Weight> codomain_hyperplane_weights(codomain_hyperplanes);
    std::vector<Weight> domain_hyperplane_out(domain_hyperplanes);
    std::vector<Weight> codomain_hyperplane_out(codomain_hyperplanes);
    affine::f2::CrossSignatureWorkspace workspace;

    fill_fake_weights(domain_hyperplane_weights, 7u);
    fill_fake_weights(codomain_hyperplane_weights, 19u);

    std::int64_t domain_checksum = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        affine::f2::domain_hyperplane_cross_signature(
            domain_dim,
            codomain_dim,
            function_table,
            codomain_hyperplane_weights,
            domain_hyperplane_out,
            workspace);
        domain_checksum += checksum(domain_hyperplane_out);
    }
    auto end = std::chrono::steady_clock::now();
    const auto domain_micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::int64_t codomain_checksum = 0;
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        affine::f2::codomain_hyperplane_cross_signature(
            domain_dim,
            codomain_dim,
            function_table,
            domain_hyperplane_weights,
            codomain_hyperplane_out,
            workspace);
        codomain_checksum += checksum(codomain_hyperplane_out);
    }
    end = std::chrono::steady_clock::now();
    const auto codomain_micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "domain_dim=" << domain_dim
              << " codomain_dim=" << codomain_dim
              << " iterations=" << iterations
              << " domain_cross_avg_ms=" << static_cast<double>(domain_micros) / 1000.0 / iterations
              << " codomain_cross_avg_ms=" << static_cast<double>(codomain_micros) / 1000.0 / iterations
              << " domain_checksum=" << domain_checksum
              << " codomain_checksum=" << codomain_checksum
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
