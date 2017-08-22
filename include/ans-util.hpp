#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

#include "bits.hpp"
#include "util.hpp"

#include "ans-constants.hpp"

inline uint8_t ans_median(uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t med = 0;
    if ((a <= b && b <= c) || (c <= b && b <= a)) {
        med = b;
    } else if ((b <= a && a <= c) || (c <= a && a <= b)) {
        med = a;
    } else {
        med = c;
    }
    return med;
}

uint32_t ans_max_val_in_mag(
    uint8_t mag, uint32_t max_val = std::numeric_limits<uint32_t>::max())
{
    uint32_t maxv = 1;
    if (mag != 0)
        maxv = (1ULL << (mag));
    if (maxv > max_val)
        maxv = max_val;
    return maxv;
}

uint32_t ans_min_val_in_mag(uint8_t mag)
{
    if (mag == 0)
        return 1;
    return (1ULL << (mag - 1)) + 1;
}

uint32_t ans_uniq_vals_in_mag(
    uint8_t mag, uint32_t max_val = std::numeric_limits<uint32_t>::max())
{
    return ans_max_val_in_mag(mag, max_val) - ans_min_val_in_mag(mag) + 1;
}

uint8_t ans_magnitude(uint32_t x)
{
    uint64_t y = x;
    if (x == 1)
        return 0;
    uint32_t res = 63 - __builtin_clzll(y);
    if ((1ULL << res) == y)
        return res;
    return res + 1;
}

uint64_t next_power_of_two(uint64_t x)
{
    if (x == 0) {
        return 1;
    }
    uint32_t res = 63 - __builtin_clzll(x);
    return (1ULL << (res + 1));
}

bool is_power_of_two(uint64_t x) { return ((x != 0) && !(x & (x - 1))); }

template <class t_itr>
void print_array(
    t_itr itr, size_t n, const char* name, std::string format = "%u")
{
    fprintf(stderr, "%s: [", name);
    for (size_t i = 0; i < n; i++) {
        fprintf(stderr, format.c_str(), *itr);
        if (i + 1 != n)
            fprintf(stderr, ",");
        ++itr;
    }
    fprintf(stderr, "]\n");
}

template <class t_vec>
t_vec normalize_power_of_two_alistair(const t_vec& mag_freqs, uint32_t max_val)
{
    uint64_t initial_sum = 0;
    for (size_t i = 0; i < mag_freqs.size(); i++) {
        initial_sum += mag_freqs[i] * ans_uniq_vals_in_mag(i, max_val);
    }

    t_vec freqs = mag_freqs;
    uint8_t max_mag = 0;
    for (size_t i = 0; i < freqs.size(); i++) {
        if (freqs[i] != 0)
            max_mag = i;
    }
    /* first phase in scaling process, distribute out the
       last bucket, assume it is the smallest n(s) area, scale
       the rest by the same amount */
    auto bucket_size = ans_uniq_vals_in_mag(max_mag, max_val);
    double C = 0.5 * bucket_size / freqs[max_mag];
    for (size_t m = 0; m <= max_mag; m++) {
        bucket_size = ans_uniq_vals_in_mag(m, max_val);
        freqs[m] = 0.5 + freqs[m] * C / bucket_size;
        if (mag_freqs[m] != 0 && freqs[m] < 1) {
            freqs[m] = 1;
        }
    }

    /* second step in scaling process, to make the first freq
       less than or equal to TOPFREQ */
    if (freqs[0] > constants::TOPFREQ) {
        C = 1.0 * constants::TOPFREQ / freqs[0];
        freqs[0] = constants::TOPFREQ;
        /* scale all the others, rounding up so not zero anywhere,
           and at the same time, spread right across the bucketed
           range */
        for (uint8_t m = 1; m <= max_mag; m++) {
            freqs[m] = 0.5 + freqs[m] * C;
            if (mag_freqs[m] != 0 && freqs[m] < 1) {
                freqs[m] = 1;
            }
        }
    }

    /* now, what does it all add up to? */
    uint64_t M = 0;
    for (size_t m = 0; m <= max_mag; m++) {
        M += freqs[m] * ans_uniq_vals_in_mag(m, max_val);
    }
    /* fourth phase, round up to a power of two and then redistribute */
    uint64_t target_power = next_power_of_two(M);
    uint64_t excess = target_power - M;

    /* flow that excess count backwards to the beginning of
       the selectors array, spreading it out across the buckets...
    */
    for (int8_t m = int8_t(max_mag); m >= 0; m--) {
        double ratio = 1.0 * excess / M;
        uint64_t adder = ratio * freqs[m];
        uint64_t sub = ans_uniq_vals_in_mag(m, max_val) * adder;
        if (sub <= excess) {
            excess -= sub;
            M -= sub;
            freqs[m] += adder;
        }
    }
    if (excess != 0) {
        freqs[0] += excess;
    }
    M = 0;
    for (size_t i = 0; i <= max_mag; i++) {
        M += int64_t(freqs[i] * ans_uniq_vals_in_mag(i, max_val));
    }

    if (!is_power_of_two(M)) {
        quit("ERROR! not power of 2 after normalization = %lu", M);
    }

    return freqs;
}

template <class t_vec>
std::vector<uint64_t> normalize_freqs_power_of_two_alistair(
    const t_vec& freqs, size_t target_power)
{
    std::vector<uint64_t> nfreqs(freqs.begin(), freqs.end());
    uint32_t n = 0;
    uint64_t initial_sum = 0;
    for (size_t i = 1; i < freqs.size(); i++) {
        if (freqs[i] != 0) {
            n = i + 1;
            initial_sum += freqs[i];
        }
    }
    /* first phase in scaling process, distribute out the
       last bucket, assume it is the smallest n(s) area, scale
       the rest by the same amount */
    double C = double(target_power) / double(initial_sum);
    for (size_t i = 1; i < n; i++) {
        nfreqs[i] = 0.95 * nfreqs[i] * C;
        if (freqs[i] != 0 && nfreqs[i] < 1) {
            nfreqs[i] = 1;
        }
    }

    /* now, what does it all add up to? */
    uint64_t M = 0;
    for (size_t m = 0; m < n; m++) {
        M += nfreqs[m];
    }
    /* fourth phase, round up to a power of two and then redistribute */
    uint64_t excess = target_power - M;
    /* flow that excess count backwards to the beginning of
       the selectors array, spreading it out across the buckets...
    */
    for (int64_t m = int64_t(n - 1); m >= 1; m--) {
        double ratio = double(excess) / double(M);
        uint64_t adder = ratio * nfreqs[m];
        if (adder > excess) {
            adder = excess;
        }
        excess -= adder;
        M -= nfreqs[m];
        nfreqs[m] += adder;
    }
    // print_array(nfreqs.begin(), n, "NC");
    if (excess != 0) {
        nfreqs[0] += excess;
    }

    M = 0;
    for (size_t i = 0; i < n; i++) {
        M += nfreqs[i];
    }
    if (!is_power_of_two(M)) {
        quit("ERROR! not power of 2 after normalization = %lu", M);
    }

    return nfreqs;
}

template <uint32_t i> inline uint8_t ans_extract7bits(const uint64_t val)
{
    uint8_t v = static_cast<uint8_t>((val >> (7 * i)) & ((1ULL << 7) - 1));
    return v;
}

template <uint32_t i>
inline uint8_t ans_extract7bitsmaskless(const uint64_t val)
{
    uint8_t v = static_cast<uint8_t>((val >> (7 * i)));
    return v;
}

inline void ans_vbyte_encode_u64(uint8_t*& out, uint64_t x)
{
    if (x < (1ULL << 7)) {
        *out++ = static_cast<uint8_t>(x & 127);
    } else if (x < (1ULL << 14)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bitsmaskless<1>(x) & 127;
    } else if (x < (1ULL << 21)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bitsmaskless<2>(x) & 127;
    } else if (x < (1ULL << 28)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bitsmaskless<3>(x) & 127;
    } else if (x < (1ULL << 35)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bitsmaskless<4>(x) & 127;
    } else if (x < (1ULL << 42)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bitsmaskless<5>(x) & 127;
    } else if (x < (1ULL << 49)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bits<5>(x) | 128;
        *out++ = ans_extract7bitsmaskless<6>(x) & 127;
    } else if (x < (1ULL << 56)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bits<5>(x) | 128;
        *out++ = ans_extract7bitsmaskless<6>(x) & 127;
    } else {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bits<5>(x) | 128;
        *out++ = ans_extract7bits<6>(x) | 128;
        *out++ = ans_extract7bitsmaskless<7>(x) & 127;
    }
}

inline uint8_t ans_vbyte_size(uint64_t x)
{
    if (x < (1ULL << 7)) {
        return 1;
    } else if (x < (1ULL << 14)) {
        return 2;
    } else if (x < (1ULL << 21)) {
        return 3;
    } else if (x < (1ULL << 28)) {
        return 4;
    } else if (x < (1ULL << 35)) {
        return 5;
    } else if (x < (1ULL << 42)) {
        return 6;
    } else if (x < (1ULL << 49)) {
        return 7;
    } else if (x < (1ULL << 56)) {
        return 8;
    } else {
        return 9;
    }
    return 9;
}

inline uint64_t ans_vbyte_decode_u64(const uint8_t*& input, size_t& enc_size)
{
    uint64_t x = 0;
    uint64_t shift = 0;
    while (true) {
        uint8_t c = *input++;
        enc_size--;
        x += (uint64_t(c & 127) << shift);
        if (!(c & 128)) {
            return x;
        }
        shift += 7;
    }
    return x;
}

inline uint64_t ans_vbyte_decode_u64(const uint8_t*& input)
{
    size_t enc_size = 123123123;
    return ans_vbyte_decode_u64(input, enc_size);
}
