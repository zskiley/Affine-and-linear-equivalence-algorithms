"""Small SageMath interface to the aleq executable."""

from dataclasses import dataclass
from pathlib import Path
import shutil
import subprocess
import tempfile


_ROOT = Path(__file__).resolve().parent
_BUILD = _ROOT / "build-sage"
_EXECUTABLE = None


@dataclass(frozen=True)
class Equivalence:
    """A paired equivalence represented by Sage affine maps."""

    domain: object
    codomain: object


@dataclass(frozen=True)
class _Map:
    matrix: tuple
    translation: tuple


@dataclass(frozen=True)
class _Pair:
    domain: _Map
    codomain: _Map


def _run(command, error_message):
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, check=False
        )
    except OSError as error:
        raise RuntimeError(f"{error_message}: {error}") from error
    if result.returncode:
        detail = (result.stderr or result.stdout).strip().splitlines()
        raise RuntimeError(
            f"{error_message}: {detail[0] if detail else 'unknown error'}"
        )
    return result.stdout


def _resolve_executable(executable):
    global _EXECUTABLE

    if executable is not None:
        path = Path(executable).expanduser()
        if not path.is_file():
            raise RuntimeError(f"aleq executable not found: {path}")
        return str(path.resolve())
    if _EXECUTABLE is not None:
        return _EXECUTABLE

    cmake = shutil.which("cmake")
    if cmake is None:
        raise RuntimeError("CMake is required to build aleq")
    if not (_BUILD / "CMakeCache.txt").is_file():
        _run(
            [
                cmake,
                "-S",
                str(_ROOT),
                "-B",
                str(_BUILD),
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            "could not configure aleq",
        )
    _run(
        [
            cmake,
            "--build",
            str(_BUILD),
            "--config",
            "Release",
            "--target",
            "aleq",
            "--parallel",
        ],
        "could not build aleq",
    )

    candidates = (
        _BUILD / "aleq",
        _BUILD / "aleq.exe",
        _BUILD / "Release" / "aleq",
        _BUILD / "Release" / "aleq.exe",
    )
    for candidate in candidates:
        if candidate.is_file():
            _EXECUTABLE = str(candidate)
            return _EXECUTABLE
    raise RuntimeError("aleq was built but its executable could not be found")


def _table(table, name):
    try:
        values = tuple(int(value) for value in table)
    except (TypeError, ValueError) as error:
        raise ValueError(f"{name} must contain integers") from error
    if not values or len(values) & (len(values) - 1):
        raise ValueError(f"{name} length must be a power of two")
    dimension = len(values).bit_length() - 1
    if not 1 <= dimension < 32:
        raise ValueError("domain dimension must be between 1 and 31")
    if any(value < 0 for value in values):
        raise ValueError(f"{name} values must be nonnegative")
    return values, dimension


def _codomain_dimension(value, default):
    dimension = default if value is None else int(value)
    if not 1 <= dimension < 32:
        raise ValueError("codomain_dimension must be between 1 and 31")
    return dimension


def _check_values(table, dimension, name):
    if any(value >= 1 << dimension for value in table):
        raise ValueError(f"{name} contains a value outside its codomain")


def _call_aleq(left, right, dimension, linear, executable):
    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        left_path = temporary / "left.tt"
        left_path.write_text(" ".join(map(str, left)), encoding="ascii")
        command = [_resolve_executable(executable), str(left_path)]
        if right is None:
            command.append("--self")
        else:
            right_path = temporary / "right.tt"
            right_path.write_text(" ".join(map(str, right)), encoding="ascii")
            command.append(str(right_path))
        if linear:
            command.append("--linear")
        command += ["--codomain-dim", str(dimension), "--show"]
        return _run(command, "aleq failed")


def _bits(text, length):
    if not text.startswith("[") or not text.endswith("]"):
        raise RuntimeError("unexpected map output from aleq")
    values = text[1:-1].split()
    if len(values) != length or any(value not in ("0", "1") for value in values):
        raise RuntimeError("unexpected map output from aleq")
    return tuple(map(int, values))


def _parse_pair(lines, index, heading, domain_dimension, codomain_dimension):
    if index >= len(lines) or lines[index] != f"{heading}:":
        raise RuntimeError(f"expected '{heading}:' in aleq output")
    index += 1

    def read_map(label, dimension):
        nonlocal index
        if index >= len(lines) or lines[index] != f"{label} matrix:":
            raise RuntimeError(f"expected '{label} matrix:' in aleq output")
        index += 1
        matrix = tuple(_bits(lines[index + row], dimension)
                       for row in range(dimension))
        index += dimension
        prefix = f"{label} translation:"
        if index >= len(lines) or not lines[index].startswith(prefix):
            raise RuntimeError(
                f"expected '{label} translation:' in aleq output"
            )
        translation = _bits(lines[index][len(prefix):].strip(), dimension)
        index += 1
        return _Map(matrix, translation)

    pair = _Pair(
        read_map("domain", domain_dimension),
        read_map("codomain", codomain_dimension),
    )
    return pair, index


def _lines(output):
    return [line.strip() for line in output.splitlines() if line.strip()]


def _parse_witness(output, domain_dimension, codomain_dimension):
    lines = _lines(output)
    if lines == ["equivalent: no"]:
        return None
    if not lines or lines[0] != "equivalent: yes":
        raise RuntimeError("unexpected equivalence result from aleq")
    witness, end = _parse_pair(
        lines, 1, "witness", domain_dimension, codomain_dimension
    )
    if end != len(lines):
        raise RuntimeError("unexpected output after the witness")
    return witness


def _parse_generators(output, domain_dimension, codomain_dimension):
    lines = _lines(output)
    if not lines or not lines[-1].startswith("generators:"):
        raise RuntimeError("unexpected self-equivalence result from aleq")
    try:
        expected = int(lines[-1].split(":", 1)[1])
    except ValueError as error:
        raise RuntimeError("invalid generator count from aleq") from error

    generators = []
    index = 0
    while index < len(lines) - 1:
        generator, index = _parse_pair(
            lines,
            index,
            f"generator {len(generators) + 1}",
            domain_dimension,
            codomain_dimension,
        )
        generators.append(generator)
    if len(generators) != expected:
        raise RuntimeError("generator count does not match aleq output")
    return tuple(generators)


def _find_equivalence_data(
    left_table,
    right_table,
    *,
    linear=False,
    codomain_dimension=None,
    executable=None,
):
    left, domain_dimension = _table(left_table, "left_table")
    right, right_dimension = _table(right_table, "right_table")
    if right_dimension != domain_dimension:
        raise ValueError("left_table and right_table must have equal lengths")
    codomain_dimension = _codomain_dimension(
        codomain_dimension, domain_dimension
    )
    _check_values(left, codomain_dimension, "left_table")
    _check_values(right, codomain_dimension, "right_table")
    output = _call_aleq(
        left, right, codomain_dimension, linear, executable
    )
    return _parse_witness(
        output, domain_dimension, codomain_dimension
    )


def _find_self_generator_data(
    table,
    *,
    linear=False,
    codomain_dimension=None,
    executable=None,
):
    table, domain_dimension = _table(table, "table")
    codomain_dimension = _codomain_dimension(
        codomain_dimension, domain_dimension
    )
    _check_values(table, codomain_dimension, "table")
    output = _call_aleq(
        table, None, codomain_dimension, linear, executable
    )
    return (
        _parse_generators(output, domain_dimension, codomain_dimension),
        domain_dimension,
        codomain_dimension,
    )


def _sage():
    try:
        from sage.all import (
            AffineGroup,
            GF,
            PermutationGroup,
            PermutationGroupElement,
            matrix,
            vector,
        )
    except ImportError as error:
        raise RuntimeError("this function must be run with SageMath") from error
    return (
        AffineGroup,
        GF,
        PermutationGroup,
        PermutationGroupElement,
        matrix,
        vector,
    )


def _affine_map(data, sage):
    AffineGroup, GF, _, _, matrix, vector = sage
    field = GF(2)
    group = AffineGroup(len(data.translation), field)
    return group(
        matrix(field, data.matrix),
        vector(field, data.translation),
    )


def _apply(data, point):
    image = 0
    for row, coefficients in enumerate(data.matrix):
        bit = data.translation[row]
        for column, coefficient in enumerate(coefficients):
            bit ^= coefficient & (point >> column)
        image |= (bit & 1) << row
    return image


def _permutation_images(pair):
    domain_size = 1 << len(pair.domain.translation)
    codomain_size = 1 << len(pair.codomain.translation)
    return tuple(
        [_apply(pair.domain, point) + 1 for point in range(domain_size)]
        + [
            domain_size + _apply(pair.codomain, point) + 1
            for point in range(codomain_size)
        ]
    )


def find_equivalence(
    left_table,
    right_table,
    *,
    linear=False,
    codomain_dimension=None,
    executable=None,
):
    """Return one Sage affine equivalence, or ``None`` if none exists."""
    sage = _sage()
    witness = _find_equivalence_data(
        left_table,
        right_table,
        linear=linear,
        codomain_dimension=codomain_dimension,
        executable=executable,
    )
    if witness is None:
        return None
    return Equivalence(
        _affine_map(witness.domain, sage),
        _affine_map(witness.codomain, sage),
    )


def self_equivalence_group(
    table,
    *,
    linear=False,
    codomain_dimension=None,
    executable=None,
):
    """Return the paired self-equivalences as a Sage permutation group."""
    sage = _sage()
    _, _, PermutationGroup, PermutationGroupElement, _, _ = sage
    generators, domain_dimension, codomain_dimension = (
        _find_self_generator_data(
            table,
            linear=linear,
            codomain_dimension=codomain_dimension,
            executable=executable,
        )
    )
    degree = (1 << domain_dimension) + (1 << codomain_dimension)
    domain = list(range(1, degree + 1))
    permutations = [
        PermutationGroupElement(list(_permutation_images(generator)))
        for generator in generators
    ]
    if not permutations:
        permutations = [PermutationGroupElement(domain)]
    return PermutationGroup(permutations, domain=domain)


__all__ = ["Equivalence", "find_equivalence", "self_equivalence_group"]
