#pragma once

#include <stdint.h>
#include <array>

using DByte = uint16_t;
using Byte = uint8_t;

enum Marker { SOI, EOI, COM, APPn, DQT, SOF0, DHT, SOS };

template <class T, const int SIZE>
using ImageBlock = std::array<std::array<T, SIZE>, SIZE>;