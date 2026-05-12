#include "../src/solution_store.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

affine::f2::AffineMap identity_map(std::uint32_t dim)
{
    affine::f2::AffineMap map;
    map.dim = dim;
    map.basis_images.resize(dim);
    for (std::uint32_t bit = 0; bit < dim; ++bit) {
        map.basis_images[bit] = std::uint32_t { 1 } << bit;
    }
    return map;
}

affine::f2::AffineMap sample_map()
{
    return affine::f2::AffineMap {
        .dim = 2,
        .translation = 1,
        .basis_images = { 2, 3 },
    };
}

} // namespace

int main()
{
    const std::vector<std::uint32_t> identity_function = { 0, 1, 2, 3 };

    affine::SolutionStore store;
    assert(store.empty());
    assert(store.size() == 0);
    assert(!store.records_a1_automorphisms());
    assert(store.a1_group().size() == 0);

    affine::Solution first;
    first.domain_map = sample_map();
    first.codomain_map = sample_map();
    first.path.push_back({
        .kind = affine::BranchKind::DomainPoint,
        .left_object = 0,
        .right_object = 1,
    });

    assert(affine::verify_solution(identity_function, identity_function, first));
    assert(store.publish(identity_function, identity_function, first));
    assert(!store.empty());
    assert(store.size() == 1);
    assert(store.a1_group().size() == 0);
    assert(!store.publish(identity_function, identity_function, first));
    assert(store.size() == 1);
    assert(store.a1_group().size() == 0);

    affine::Solution second;
    second.domain_map = identity_map(2);
    second.codomain_map = identity_map(2);
    assert(store.publish(identity_function, identity_function, second));
    assert(store.size() == 2);
    assert(store.a1_group().size() == 0);

    affine::Solution invalid;
    invalid.domain_map = sample_map();
    invalid.codomain_map = identity_map(2);
    assert(!affine::verify_solution(identity_function, identity_function, invalid));
    assert(!store.publish(identity_function, identity_function, invalid));
    assert(store.size() == 2);

    const std::vector<affine::Solution> snapshot = store.snapshot();
    assert(snapshot.size() == 2);
    assert(snapshot[0].domain_map.translation == 1);
    assert(snapshot[0].path.size() == 1);
    assert(snapshot[0].path[0].right_object == 1);
    assert(snapshot[1].domain_map.dim == 2);

    affine::SolutionStore automorphism_store(
        affine::SolutionStore::Options {
            .record_a1_automorphisms = true,
        });
    assert(automorphism_store.records_a1_automorphisms());
    assert(automorphism_store.a1_group().size() == 0);
    assert(automorphism_store.publish(identity_function, identity_function, first));
    assert(automorphism_store.a1_group().size() == 1);
    assert(!automorphism_store.publish(identity_function, identity_function, first));
    assert(automorphism_store.a1_group().size() == 1);
    assert(automorphism_store.publish(identity_function, identity_function, second));
    assert(automorphism_store.a1_group().size() == 1);

    affine::SolutionStore count_only_store(
        affine::SolutionStore::Options {
            .record_a1_automorphisms = true,
            .count_verified_only = true,
        });
    assert(count_only_store.publish(identity_function, identity_function, first));
    assert(count_only_store.publish(identity_function, identity_function, first));
    assert(count_only_store.size() == 2);
    assert(count_only_store.snapshot().empty());
    assert(count_only_store.a1_group().size() == 0);

    return 0;
}
