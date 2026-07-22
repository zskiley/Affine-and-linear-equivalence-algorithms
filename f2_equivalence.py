"""Sage interface for affine and linear equivalence over F2."""

from pathlib import Path
import shutil
import subprocess
import tempfile


_ROOT = Path(__file__).resolve().parent


def _resolve_executable(executable):
    if executable is not None:
        return str(executable)

    candidates = [
        _ROOT / "build" / "aleq",
        _ROOT / "build" / "aleq.exe",
        _ROOT / "build" / "Release" / "aleq",
        _ROOT / "build" / "Release" / "aleq.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)

    installed = shutil.which("aleq")
    if installed is not None:
        return installed

    try:
        subprocess.run(
            [
                "cmake",
                "-S",
                str(_ROOT),
                "-B",
                str(_ROOT / "build"),
                "-DBUILD_TESTING=OFF",
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            [
                "cmake",
                "--build",
                str(_ROOT / "build"),
                "--config",
                "Release",
                "--target",
                "aleq",
                "--parallel",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as error:
        raise RuntimeError("CMake is required to build aleq") from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or "").strip()
        raise RuntimeError("could not build aleq: " + detail) from error

    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    raise RuntimeError("aleq was built but its executable could not be found")


def _read_map(values, name):
    return {
        "translation": int(values[f"{name} translation"]),
        "linear_columns": [
            int(column) for column in values[f"{name} linear columns"].split()
        ],
    }


def _read_solution(values, prefix=""):
    return {
        "domain_map": _read_map(values, prefix + "domain map"),
        "codomain_map": _read_map(values, prefix + "codomain map"),
    }


def _run(
    left_table,
    right_table,
    kind,
    codomain_dimension,
    threads,
    executable,
    all_solutions,
    self_equivalence=False,
    generators=False,
):
    if kind not in ("affine", "linear"):
        raise ValueError("kind must be 'affine' or 'linear'")

    left = [int(value) for value in left_table]
    right = [int(value) for value in right_table]

    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        left_path = directory / "left.tt"
        right_path = directory / "right.tt"
        left_path.write_text(" ".join(map(str, left)), encoding="ascii")
        right_path.write_text(" ".join(map(str, right)), encoding="ascii")

        command = [_resolve_executable(executable), str(left_path)]
        if self_equivalence:
            command.append("--self")
        else:
            command.append(str(right_path))
        command.extend(["--type", kind, "--threads", str(threads)])
        if codomain_dimension is not None:
            command.extend(["--codomain-dim", str(int(codomain_dimension))])
        if all_solutions:
            command.append("--all-solutions")
        if generators:
            command.append("--generators")

        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
        )

    if completed.returncode != 0:
        message = completed.stderr.strip() or "aleq failed"
        raise RuntimeError(message)

    values = {}
    for line in completed.stdout.splitlines():
        key, separator, value = line.partition(":")
        if separator:
            values[key.strip()] = value.strip()
    if values.get("equivalent") not in ("yes", "no"):
        raise RuntimeError("unexpected output from aleq")
    return values


def find_equivalences(
    left_table,
    right_table,
    *,
    kind="affine",
    codomain_dimension=None,
    threads="auto",
    executable=None,
):
    """Return all affine or linear equivalences between two truth tables."""
    values = _run(
        left_table,
        right_table,
        kind,
        codomain_dimension,
        threads,
        executable,
        True,
    )
    if values["equivalent"] == "no":
        return []

    count = int(values["solutions"])
    return [
        _read_solution(values, f"solution {index} ")
        for index in range(count)
    ]


def find_equivalence(
    left_table,
    right_table,
    *,
    kind="affine",
    codomain_dimension=None,
    threads="auto",
    executable=None,
):
    """Return one equivalence and the total count, or ``None``."""
    values = _run(
        left_table,
        right_table,
        kind,
        codomain_dimension,
        threads,
        executable,
        False,
    )
    if values["equivalent"] == "no":
        return None

    solution = _read_solution(values)
    solution["solution_count"] = int(values["solutions"])
    return solution


def find_self_equivalences(
    table,
    *,
    kind="affine",
    codomain_dimension=None,
    threads="auto",
    executable=None,
):
    """Return all equivalences from a truth table to itself."""
    table = list(table)
    values = _run(
        table,
        table,
        kind,
        codomain_dimension,
        threads,
        executable,
        True,
        self_equivalence=True,
    )
    count = int(values["solutions"])
    return [
        _read_solution(values, f"solution {index} ")
        for index in range(count)
    ]


def _find_self_equivalence_generators(
    table,
    *,
    kind="affine",
    codomain_dimension=None,
    threads="auto",
    executable=None,
):
    table = list(table)
    values = _run(
        table,
        table,
        kind,
        codomain_dimension,
        threads,
        executable,
        False,
        self_equivalence=True,
        generators=True,
    )
    count = int(values["generators"])
    return [
        _read_solution(values, f"generator {index} ")
        for index in range(count)
    ]


def _apply_map(affine_map, point):
    image = affine_map["translation"]
    for bit, column in enumerate(affine_map["linear_columns"]):
        if point & (1 << bit):
            image ^= column
    return image


def _permutation_images(equivalence):
    domain_map = equivalence["domain_map"]
    codomain_map = equivalence["codomain_map"]
    domain_size = 1 << len(domain_map["linear_columns"])
    codomain_size = 1 << len(codomain_map["linear_columns"])

    return [
        *(_apply_map(domain_map, point) + 1 for point in range(domain_size)),
        *(
            domain_size + _apply_map(codomain_map, point) + 1
            for point in range(codomain_size)
        ),
    ]


def self_equivalence_group(table, **options):
    """Return the self-equivalences as a Sage permutation group."""
    try:
        from sage.all import PermutationGroup, PermutationGroupElement
    except ImportError as error:
        raise RuntimeError(
            "self_equivalence_group must be called from SageMath"
        ) from error

    table = list(table)
    generators = _find_self_equivalence_generators(table, **options)
    domain_size = len(table)
    domain_dimension = domain_size.bit_length() - 1
    codomain_dimension = options.get("codomain_dimension", domain_dimension)
    if codomain_dimension is None:
        codomain_dimension = domain_dimension
    codomain_size = 1 << int(codomain_dimension)
    degree = domain_size + codomain_size
    domain = list(range(1, degree + 1))
    permutations = [
        PermutationGroupElement(_permutation_images(equivalence))
        for equivalence in generators
    ]
    if not permutations:
        permutations.append(PermutationGroupElement(domain))
    return PermutationGroup(permutations, domain=domain)


def is_equivalent(*args, **kwargs):
    """Return whether the two truth tables are equivalent."""
    return find_equivalence(*args, **kwargs) is not None


equivalences = find_equivalences
self_equivalences = find_self_equivalences
automorphism_group = self_equivalence_group
