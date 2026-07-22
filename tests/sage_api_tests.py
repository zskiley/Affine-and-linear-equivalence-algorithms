from pathlib import Path
import subprocess
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from f2_equivalence import (
    _find_self_equivalence_generators,
    _permutation_images,
    find_equivalence,
    self_equivalences,
)


executable = sys.argv[1]
identity_path = Path(__file__).resolve().parents[1] / "examples" / "identity_2.tt"
identity = [0, 1, 2, 3]
translated_identity = [1, 0, 3, 2]

affine = find_equivalence(
    identity,
    translated_identity,
    kind="affine",
    threads=2,
    executable=executable,
)
assert affine is not None

linear = find_equivalence(
    identity,
    translated_identity,
    kind="linear",
    threads=2,
    executable=executable,
)
assert linear is None

affine_self_equivalences = self_equivalences(
    identity,
    kind="affine",
    threads=2,
    executable=executable,
)
assert len(affine_self_equivalences) == 24

linear_self_equivalences = self_equivalences(
    identity,
    kind="linear",
    threads=4,
    executable=executable,
)
assert len(linear_self_equivalences) == 6
for equivalence in linear_self_equivalences:
    assert equivalence["domain_map"]["translation"] == 0
    assert equivalence["codomain_map"]["translation"] == 0

linear_generators = _find_self_equivalence_generators(
    identity,
    kind="linear",
    threads=2,
    executable=executable,
)
assert 0 < len(linear_generators) < len(linear_self_equivalences)
for generator in linear_generators:
    assert generator["domain_map"]["translation"] == 0
    assert generator["codomain_map"]["translation"] == 0

sample_equivalence = {
    "domain_map": {"translation": 1, "linear_columns": [1, 2]},
    "codomain_map": {"translation": 0, "linear_columns": [2, 1]},
}
assert _permutation_images(sample_equivalence) == [2, 1, 4, 3, 5, 7, 6, 8]

constant_linear = self_equivalences(
    [0, 0, 0, 0],
    kind="linear",
    threads=2,
    executable=executable,
)
assert len(constant_linear) == 36
assert len({tuple(_permutation_images(item)) for item in constant_linear}) == 36

self_cli = subprocess.run(
    [executable, str(identity_path), "--self", "--type", "linear"],
    check=False,
    capture_output=True,
    text=True,
)
assert self_cli.returncode == 0
assert "solutions: 6" in self_cli.stdout
assert "generators:" in self_cli.stdout

wrong_self_arity = subprocess.run(
    [executable, str(identity_path), str(identity_path), "--self"],
    check=False,
    capture_output=True,
    text=True,
)
assert wrong_self_arity.returncode != 0

missing_right_table = subprocess.run(
    [executable, str(identity_path)],
    check=False,
    capture_output=True,
    text=True,
)
assert missing_right_table.returncode != 0

print("Sage API tests passed")
