#pragma once

#include "partition.hpp"

#include <cstdint>

namespace affine {

struct PartitionPair {
    Partition left;
    Partition right;

    PartitionPair() = default;

    explicit PartitionPair(std::uint32_t object_count)
        : left(object_count)
        , right(object_count)
    {
    }
};

} // namespace affine
