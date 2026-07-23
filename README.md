# Affine and Linear Equivalence Algorithms

A C++20 implementation of affine and linear equivalence algorithms for
vectorial Boolean functions from `F_2^n` to `F_2^m`.

## Getting Started from Git

Install Git, CMake 3.20 or newer, and a C++20 compiler. SageMath is only
required for the Sage interface.

### Windows PowerShell

Clone the repository, enter it, and build:

```powershell
git clone https://github.com/zskiley/Affine-and-linear-equivalence-algorithms.git
cd Affine-and-linear-equivalence-algorithms
.\aleq.cmd
```

Then run an included example:

```powershell
.\aleq.cmd examples\identity_3.tt examples\affine_translate_3.tt
```

### macOS and Linux

Clone the repository, enter it, and build:

```bash
git clone https://github.com/zskiley/Affine-and-linear-equivalence-algorithms.git
cd Affine-and-linear-equivalence-algorithms
./aleq
```

Then run an included example:

```bash
./aleq examples/identity_3.tt examples/affine_translate_3.tt
```

There is no separate installation step. Both launchers configure and rebuild
the program automatically when needed. To update an existing clone, run:

```bash
git pull
```

To build and run all included checks, use:

```bash
cmake -P run_examples.cmake
```

### SageMath

The Sage interface does not need a separate build command:

```bash
git clone https://github.com/zskiley/Affine-and-linear-equivalence-algorithms.git
cd Affine-and-linear-equivalence-algorithms
sage sage_example.sage
```

The first Sage call builds `aleq` automatically. CMake and the C++ compiler
must be available in the same environment as Sage. On Windows, run this inside
the environment where Sage is installed, normally WSL.

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

## SageMath

The Sage interface accepts truth tables directly as lists. It builds `aleq`
automatically on the first call:

```python
from f2_equivalence import find_equivalence, self_equivalence_group

F = [0, 1, 2, 3]
G = [1, 0, 3, 2]

witness = find_equivalence(F, G)
print(witness.domain)
print(witness.codomain)

H = self_equivalence_group(F)
print(H.order())
print(H.gens())
```

`find_equivalence` returns `None` when the functions are not equivalent.
Otherwise, its `domain` and `codomain` fields are Sage affine maps.
`self_equivalence_group` returns an actual Sage permutation group. Each group
generator represents a paired domain and codomain map, so the pairing is not
lost. In this permutation action, labels `1, ..., 2^n` are domain points and
the following `2^m` labels are codomain points.

Use `linear=True` for linear equivalence. For functions from `F_2^n` to
`F_2^m` with `m != n`, pass `codomain_dimension=m`.

Run the included example from the repository directory:

```bash
sage sage_example.sage
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
