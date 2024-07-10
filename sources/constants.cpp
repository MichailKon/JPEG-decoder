#include "constants.h"

const std::unordered_map<int, Marker> kCode2Marker = {
    {0xffd8, SOI},  {0xffd9, EOI}, {0xfffe, COM}, {0xffdb, DQT},
    {0xffc0, SOF0}, {0xffc4, DHT}, {0xffda, SOS}};
