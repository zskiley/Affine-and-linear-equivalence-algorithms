#include "../src/refinement.hpp"

#include <cassert>
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

bool is_apn_folder(const std::filesystem::directory_entry& entry)
{
    if (!entry.is_directory()) {
        return false;
    }

    const std::string name = entry.path().filename().string();
    return name.rfind("apn_", 0) == 0;
}

void test_affine_variants_for_dimension(
    const std::filesystem::path& root,
    std::uint32_t dim)
{
    const std::filesystem::path dim_root = root / std::to_string(dim);
    if (!std::filesystem::exists(dim_root)) {
        throw std::runtime_error("missing APN dimension folder " + dim_root.string());
    }

    std::uint32_t tested = 0;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dim_root)) {
        if (!is_apn_folder(entry)) {
            continue;
        }

        const std::filesystem::path folder = entry.path();
        const std::vector<std::uint32_t> original = read_truth_table(folder / "function.tt");
        const std::uint32_t expected_size = affine::f2::point_count(dim);
        assert(original.size() == expected_size);

        for (const char* variant : { "affine_01.tt", "affine_02.tt", "affine_03.tt" }) {
            const std::vector<std::uint32_t> affine_variant = read_truth_table(folder / variant);
            assert(affine_variant.size() == expected_size);

            affine::SearchPartitions partitions = affine::make_initial_partitions(dim, dim);
            affine::RefinementWorkspace workspace;
            (void)affine::refine_until_stable(
                dim,
                dim,
                original,
                affine_variant,
                partitions,
                workspace);

            assert(affine::same_shape(partitions.P.left, partitions.P.right));
            assert(affine::same_shape(partitions.Q.left, partitions.Q.right));
            assert(affine::same_shape(partitions.L.left, partitions.L.right));
            assert(affine::same_shape(partitions.R.left, partitions.R.right));
            ++tested;
        }
    }

    assert(tested > 0);
    std::cout << "dim=" << dim << " affine variants tested=" << tested << '\n';
}

void test_affine_variants_in_folder(
    const std::filesystem::path& folder,
    std::uint32_t dim,
    std::string_view label)
{
    const std::vector<std::uint32_t> original = read_truth_table(folder / "function.tt");
    const std::uint32_t expected_size = affine::f2::point_count(dim);
    assert(original.size() == expected_size);

    std::uint32_t tested = 0;
    for (const char* variant : { "affine_01.tt", "affine_02.tt", "affine_03.tt" }) {
        const std::vector<std::uint32_t> affine_variant = read_truth_table(folder / variant);
        assert(affine_variant.size() == expected_size);

        affine::SearchPartitions partitions = affine::make_initial_partitions(dim, dim);
        affine::RefinementWorkspace workspace;
        (void)affine::refine_until_stable(
            dim,
            dim,
            original,
            affine_variant,
            partitions,
            workspace);

        assert(affine::same_shape(partitions.P.left, partitions.P.right));
        assert(affine::same_shape(partitions.Q.left, partitions.Q.right));
        assert(affine::same_shape(partitions.L.left, partitions.L.right));
        assert(affine::same_shape(partitions.R.left, partitions.R.right));
        ++tested;
    }

    std::cout << "dim=" << dim << ' ' << label << " variants tested=" << tested << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path root =
            argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("apn_original_function_folders");

        for (const std::uint32_t dim : { 8u, 10u, 12u, 14u }) {
            test_affine_variants_for_dimension(root, dim);
        }
        test_affine_variants_in_folder(root / "18" / "inverse", 18, "inverse");

        std::cout << "APN refinement tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
