"""Small Sage interface for the aleq command-line program."""

from pathlib import Path
import subprocess
import tempfile


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

        command = [
            str(executable),
            str(left_path),
            str(right_path),
            "--type",
            kind,
            "--threads",
            str(threads),
        ]
        if codomain_dimension is not None:
            command.extend(["--codomain-dim", str(int(codomain_dimension))])
        if all_solutions:
            command.append("--all-solutions")

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
    executable="aleq",
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
    executable="aleq",
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


def find_self_equivalences(table, **options):
    """Return all equivalences from a truth table to itself."""
    table = list(table)
    return find_equivalences(table, table, **options)


def is_equivalent(*args, **kwargs):
    """Return whether the two truth tables are equivalent."""
    return find_equivalence(*args, **kwargs) is not None
