#pragma once

#include <istream>
#include "types.h"
#include "constants.h"
#include "huffman.h"

class BitReader {
public:
    explicit BitReader(std::istream& input);

    bool ReadBit();

    DByte ReadDByte();

    Byte ReadByte();

    Marker ReadMarker();

    DByte ReadSectionLength();

    std::vector<DByte> ReadNDBytes(size_t n);

    std::vector<Byte> ReadNBytes(size_t n);

    uint8_t ReadRawDataLen(HuffmanTree& tree);

    int ReadRawDataItem(uint8_t len);

    void SkipCurrentByte();

    void SetIsSos(bool is_sos);

private:
    std::istream* input_;
    Byte last_ = 0xff;
    char pos_ = -1;
    bool is_sos_ = false;

    void RefreshState();
};