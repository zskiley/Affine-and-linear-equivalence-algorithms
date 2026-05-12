#include "../src/refinement.hpp"

#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint32_t> read_truth_table(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open " + path.string());
    }

    std::vector<std::uint32_t> table;
    std::uint32_t value = 0;
    bool in_number = false;

    char ch = 0;
    while (input.get(ch)) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isdigit(uch)) {
            value = value * 10u + static_cast<std::uint32_t>(uch - '0');
            in_number = true;
        } else if (in_number) {
            table.push_back(value);
            value = 0;
            in_number = false;
        }
    }

    if (in_number) {
        table.push_back(value);
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

std::uint64_t checksum_partitions(const affine::SearchPartitions& partitions)
{
    return checksum_pair(partitions.P)
        + checksum_pair(partitions.Q)
        + checksum_pair(partitions.L)
        + checksum_pair(partitions.R);
}

void benchmark_pair(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t left_index,
    std::uint32_t right_index)
{
    const std::filesystem::path dir = root / std::to_string(dim);
    const std::filesystem::path left_path = dir / ("f_" + std::to_string(left_index) + ".tt");
    const std::filesystem::path right_path = dir / ("f_" + std::to_string(right_index) + ".tt");

    const std::vector<std::uint32_t> left_table = read_truth_table(left_path);
    const std::vector<std::uint32_t> right_table = read_truth_table(right_path);
    const std::uint32_t expected_size = affine::f2::point_count(dim);

    if (left_table.size() != expected_size || right_table.size() != expected_size) {
        throw std::runtime_error("truth-table size mismatch for dim " + std::to_string(dim));
    }

    affine::SearchPartitions partitions = affine::make_initial_partitions(dim, dim);
    affine::RefinementWorkspace workspace;

    const auto start = std::chrono::steady_clock::now();
    const affine::RefineResult result = affine::refine_until_stable(
        dim,
        dim,
        left_table,
        right_table,
        partitions,
        workspace);
    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "dim=" << dim
              << " pair=f_" << left_index << "_f_" << right_index
              << " result=" << (result == affine::RefineResult::Stop ? "stop" : "continue")
              << " ms=" << static_cast<double>(micros) / 1000.0
              << " checksum=" << checksum_partitions(partitions)
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path root =
            argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("ccz-benchmark-master");

        for (const std::uint32_t dim : { 6u, 8u, 10u, 12u, 14u }) {
            benchmark_pair(root, dim, 1, 1);
            benchmark_pair(root, dim, 1, 2);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
