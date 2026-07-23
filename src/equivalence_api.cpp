#include "equivalence_api.hpp"

#include "dfs.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace affine::api {

namespace {

[[nodiscard]] std::uint32_t infer_dimension(
    std::span<const std::uint32_t> table,
    std::string_view name)
{
    if (table.empty() || !std::has_single_bit(table.size())) {
        throw std::invalid_argument(
            std::string(name) + " length must be a power of two");
    }

    const std::uint32_t dimension =
        static_cast<std::uint32_t>(std::bit_width(table.size()) - 1u);
    if (dimension >= 32) {
        throw std::invalid_argument("dimensions must be less than 32");
    }

    return dimension;
}

void validate_table(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> table,
    std::string_view name)
{
    if (domain_dimension == 0 || codomain_dimension == 0
        || domain_dimension >= 32 || codomain_dimension >= 32) {
        throw std::invalid_argument("dimensions must be between 1 and 31");
    }

    const std::uint64_t expected_size =
        std::uint64_t { 1 } << domain_dimension;
    if (table.size() != expected_size) {
        throw std::invalid_argument(
            std::string(name) + " must contain 2^domain_dimension values");
    }

    const std::uint64_t value_count =
        std::uint64_t { 1 } << codomain_dimension;
    if (std::any_of(table.begin(), table.end(), [value_count](std::uint32_t value) {
            return value >= value_count;
        })) {
        throw std::invalid_argument(
            std::string(name) + " contains a value outside its codomain");
    }
}

void run_search(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    EquivalenceMode mode,
    bool stop_after_first_solution,
    SolutionStore& solutions)
{
    const DfsProblem problem {
        .domain_dim = domain_dimension,
        .codomain_dim = codomain_dimension,
        .left_function_table = left_function_table,
        .right_function_table = right_function_table,
    };
    SearchTask task = make_search_task(
        problem,
        DfsOptions {
            .branch_policy = BranchPolicy::HyperplanesOnly,
            .stop_after_first_leaf = stop_after_first_solution,
            .mode = mode,
        });
    dfs(problem, task, &solutions);
}

void find_self_group(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> function_table,
    EquivalenceMode mode,
    SolutionStore& solutions)
{
    run_search(
        domain_dimension,
        codomain_dimension,
        function_table,
        function_table,
        mode,
        false,
        solutions);
}

[[nodiscard]] std::vector<group::PairedAffineMap> paired_generators(
    const SolutionStore& solutions)
{
    const group::PairedAffineGroupSnapshot snapshot =
        solutions.paired_group().snapshot();
    std::vector<group::PairedAffineMap> result;
    result.reserve(snapshot.generators.size());
    for (const group::PairedAffineGroupGenerator& generator :
         snapshot.generators) {
        result.push_back(group::generator_map(generator));
    }
    return result;
}

void seed_paired_group(
    const SolutionStore& source,
    SolutionStore& destination)
{
    for (const group::PairedAffineGroupGenerator& generator :
         source.paired_group().snapshot().generators) {
        (void)destination.paired_group().add_generator(
            generator.domain.map,
            generator.codomain.map);
    }
}

[[nodiscard]] group::PairedAffineMap as_paired_map(const Solution& solution)
{
    return group::PairedAffineMap {
        .domain_map = solution.domain_map,
        .codomain_map = solution.codomain_map,
    };
}

} // namespace

EquivalenceGenerator::EquivalenceGenerator(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::vector<group::PairedAffineMap> generators,
    group::PairedAffineMap representative)
    : generators_(std::move(generators))
    , representative_(std::move(representative))
{
    group::PairedAffineMap identity =
        group::identity_paired_affine_map(
            domain_dimension,
            codomain_dimension);
    seen_.insert(group::make_paired_group_key(identity));
    pending_.push_back(std::move(identity));
}

std::span<const group::PairedAffineMap>
EquivalenceGenerator::paired_group_generators() const & noexcept
{
    return generators_;
}

const group::PairedAffineMap* EquivalenceGenerator::witness() const & noexcept
{
    return representative_.has_value() ? &*representative_ : nullptr;
}

std::optional<Solution> EquivalenceGenerator::next()
{
    if (!representative_.has_value() || pending_.empty()) {
        return std::nullopt;
    }

    group::PairedAffineMap current = std::move(pending_.front());
    pending_.pop_front();

    for (const group::PairedAffineMap& generator : generators_) {
        group::PairedAffineMap candidate =
            group::compose_paired_affine_maps(generator, current);
        group::PairedAffineGroupKey key =
            group::make_paired_group_key(candidate);
        if (seen_.insert(std::move(key)).second) {
            pending_.push_back(std::move(candidate));
        }
    }

    group::PairedAffineMap result =
        group::compose_paired_affine_maps(current, *representative_);
    return Solution {
        .domain_map = std::move(result.domain_map),
        .codomain_map = std::move(result.codomain_map),
        .path = {},
    };
}

EquivalenceGenerator generate_self_equivalences(
    std::span<const std::uint32_t> function_table,
    SearchOptions options)
{
    const std::uint32_t dimension =
        infer_dimension(function_table, "function table");
    return generate_self_equivalences(
        dimension,
        dimension,
        function_table,
        options);
}

EquivalenceGenerator generate_self_equivalences(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> function_table,
    SearchOptions options)
{
    validate_table(
        domain_dimension,
        codomain_dimension,
        function_table,
        "function table");
    SolutionStore self_group(
        SolutionStore::Options {
            .record_a1_automorphisms = true,
        });
    find_self_group(
        domain_dimension,
        codomain_dimension,
        function_table,
        options.mode,
        self_group);
    return EquivalenceGenerator(
        domain_dimension,
        codomain_dimension,
        paired_generators(self_group),
        group::identity_paired_affine_map(
            domain_dimension,
            codomain_dimension));
}

EquivalenceGenerator generate_equivalences(
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    SearchOptions options)
{
    const std::uint32_t dimension =
        infer_dimension(left_function_table, "left function table");
    const std::uint32_t right_dimension =
        infer_dimension(right_function_table, "right function table");
    if (right_dimension != dimension) {
        throw std::invalid_argument(
            "left and right function tables have different lengths");
    }
    return generate_equivalences(
        dimension,
        dimension,
        left_function_table,
        right_function_table,
        options);
}

EquivalenceGenerator generate_equivalences(
    std::uint32_t domain_dimension,
    std::uint32_t codomain_dimension,
    std::span<const std::uint32_t> left_function_table,
    std::span<const std::uint32_t> right_function_table,
    SearchOptions options)
{
    validate_table(
        domain_dimension,
        codomain_dimension,
        left_function_table,
        "left function table");
    validate_table(
        domain_dimension,
        codomain_dimension,
        right_function_table,
        "right function table");

    SolutionStore target_self_group(
        SolutionStore::Options {
            .record_a1_automorphisms = true,
        });
    find_self_group(
        domain_dimension,
        codomain_dimension,
        right_function_table,
        options.mode,
        target_self_group);
    std::vector<group::PairedAffineMap> generators =
        paired_generators(target_self_group);

    if (std::equal(
            left_function_table.begin(),
            left_function_table.end(),
            right_function_table.begin())) {
        return EquivalenceGenerator(
            domain_dimension,
            codomain_dimension,
            std::move(generators),
            group::identity_paired_affine_map(
                domain_dimension,
                codomain_dimension));
    }

    SolutionStore witnesses;
    seed_paired_group(target_self_group, witnesses);
    run_search(
        domain_dimension,
        codomain_dimension,
        left_function_table,
        right_function_table,
        options.mode,
        true,
        witnesses);

    const std::optional<Solution> witness = witnesses.first_solution();
    if (!witness.has_value()) {
        return {};
    }
    return EquivalenceGenerator(
        domain_dimension,
        codomain_dimension,
        std::move(generators),
        as_paired_map(*witness));
}

} // namespace affine::api
