#pragma once

#include "binpackinghelpers.h"
#include "simple16.hpp"
#include "util.hpp"

#include <assert.h>

uint32_t gccbits(const uint32_t v) {
    return v == 0 ? 0 : 32 - __builtin_clz(v);
}

template <class iterator>
uint32_t maxbits(const iterator& begin, const iterator& end) {
    uint32_t accumulator = 0;
    for (iterator k = begin; k != end; ++k) {
        accumulator |= *k;
    }
    return gccbits(accumulator);
}
constexpr uint32_t div_roundup(uint32_t v, uint32_t divisor) {
    return (v + (divisor - 1)) / divisor;
}

template <uint32_t t_block_size = 128> struct op4 {
  private:
    enum {
        PFORDELTA_B = 6,
        PFORDELTA_NEXCEPT = 10,
        PFORDELTA_EXCEPTSZ = 16,
        TAIL_MERGIN = 1024,
        PFORDELTA_INVERSERATIO = 10,
        PACKSIZE = 32,
        BlockSize = t_block_size
    };

  private:
    static_assert(t_block_size % PACKSIZE == 0, "blocksize incorrect");

    std::vector<uint32_t> possLogs = {0, 1,  2,  3,  4,  5,  6,  7, 8,
                                      9, 10, 11, 12, 13, 16, 20, 32};
    std::vector<uint32_t> exceptionsPositions =
        std::vector<uint32_t>(BlockSize);
    std::vector<uint32_t> exceptionsValues = std::vector<uint32_t>(BlockSize);
    std::vector<uint32_t> exceptions =
        std::vector<uint32_t>(4 * BlockSize + TAIL_MERGIN + 1);
    std::vector<uint32_t> tobecoded = std::vector<uint32_t>(BlockSize);
    Simple16<false> simple16;

  private:
    uint32_t tryB(uint32_t b, const uint32_t* in, uint32_t len) {
        assert(b <= 32);
        if (b == 32) {
            return len;
        }
        uint32_t size = div_roundup(len * b, 32);
        uint32_t curExcept = 0;

        for (uint32_t i = 0; i < len; i++) {
            if (in[i] >= (1U << b)) {
                const uint32_t e = in[i] >> b;
                exceptionsPositions[curExcept] = i;
                exceptionsValues[curExcept] = e;
                curExcept++;
            }
        }

        if (curExcept > 0) {
            for (uint32_t i = curExcept - 1; i > 0; i--) {
                const uint32_t cur = exceptionsPositions[i];
                const uint32_t prev = exceptionsPositions[i - 1];
                const uint32_t gap = cur - prev;
                exceptionsPositions[i] = gap;
            }
            for (uint32_t i = 0; i < curExcept; i++) {
                const uint32_t excPos = (i > 0) ? exceptionsPositions[i] - 1
                                                : exceptionsPositions[i];
                const uint32_t excVal = exceptionsValues[i] - 1;
                exceptions[i] = excPos;
                exceptions[i + curExcept] = excVal;
            }
            size_t encodedExceptions_sz;
            simple16.fakeencodeArray(&exceptions[0], 2 * curExcept,
                                     encodedExceptions_sz);
            size += static_cast<uint32_t>(encodedExceptions_sz);
        }
        return size;
    }

    uint32_t findBestB(const uint32_t* in, uint32_t len) {
        uint32_t b = possLogs.back();
        assert(b == 32);
        uint32_t bsize = tryB(b, in, len);
        const uint32_t mb = maxbits(in, in + len);
        uint32_t i = 0;
        while (mb > 28 + possLogs[i])
            ++i; // some schemes such as Simple16 don't code numbers greater
                 // than 28

        for (; i < possLogs.size() - 1; i++) {
            const uint32_t csize = tryB(possLogs[i], in, len);

            if (csize <= bsize) {
                b = possLogs[i];
                bsize = csize;
            }
        }
        return b;
    }

    void encodeBlock(const uint32_t* in, uint32_t* out, size_t& nvalue) {
        const uint32_t len = BlockSize;

        uint32_t b = findBestB(in, len);
        if (b < 32) {

            uint32_t nExceptions = 0;
            size_t encodedExceptions_sz = 0;

            const uint32_t* const initout(out);
            for (uint32_t i = 0; i < len; i++) {

                if (in[i] >= (1U << b)) {
                    tobecoded[i] = in[i] & ((1U << b) - 1);
                    exceptionsPositions[nExceptions] = i;
                    exceptionsValues[nExceptions] = (in[i] >> b);
                    nExceptions++;
                } else {
                    tobecoded[i] = in[i];
                }
            }

            if (nExceptions > 0) {
                for (uint32_t i = nExceptions - 1; i > 0; i--) {
                    const uint32_t cur = exceptionsPositions[i];
                    const uint32_t prev = exceptionsPositions[i - 1];

                    exceptionsPositions[i] = cur - prev;
                }

                for (uint32_t i = 0; i < nExceptions; i++) {

                    exceptions[i] = (i > 0) ? exceptionsPositions[i] - 1
                                            : exceptionsPositions[i];
                    exceptions[i + nExceptions] = exceptionsValues[i] - 1;
                }

                simple16.encodeArray(&exceptions[0], 2 * nExceptions, out + 1,
                                     encodedExceptions_sz);
            }

            *out++ = (b << (PFORDELTA_NEXCEPT + PFORDELTA_EXCEPTSZ)) |
                     (nExceptions << PFORDELTA_EXCEPTSZ) |
                     static_cast<uint32_t>(encodedExceptions_sz);
            /* Write exceptional values */

            out += static_cast<uint32_t>(encodedExceptions_sz);
            for (uint32_t i = 0; i < len; i += 32) {
                fastpackwithoutmask(&tobecoded[i], out, b);
                out += b;
            }
            nvalue = out - initout;

        } else {
            *out++ = (b << (PFORDELTA_NEXCEPT + PFORDELTA_EXCEPTSZ));
            for (uint32_t i = 0; i < len; i++)
                out[i] = in[i];
            nvalue = len + 1;
        }
    }

    const uint32_t* decodeBlock(const uint32_t* in, uint32_t* out,
                                size_t& nvalue) {
        const uint32_t* const initout(out);
        const uint32_t b = *in >> (32 - PFORDELTA_B);
        const size_t nExceptions =
            (*in >> (32 - (PFORDELTA_B + PFORDELTA_NEXCEPT))) &
            ((1 << PFORDELTA_NEXCEPT) - 1);
        const uint32_t encodedExceptionsSize =
            *in & ((1 << PFORDELTA_EXCEPTSZ) - 1);

        size_t twonexceptions = 2 * nExceptions;
        ++in;
        if (encodedExceptionsSize > 0)
            simple16.decodeArray(in, encodedExceptionsSize, &exceptions[0],
                                 twonexceptions);
        assert(twonexceptions >= 2 * nExceptions);
        in += encodedExceptionsSize;

        uint32_t* beginout(out); // we use this later

        for (uint32_t j = 0; j < BlockSize; j += 32) {
            fastunpack(in, out, b);
            in += b;
            out += 32;
        }

        for (uint32_t e = 0, lpos = -1; e < nExceptions; e++) {
            lpos += exceptions[e] + 1;
            beginout[lpos] |= (exceptions[e + nExceptions] + 1) << b;
        }

        nvalue = out - initout;
        return in;
    }

  public:
    inline size_t encode(const uint32_t* in, size_t n, uint32_t* out) {
        size_t num_blocks = n / t_block_size;
        size_t total_u32_written = 0;
        for (size_t i = 0; i < num_blocks; i++) {
            size_t u32_written = n * 2;
            encodeBlock(in, out, u32_written);
            total_u32_written += u32_written;
            out += u32_written;
            in += t_block_size;
        }
        return total_u32_written;
    }

    inline void decode(const uint32_t* in, size_t encoding_u32, uint32_t* out,
                       size_t n) {
        size_t num_blocks = n / t_block_size;
        size_t num_encoded = t_block_size;
        for (size_t i = 0; i < num_blocks; i++) {
            in = decodeBlock(in, out, num_encoded);
            out += t_block_size;
        }
    }
};

size_t op4_encode(const uint32_t* in, size_t n, uint32_t* out) {
    // (1) write length later
    uint32_t* output_len = out;
    out++;
    // (2) delegate to FastPFor code
    static op4<128> op4_coder;
    uint32_t written_u32 = op4_coder.encode(in, n, out);
    *output_len = written_u32;
    return written_u32 + 1; // for encoding size and list len
}

size_t op4_decode(const uint32_t* in, uint32_t* out, uint32_t list_len) {
    uint32_t encoding_u32 = *in++;
    static op4<128> op4_coder;
    op4_coder.decode(in, encoding_u32, out, list_len);
    return encoding_u32 + 1; // returns consumed u32s
}
