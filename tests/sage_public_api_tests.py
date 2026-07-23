from pathlib import Path
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from f2_equivalence import find_equivalence, self_equivalence_group


executable = sys.argv[1]
identity = [0, 1, 2, 3]
translated_identity = [1, 0, 3, 2]

witness = find_equivalence(
    identity,
    translated_identity,
    executable=executable,
)
assert witness is not None
assert witness.domain is not None
assert witness.codomain is not None

affine_group = self_equivalence_group(identity, executable=executable)
assert affine_group.order() == 24
assert affine_group.degree() == 8

linear_group = self_equivalence_group(
    identity,
    linear=True,
    executable=executable,
)
assert linear_group.order() == 6

rectangular_group = self_equivalence_group(
    [0, 1],
    codomain_dimension=2,
    executable=executable,
)
assert rectangular_group.order() == 4
assert rectangular_group.degree() == 6

print("Sage public API tests passed")
