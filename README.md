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

The domain dimension is inferred from the table length. For a function from
`F_2^n` to `F_2^m` with `m != n`, specify `m`:

```bash
./aleq F.tt G.tt --codomain-dim M
```

Normal output is deliberately short:

```text
equivalent: yes
```

Self-equivalence mode prints `generators: N`, the size of the paired generating
set (which is not necessarily minimal). `--all` prints the number of elements
enumerated. Internal search counters and raw affine-map columns are not printed.

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

In PowerShell, use `.\aleq.cmd` in place of `./aleq`.

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

## Diagnostic Tool

The older `affine_equiv` executable remains available for profiling and
branch-policy experiments. Most users only need `aleq` or the C++ API.
