# Affine and linear equivalence algorithms

`aleq` checks affine or linear equivalence between vectorial Boolean functions
given as truth tables. It searches for invertible maps satisfying

$$
A_{out}(F(x)) = G(A_{in}(x)).
$$

In linear mode, both translations are constrained to zero.

## Quick start

Install Git, CMake 3.20 or newer, and a C++20 compiler. Then:

```sh
git clone https://github.com/zskiley/Affine-and-linear-equivalence-algorithms.git
cd Affine-and-linear-equivalence-algorithms
./aleq
```

On Windows, use:

```powershell
git clone https://github.com/zskiley/Affine-and-linear-equivalence-algorithms.git
cd Affine-and-linear-equivalence-algorithms
.\aleq.cmd
```

This builds the program. Pass two truth tables to run it:

```sh
./aleq examples/identity_2.tt examples/translated_identity_2.tt
```

On Windows, use `.\aleq.cmd` instead of `./aleq`.

## Command line

```text
aleq LEFT RIGHT [--type affine|linear] [--codomain-dim M] [--threads auto|N] [--all-solutions]
aleq TABLE --self [--generators|--all-solutions] [--type affine|linear] [--threads auto|N]
```

The domain dimension is inferred from the truth-table length. The codomain
dimension defaults to the domain dimension. Table values are unsigned decimal
integers separated by whitespace, commas, or brackets, for example:

```text
[0, 1, 2, 3]
```

Affine equivalence is the default. Use `--type linear` for linear equivalence:

```sh
aleq examples/identity_2.tt examples/translated_identity_2.tt
aleq examples/identity_2.tt examples/translated_identity_2.tt --type linear
```

The first command reports an affine equivalence; the second reports that the
same functions are not linearly equivalent. The program reports the number of
solutions and prints one witness, or every witness with `--all-solutions`.

For self-equivalences, pass one table with `--self`:

```sh
aleq examples/identity_2.tt --self
aleq examples/identity_2.tt --self --all-solutions
```

The first command prints a set of generators for the self-equivalence group.
The second prints every group element. `--generators` may be added explicitly,
but it is the default with `--self`.

`--threads auto` uses the available hardware concurrency. Use `--threads N`
to select a fixed number of workers.

## Manual build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Sage API

From the repository root, start Sage and import the wrapper:

```python
from f2_equivalence import find_equivalence, self_equivalence_group

identity = [0, 1, 2, 3]
translated = [1, 0, 3, 2]

result = find_equivalence(identity, translated)
group = self_equivalence_group(identity, kind="linear")
group.order()  # 6
group.gens()
```

`find_equivalence` returns one witness and the total solution count, or `None`
when the functions are not equivalent. `equivalences` returns every witness,
and `self_equivalences` returns every equivalence of a function with itself.
`self_equivalence_group` returns an actual Sage permutation group built from
the C++ generator set; `automorphism_group` is a shorter alias. Use
`kind="linear"` for linear equivalence. `is_equivalent` returns only a Boolean.

The group acts on the domain points followed by the codomain points. Sage label
`x + 1` represents domain point `x`; the codomain labels follow the domain
block.

The first call finds an installed or previously built `aleq`; if necessary, it
builds the executable automatically with CMake. Pass `executable="..."` only to
select a custom binary.

A complete runnable example is in `sage_example.sage`:

```sh
sage sage_example.sage
```

## C++ API

`src/equivalence_api.hpp` contains the small reusable C++ entry point used by
the command line program. `affine::api::find_equivalences` accepts two truth
tables, dimensions, the affine/linear mode, and a thread count.

To get generators for a self-equivalence group:

```cpp
const std::vector<std::uint32_t> table { 0, 1, 2, 3 };
const affine::api::EquivalenceProblem problem {
    .domain_dimension = 2,
    .codomain_dimension = 2,
    .left_table = table,
    .right_table = table,
};

const affine::api::SelfEquivalenceGroup group =
    affine::api::find_self_equivalence_group(problem);

// group.order contains the number of self-equivalences.
// group.generators contains a generating set of map pairs.
```

Use `find_equivalences(problem)` instead when every self-equivalence is needed.
Set `SearchOptions::kind` to `EquivalenceKind::Linear` for the linear group.
