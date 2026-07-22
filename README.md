# Affine and Linear Equivalence Algorithms

This repository contains a C++20 implementation of partition-refinement and
depth-first-search algorithms for linear and affine equivalence of vectorial
Boolean functions over `F_2^n`.

The submission entry point is the `affine_equiv` command-line tool.

## Quick Start

The code has no external library dependency beyond a C++20 compiler and CMake.
Run the included script to build the tool and check the examples:

```bash
cmake -P run_examples.cmake
```

The important output line for equivalence tests is:

```text
found=1
```

which means that an equivalence was found.

## Input Format

A function is given by its truth table as whitespace- or comma-separated
nonnegative integers. For an `n -> n` function, the file must contain exactly
`2^n` values, each in `[0, 2^n)`.

Example, the identity function on `F_2^3`:

```text
0 1 2 3 4 5 6 7
```

## Commands

Test linear equivalence:

```bash
./build/affine_equiv equiv --mode linear \
  --left F.tt \
  --right G.tt
```

Test affine equivalence:

```bash
./build/affine_equiv equiv --mode affine \
  --left F.tt \
  --right G.tt
```

Compute generators for the affine self-equivalence group of one function:

```bash
./build/affine_equiv self --mode affine --function F.tt
```

Run seeded equivalence: first compute the target function's self-equivalence
group, then use its paired `(A1,A2)` generators for orbit pruning in the
equivalence search.

```bash
./build/affine_equiv seeded-equiv --mode affine \
  --left examples/identity_3.tt \
  --right examples/affine_translate_3.tt
```

## Options

`--mode linear|affine`
: Selects linear or affine equivalence. Linear mode fixes the zero point.

`--branch POLICY`
: Selects the branching policy. The default is `hyperplanes_only`. Available
  policies are `hyperplanes_only`, `hyperplanes_first`, `smallest_any`,
  `domain_point_first`, `codomain_point_first`,
  `domain_hyperplane_first`, and `codomain_hyperplane_first`.

`--profile`
: Prints selected profiling counters after the run.

## Output

The tool prints key-value lines intended to be easy to parse:

```text
task=equiv
mode=linear
branch_policy=hyperplanes_only
dimension=3
found=1
elapsed_ms=...
nodes=...
solutions=1
a1_generators=...
a2_generators=...
```

For self-equivalence runs, `solutions` is the number of verified
self-equivalences found while discovering the group. Because the search uses
orbit pruning after new automorphisms are found, this is not necessarily the
full group order. `a1_generators` and `a2_generators` are the number of stored
generators in the discovered domain and codomain automorphism groups.

Orbit pruning is performed with paired self-equivalence generators. When the
search path has fixed both domain-side and codomain-side right objects, the
algorithm stabilizes the paired group against all of those fixed objects before
projecting to the current branch side.
