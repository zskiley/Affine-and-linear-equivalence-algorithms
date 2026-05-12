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

std::filesystem::path apn_folder(
    const std::filesystem::path& root,
    std::uint32_t dim,
    std::uint32_t class_index)
{
    static constexpr std::uint32_t source_stride = 11;
    const std::uint32_t source_index = 1u + (class_index - 1u) * source_stride;

    std::string folder = "apn_";
    if (class_index < 10) {
        folder += '0';
    }
    folder += std::to_string(class_index);
    folder += "_f_";
    folder += std::to_string(source_index);

    return root / std::to_string(dim) / folder;
}

void benchmark_pair(
    std::uint32_t dim,
    const std::filesystem::path& left_path,
    const std::filesystem::path& right_path,
    std::string_view label)
{
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
              << " case=" << label
              << " result=" << (result == affine::RefineResult::Stop ? "stop" : "continue")
              << " ms=" << static_cast<double>(micros) / 1000.0
              << " checksum=" << checksum_partitions(partitions)
              << '\n';
}

void benchmark_dimension(const std::filesystem::path& root, std::uint32_t dim)
{
    const std::filesystem::path class_one = apn_folder(root, dim, 1);
    const std::filesystem::path class_two = apn_folder(root, dim, 2);

    const std::filesystem::path original = class_one / "function.tt";

    benchmark_pair(dim, original, class_one / "affine_01.tt", "apn01_affine01");
    benchmark_pair(dim, original, class_one / "affine_02.tt", "apn01_affine02");
    benchmark_pair(dim, original, class_one / "affine_03.tt", "apn01_affine03");
    benchmark_pair(dim, original, class_two / "function.tt", "apn01_vs_apn02");
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path root =
            argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("apn_original_function_folders");

        for (const std::uint32_t dim : { 8u, 10u, 12u, 14u }) {
            benchmark_dimension(root, dim);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
