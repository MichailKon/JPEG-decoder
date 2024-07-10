#pragma once

#include "types.h"
#include <unordered_map>

constexpr int kBlockSize = 8;
constexpr int kFullBlock = kBlockSize * kBlockSize;

constexpr int kAPPnMin = 0xffe0;
constexpr int kAPPnMax = 0xffef;

constexpr int kMaxQuantizationTables = 255;
constexpr int kMaxHuffmanTrees = 255 * 2;

constexpr int kZigZagIndexesMatching[8][8] = {
    {0, 1, 5, 6, 14, 15, 27, 28},     {2, 4, 7, 13, 16, 26, 29, 42},
    {3, 8, 12, 17, 25, 30, 41, 43},   {9, 11, 18, 24, 31, 40, 44, 53},
    {10, 19, 23, 32, 39, 45, 52, 54}, {20, 22, 33, 38, 46, 51, 55, 60},
    {21, 34, 37, 47, 50, 56, 59, 61}, {35, 36, 48, 49, 57, 58, 62, 63},
};

extern const std::unordered_map<int, Marker> kCode2Marker;
