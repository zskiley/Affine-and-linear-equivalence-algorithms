from pathlib import Path
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from f2_equivalence import find_equivalence, self_equivalences


executable = sys.argv[1]
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

print("Sage API tests passed")
