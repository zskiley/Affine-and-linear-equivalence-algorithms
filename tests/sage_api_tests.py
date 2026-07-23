from pathlib import Path
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from f2_equivalence import (
    _apply,
    _find_equivalence_data,
    _find_self_generator_data,
    _parse_generators,
    _permutation_images,
)


executable = sys.argv[1]

identity = [0, 1, 2, 3]
translated_identity = [1, 0, 3, 2]


def generated_group_order(generators, degree):
    generators = [tuple(generator) for generator in generators]
    identity_permutation = tuple(range(1, degree + 1))
    seen = {identity_permutation}
    pending = [identity_permutation]
    while pending:
        current = pending.pop()
        for generator in generators:
            product = tuple(generator[image - 1] for image in current)
            if product not in seen:
                seen.add(product)
                pending.append(product)
    return len(seen)


witness = _find_equivalence_data(
    identity,
    translated_identity,
    executable=executable,
)
assert witness is not None
assert len(witness.domain.matrix) == 2
assert len(witness.codomain.matrix) == 2
for point, value in enumerate(identity):
    assert (
        _apply(witness.codomain, value)
        == translated_identity[_apply(witness.domain, point)]
    )

linear_witness = _find_equivalence_data(
    identity,
    translated_identity,
    linear=True,
    executable=executable,
)
assert linear_witness is None

affine_generators, _, _ = _find_self_generator_data(
    identity,
    executable=executable,
)
assert generated_group_order(
    map(_permutation_images, affine_generators), 8
) == 24

linear_generators, domain_dimension, codomain_dimension = (
    _find_self_generator_data(
        identity,
        linear=True,
        executable=executable,
    )
)
assert domain_dimension == 2
assert codomain_dimension == 2
assert linear_generators
for generator in linear_generators:
    for point, value in enumerate(identity):
        assert (
            _apply(generator.codomain, value)
            == identity[_apply(generator.domain, point)]
        )
    images = _permutation_images(generator)
    assert sorted(images) == list(range(1, 9))
assert generated_group_order(
    map(_permutation_images, linear_generators), 8
) == 6

rectangular_generators, domain_dimension, codomain_dimension = (
    _find_self_generator_data(
        [0, 1],
        codomain_dimension=2,
        executable=executable,
    )
)
assert domain_dimension == 1
assert codomain_dimension == 2
for generator in rectangular_generators:
    for point, value in enumerate([0, 1]):
        assert (
            _apply(generator.codomain, value)
            == [0, 1][_apply(generator.domain, point)]
        )
    images = _permutation_images(generator)
    assert sorted(images) == list(range(1, 7))
assert generated_group_order(
    map(_permutation_images, rectangular_generators), 6
) == 4

assert _parse_generators("generators: 0\n", 1, 1) == ()

print("Sage wrapper tests passed")
