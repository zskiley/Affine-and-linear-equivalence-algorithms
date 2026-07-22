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
aleq LEFT RIGHT [--type affine|linear] [--codomain-dim M] [--threads auto|N]
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
solutions and prints one witness.

`--threads auto` uses the available hardware concurrency. Use `--threads N`
to select a fixed number of workers.

## C++ and future Sage API

`src/equivalence_api.hpp` contains the small reusable C++ entry point used by
the command line program. `affine::api::find_equivalences` accepts two truth
tables, dimensions, the affine/linear mode, and a thread count.

A future Sage binding can wrap this function directly; no separate search
implementation is needed.
