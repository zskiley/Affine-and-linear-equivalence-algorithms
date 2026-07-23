#include "equivalence_api.hpp"

#include <bit>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct CliOptions {
    std::vector<std::filesystem::path> input_paths;
    std::optional<std::uint32_t> codomain_dimension;
    bool self = false;
    bool linear = false;
    bool all = false;
    bool show = false;
    bool help = false;
};

void print_usage(std::ostream& output)
{
    output
        << "Usage:\n"
        << "  aleq F.tt G.tt [--linear] [--all] [--show]"
           " [--codomain-dim M]\n"
        << "  aleq F.tt --self [--linear] [--all] [--show]"
           " [--codomain-dim M]\n\n"
        << "Options:\n"
        << "  --self              compute self-equivalences of F\n"
        << "  --linear            use linear instead of affine equivalence\n"
        << "  --all               enumerate and count every equivalence\n"
        << "  --show              print the selected maps\n"
        << "  --codomain-dim M    use functions from F_2^n to F_2^M\n"
        << "  --help               show this help text\n";
}

[[nodiscard]] std::uint32_t parse_dimension(std::string_view text)
{
    std::uint32_t value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const std::from_chars_result parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc {} || parsed.ptr != end
        || value == 0 || value >= 32) {
        throw std::runtime_error(
            "codomain dimension must be an integer between 1 and 31");
    }
    return value;
}

[[nodiscard]] CliOptions parse_args(int argc, char** argv)
{
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--self") {
            options.self = true;
        } else if (arg == "--linear") {
            options.linear = true;
        } else if (arg == "--all") {
            options.all = true;
        } else if (arg == "--show") {
            options.show = true;
        } else if (arg == "--codomain-dim") {
            if (++index >= argc) {
                throw std::runtime_error("--codomain-dim requires a value");
            }
            options.codomain_dimension = parse_dimension(argv[index]);
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("unknown option: " + std::string(arg));
        } else {
            options.input_paths.emplace_back(arg);
        }
    }

    if (options.help) {
        return options;
    }
    const std::size_t required_paths = options.self ? 1u : 2u;
    if (options.input_paths.size() != required_paths) {
        throw std::runtime_error(
            options.self
                ? "self-equivalence requires exactly one truth table"
                : "equivalence requires exactly two truth tables");
    }
    return options;
}

[[nodiscard]] std::vector<std::uint32_t> read_truth_table(
    const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open " + path.string());
    }

    std::vector<std::uint32_t> table;
    std::uint64_t value = 0;
    bool in_number = false;
    char ch = 0;
    while (input.get(ch)) {
        const unsigned char unsigned_ch = static_cast<unsigned char>(ch);
        if (std::isdigit(unsigned_ch)) {
            const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
            const std::uint64_t maximum =
                std::numeric_limits<std::uint32_t>::max();
            if (value > (maximum - digit) / 10u) {
                throw std::runtime_error(
                    "truth-table value is too large in " + path.string());
            }
            value = value * 10u + digit;
            in_number = true;
        } else if (ch == '-') {
            throw std::runtime_error(
                "truth-table values must be nonnegative in " + path.string());
        } else if (in_number) {
            table.push_back(static_cast<std::uint32_t>(value));
            value = 0;
            in_number = false;
        }
    }
    if (in_number) {
        table.push_back(static_cast<std::uint32_t>(value));
    }
    if (table.empty()) {
        throw std::runtime_error("truth table is empty: " + path.string());
    }
    return table;
}

[[nodiscard]] std::uint32_t infer_domain_dimension(std::size_t table_size)
{
    if (!std::has_single_bit(table_size)) {
        throw std::runtime_error("truth-table length must be a power of two");
    }
    const std::uint32_t dimension =
        static_cast<std::uint32_t>(std::bit_width(table_size) - 1u);
    if (dimension == 0 || dimension >= 32) {
        throw std::runtime_error("domain dimension must be between 1 and 31");
    }
    return dimension;
}

void print_affine_map(
    std::string_view label,
    const affine::f2::AffineMap& map);

[[nodiscard]] std::size_t count_all(
    affine::api::EquivalenceGenerator& generator,
    bool show,
    std::string_view label)
{
    std::size_t count = 0;
    while (std::optional<affine::Solution> equivalence = generator.next()) {
        ++count;
        if (show) {
            std::cout << label << ' ' << count << ":\n";
            print_affine_map("domain", equivalence->domain_map);
            print_affine_map("codomain", equivalence->codomain_map);
        }
    }
    return count;
}

void print_binary_vector(std::uint32_t value, std::uint32_t dimension)
{
    std::cout << '[';
    for (std::uint32_t bit = 0; bit < dimension; ++bit) {
        if (bit != 0) {
            std::cout << ' ';
        }
        std::cout << ((value >> bit) & 1u);
    }
    std::cout << "]\n";
}

void print_affine_map(
    std::string_view label,
    const affine::f2::AffineMap& map)
{
    std::cout << "  " << label << " matrix:\n";
    for (std::uint32_t row = 0; row < map.dim; ++row) {
        std::cout << "    [";
        for (std::uint32_t column = 0; column < map.dim; ++column) {
            if (column != 0) {
                std::cout << ' ';
            }
            std::cout << ((map.basis_images[column] >> row) & 1u);
        }
        std::cout << "]\n";
    }
    std::cout << "  " << label << " translation: ";
    print_binary_vector(map.translation, map.dim);
}

void print_paired_map(
    std::string_view heading,
    const affine::group::PairedAffineMap& map)
{
    std::cout << heading << ":\n";
    print_affine_map("domain", map.domain_map);
    print_affine_map("codomain", map.codomain_map);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const CliOptions options = parse_args(argc, argv);
        if (options.help) {
            print_usage(std::cout);
            return 0;
        }

        const std::vector<std::uint32_t> left =
            read_truth_table(options.input_paths.front());
        const std::uint32_t domain_dimension =
            infer_domain_dimension(left.size());
        const std::uint32_t codomain_dimension =
            options.codomain_dimension.value_or(domain_dimension);
        const affine::api::SearchOptions search_options {
            .mode = options.linear
                ? affine::EquivalenceMode::Linear
                : affine::EquivalenceMode::Affine,
        };

        if (options.self) {
            affine::api::EquivalenceGenerator generator =
                affine::api::generate_self_equivalences(
                    domain_dimension,
                    codomain_dimension,
                    left,
                    search_options);
            if (options.all) {
                const std::size_t count = count_all(
                    generator,
                    options.show,
                    "self-equivalence");
                std::cout << "self-equivalences: " << count << '\n';
            } else {
                if (options.show) {
                    std::size_t index = 0;
                    for (const affine::group::PairedAffineMap& group_generator :
                         generator.paired_group_generators()) {
                        ++index;
                        print_paired_map(
                            "generator " + std::to_string(index),
                            group_generator);
                    }
                }
                std::cout << "generators: "
                          << generator.paired_group_generators().size()
                          << '\n';
            }
            return 0;
        }

        const std::vector<std::uint32_t> right =
            read_truth_table(options.input_paths.back());
        affine::api::EquivalenceGenerator generator =
            affine::api::generate_equivalences(
                domain_dimension,
                codomain_dimension,
                left,
                right,
                search_options);
        if (options.all) {
            const std::size_t count =
                count_all(generator, options.show, "equivalence");
            std::cout << "equivalences: " << count << '\n';
        } else {
            const affine::group::PairedAffineMap* witness = generator.witness();
            std::cout << "equivalent: "
                      << (witness == nullptr ? "no" : "yes")
                      << '\n';
            if (options.show && witness != nullptr) {
                print_paired_map("witness", *witness);
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        std::cerr << "use --help for usage\n";
        return 1;
    }
}
