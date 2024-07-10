#include "types.h"
#include "constants.h"
#include "bit_reader.h"

BitReader::BitReader(std::istream& input) : input_(&input) {
}

bool BitReader::ReadBit() {
    if (pos_ < 0) {
        RefreshState();
    }
    bool res = (last_ >> pos_) & 1;
    pos_--;
    if (pos_ < 0 && !input_->eof()) {
        RefreshState();
    }
    return res;
}

DByte BitReader::ReadDByte() {
    Byte c1 = ReadByte();
    Byte c2 = ReadByte();
    return (static_cast<DByte>(c1) << 8) | c2;
}

Byte BitReader::ReadByte() {
    Byte res = 0;
    for (int i = 0; i < 8; i++) {
        res <<= 1;
        res |= ReadBit();
    }
    return res;
}

Marker BitReader::ReadMarker() {
    DByte marker = ReadDByte();
    if (kCode2Marker.contains(marker)) {
        return kCode2Marker.at(marker);
    }
    if (kAPPnMin <= marker && marker <= kAPPnMax) {
        return APPn;
    }
    throw std::runtime_error("Got unknown marker");
}

DByte BitReader::ReadSectionLength() {
    return ReadDByte();
}

std::vector<DByte> BitReader::ReadNDBytes(size_t n) {
    std::vector<DByte> res;
    res.reserve(n);
    while (n--) {
        res.emplace_back(ReadDByte());
    }
    return res;
}

std::vector<Byte> BitReader::ReadNBytes(size_t n) {
    std::vector<Byte> res;
    res.reserve(n);
    while (n--) {
        res.emplace_back(ReadByte());
    }
    return res;
}

uint8_t BitReader::ReadRawDataLen(HuffmanTree& tree) {
    int out;
    while (true) {
        bool bit = ReadBit();
        if (tree.Move(bit, out)) {
            break;
        }
    }
    return out;
}

int BitReader::ReadRawDataItem(uint8_t len) {
    if (len == 0) {
        return 0;
    }
    int real = ReadBit();
    bool invert = !real;
    for (int i = 1; i < len; i++) {
        real = (real << 1) | ReadBit();
    }
    if (invert) {
        real = real - (1 << len) + 1;
    }
    return real;
}

void BitReader::SkipCurrentByte() {
    if (pos_ != 7) {
        pos_ = -1;
        //            RefreshState();
    }
}

void BitReader::SetIsSos(bool is_sos) {
    is_sos_ = is_sos;
}

void BitReader::RefreshState() {
    if (input_->eof()) {
        throw std::runtime_error("reading from an empty input");
    }
    Byte tmp;
    *input_ >> tmp;
    if (is_sos_ && tmp == 0 && last_ == 0xFF) {  // 0xFF + 0x00 means only FF
        if (input_->eof()) {
            throw std::runtime_error("reading from an empty input");
        }
        *input_ >> tmp;
    }
    last_ = tmp;
    pos_ = 7;
}
