# Affine and linear equivalence algorithms

`aleq` checks affine or linear equivalence between vectorial Boolean functions
given as truth tables. It searches for invertible maps satisfying

$$
A_{out}(F(x)) = G(A_{in}(x)).
$$

In linear mode, both translations are constrained to zero.

## Build

A C++20 compiler and CMake 3.20 or newer are required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The optional install step places `aleq` in the selected prefix:

```sh
cmake --install build --config Release --prefix ./install
```

## Usage

```text
aleq LEFT RIGHT [--type affine|linear] [--codomain-dim M] [--threads auto|N] [--all-solutions]
```

The domain dimension is inferred from the truth-table length. The codomain
dimension defaults to the domain dimension. Table values are unsigned decimal
integers separated by whitespace, commas, or brackets, for example:

```text
[0, 1, 2, 3]
```

Affine equivalence is the default. Use `--type linear` for linear equivalence:

```sh
aleq examples/identity_2.tt examples/translated_identity_2.tt --type affine
aleq examples/identity_2.tt examples/translated_identity_2.tt --type linear
```

The first command reports an affine equivalence; the second reports that the
same functions are not linearly equivalent. The program reports the number of
solutions and prints one witness, or every witness with `--all-solutions`.

`--threads auto` uses the available hardware concurrency. Use `--threads N`
to select a fixed number of workers.

## Sage API

After building `aleq`, import the small Sage wrapper from the repository root:

```python
from aleq_sage import find_equivalence, find_self_equivalences, is_equivalent

identity = [0, 1, 2, 3]
translated = [1, 0, 3, 2]

result = find_equivalence(
    identity,
    translated,
    kind="affine",
    executable="./build/aleq",
)

linear_self_equivalences = find_self_equivalences(
    identity,
    kind="linear",
    executable="./build/aleq",
)
```

`find_equivalence` returns one witness and the total solution count, or `None`
when the functions are not equivalent. `find_equivalences` returns every
witness, and `find_self_equivalences` returns every equivalence of a function
with itself. Use `kind="linear"` for linear equivalence. `is_equivalent` returns
only a Boolean.

The wrapper uses only Sage's Python standard library and calls the same `aleq`
executable as the command line interface.

## C++ API

`src/equivalence_api.hpp` contains the small reusable C++ entry point used by
the command line program. `affine::api::find_equivalences` accepts two truth
tables, dimensions, the affine/linear mode, and a thread count.
