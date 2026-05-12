#include "../src/radon_f2.hpp"

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

void fill_fake_point_partition_weights(std::vector<Weight>& weights)
{
    for (std::uint32_t i = 0; i < weights.size(); ++i) {
        weights[i] = (i * 2'654'435'761u) % 31u;
    }
}

void fill_fake_hyperplane_partition_weights(std::vector<Weight>& weights)
{
    for (std::uint32_t i = 0; i < weights.size(); ++i) {
        weights[i] = (i * 1'103'515'245u + 12'345u) % 31u;
    }
}

void benchmark_dim(std::uint32_t dim, int iterations)
{
    const std::uint32_t point_count = affine::f2::point_count(dim);
    const std::uint32_t hyperplane_count = affine::f2::hyperplane_count(dim);

    std::vector<Weight> point_weights(point_count);
    std::vector<Weight> hyperplane_weights(hyperplane_count);
    std::vector<Weight> hyperplane_out(hyperplane_count);
    std::vector<Weight> point_out(point_count);
    affine::f2::RadonWorkspace workspace;

    fill_fake_point_partition_weights(point_weights);
    fill_fake_hyperplane_partition_weights(hyperplane_weights);

    std::int64_t radon_checksum = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        affine::f2::radon_transform(dim, point_weights, hyperplane_out, workspace);
        radon_checksum += checksum(hyperplane_out);
    }
    auto end = std::chrono::steady_clock::now();
    const auto radon_micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::int64_t backproject_checksum = 0;
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        affine::f2::backproject(dim, hyperplane_weights, point_out, workspace);
        backproject_checksum += checksum(point_out);
    }
    end = std::chrono::steady_clock::now();
    const auto backproject_micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "dim=" << dim
              << " points=" << point_count
              << " hyperplanes=" << hyperplane_count
              << " iterations=" << iterations
              << " radon_avg_ms=" << static_cast<double>(radon_micros) / 1000.0 / iterations
              << " backproject_avg_ms=" << static_cast<double>(backproject_micros) / 1000.0 / iterations
              << " radon_checksum=" << radon_checksum
              << " backproject_checksum=" << backproject_checksum
              << '\n';
}

} // namespace

int main()
{
    benchmark_dim(10, 10'000);
    benchmark_dim(16, 1'000);
    benchmark_dim(20, 100);
    benchmark_dim(22, 20);
    benchmark_dim(24, 5);

    return 0;
}
