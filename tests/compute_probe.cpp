#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {

struct alignas(64) WorkerResult {
    std::uint64_t batches = 0;
    std::uint64_t checksum = 0;
};

[[nodiscard]] std::uint32_t arg_u32(char** argv, int index, std::uint32_t fallback)
{
    return argv[index] == nullptr
        ? fallback
        : static_cast<std::uint32_t>(std::strtoul(argv[index], nullptr, 10));
}

[[nodiscard]] std::uint64_t mix(std::uint64_t value)
{
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31u;
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    const std::uint32_t thread_count =
        argc >= 2 ? arg_u32(argv, 1, 1) : 1;
    const std::uint32_t seconds =
        argc >= 3 ? arg_u32(argv, 2, 10) : 10;

    std::atomic<bool> start = false;
    std::atomic<bool> stop = false;
    std::vector<WorkerResult> results(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::uint32_t worker = 0; worker < thread_count; ++worker) {
        threads.emplace_back([&, worker] {
            std::uint64_t value =
                0x9e3779b97f4a7c15ull ^ static_cast<std::uint64_t>(worker + 1u);
            std::uint64_t batches = 0;

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                for (std::uint32_t index = 0; index < 4096; ++index) {
                    value = mix(value + index + batches);
                }
                ++batches;
            }

            results[worker].batches = batches;
            results[worker].checksum = value;
        });
    }

    const auto begin = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop.store(true, std::memory_order_release);
    for (std::thread& thread : threads) {
        thread.join();
    }
    const auto end = std::chrono::steady_clock::now();

    std::uint64_t total_batches = 0;
    std::uint64_t checksum = 0;
    for (const WorkerResult& result : results) {
        total_batches += result.batches;
        checksum ^= result.checksum;
    }

    const double elapsed_seconds =
        std::chrono::duration<double>(end - begin).count();
    const double million_mixes_per_second =
        static_cast<double>(total_batches) * 4096.0
        / 1'000'000.0
        / elapsed_seconds;

    std::cout << "threads=" << thread_count
              << " seconds=" << elapsed_seconds
              << " million_mixes_per_second=" << million_mixes_per_second
              << " checksum=" << checksum
              << '\n';
    return 0;
}
