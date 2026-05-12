#include "../src/fwht.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

template <class Integer>
std::int64_t checksum(const std::vector<Integer>& values)
{
    return std::accumulate(values.begin(), values.end(), std::int64_t { 0 });
}

template <class Integer>
void benchmark_size(std::size_t size, int iterations)
{
    std::vector<Integer> values(size);
    for (std::size_t i = 0; i < size; ++i) {
        values[i] = static_cast<Integer>(
            static_cast<std::int64_t>((i * 1'103'515'245u + 12'345u) & 1023u) - 512);
    }

    const auto start = std::chrono::steady_clock::now();
    std::int64_t total_checksum = 0;

    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::vector<Integer> work = values;
        affine::fwht(std::span<Integer>(work));
        total_checksum += checksum(work);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const double per_iteration_ms = static_cast<double>(micros) / 1000.0 / iterations;

    std::cout << "fwht bits=" << (sizeof(Integer) * 8)
              << " size=" << size
              << " iterations=" << iterations
              << " avg_ms=" << per_iteration_ms
              << " checksum=" << total_checksum
              << '\n';
}

} // namespace

int main()
{
    benchmark_size<std::int32_t>(1u << 10, 10'000);
    benchmark_size<std::int64_t>(1u << 10, 10'000);
    benchmark_size<std::int32_t>(1u << 16, 1'000);
    benchmark_size<std::int64_t>(1u << 16, 1'000);
    benchmark_size<std::int32_t>(1u << 20, 100);
    benchmark_size<std::int64_t>(1u << 20, 100);
    benchmark_size<std::int32_t>(1u << 22, 20);
    benchmark_size<std::int64_t>(1u << 22, 20);

    return 0;
}
