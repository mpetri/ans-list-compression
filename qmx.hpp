#pragma once

#include <memory>

#include "compress_qmx.h"
#include "util.hpp"

size_t qmx_encode(const uint32_t* in, size_t n, uint32_t* out)
{
    // (1) write length later
    uint32_t* output_len = out;
    out++;
    // (2) delegate to FastPFor code
    static compress_qmx qmx_coder;
    size_t written_u32 = 1;

    // align output ptr to 128 bit boundaries as required by qmx_coder
    while (uint64_t(out) & 15ULL) {
        ++out;
        written_u32++;
    }
    if (written_u32 != 1) {
        fprintf(stderr, "%lu alignment u32 = %lu\n", n, written_u32 - 1);
    }

    size_t qmx_bytes = 0;
    qmx_coder.encodeArray(in, n, out, &qmx_bytes);
    *output_len = qmx_bytes;
    qmx_bytes += (qmx_bytes % sizeof(uint32_t));
    return (qmx_bytes / sizeof(uint32_t)) + written_u32;
}

size_t qmx_decode(const uint32_t* in, uint32_t* out, uint32_t list_len)
{
    uint32_t qmx_bytes = *in++;
    size_t decoded_u32 = 1;
    // align output ptr to 128 bit boundaries as required by qmx_coder
    while (uint64_t(in) & 15ULL) {
        ++in;
        decoded_u32++;
    }
    if (decoded_u32 != 1) {
        fprintf(stderr, "%lu alignment u32 = %lu -> %llu\n", list_len,
            decoded_u32 - 1, uint64_t(in));
    }

    static compress_qmx qmx_coder;
    qmx_coder.decodeArray(in, qmx_bytes, out, list_len);
    qmx_bytes += (qmx_bytes % sizeof(uint32_t));
    fprintf(stderr, "DONE!!\n");
    fflush(stderr);
    return (qmx_bytes / sizeof(uint32_t)) + decoded_u32;
}
