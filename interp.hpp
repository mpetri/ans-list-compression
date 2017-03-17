#pragma once

#include <vector>

#include "bits.hpp"
#include "util.hpp"

static std::vector<uint64_t> M;
static std::vector<uint64_t> B;
static std::vector<uint64_t> U;
static std::vector<uint64_t> V;
static std::vector<uint64_t> D;
static std::vector<uint64_t> F;
static std::vector<int64_t> Bit;

static std::vector<uint64_t> H;
static std::vector<uint64_t> LOW;
static std::vector<uint64_t> HIGH;
static std::vector<uint64_t> N1;
static std::vector<uint64_t> N2;
static std::vector<uint64_t> EV;
static std::vector<uint64_t> B1;

struct interpolative {

private:
    static inline void write_center_mid(
        bit_stream& os, uint64_t val, uint64_t u)
    {
        if (u == 1)
            return;
        auto b = bits::hi(u - 1) + 1ULL;
        uint64_t d = 2ULL * u - (1ULL << b);
        val = val + (u - d / 2);
        if (val > u)
            val -= u;
        uint32_t m = (1ULL << b) - u;
        if (val <= m) {
            os.put_int(val - 1, b - 1);
        } else {
            val += m;
            os.put_int((val - 1) >> 1, b - 1);
            os.put_int((val - 1) & 1, 1);
        }
    }
    static inline uint64_t read_center_mid(bit_stream& is, uint64_t u)
    {
        auto b = u == 1 ? 0 : bits::hi(u - 1) + 1ULL;
        auto d = 2ULL * u - (1ULL << b);
        uint64_t val = 1;
        if (u != 1) {
            uint64_t m = (1ULL << b) - u;
            val = is.get_int(b - 1) + 1;
            if (val > m) {
                val = (2ULL * val + is.get_int(1)) - m - 1;
            }
        }
        val = val + d / 2;
        if (val > u)
            val -= u;
        return val;
    }

    static inline void encode_interpolative(bit_stream& os,
        const uint32_t* in_buf, size_t n, size_t low, size_t high)
    {
        if (n == 0ULL)
            return;
        uint64_t h = (n + 1ULL) >> 1;
        uint64_t n1 = h - 1ULL;
        uint64_t n2 = n - h;
        uint64_t v = in_buf[h - 1ULL] + 1ULL; // we don't encode 0

        LOW.push_back(low);
        HIGH.push_back(high);
        H.push_back(h);
        N1.push_back(n1);
        N2.push_back(n2);
        EV.push_back(v);

        write_center_mid(os, v - low - n1 + 1, high - n2 - low - n1 + 1ULL);

        encode_interpolative(os, in_buf, n1, low, v - 1ULL);
        encode_interpolative(os, in_buf + h, n2, v + 1ULL, high);
    }

    static inline void decode_interpolative(
        bit_stream& is, uint32_t* out_buf, size_t n, size_t low, size_t high)
    {
        if (n == 0ULL)
            return;
        uint64_t h = (n + 1ULL) >> 1ULL;
        uint64_t n1 = h - 1ULL;
        uint64_t n2 = n - h;
        uint64_t v = low + n1 - 1ULL
            + read_center_mid(is, high - n2 - low - n1 + 1ULL);

        static int i = 0;
        auto otherH = H[i];
        auto otherLOW = LOW[i];
        auto otherHIGH = HIGH[i];
        auto otherN1 = N1[i];
        auto otherN2 = N2[i];
        auto otherV = EV[i++];

        // if (h != otherH || low != otherLOW || high != otherHIGH || n1 !=
        // otherN1
        //     || n2 != otherN2 || v != otherV) {
        //     printf("%d h=%llu other=%llu\n", i - 1, h, otherH);
        //     printf("%d low=%llu other=%llu\n", i - 1, low, otherLOW);
        //     printf("%d high=%llu other=%llu\n", i - 1, high, otherHIGH);
        //     printf("%d n1=%llu other=%llu\n", i - 1, n1, otherN1);
        //     printf("%d n2=%llu other=%llu\n", i - 1, n2, otherN2);
        //     printf("%d v=%llu other=%llu\n", i - 1, v, otherV);
        // }

        out_buf[h - 1] = v - 1; // we don't encode 0
        if (n1)
            decode_interpolative(is, out_buf, n1, low, v - 1ULL);
        if (n2)
            decode_interpolative(is, out_buf + h, n2, v + 1ULL, high);
    }

public:
    static inline size_t encode(
        uint32_t* f, const uint32_t* in_buf, size_t n, size_t u)
    {
        bit_stream os(f, true);
        size_t low = 1;
        size_t high = u + 1;
        encode_interpolative(os, in_buf, n, low, high);
        return os.flush();
    }

    static inline size_t decode(
        const uint32_t* f, uint32_t* out_buf, size_t n, size_t u)
    {
        bit_stream is((uint32_t*)f, false);
        size_t low = 1;
        size_t high = u + 1;
        decode_interpolative(is, out_buf, n, low, high);
        return is.u32_read();
    }
};

size_t interp_encode(const uint32_t* in, size_t n, uint32_t* out)
{
    // (1) write length later
    *out++ = in[n - 1];
    // (2) delegate to FastPFor code
    uint32_t bw = interpolative::encode(out, in, n - 1, in[n - 1]);
    return (bw / sizeof(uint32_t)) + 1; // for list len and universe
}

size_t interp_decode(const uint32_t* in, uint32_t* out, uint32_t list_len)
{
    uint32_t universe = *in++;
    size_t u32_read = interpolative::decode(in, out, list_len - 1, universe);
    out[list_len - 1] = universe;
    return u32_read + 1; // returns consumed u32s -> incorrect?
}
