#include "../src/affine_map.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

std::uint32_t sample_linear(std::uint32_t x)
{
    const std::uint32_t bit0 = x & 1u;
    const std::uint32_t bit1 = (x >> 1u) & 1u;
    const std::uint32_t bit2 = (x >> 2u) & 1u;
    return (bit1 << 0u) | (bit2 << 1u) | ((bit0 ^ bit1) << 2u);
}

std::uint32_t sample_affine(std::uint32_t x)
{
    return sample_linear(x) ^ 5u;
}

void add_singleton_pair(
    affine::PartitionPair& pair,
    std::uint32_t left,
    std::uint32_t right)
{
    pair.left.individualize(left);
    pair.right.individualize(right);
}

void test_determined_affine_map()
{
    affine::PartitionPair pair(8);
    for (const std::uint32_t x : { 0u, 1u, 2u, 4u }) {
        add_singleton_pair(pair, x, sample_affine(x));
    }

    affine::f2::AffineMap map;
    affine::f2::AffineFitWorkspace workspace;
    const affine::f2::AffineFitStatus status =
        affine::f2::fit_affine_map(3, pair, map, workspace);

    assert(status == affine::f2::AffineFitStatus::Determined);
    for (std::uint32_t x = 0; x < 8; ++x) {
        assert(affine::f2::apply(map, x) == sample_affine(x));
    }
}

void test_underdetermined_affine_map()
{
    affine::PartitionPair pair(8);
    add_singleton_pair(pair, 0, sample_affine(0));
    add_singleton_pair(pair, 1, sample_affine(1));

    affine::f2::AffineMap map;
    affine::f2::AffineFitWorkspace workspace;
    const affine::f2::AffineFitStatus status =
        affine::f2::fit_affine_map(3, pair, map, workspace);

    assert(status == affine::f2::AffineFitStatus::Underdetermined);
}

void test_inconsistent_affine_map()
{
    affine::PartitionPair pair(8);
    add_singleton_pair(pair, 0, 0);
    add_singleton_pair(pair, 1, 1);
    add_singleton_pair(pair, 2, 2);
    add_singleton_pair(pair, 4, 4);
    add_singleton_pair(pair, 3, 5);

    affine::f2::AffineMap map;
    affine::f2::AffineFitWorkspace workspace;
    const affine::f2::AffineFitStatus status =
        affine::f2::fit_affine_map(3, pair, map, workspace);

    assert(status == affine::f2::AffineFitStatus::Inconsistent);
}

void test_affine_map_respects_partition()
{
    affine::f2::AffineMap map {
        .dim = 3,
        .translation = 5,
        .basis_images = { 4, 5, 2 },
    };

    affine::PartitionPair compatible(8);
    add_singleton_pair(compatible, 0, affine::f2::apply(map, 0));
    assert(affine::f2::affine_map_respects_partition(map, compatible));

    affine::PartitionPair incompatible(8);
    add_singleton_pair(incompatible, 0, affine::f2::apply(map, 1));
    assert(!affine::f2::affine_map_respects_partition(map, incompatible));
}

} // namespace

int main()
{
    test_determined_affine_map();
    test_underdetermined_affine_map();
    test_inconsistent_affine_map();
    test_affine_map_respects_partition();

    std::cout << "affine map tests passed\n";
    return 0;
}
