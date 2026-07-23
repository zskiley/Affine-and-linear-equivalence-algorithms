#include "equivalence_api.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    // Two affine-equivalent functions from F_2^1 to F_2^2.
    const std::vector<std::uint32_t> left { 0, 1 };
    const std::vector<std::uint32_t> right { 0, 2 };

    auto equivalences = affine::api::generate_equivalences(
        1,
        2,
        left,
        right);

    std::size_t count = 0;
    while (const auto equivalence = equivalences.next()) {
        if (count == 0) {
            std::cout << "domain dimension: "
                      << equivalence->domain_map.dim << '\n';
            std::cout << "codomain dimension: "
                      << equivalence->codomain_map.dim << '\n';
        }
        ++count;
    }
    std::cout << "equivalences: " << count << '\n';
}
