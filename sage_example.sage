from f2_equivalence import find_equivalence, self_equivalence_group


identity = [0, 1, 2, 3]
translated_identity = [1, 0, 3, 2]

affine = find_equivalence(identity, translated_identity)
print("Affine equivalence:", affine)

linear_group = self_equivalence_group(identity, kind="linear")
print("Linear self-equivalence group order:", linear_group.order())
print("Group generators:", linear_group.gens())
