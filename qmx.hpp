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
    size_t bytes_left = 10000000;
    out = align_ptr(16, 1, out, bytes_left);
    written_u32 += (10000000 - bytes_left) / (sizeof(uint32_t));

    size_t qmx_bytes = 0;
    qmx_coder.encodeArray(in, n, out, &qmx_bytes);
    *output_len = qmx_bytes;
    qmx_bytes += (sizeof(uint32_t) - (qmx_bytes % sizeof(uint32_t)));
    return (qmx_bytes / sizeof(uint32_t)) + written_u32;
}

size_t qmx_decode(const uint32_t* in, uint32_t* out, uint32_t list_len)
{
    uint32_t qmx_bytes = *in++;
    size_t decoded_u32 = 1;

    // align output ptr to 128 bit boundaries as required by qmx_coder
    size_t bytes_left = 10000000;
    in = align_ptr(16, 1, in, bytes_left);
    decoded_u32 += (10000000 - bytes_left) / (sizeof(uint32_t));

    bool isInputAligned = ((reinterpret_cast<size_t>(in) % 16) == 0);
    bool isOutputAligned = ((reinterpret_cast<size_t>(out) % 16) == 0);

    if (!isInputAligned || !isOutputAligned) {
        printf("qmx decode alignment error: %lu %lu\n",
            reinterpret_cast<size_t>(in), reinterpret_cast<size_t>(out));
    }

    static compress_qmx qmx_coder;
    qmx_coder.decodeArray(in, qmx_bytes, out, list_len);
    qmx_bytes += (sizeof(uint32_t) - (qmx_bytes % sizeof(uint32_t)));
    return (qmx_bytes / sizeof(uint32_t)) + decoded_u32;
}
