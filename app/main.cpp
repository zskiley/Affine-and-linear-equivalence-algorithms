#include "equivalence_api.hpp"

#include <bit>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct CliOptions {
    std::filesystem::path left_path;
    std::filesystem::path right_path;
    std::optional<std::uint32_t> codomain_dimension;
    std::uint32_t threads = 0;
    affine::api::EquivalenceKind kind = affine::api::EquivalenceKind::Affine;
    bool help = false;
};

void print_usage()
{
    std::cout
        << "Usage: aleq LEFT RIGHT [options]\n\n"
        << "Options:\n"
        << "  --type affine|linear  Equivalence type (default: affine)\n"
        << "  --codomain-dim M      Codomain dimension (default: domain dimension)\n"
        << "  --threads auto|N      Number of worker threads (default: auto)\n"
        << "  -h, --help            Show this help\n";
}

[[nodiscard]] std::uint32_t parse_number(
    std::string_view text,
    std::string_view option,
    bool allow_zero)
{
    std::uint32_t value = 0;
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc {}
        || parsed.ptr != text.data() + text.size()) {
        throw CliError(std::string(option) + " expects an unsigned integer");
    }
    if (!allow_zero && value == 0) {
        throw CliError(std::string(option) + " must be at least 1");
    }
    return value;
}

[[nodiscard]] CliOptions parse_cli(int argc, char** argv)
{
    CliOptions options;
    std::vector<std::filesystem::path> positional;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "-h" || argument == "--help") {
            options.help = true;
        } else if (argument == "--type"
            || argument == "--codomain-dim"
            || argument == "--threads") {
            if (++index >= argc) {
                throw CliError(std::string(argument) + " requires a value");
            }
            const std::string_view value(argv[index]);
            if (argument == "--type") {
                if (value == "affine") {
                    options.kind = affine::api::EquivalenceKind::Affine;
                } else if (value == "linear") {
                    options.kind = affine::api::EquivalenceKind::Linear;
                } else {
                    throw CliError("--type expects 'affine' or 'linear'");
                }
            } else if (argument == "--codomain-dim") {
                options.codomain_dimension =
                    parse_number(value, argument, true);
            } else if (value == "auto") {
                options.threads = 0;
            } else {
                options.threads = parse_number(value, argument, false);
            }
        } else if (!argument.empty() && argument.front() == '-') {
            throw CliError("unknown option: " + std::string(argument));
        } else {
            positional.emplace_back(argument);
        }
    }

    if (!options.help && positional.size() != 2) {
        throw CliError("expected LEFT and RIGHT truth-table paths");
    }
    if (positional.size() == 2) {
        options.left_path = std::move(positional[0]);
        options.right_path = std::move(positional[1]);
    }
    return options;
}

[[nodiscard]] bool is_separator(unsigned char character)
{
    return std::isspace(character) != 0
        || character == '['
        || character == ']'
        || character == ',';
}

[[nodiscard]] std::vector<std::uint32_t> read_truth_table(
    const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) {
        throw CliError("could not open truth table: " + path.string());
    }

    std::string contents {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
    for (char& raw_character : contents) {
        const unsigned char character =
            static_cast<unsigned char>(raw_character);
        if (is_separator(character)) {
            raw_character = ' ';
        } else if (std::isdigit(character) == 0) {
            throw CliError("invalid truth table: " + path.string());
        }
    }

    std::istringstream input(contents);
    std::vector<std::uint32_t> table;
    std::uint64_t value = 0;
    while (input >> value) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            throw CliError("truth-table value is too large: " + path.string());
        }
        table.push_back(static_cast<std::uint32_t>(value));
    }
    if (!input.eof() || table.empty()) {
        throw CliError("invalid truth table: " + path.string());
    }
    return table;
}

[[nodiscard]] std::uint32_t infer_domain_dimension(std::size_t table_size)
{
    if (!std::has_single_bit(table_size)) {
        throw CliError("truth-table length must be a power of two");
    }
    const std::size_t dimension = std::bit_width(table_size) - 1u;
    if (dimension >= 32) {
        throw CliError("domain dimension must be less than 32");
    }
    return static_cast<std::uint32_t>(dimension);
}

void print_map(std::string_view name, const affine::f2::AffineMap& map)
{
    std::cout << name << " translation: " << map.translation << '\n'
              << name << " linear columns:";
    for (const std::uint32_t column : map.basis_images) {
        std::cout << ' ' << column;
    }
    std::cout << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const CliOptions options = parse_cli(argc, argv);
        if (options.help) {
            print_usage();
            return 0;
        }

        const std::vector<std::uint32_t> left =
            read_truth_table(options.left_path);
        const std::vector<std::uint32_t> right =
            read_truth_table(options.right_path);
        if (left.size() != right.size()) {
            throw CliError("truth tables must have the same length");
        }

        const std::uint32_t domain_dimension =
            infer_domain_dimension(left.size());
        const std::vector<affine::Solution> solutions =
            affine::api::find_equivalences(
                affine::api::EquivalenceProblem {
                    .domain_dimension = domain_dimension,
                    .codomain_dimension =
                        options.codomain_dimension.value_or(domain_dimension),
                    .left_table = left,
                    .right_table = right,
                },
                affine::api::SearchOptions {
                    .threads = options.threads,
                    .kind = options.kind,
                });

        std::cout << "equivalent: " << (solutions.empty() ? "no" : "yes")
                  << '\n'
                  << "solutions: " << solutions.size() << '\n';
        if (!solutions.empty()) {
            print_map("domain map", solutions.front().domain_map);
            print_map("codomain map", solutions.front().codomain_map);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
