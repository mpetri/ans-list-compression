#pragma once

#include "util.hpp"

template <uint32_t i> uint8_t extract7bits(const uint32_t val) {
    return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
}

template <uint32_t i> uint8_t extract7bitsmaskless(const uint32_t val) {
    return static_cast<uint8_t>((val >> (7 * i)));
}

void vbyte_encode_u32(std::vector<uint8_t>& out, uint32_t x) {
    if (x < (1U << 7)) {
        out.push_back(static_cast<uint8_t>(x & 127));
    } else if (x < (1U << 14)) {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bitsmaskless<1>(x) & 127);
    } else if (x < (1U << 21)) {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bits<1>(x) | 128);
        out.push_back(extract7bitsmaskless<2>(x) & 127);
    } else if (x < (1U << 28)) {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bits<1>(x) | 128);
        out.push_back(extract7bits<2>(x) | 128);
        out.push_back(extract7bitsmaskless<3>(x) & 127);
    } else {
        out.push_back(extract7bits<0>(x) | 128);
        out.push_back(extract7bits<1>(x) | 128);
        out.push_back(extract7bits<2>(x) | 128);
        out.push_back(extract7bits<3>(x) | 128);
        out.push_back(extract7bitsmaskless<4>(x) & 127);
    }
}

void vbyte_encode_u32(uint8_t*& out, uint32_t x) {
    if (x < (1U << 7)) {
        *out++ = static_cast<uint8_t>(x & 127);
    } else if (x < (1U << 14)) {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bitsmaskless<1>(x) & 127;
    } else if (x < (1U << 21)) {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bits<1>(x) | 128;
        *out++ = extract7bitsmaskless<2>(x) & 127;
    } else if (x < (1U << 28)) {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bits<1>(x) | 128;
        *out++ = extract7bits<2>(x) | 128;
        *out++ = extract7bitsmaskless<3>(x) & 127;
    } else {
        *out++ = extract7bits<0>(x) | 128;
        *out++ = extract7bits<1>(x) | 128;
        *out++ = extract7bits<2>(x) | 128;
        *out++ = extract7bits<3>(x) | 128;
        *out++ = extract7bitsmaskless<4>(x) & 127;
    }
}

uint32_t vbyte_decode_u32(const uint8_t*& input) {
    uint32_t x = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t c = *input++;
        x += ((c & 127) << shift);
        if (!(c & 128)) {
            return x;
        }
        shift += 7;
    }
    return x;
}

size_t write_vbyte(FILE* f, uint32_t x) {
    std::vector<uint8_t> buf;
    vbyte_encode_u32(buf, x);
    int ret = fwrite(buf.data(), 1, buf.size(), f);
    if (ret != (int)buf.size()) {
        quit("writing vbyte to file: %d != %d", ret, (int)buf.size());
    }
    return buf.size();
}

uint32_t read_vbyte(FILE* f) {
    uint32_t x = 0;
    unsigned int shift = 0;
    uint8_t chr = 0;
    while (true) {
        int ret = fread(&chr, 1, 1, f);
        if (ret != 1) {
            quit("reading vbyte from file: %d != %d", ret, 1);
        }
        x += ((chr & 127) << shift);
        if (!(chr & 128)) {
            return x;
        }
        shift += 7;
    }
}

size_t vbyte_encode(const uint32_t* in, size_t n, uint32_t* out) {
    uint32_t* initout = out;
    uint8_t* out8 = (uint8_t*)out;
    for (size_t i = 0; i < n; i++) {
        vbyte_encode_u32(out8, in[i]);
    }
    // pad to 32bit boundary
    while (uint64_t(out8) & 3ULL) {
        ++out8;
    }
    out = (uint32_t*)out8;
    return (out - initout);
}

size_t vbyte_decode(const uint32_t* in, uint32_t* out, uint32_t list_len) {
    const uint32_t* initin = in;
    const uint8_t* in8 = (const uint8_t*)in;
    for (size_t i = 0; i < list_len; i++) {
        out[i] = vbyte_decode_u32(in8);
    }
    // pad to 32bit boundary
    while (uint64_t(in8) & 3ULL) {
        ++in8;
    }
    in = (uint32_t*)in8;
    return (in - initin);
}
