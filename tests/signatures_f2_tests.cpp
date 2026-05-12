#include "../src/cross_signature.hpp"
#include "../src/signatures_f2.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

affine::Partition make_fake_partition(std::uint32_t object_count, std::uint32_t class_count)
{
    affine::Partition partition(object_count);
    affine::Partition::RefinementScratch scratch;
    std::vector<affine::Partition::Signature> signatures(object_count);

    for (std::uint32_t object = 0; object < object_count; ++object) {
        signatures[object] = (object * 2'654'435'761u) % class_count;
    }

    partition.refine_cell_by_signature(partition.first_cell(), signatures, scratch);
    return partition;
}

void test_against_separate_computation()
{
    for (std::uint32_t domain_dim = 1; domain_dim <= 7; ++domain_dim) {
        for (std::uint32_t codomain_dim = 1; codomain_dim <= 7; ++codomain_dim) {
            const std::uint32_t domain_points = affine::f2::point_count(domain_dim);
            const std::uint32_t codomain_points = affine::f2::point_count(codomain_dim);
            const std::uint32_t domain_hyperplanes = affine::f2::hyperplane_count(domain_dim);
            const std::uint32_t codomain_hyperplanes = affine::f2::hyperplane_count(codomain_dim);

            affine::Partition P = make_fake_partition(domain_points, 5);
            affine::Partition Q = make_fake_partition(codomain_points, 7);
            affine::Partition L = make_fake_partition(domain_hyperplanes, 6);
            affine::Partition R = make_fake_partition(codomain_hyperplanes, 8);

            std::vector<std::uint32_t> function_table(domain_points);
            for (std::uint32_t x = 0; x < domain_points; ++x) {
                function_table[x] = ((x * 13u) ^ (x >> 1u) ^ 7u) & (codomain_points - 1u);
            }

            affine::f2::FunctionSignatures fast;
            affine::f2::SignatureWorkspace signature_workspace;
            affine::f2::compute_function_signatures(
                domain_dim,
                codomain_dim,
                function_table,
                P,
                Q,
                L,
                R,
                fast,
                signature_workspace);

            std::vector<affine::f2::Weight> wP(domain_points);
            std::vector<affine::f2::Weight> wQ(codomain_points);
            std::vector<affine::f2::Weight> wL(domain_hyperplanes);
            std::vector<affine::f2::Weight> wR(codomain_hyperplanes);
            affine::f2::build_partition_weights(P, wP);
            affine::f2::build_partition_weights(Q, wQ);
            affine::f2::build_partition_weights(L, wL);
            affine::f2::build_partition_weights(R, wR);

            affine::f2::RadonWorkspace domain_radon;
            affine::f2::RadonWorkspace codomain_radon;
            std::vector<affine::f2::Weight> expected_domain_points(domain_points);
            std::vector<affine::f2::Weight> expected_codomain_points(codomain_points);
            std::vector<affine::f2::Weight> expected_domain_direct(domain_hyperplanes);
            std::vector<affine::f2::Weight> expected_codomain_direct(codomain_hyperplanes);
            std::vector<affine::f2::Weight> expected_domain_cross(domain_hyperplanes);
            std::vector<affine::f2::Weight> expected_codomain_cross(codomain_hyperplanes);
            std::vector<affine::f2::Weight> expected_codomain_preimage(codomain_points, 0);

            affine::f2::backproject(domain_dim, wL, expected_domain_points, domain_radon);
            affine::f2::backproject(codomain_dim, wR, expected_codomain_points, codomain_radon);
            affine::f2::radon_transform(domain_dim, wP, expected_domain_direct, domain_radon);
            affine::f2::radon_transform(codomain_dim, wQ, expected_codomain_direct, codomain_radon);

            affine::f2::CrossSignatureWorkspace cross_workspace;
            affine::f2::domain_hyperplane_cross_signature(
                domain_dim,
                codomain_dim,
                function_table,
                wR,
                expected_domain_cross,
                cross_workspace);
            affine::f2::codomain_hyperplane_cross_signature(
                domain_dim,
                codomain_dim,
                function_table,
                wL,
                expected_codomain_cross,
                cross_workspace);

            for (std::uint32_t x = 0; x < domain_points; ++x) {
                const std::uint32_t y = function_table[x];
                expected_codomain_preimage[y] = affine::f2::add_mod(
                    expected_codomain_preimage[y],
                    wP[x]);
            }

            for (std::uint32_t x = 0; x < domain_points; ++x) {
                const std::uint32_t y = function_table[x];
                assert(fast.domain_points[x] ==
                    affine::f2::pack_pair_signature(expected_domain_points[x], wQ[y]));
            }
            for (std::uint32_t y = 0; y < codomain_points; ++y) {
                assert(fast.codomain_points[y] ==
                    affine::f2::pack_pair_signature(
                        expected_codomain_points[y],
                        expected_codomain_preimage[y]));
            }
            for (std::uint32_t h = 0; h < domain_hyperplanes; ++h) {
                assert(fast.domain_hyperplanes[h] ==
                    affine::f2::pack_pair_signature(
                        expected_domain_direct[h],
                        expected_domain_cross[h]));
            }
            for (std::uint32_t h = 0; h < codomain_hyperplanes; ++h) {
                assert(fast.codomain_hyperplanes[h] ==
                    affine::f2::pack_pair_signature(
                        expected_codomain_direct[h],
                        expected_codomain_cross[h]));
            }
        }
    }
}

} // namespace

int main()
{
    test_against_separate_computation();
    std::cout << "signatures_f2 tests passed\n";
    return 0;
}
