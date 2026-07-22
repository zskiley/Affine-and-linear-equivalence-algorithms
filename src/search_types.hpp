#pragma once

#include "partition.hpp"

namespace affine {

enum class EquivalenceMode {
    Affine,
    Linear,
};

enum class BranchKind {
    DomainPoint,
    CodomainPoint,
    DomainHyperplane,
    CodomainHyperplane,
};

struct BranchMove {
    BranchKind kind = BranchKind::DomainPoint;
    Partition::ObjectId left_object = 0;
    Partition::ObjectId right_object = 0;
};

[[nodiscard]] inline bool operator==(const BranchMove& left, const BranchMove& right)
{
    return left.kind == right.kind
        && left.left_object == right.left_object
        && left.right_object == right.right_object;
}

[[nodiscard]] inline bool operator!=(const BranchMove& left, const BranchMove& right)
{
    return !(left == right);
}

} // namespace affine
