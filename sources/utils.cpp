#include "utils.h"

const char* ToString(Marker v) {
    switch (v) {
        case SOI:
            return "SOI";
        case EOI:
            return "EIO";
        case COM:
            return "COM";
        case APPn:
            return "APPn";
        case DQT:
            return "DQT";
        case SOF0:
            return "SOF0";
        case DHT:
            return "DHT";
        case SOS:
            return "SOS";
        default:
            return "[Unknown Marker]";
    }
}
