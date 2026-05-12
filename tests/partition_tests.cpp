#include "../src/partition.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

using Partition = affine::Partition;
using ObjectId = Partition::ObjectId;
using Signature = Partition::Signature;

std::vector<ObjectId> to_vector(std::span<const ObjectId> objects)
{
    return { objects.begin(), objects.end() };
}

void expect_objects(
    std::span<const ObjectId> actual,
    const std::vector<ObjectId>& expected)
{
    assert(to_vector(actual) == expected);
}

void test_initial_partition()
{
    Partition partition(6);

    assert(partition.check_invariants());
    assert(partition.first_cell() == partition.last_cell());
    expect_objects(partition.objects(partition.first_cell()), { 0, 1, 2, 3, 4, 5 });

    for (ObjectId object = 0; object < 6; ++object) {
        assert(partition.cell_of(object) == partition.first_cell());
        assert(partition.position_of(object) == object);
    }
}

void test_individualize_and_rollback()
{
    Partition partition(6);
    const auto snapshot = partition.snapshot();

    partition.individualize(3);

    assert(partition.check_invariants());
    const auto first = partition.first_cell();
    const auto second = partition.next_cell(first);

    assert(second != Partition::npos);
    assert(partition.next_cell(second) == Partition::npos);
    expect_objects(partition.objects(first), { 3 });
    expect_objects(partition.objects(second), { 0, 1, 2, 4, 5 });
    assert(partition.cell_of(3) == first);

    partition.rollback(snapshot);

    assert(partition.check_invariants());
    assert(partition.first_cell() == partition.last_cell());
    expect_objects(partition.objects(partition.first_cell()), { 0, 1, 2, 3, 4, 5 });
}

void test_refine_by_signature_and_rollback()
{
    Partition partition(6);
    Partition::RefinementScratch scratch;
    const std::vector<Signature> signatures = { 2, 1, 2, 1, 3, 3 };
    const auto snapshot = partition.snapshot();

    const bool changed = partition.refine_cell_by_signature(
        partition.first_cell(),
        signatures,
        scratch);

    assert(changed);
    assert(partition.check_invariants());

    const auto c0 = partition.first_cell();
    const auto c1 = partition.next_cell(c0);
    const auto c2 = partition.next_cell(c1);

    assert(c1 != Partition::npos);
    assert(c2 != Partition::npos);
    assert(partition.next_cell(c2) == Partition::npos);

    expect_objects(partition.objects(c0), { 1, 3 });
    expect_objects(partition.objects(c1), { 0, 2 });
    expect_objects(partition.objects(c2), { 4, 5 });

    for (ObjectId object : { ObjectId { 1 }, ObjectId { 3 } }) {
        assert(partition.cell_of(object) == c0);
    }
    for (ObjectId object : { ObjectId { 0 }, ObjectId { 2 } }) {
        assert(partition.cell_of(object) == c1);
    }
    for (ObjectId object : { ObjectId { 4 }, ObjectId { 5 } }) {
        assert(partition.cell_of(object) == c2);
    }

    partition.rollback(snapshot);

    assert(partition.check_invariants());
    assert(partition.first_cell() == partition.last_cell());
    expect_objects(partition.objects(partition.first_cell()), { 0, 1, 2, 3, 4, 5 });
}

void test_refine_without_split()
{
    Partition partition(5);
    Partition::RefinementScratch scratch;
    const std::vector<Signature> signatures = { 7, 7, 7, 7, 7 };
    const auto snapshot = partition.snapshot();

    const bool changed = partition.refine_cell_by_signature(
        partition.first_cell(),
        signatures,
        scratch);

    assert(!changed);
    assert(partition.check_invariants());
    assert(partition.snapshot() == snapshot);
    expect_objects(partition.objects(partition.first_cell()), { 0, 1, 2, 3, 4 });
}

void test_nested_snapshots()
{
    Partition partition(8);
    Partition::RefinementScratch scratch;
    const std::vector<Signature> signatures = { 0, 1, 0, 1, 2, 2, 3, 3 };

    const auto root = partition.snapshot();
    partition.individualize(5);
    assert(partition.check_invariants());

    const auto child = partition.snapshot();
    partition.refine_cell_by_signature(partition.next_cell(partition.first_cell()), signatures, scratch);
    assert(partition.check_invariants());

    partition.rollback(child);
    assert(partition.check_invariants());
    expect_objects(partition.objects(partition.first_cell()), { 5 });

    partition.rollback(root);
    assert(partition.check_invariants());
    assert(partition.first_cell() == partition.last_cell());
    expect_objects(partition.objects(partition.first_cell()), { 0, 1, 2, 3, 4, 5, 6, 7 });
}

void benchmark_refine_rollback()
{
    constexpr ObjectId object_count = 50'000;
    constexpr int iterations = 100;

    Partition partition(object_count);
    Partition::RefinementScratch scratch;
    std::vector<Signature> signatures(object_count);

    for (ObjectId object = 0; object < object_count; ++object) {
        signatures[object] = static_cast<Signature>((object * 2'654'435'761u) % 257u);
    }

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t changed_count = 0;

    for (int i = 0; i < iterations; ++i) {
        const auto snapshot = partition.snapshot();
        if (partition.refine_cell_by_signature(partition.first_cell(), signatures, scratch)) {
            ++changed_count;
        }
        partition.rollback(snapshot);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    assert(changed_count == iterations);
    assert(partition.check_invariants());

    std::cout << "partition refine/rollback benchmark: "
              << iterations << " iterations over "
              << object_count << " objects in "
              << millis << " ms\n";
}

} // namespace

int main()
{
    test_initial_partition();
    test_individualize_and_rollback();
    test_refine_by_signature_and_rollback();
    test_refine_without_split();
    test_nested_snapshots();
    benchmark_refine_rollback();

    std::cout << "partition tests passed\n";
    return 0;
}
