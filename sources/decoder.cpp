#include <iostream>
#include <algorithm>
#include <cassert>

#include "bit_reader.h"
#include "constants.h"
#include "decoder.h"
#include "fft.h"
#include "huffman.h"
#include "types.h"
#include "utils.h"

struct YCbCr {
    int y, cb, cr;

    YCbCr(int y, int cb, int cr) : y(y), cb(cb), cr(cr) {
    }

    RGB ToRGB() const {
        RGB res{};

        res.r = round(y + 1.402 * (cr - 128));
        res.g = round(y - 0.34414 * (cb - 128) - 0.71414 * (cr - 128));
        res.b = round(y + 1.772 * (cb - 128));
        res.r = std::min(255, std::max(0, res.r));
        res.g = std::min(255, std::max(0, res.g));
        res.b = std::min(255, std::max(0, res.b));

        return res;
    }
};

struct Channel {
    int id, horizontal, vertical, dqt_id;
    int huffman_dc, huffman_ac;
};

struct QuantizationTable {
    int id;
    std::vector<DByte> items;

    QuantizationTable(int id, const std::vector<DByte>& items) : id(id), items(items) {
    }
};

struct HuffmanTable {
    int cl, id;
    HuffmanTree tree;

    HuffmanTable(int cl, int id, HuffmanTree&& tree) : cl(cl), id(id), tree(std::move(tree)) {
    }
};

struct MetaDataHandler {
    size_t height, width;
    std::vector<Channel> channels;
    std::vector<HuffmanTable> huffs;
    std::vector<QuantizationTable> dqt_tables;

    std::pair<int, int> MaxThinning() const {
        int hor = std::max_element(channels.begin(), channels.end(), [](auto l, auto r) {
                      return l.horizontal < r.horizontal;
                  })->horizontal;
        int ver = std::max_element(channels.begin(), channels.end(), [](auto l, auto r) {
                      return l.vertical < r.vertical;
                  })->vertical;
        return {hor, ver};
    }

    const QuantizationTable& FindQTForChannel(int chan) const {
        size_t res = std::find_if(dqt_tables.begin(), dqt_tables.end(),
                                  [&](const QuantizationTable& cur) {
                                      return cur.id == channels[chan].dqt_id;
                                  }) -
                     dqt_tables.begin();
        if (res == dqt_tables.size()) {
            throw std::runtime_error("can't find desired dqt_table");
        }
        return dqt_tables[res];
    }

    HuffmanTree& FindHuffmanTreeForChannel(int chan, int cl) {
        size_t res = std::find_if(huffs.begin(), huffs.end(),
                                  [&](const HuffmanTable& table) {
                                      return table.cl == cl &&
                                             table.id == (cl == 0 ? channels[chan].huffman_dc
                                                                  : channels[chan].huffman_ac);
                                  }) -
                     huffs.begin();
        if (res == huffs.size()) {
            throw std::runtime_error("can't find desired huffman table");
        }
        return huffs[res].tree;
    }

    void SetHuffmanACDCIndex(int id, int huffman_ids) {
        size_t i = std::find_if(channels.begin(), channels.end(),
                                [id](const Channel& c) { return c.id == id; }) -
                   channels.begin();
        if (i == channels.size()) {
            throw std::runtime_error("can't find channel for SOS");
        }
        channels[i].huffman_dc = huffman_ids >> 4;
        channels[i].huffman_ac = huffman_ids & 0xf;
    }
};

class MyDctCalculator {
public:
    MyDctCalculator()
        : input_(kFullBlock), output_(kFullBlock), calculator_(kBlockSize, &input_, &output_) {
    }

    template <class BidirectionalIterator>
    void LoadZigZagInput(BidirectionalIterator data_begin) {
        utils::Vector2ZigZagFlatten(data_begin, input_);
    }

    template <class T>
    void LoadInput(const std::vector<T>& new_input) {
        for (int i = 0; i < kFullBlock; i++) {
            input_[i] = new_input[i];
        }
    }

    void Inverse() {
        calculator_.Inverse();
    }

    const std::vector<double>* GetOutput() const {
        return &output_;
    }

private:
    std::vector<double> input_, output_;
    DctCalculator calculator_;
};

std::vector<QuantizationTable> ReadQuantizationTables(BitReader& reader, DByte len) {
    std::vector<QuantizationTable> res;
    while (len > 0) {
        Byte info = reader.ReadByte();
        len--;
        int val_len = info >> 4 & 0xf;
        int id = info & 0xf;

        std::vector<DByte> data;
        if (val_len == 1) {
            data = reader.ReadNDBytes(kFullBlock);
            len -= kFullBlock * 2;
        } else {
            data.reserve(len);
            for (int i = 0; i < kFullBlock; i++) {
                data.push_back(reader.ReadByte());
                len--;
            }
        }

        res.emplace_back(id, data);
    }
    return res;
}

std::vector<HuffmanTable> ReadHuffmanTables(BitReader& reader, DByte len) {
    std::vector<HuffmanTable> res;
    while (len > 0) {
        Byte info = reader.ReadByte();
        int cl = info >> 4;
        int id = info & 0xf;
        len--;
        std::vector<Byte> lens(16);
        int total = 0;
        for (auto& i : lens) {
            i = reader.ReadByte();
            total += i;
            len--;
        }

        std::vector<Byte> values;
        values.reserve(total);

        while (total--) {
            values.push_back(reader.ReadByte());
            len--;
        }
        HuffmanTree tree;
        tree.Build(lens, values);
        res.emplace_back(cl, id, std::move(tree));
    }

    return res;
}

ImageBlock<Byte, kBlockSize> ReadNextMCU(BitReader& reader, const QuantizationTable& table,
                                         int& last_dc, HuffmanTree& huffman_dc,
                                         HuffmanTree& huffman_ac, MyDctCalculator& calculator) {
    static int ind = 0;
    ind++;

    std::array<short, kFullBlock> raw_data;
    size_t raw_data_ind = 0;
    // dc
    raw_data[raw_data_ind++] = reader.ReadRawDataItem(reader.ReadRawDataLen(huffman_dc));
    // ac
    while (raw_data_ind < kFullBlock) {
        Byte cur = reader.ReadRawDataLen(huffman_ac);
        if (cur == 0) {
            break;
        }
        for (int i = 0; i < ((cur >> 4) & 0xf); i++) {
            if (raw_data_ind >= kFullBlock) {
                throw std::runtime_error("wrong AC coef in MCU");
            }
            raw_data[raw_data_ind++] = 0;
        }
        if (raw_data_ind >= kFullBlock) {
            throw std::runtime_error("wrong AC coef in MCU");
        }
        raw_data[raw_data_ind++] = reader.ReadRawDataItem(cur & 0xf);
    }
    while (raw_data_ind < kFullBlock) {
        raw_data[raw_data_ind++] = 0;
    }
    // CUM
    raw_data[0] += last_dc;
    last_dc = raw_data[0];

    for (int i = 0; i < kFullBlock; i++) {
        raw_data[i] *= table.items[i];
    }
    calculator.LoadZigZagInput(raw_data.begin());
    calculator.Inverse();

    ImageBlock<Byte, kBlockSize> out;
    for (int i = 0; i < kBlockSize; i++) {
        for (int j = 0; j < kBlockSize; j++) {
            out[i][j] = std::min(
                255., std::max(0., 128 + round((*calculator.GetOutput())[i * kBlockSize + j])));
        }
    }
    return out;
}

void ScanImageData(Image& res, BitReader& reader, MetaDataHandler& metainfo) {
    size_t channels_count = metainfo.channels.size();
    std::vector<int> prev_values(channels_count);
    reader.SetIsSos(true);
    int table_no = 0;
    int need_tables;
    auto [hor, ver] = metainfo.MaxThinning();
    int cnt_hor = ((res.Width() + kBlockSize - 1) / kBlockSize + ver - 1) / ver;
    int cnt_ver = ((res.Height() + kBlockSize - 1) / kBlockSize + hor - 1) / hor;
    need_tables = cnt_hor * cnt_ver;

    size_t out_i = 0, out_j = 0;
    std::vector<ImageBlock<int, 2 * kBlockSize>> ycbcr_data(channels_count);
    MyDctCalculator calculator;

    while (table_no++ < need_tables) {
        for (size_t i = 0; i < channels_count; i++) {
            int cur_hor = metainfo.channels[i].horizontal, cur_ver = metainfo.channels[i].vertical;
            const QuantizationTable& dqt = metainfo.FindQTForChannel(i);
            HuffmanTree& dc_tree = metainfo.FindHuffmanTreeForChannel(i, 0);
            HuffmanTree& ac_tree = metainfo.FindHuffmanTreeForChannel(i, 1);
            for (int h = 0; h < cur_hor; h++) {
                for (int v = 0; v < cur_ver; ++v) {
                    ImageBlock<Byte, kBlockSize> table =
                        ReadNextMCU(reader, dqt, prev_values[i], dc_tree, ac_tree, calculator);
                    for (int x = 0; x < kBlockSize; x++) {
                        for (int y = 0; y < kBlockSize; y++) {
                            ycbcr_data[i][x + h * kBlockSize][y + v * kBlockSize] = table[x][y];
                        }
                    }
                }
            }
        }

        for (int i = 0; i < hor; i++) {
            for (int j = 0; j < ver; j++) {
                for (int y = 0; y < kBlockSize; y++) {
                    for (int x = 0; x < kBlockSize; x++) {
                        if (channels_count == 1) {
                            int c = ycbcr_data[0][y][x];
                            res.SetPixel(out_i + y + kBlockSize * i, out_j + x + kBlockSize * j,
                                         {c, c, c});
                            continue;
                        }
                        int real_x = x + kBlockSize * j, real_y = y + kBlockSize * i;
                        int cury = ycbcr_data[0][real_y * metainfo.channels[0].horizontal / hor]
                                             [real_x * metainfo.channels[0].vertical / ver];
                        int curcb = ycbcr_data[1][real_y * metainfo.channels[1].horizontal / hor]
                                              [real_x * metainfo.channels[1].vertical / ver];
                        int curcr = ycbcr_data[2][real_y * metainfo.channels[2].horizontal / hor]
                                              [real_x * metainfo.channels[2].vertical / ver];
                        if (out_i + real_y < metainfo.height && out_j + real_x < metainfo.width) {
                            res.SetPixel(out_i + real_y, out_j + real_x,
                                         YCbCr(cury, curcb, curcr).ToRGB());
                        }
                    }
                }
            }
        }
        out_j += ver * kBlockSize;
        if (out_j >= metainfo.width) {
            out_i += hor * kBlockSize;
            out_j = 0;
        }
    }

    reader.SkipCurrentByte();
}

Image Decode(std::istream& input) {
    input >> std::noskipws;

    BitReader reader(input);
    if (reader.ReadMarker() != SOI) {
        throw std::runtime_error("no SOI at the beginning of the file");
    }
    Image res;

    MetaDataHandler metainfo;

    bool have_sof0 = false;
    while (true) {
        Marker cur = reader.ReadMarker();
        if (cur == EOI) {
            break;
        } else if (cur == COM) {
            DByte len = reader.ReadSectionLength();
            std::vector<Byte> comment = reader.ReadNBytes(len - 2);
            res.SetComment(std::string{comment.begin(), comment.end()});
        } else if (cur == APPn) {
            DByte len = reader.ReadSectionLength();
            std::vector<Byte> appn = reader.ReadNBytes(len - 2);
        } else if (cur == DQT) {
            DByte len = reader.ReadSectionLength();
            for (auto& table : ReadQuantizationTables(reader, len - 2)) {
                metainfo.dqt_tables.emplace_back(std::move(table));
                metainfo.dqt_tables.shrink_to_fit();
            }
            if (metainfo.dqt_tables.size() > kMaxQuantizationTables) {
                throw std::runtime_error("too much huffman trees");
            }
        } else if (cur == SOF0) {
            if (have_sof0) {
                throw std::runtime_error("multiple sof0");
            }
            have_sof0 = true;
            [[maybe_unused]] DByte len = reader.ReadSectionLength();
            (void)reader.ReadByte();
            metainfo.height = reader.ReadDByte();
            metainfo.width = reader.ReadDByte();
            int channels_cnt = reader.ReadByte();
            if (!(channels_cnt == 1 || channels_cnt == 3)) {
                throw std::runtime_error("number of channels is not equal to 1 or 3");
            }
            res.SetSize(metainfo.width, metainfo.height);

            for (int i = 0; i < channels_cnt; i++) {
                std::vector<Byte> tmp = reader.ReadNBytes(3);
                metainfo.channels.push_back(
                    {tmp[0], tmp[1] & 0xf, tmp[1] >> 4 & 0xf, tmp[2], -1, -1});
            }
        } else if (cur == DHT) {
            DByte len = reader.ReadSectionLength();
            for (auto& tree : ReadHuffmanTables(reader, len - 2)) {
                metainfo.huffs.emplace_back(std::move(tree));
            }
            metainfo.huffs.shrink_to_fit();
            if (metainfo.huffs.size() > kMaxHuffmanTrees) {
                throw std::runtime_error("too much huffman trees");
            }
        } else if (cur == SOS) {
            [[maybe_unused]] DByte len = reader.ReadSectionLength() - 2;
            Byte channels_count = reader.ReadByte();
            len--;
            for (int ch = 0; ch < channels_count; ch++) {
                Byte id = reader.ReadByte();
                Byte huffman_ids = reader.ReadByte();
                len -= 2;
                metainfo.SetHuffmanACDCIndex(id, huffman_ids);
            }
            {
                // just a check for progressive
                auto prog = reader.ReadNBytes(3);
                if (prog[0] != 0 || prog[1] != 63 || prog[2] != 0) {
                    throw std::runtime_error("wrong SOS section");
                }
            }

            ScanImageData(res, reader, metainfo);

            if (reader.ReadMarker() != EOI) {
                throw std::runtime_error("something after eoi");
            }
        }
    }

    return res;
}
