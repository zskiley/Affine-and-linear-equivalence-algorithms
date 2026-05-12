#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace {

struct alignas(64) WorkerResult {
    std::uint64_t bytes = 0;
    std::uint64_t checksum = 0;
};

[[nodiscard]] std::uint32_t arg_u32(char** argv, int index, std::uint32_t fallback)
{
    return argv[index] == nullptr
        ? fallback
        : static_cast<std::uint32_t>(std::strtoul(argv[index], nullptr, 10));
}

} // namespace

int main(int argc, char** argv)
{
    const std::uint32_t thread_count =
        argc >= 2 ? arg_u32(argv, 1, 16) : 16;
    const std::uint32_t mib_per_thread =
        argc >= 3 ? arg_u32(argv, 2, 64) : 64;
    const std::uint32_t seconds =
        argc >= 4 ? arg_u32(argv, 3, 3) : 3;

    const std::size_t words_per_thread =
        (static_cast<std::size_t>(mib_per_thread) << 20u) / sizeof(std::uint64_t);
    std::vector<std::vector<std::uint64_t>> buffers(thread_count);
    for (std::uint32_t worker = 0; worker < thread_count; ++worker) {
        buffers[worker].resize(words_per_thread);
        for (std::size_t index = 0; index < words_per_thread; ++index) {
            buffers[worker][index] =
                (static_cast<std::uint64_t>(worker + 1u) << 32u) ^ index;
        }
    }

    std::atomic<bool> start = false;
    std::atomic<bool> stop = false;
    std::vector<WorkerResult> results(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::uint32_t worker = 0; worker < thread_count; ++worker) {
        threads.emplace_back([&, worker] {
            while (!start.load(std::memory_order_acquire)) {
            }

            std::uint64_t bytes = 0;
            std::uint64_t checksum = worker + 1u;
            std::vector<std::uint64_t>& buffer = buffers[worker];
            while (!stop.load(std::memory_order_relaxed)) {
                for (std::size_t index = 0; index < buffer.size(); ++index) {
                    const std::uint64_t value = buffer[index] + checksum + index;
                    buffer[index] = value;
                    checksum ^= value;
                }
                bytes += static_cast<std::uint64_t>(buffer.size())
                    * sizeof(std::uint64_t) * 2u;
            }

            results[worker].bytes = bytes;
            results[worker].checksum = checksum;
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

    std::uint64_t total_bytes = 0;
    std::uint64_t checksum = 0;
    for (const WorkerResult& result : results) {
        total_bytes += result.bytes;
        checksum ^= result.checksum;
    }

    const double elapsed_seconds =
        std::chrono::duration<double>(end - begin).count();
    const double gib_per_second =
        static_cast<double>(total_bytes)
        / (1024.0 * 1024.0 * 1024.0)
        / elapsed_seconds;

    std::cout << "threads=" << thread_count
              << " mib_per_thread=" << mib_per_thread
              << " seconds=" << elapsed_seconds
              << " gib_per_second=" << gib_per_second
              << " checksum=" << checksum
              << '\n';
    return 0;
}
