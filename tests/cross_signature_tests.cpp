#include "../src/cross_signature.hpp"

#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

int parity(std::uint32_t value)
{
    return std::popcount(value) & 1;
}

void test_against_direct_formulas()
{
    for (std::uint32_t domain_dim = 1; domain_dim <= 5; ++domain_dim) {
        for (std::uint32_t codomain_dim = 1; codomain_dim <= 5; ++codomain_dim) {
            const std::uint32_t domain_points = affine::f2::point_count(domain_dim);
            const std::uint32_t codomain_points = affine::f2::point_count(codomain_dim);
            const std::uint32_t domain_hyperplanes = affine::f2::hyperplane_count(domain_dim);
            const std::uint32_t codomain_hyperplanes = affine::f2::hyperplane_count(codomain_dim);

            std::vector<std::uint32_t> function_table(domain_points);
            for (std::uint32_t x = 0; x < domain_points; ++x) {
                function_table[x] = ((x * 13u) ^ (x >> 1u) ^ 7u) & (codomain_points - 1u);
            }

            affine::f2::CrossSignatureWorkspace workspace;

            std::vector<affine::f2::Weight> codomain_hyperplane_weights(codomain_hyperplanes);
            for (std::uint32_t h = 0; h < codomain_hyperplanes; ++h) {
                codomain_hyperplane_weights[h] = (h * 17u + 5u) % 11u;
            }

            std::vector<affine::f2::Weight> fast_domain(domain_hyperplanes);
            std::vector<affine::f2::Weight> slow_domain(domain_hyperplanes);

            affine::f2::domain_hyperplane_cross_signature(
                domain_dim,
                codomain_dim,
                function_table,
                codomain_hyperplane_weights,
                fast_domain,
                workspace);

            for (std::uint32_t h = 0; h < domain_hyperplanes; ++h) {
                const std::uint32_t a = affine::f2::hyperplane_normal(h);
                const std::uint32_t b = affine::f2::hyperplane_offset(h);
                affine::f2::Weight sum = 0;

                for (std::uint32_t x = 0; x < domain_points; ++x) {
                    if (parity(a & x) != static_cast<int>(b)) {
                        continue;
                    }

                    const std::uint32_t y = function_table[x];
                    for (std::uint32_t r = 0; r < codomain_hyperplanes; ++r) {
                        const std::uint32_t c = affine::f2::hyperplane_normal(r);
                        const std::uint32_t d = affine::f2::hyperplane_offset(r);
                        if (parity(c & y) == static_cast<int>(d)) {
                            sum += codomain_hyperplane_weights[r];
                        }
                    }
                }

                slow_domain[h] = sum;
            }

            assert(fast_domain == slow_domain);

            std::vector<affine::f2::Weight> domain_hyperplane_weights(domain_hyperplanes);
            for (std::uint32_t h = 0; h < domain_hyperplanes; ++h) {
                domain_hyperplane_weights[h] = (h * 31u + 3u) % 13u;
            }

            std::vector<affine::f2::Weight> fast_codomain(codomain_hyperplanes);
            std::vector<affine::f2::Weight> slow_codomain(codomain_hyperplanes);

            affine::f2::codomain_hyperplane_cross_signature(
                domain_dim,
                codomain_dim,
                function_table,
                domain_hyperplane_weights,
                fast_codomain,
                workspace);

            for (std::uint32_t v = 0; v < codomain_hyperplanes; ++v) {
                const std::uint32_t c = affine::f2::hyperplane_normal(v);
                const std::uint32_t d = affine::f2::hyperplane_offset(v);
                affine::f2::Weight sum = 0;

                for (std::uint32_t x = 0; x < domain_points; ++x) {
                    const std::uint32_t y = function_table[x];
                    if (parity(c & y) != static_cast<int>(d)) {
                        continue;
                    }

                    for (std::uint32_t h = 0; h < domain_hyperplanes; ++h) {
                        const std::uint32_t a = affine::f2::hyperplane_normal(h);
                        const std::uint32_t b = affine::f2::hyperplane_offset(h);
                        if (parity(a & x) == static_cast<int>(b)) {
                            sum += domain_hyperplane_weights[h];
                        }
                    }
                }

                slow_codomain[v] = sum;
            }

            assert(fast_codomain == slow_codomain);
        }
    }
}

} // namespace

int main()
{
    test_against_direct_formulas();
    std::cout << "cross signature tests passed\n";
    return 0;
}
