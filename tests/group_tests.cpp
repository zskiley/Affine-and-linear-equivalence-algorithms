#include "../src/group/group.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

affine::f2::AffineMap identity_map()
{
    return affine::f2::AffineMap {
        .dim = 3,
        .translation = 0,
        .basis_images = { 1, 2, 4 },
    };
}

affine::f2::AffineMap swap_first_two_coordinates()
{
    return affine::f2::AffineMap {
        .dim = 3,
        .translation = 0,
        .basis_images = { 2, 1, 4 },
    };
}

affine::f2::AffineMap swap_last_two_coordinates()
{
    return affine::f2::AffineMap {
        .dim = 3,
        .translation = 0,
        .basis_images = { 1, 4, 2 },
    };
}

affine::f2::AffineMap translation_map(std::uint32_t translation)
{
    affine::f2::AffineMap map = identity_map();
    map.translation = translation;
    return map;
}

void test_point_action()
{
    affine::group::AffineGroupGenerator generator;
    assert(affine::group::make_generator(
        swap_first_two_coordinates(),
        generator));

    assert(generator.apply_point(1) == 2);
    assert(generator.apply_point(2) == 1);
    assert(generator.apply_point(5) == 6);
}

void test_translation_changes_hyperplane_offset()
{
    affine::f2::AffineMap map = identity_map();
    map.translation = 1;

    affine::group::AffineGroupGenerator generator;
    assert(affine::group::make_generator(map, generator));

    const std::uint32_t x0_equals_zero = affine::f2::hyperplane_id(1, 0);
    const std::uint32_t x0_equals_one = affine::f2::hyperplane_id(1, 1);

    assert(generator.apply_hyperplane(x0_equals_zero) == x0_equals_one);
    assert(generator.apply_hyperplane(x0_equals_one) == x0_equals_zero);
}

void test_linear_part_moves_hyperplane_normal_by_dual_action()
{
    affine::group::AffineGroupGenerator generator;
    assert(affine::group::make_generator(
        swap_first_two_coordinates(),
        generator));

    const std::uint32_t x0_equals_zero = affine::f2::hyperplane_id(1, 0);
    const std::uint32_t x1_equals_zero = affine::f2::hyperplane_id(2, 0);

    assert(generator.apply_hyperplane(x0_equals_zero) == x1_equals_zero);
    assert(generator.apply_hyperplane(x1_equals_zero) == x0_equals_zero);
}

void test_group_add_and_snapshot()
{
    affine::group::AffineGroup group;

    assert(group.add_generator(swap_first_two_coordinates()));
    assert(!group.add_generator(swap_first_two_coordinates()));

    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();
    assert(snapshot.version == 1);
    assert(snapshot.dim == 3);
    assert(snapshot.generators.size() == 1);
    assert(group.size() == 1);
}

void test_group_add_rejects_generated_translation()
{
    affine::group::AffineGroup group;

    assert(group.add_generator(translation_map(1)));
    assert(group.add_generator(translation_map(2)));
    assert(!group.add_generator(translation_map(3)));

    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();
    assert(snapshot.version == 2);
    assert(snapshot.generators.size() == 2);
    assert(group.size() == 2);
}

void test_group_add_rejects_generated_coordinate_permutation()
{
    affine::group::AffineGroup group;
    const affine::f2::AffineMap cycle =
        affine::group::compose_affine_maps(
            swap_last_two_coordinates(),
            swap_first_two_coordinates());

    assert(group.add_generator(swap_first_two_coordinates()));
    assert(group.add_generator(swap_last_two_coordinates()));
    assert(!group.add_generator(cycle));

    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();
    assert(snapshot.version == 2);
    assert(snapshot.generators.size() == 2);
    assert(group.size() == 2);
}

void test_point_orbit_representatives()
{
    affine::group::AffineGroup group;
    assert(group.add_generator(swap_first_two_coordinates()));
    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();

    const std::vector<std::uint32_t> candidates { 1, 2, 4 };
    const std::vector<std::uint32_t> representatives =
        affine::group::point_representatives(snapshot, {}, candidates);

    assert(representatives.size() == 2);
    assert(representatives[0] == 1);
    assert(representatives[1] == 4);
}

void test_stabilizer_restriction()
{
    affine::group::AffineGroup group;
    assert(group.add_generator(swap_first_two_coordinates()));
    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();

    const std::vector<std::uint32_t> fixed_points { 1 };
    const affine::group::FixedObjects fixed {
        .points = fixed_points,
        .hyperplanes = {},
    };

    const std::vector<std::uint32_t> candidates { 1, 2, 4 };
    const std::vector<std::uint32_t> representatives =
        affine::group::point_representatives(snapshot, fixed, candidates);

    assert(representatives.size() == candidates.size());
}

void test_pointwise_stabilizer_api()
{
    affine::group::AffineGroup group;
    assert(group.add_generator(swap_first_two_coordinates()));
    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();

    const affine::group::Stabilizer unrestricted =
        affine::group::pointwise_stabilizer(snapshot, {});
    assert(unrestricted.group_version == snapshot.version);
    assert(unrestricted.generators.size() == 1);

    const std::vector<std::uint32_t> fixed_points { 1 };
    const affine::group::FixedObjects fixed {
        .points = fixed_points,
        .hyperplanes = {},
    };
    const affine::group::Stabilizer restricted =
        affine::group::pointwise_stabilizer(snapshot, fixed);
    assert(restricted.group_version == snapshot.version);
    assert(restricted.empty());
}

void test_generated_pointwise_stabilizer_is_used_for_orbits()
{
    const affine::f2::AffineMap cycle =
        affine::group::compose_affine_maps(
            swap_last_two_coordinates(),
            swap_first_two_coordinates());

    affine::group::AffineGroupSnapshot snapshot {
        .generators = {},
        .version = 1,
        .dim = 3,
    };
    affine::group::AffineGroupGenerator swap_generator;
    affine::group::AffineGroupGenerator cycle_generator;
    assert(affine::group::make_generator(
        swap_first_two_coordinates(),
        swap_generator));
    assert(affine::group::make_generator(cycle, cycle_generator));
    snapshot.generators.push_back(swap_generator);
    snapshot.generators.push_back(cycle_generator);

    const std::vector<std::uint32_t> fixed_points { 1 };
    const affine::group::FixedObjects fixed {
        .points = fixed_points,
        .hyperplanes = {},
    };
    const std::vector<std::uint32_t> candidates { 2, 4 };
    const std::vector<std::uint32_t> representatives =
        affine::group::point_representatives(snapshot, fixed, candidates);

    assert(representatives.size() == 1);
    assert(representatives[0] == 2);
}

void test_point_orbit_partition_api()
{
    affine::group::AffineGroup group;
    assert(group.add_generator(swap_first_two_coordinates()));
    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();

    const std::vector<std::uint32_t> candidates { 1, 2, 4 };
    const affine::group::OrbitPartition partition =
        affine::group::point_orbit_partition(snapshot, {}, candidates);

    assert(partition.group_version == snapshot.version);
    assert(partition.orbit_count == 2);
    assert(partition.representatives.size() == 2);
    assert(partition.representatives[0] == 1);
    assert(partition.representatives[1] == 4);
    assert(partition.orbit_of_index.size() == candidates.size());
    assert(partition.orbit_of_index[0] == 0);
    assert(partition.orbit_of_index[1] == 0);
    assert(partition.orbit_of_index[2] == 1);
}

void test_hyperplane_orbit_representatives()
{
    affine::group::AffineGroup group;
    assert(group.add_generator(swap_first_two_coordinates()));
    const affine::group::AffineGroupSnapshot snapshot = group.snapshot();

    const std::vector<std::uint32_t> candidates {
        affine::f2::hyperplane_id(1, 0),
        affine::f2::hyperplane_id(2, 0),
        affine::f2::hyperplane_id(4, 1),
    };

    const std::vector<std::uint32_t> representatives =
        affine::group::hyperplane_representatives(snapshot, {}, candidates);

    assert(representatives.size() == 2);
    assert(representatives[0] == affine::f2::hyperplane_id(1, 0));
    assert(representatives[1] == affine::f2::hyperplane_id(4, 1));
}

void test_affine_map_composition_and_inverse()
{
    const affine::f2::AffineMap translation = translation_map(3);
    const affine::f2::AffineMap swap = swap_first_two_coordinates();
    const affine::f2::AffineMap composed =
        affine::group::compose_affine_maps(swap, translation);
    const affine::f2::AffineMap inverse =
        affine::group::inverse_affine_map(composed);

    for (std::uint32_t point = 0; point < 8; ++point) {
        const std::uint32_t image = affine::f2::apply(composed, point);
        assert(affine::f2::apply(inverse, image) == point);
    }
}

void test_schreier_sims_translation_membership()
{
    const std::vector<affine::f2::AffineMap> generators {
        translation_map(1),
        translation_map(2),
    };

    affine::group::SchreierSims sims;
    assert(sims.rebuild(3, generators));

    assert(sims.contains(affine::group::identity_affine_map(3)));
    assert(sims.contains(translation_map(1)));
    assert(sims.contains(translation_map(2)));
    assert(sims.contains(translation_map(3)));
    assert(!sims.contains(translation_map(4)));
}

void test_schreier_sims_coordinate_permutation_membership()
{
    const std::vector<affine::f2::AffineMap> generators {
        swap_first_two_coordinates(),
        swap_last_two_coordinates(),
    };

    const affine::f2::AffineMap cycle =
        affine::group::compose_affine_maps(
            swap_last_two_coordinates(),
            swap_first_two_coordinates());

    affine::group::SchreierSims sims;
    assert(sims.rebuild(3, generators));

    assert(sims.contains(swap_first_two_coordinates()));
    assert(sims.contains(swap_last_two_coordinates()));
    assert(sims.contains(cycle));
    assert(!sims.contains(translation_map(1)));
}

} // namespace

int main()
{
    test_point_action();
    test_translation_changes_hyperplane_offset();
    test_linear_part_moves_hyperplane_normal_by_dual_action();
    test_group_add_and_snapshot();
    test_group_add_rejects_generated_translation();
    test_group_add_rejects_generated_coordinate_permutation();
    test_point_orbit_representatives();
    test_stabilizer_restriction();
    test_pointwise_stabilizer_api();
    test_generated_pointwise_stabilizer_is_used_for_orbits();
    test_point_orbit_partition_api();
    test_hyperplane_orbit_representatives();
    test_affine_map_composition_and_inverse();
    test_schreier_sims_translation_membership();
    test_schreier_sims_coordinate_permutation_membership();

    std::cout << "group tests passed\n";
    return 0;
}
