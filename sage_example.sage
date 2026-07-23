from f2_equivalence import find_equivalence, self_equivalence_group


identity = [0, 1, 2, 3]
translated_identity = [1, 0, 3, 2]

witness = find_equivalence(identity, translated_identity)
print("Domain map:")
print(witness.domain)
print("Codomain map:")
print(witness.codomain)

group = self_equivalence_group(identity)
print("Self-equivalence group order:", group.order())
print("Group generators:", group.gens())
