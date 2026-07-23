# Affine and Linear Equivalence Algorithms

A C++20 implementation of affine and linear equivalence algorithms for
vectorial Boolean functions from `F_2^n` to `F_2^m`.

## Build

The only requirements are CMake and a C++20 compiler. Build the program with
one command:

```powershell
.\aleq.cmd
```

or on macOS and Linux:

```bash
./aleq
```

To build and run all included checks instead, use:

```bash
cmake -P run_examples.cmake
```

The launchers rebuild automatically when the source changes and pass any
arguments to the `aleq` program.

## Command-Line Usage

All commands below use the public `aleq` interface. On Windows PowerShell,
replace `./aleq` with `.\aleq.cmd`.

Test affine equivalence:

```bash
./aleq F.tt G.tt
```

Test linear equivalence:

```bash
./aleq F.tt G.tt --linear
```

Compute the number of paired generators for the self-equivalence group:

```bash
./aleq F.tt --self
```

Enumerate and count every self-equivalence or equivalence:

```bash
./aleq F.tt --self --all
./aleq F.tt G.tt --all
```

Print the actual maps:

```bash
./aleq F.tt G.tt --show
./aleq F.tt --self --show
./aleq F.tt G.tt --all --show
```

Without `--all`, `--show` prints one witness for an equivalence test or the
paired generators for a self-equivalence search. Together, `--all --show`
prints every equivalence as it is generated.

The domain dimension is inferred from the table length. For a function from
`F_2^n` to `F_2^m` with `m != n`, specify `m`:

```bash
./aleq F.tt G.tt --codomain-dim M
```

Normal output is deliberately short. An equivalence test prints exactly one
of these lines:

```text
equivalent: yes
equivalent: no
```

Self-equivalence mode prints:

```text
generators: N
```

The number is the size of the paired generating set, which is not necessarily
minimal. With `--all`, the program prints only the number of elements it
enumerated:

```text
self-equivalences: N
equivalences: N
```

`aleq` never prints internal timings, search-node counts, branch policies, or
raw affine-map columns.

When `--show` is present, each affine map is displayed as a binary matrix and
a binary translation vector. Coordinates are listed from lowest-index bit to
highest-index bit. A displayed map acts as `x -> Mx + t` over `F_2`. The
domain map `A` and codomain map `B` satisfy
`B(F(x)) = G(A(x))`.

## Input Format

A truth-table file contains whitespace- or comma-separated nonnegative
integers. A function from `F_2^n` to `F_2^m` requires exactly `2^n` values,
each in `[0, 2^m)`.

The identity function on `F_2^3` is:

```text
0 1 2 3 4 5 6 7
```

Files in `examples/` can be used immediately, for example:

```bash
./aleq examples/identity_3.tt examples/affine_translate_3.tt
./aleq examples/identity_3.tt --self --all
```

## C++ API

Include `equivalence_api.hpp`. The two public functions return an
`EquivalenceGenerator`:

```cpp
auto self = affine::api::generate_self_equivalences(function_table);
auto between = affine::api::generate_equivalences(left_table, right_table);
```

The compact representation is available without enumerating the group:

```cpp
const auto* witness = between.witness();
auto generators = between.paired_group_generators();
```

`witness` is null when the functions are not equivalent. Otherwise, the
witness together with the paired generators represents every equivalence.
Call `next()` to obtain them one at a time:

```cpp
while (auto equivalence = between.next()) {
    // equivalence->domain_map and equivalence->codomain_map
}
```

For distinct dimensions, pass both explicitly:

```cpp
auto self = affine::api::generate_self_equivalences(n, m, function_table);
auto between = affine::api::generate_equivalences(
    n, m, left_table, right_table);
```

Affine mode is the default. Select linear mode with:

```cpp
affine::api::SearchOptions {
    .mode = affine::EquivalenceMode::Linear,
}
```

See `examples/rectangular_api.cpp` for a complete small example.
