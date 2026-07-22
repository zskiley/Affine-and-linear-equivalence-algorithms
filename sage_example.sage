from f2_equivalence import find_equivalence, self_equivalences


identity = [0, 1, 2, 3]
translated_identity = [1, 0, 3, 2]

affine = find_equivalence(identity, translated_identity)
print("Affine equivalence:", affine)

linear_self_equivalences = self_equivalences(identity, kind="linear")
print("Linear self-equivalences:", len(linear_self_equivalences))
