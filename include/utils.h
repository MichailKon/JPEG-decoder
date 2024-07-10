#pragma once

#include "types.h"
#include "constants.h"

namespace utils {
const char* ToString(Marker v);

template <class BidirectionalIterator, class T>
void Vector2ZigZagFlatten(BidirectionalIterator data, std::vector<T>& out) {
    if (out.size() != kFullBlock) {
        throw std::runtime_error("out.size() for zigzag must be equal to kFullBlock");
    }
    size_t ind = 0;
    for (int i = 0; i < kBlockSize; i++) {
        for (int j = 0; j < kBlockSize; j++) {
            out[ind++] = data[kZigZagIndexesMatching[i][j]];
        }
    }
}
}  // namespace utils
