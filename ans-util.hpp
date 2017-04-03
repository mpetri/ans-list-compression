#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "bits.hpp"
#include "util.hpp"

uint32_t ans_max_val_in_mag(uint8_t mag)
{
    if (mag == 0)
        return 1;
    return (1ULL << (mag));
}

uint32_t ans_min_val_in_mag(uint8_t mag)
{
    if (mag == 0)
        return 1;
    return (1ULL << (mag - 1)) + 1;
}

uint32_t ans_uniq_vals_in_mag(uint8_t mag)
{
    return ans_max_val_in_mag(mag) - ans_min_val_in_mag(mag) + 1;
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
t_vec normalize_power_of_two_alistair(const t_vec& mag_freqs)
{
    t_vec freqs = mag_freqs;
    uint8_t n = 0;
    for (size_t i = 0; i < freqs.size(); i++) {
        if (freqs[i] != 0)
            n = i + 1;
    }
    print_array(freqs.begin(), n, "F0");
    /* first phase in scaling process, distribute out the
       last bucket, assume it is the smallest n(s) area, scale
       the rest by the same amount */
    auto bucket_size = ans_uniq_vals_in_mag(n - 1);
    double C = 0.5 * bucket_size / freqs[n - 1];
    fprintf(stderr, "bucket_max = %lu C = %lf\n", bucket_size, C);
    for (size_t m = 0; m < n; m++) {
        bucket_size = ans_uniq_vals_in_mag(m);
        freqs[m] = 0.5 + freqs[m] * C / bucket_size;
        if (freqs[m] < 1) {
            freqs[m] = 1;
        }
    }
    print_array(freqs.begin(), n, "F1");

    /* now, what does it all add up to? */
    uint64_t M = 0;
    for (size_t m = 0; m < n; m++) {
        M += freqs[m] * ans_uniq_vals_in_mag(m);
    }
    /* fourth phase, round up to a power of two and then redistribute */
    uint64_t target_power = next_power_of_two(M);
    uint64_t excess = target_power - M;
    fprintf(stderr, "M = %lu TP = %lu E = %lu\n", M, target_power, excess);
    /* flow that excess count backwards to the beginning of
       the selectors array, spreading it out across the buckets...
    */
    for (int8_t m = int8_t(n - 1); m >= 0; m--) {
        double ratio = 1.0 * excess / M;
        uint64_t adder = ratio * freqs[m];
        excess -= ans_uniq_vals_in_mag(m) * adder;
        M -= ans_uniq_vals_in_mag(m) * freqs[m];
        freqs[m] += adder;
    }
    fprintf(stderr, "M = %lu TP = %lu E = %lu\n", M, target_power, excess);
    print_array(freqs.begin(), n, "NC");

    if (excess != 0) {
        freqs[0] += excess;
    }

    M = 0;
    for (size_t i = 0; i < n; i++) {
        M += int64_t(freqs[i] * ans_uniq_vals_in_mag(i));
    }
    if (!is_power_of_two(M)) {
        quit("ERROR! not power of 2 after normalization = %lu", M);
    }

    return freqs;
}

template <class t_vec> t_vec normalize_power_of_two(const t_vec& mags)
{
    t_vec pot_cnts = mags;
    // (1) compute desired probs
    uint64_t total_cnt = std::accumulate(mags.begin(), mags.end(), 0ULL);
    std::vector<double> desired_probs(mags.size());
    uint8_t max_freq = 0;
    uint8_t max_mag = 0;
    for (size_t i = 0; i < mags.size(); i++) {
        desired_probs[i] = double(mags[i]) / double(total_cnt);
        if (mags[i] > mags[max_freq])
            max_freq = i;
        if (mags[i] != 0)
            max_mag = i;
    }

    // (2) compute the initial counts
    uint64_t initial_sum = 0;
    uint16_t max_val = std::min(uint32_t((max_mag + 1) * 512),
        uint32_t(std::numeric_limits<uint16_t>::max()));
    uint32_t value_sum = (1 / desired_probs[max_freq]) * max_val;
    for (size_t i = 0; i < mags.size(); i++) {
        initial_sum += (desired_probs[i] * value_sum) / ans_uniq_vals_in_mag(i);
    }
    uint64_t target_power = next_power_of_two(initial_sum);
    uint64_t max_target_power = 1ULL << 26;
    size_t i = 0;
    while (i < mags.size()) {
        if (mags[i] != 0) {
            double cnt
                = (target_power * desired_probs[i]) / ans_uniq_vals_in_mag(i);
            pot_cnts[i] = cnt;
            if (pot_cnts[i] == 0) {
                // always has to be at least 1. try to increase target power and
                // retry
                if (target_power != max_target_power) {
                    target_power = next_power_of_two(target_power);
                    i = 0;
                    continue;
                } else {
                    pot_cnts[i] = 1;
                }
            }
        } else {
            pot_cnts[i] = 0;
        }
        ++i;
    }
    int64_t cur_cnt = 0;
    for (size_t i = 0; i < mags.size(); i++) {
        cur_cnt += int64_t(pot_cnts[i] * ans_uniq_vals_in_mag(i));
    }
    int64_t difference = target_power - cur_cnt;
    if (difference != 0) {
        if (difference < 0) { // we have to decrease some counts
            uint64_t decrease_total = uint64_t(-difference);
            uint64_t decrease_per_mag = decrease_total / (max_mag);
            for (size_t i = max_mag; i != 0; i--) {
                uint64_t val = decrease_per_mag / ans_uniq_vals_in_mag(i);
                if (val >= pot_cnts[i])
                    val = pot_cnts[i] - 1;
                pot_cnts[i] -= val;
                decrease_total -= val * ans_uniq_vals_in_mag(i);
            }
            if (decrease_total) {
                pot_cnts[0] -= decrease_total;
            }
        } else { // we have to increase some counts
            uint64_t increase_total = difference;
            uint64_t increase_per_mag = increase_total / (max_mag);
            for (size_t i = max_mag; i != 0; i--) {
                pot_cnts[i] += increase_per_mag / ans_uniq_vals_in_mag(i);
                increase_total -= (increase_per_mag / ans_uniq_vals_in_mag(i))
                    * ans_uniq_vals_in_mag(i);
            }
            if (increase_total) {
                pot_cnts[0] += increase_total;
            }
        }
    }

    // (3) adjust to powers of two
    cur_cnt = 0;
    for (size_t i = 0; i < mags.size(); i++) {
        cur_cnt += int64_t(pot_cnts[i] * ans_uniq_vals_in_mag(i));
    }
    int64_t diff = int64_t(target_power) - cur_cnt;
    if (diff != 0) {
        quit("ERROR! not power of 2 after normalization = %ld", diff);
    }

    return pot_cnts;
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

inline uint64_t ans_vbyte_decode_u64(const uint8_t*& input)
{
    uint64_t x = 0;
    uint64_t shift = 0;
    while (true) {
        uint8_t c = *input++;
        x += (uint64_t(c & 127) << shift);
        if (!(c & 128)) {
            return x;
        }
        shift += 7;
    }
    return x;
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

inline uint8_t ans_eliasdelta_size(uint64_t x)
{
    uint8_t len_x = bits::hi(x);
    uint8_t len_len = bits::hi(len_x);
    uint8_t len_bits = 2 * len_len + 1;
    uint8_t bits = len_bits + len_x;
    uint8_t bytes = (bits >> 3) + ((bits & 7) != 0);
    return bytes;
}

static const uint8_t hi_set8[9]
    = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF };

void write_unary(uint8_t*& out, uint8_t& in_byte_offset, uint64_t x)
{
    uint32_t tmp_buf[4] = { 0 };
    uint32_t* ptr = tmp_buf;
    uint8_t in_word_offset = in_byte_offset;
    bits::write_unary_and_move(ptr, x, in_word_offset);
    uint64_t bits_written = (ptr - tmp_buf) * 32;
    bits_written += in_word_offset - in_byte_offset;
    uint8_t* optr = (uint8_t*)tmp_buf;
    size_t left_in_byte = 8 - in_byte_offset;
    if (left_in_byte >= bits_written) {
        uint8_t cur_byte = *out;
        *out = cur_byte ^ ((*optr) & hi_set8[left_in_byte]);
        in_byte_offset += bits_written;
        return;
    }

    if (in_byte_offset != 0) {
        uint8_t cur_byte = *out;
        *out++ = cur_byte ^ ((*optr) & hi_set8[left_in_byte]);
        bits_written -= left_in_byte;
        optr++;
    }
    while (bits_written >= 8) {
        *out++ = *optr++;
        bits_written -= 8;
    }
    if (bits_written != 0) {
        *out = *optr;
    }
    in_byte_offset = bits_written;
}

uint64_t read_unary(const uint8_t*& in, uint8_t& in_byte_offset)
{
    const uint32_t* in32 = (const uint32_t*)in;
    uint8_t in_word_offset = in_byte_offset;
    uint64_t x = bits::read_unary_and_move(in32, in_word_offset);
    while ((const uint8_t*)in32 != in) {
        ++in;
    }
    while (in_word_offset > 7) {
        ++in;
        in_word_offset -= 8;
    }
    in_byte_offset = in_word_offset;
    return x;
}

void write_int8(uint8_t*& out, uint8_t& in_byte_offset, uint64_t x, uint8_t len)
{
    uint32_t* initout32 = (uint32_t*)out;
    uint32_t* ptr = initout32;
    uint8_t in_word_offset = in_byte_offset;
    if (len > 32) {
        bits::write_int(ptr, x & 0xFFFFFFFF, in_word_offset, 32);
        x = x >> 32;
        len -= 32;
    }
    bits::write_int(ptr, x, in_word_offset, len);
    uint64_t words_written = (ptr - initout32);
    out += (4 * words_written);
    out += (in_word_offset / 8);
    in_byte_offset = in_word_offset & 7;
}

uint64_t read_int8(const uint8_t*& in, uint8_t& in_byte_offset, uint8_t len)
{
    const uint32_t* in32 = (const uint32_t*)in;
    uint8_t in_word_offset = in_byte_offset;
    uint64_t x;
    if (len > 32) {
        uint64_t xa = bits::read_int(in32, in_word_offset, 32);
        uint64_t xb = bits::read_int(in32, in_word_offset, len - 32);
        x = (xb << 32) + xa;
    } else {
        x = bits::read_int(in32, in_word_offset, len);
    }

    while ((const uint8_t*)in32 != in) {
        ++in;
    }
    while (in_word_offset > 7) {
        ++in;
        in_word_offset -= 8;
    }
    in_byte_offset = in_word_offset;
    return x;
}

inline void ans_eliasdelta_encode_u64(uint8_t*& out, uint64_t x)
{
    uint64_t len_x = bits::hi(x) + 1;
    uint8_t len_len = bits::hi(len_x);
    // (1) unary encode len of len
    uint8_t in_byte_offset = 0;
    write_unary(out, in_byte_offset, len_len);
    // fprintff(stderr, "x %lu len_x %lu len_len %lu\n", x, len_x, len_len);
    if (len_len) {
        write_int8(out, in_byte_offset, len_x, len_len);
        // (2) write the lower part of x
        write_int8(out, in_byte_offset, x, len_x - 1);
    }
    if (in_byte_offset != 0) {
        out++;
    }
}

inline uint8_t ans_eliasdelta_bytes(uint64_t x)
{
    uint64_t len_x = bits::hi(x) + 1;
    uint8_t len_len = bits::hi(len_x);
    size_t bits = len_len + 1;
    if (len_len) {
        bits += len_len;
        bits += len_x - 1;
    }
    uint8_t bytes = (bits / 8) + (bits % 8 != 0);
    return bytes;
}

inline uint64_t ans_eliasdelta_decode_u64(const uint8_t*& in, size_t& enc_size)
{
    const uint8_t* initin = in;
    uint8_t in_byte_offset = 0;
    // (1) unary encode len of len
    uint8_t len_len = read_unary(in, in_byte_offset);
    uint64_t x = 1;
    if (len_len) {
        // (2) read the lower part of len_x
        uint64_t len_x = read_int8(in, in_byte_offset, len_len);
        len_x = len_x + (uint64_t(1) << len_len);
        // (2) read the lower part of x
        x = read_int8(in, in_byte_offset, len_x - 1);
        x = x + (uint64_t(1) << (len_x - 1));
    }

    if (in_byte_offset != 0) {
        in++;
    }
    enc_size -= (in - initin);
    return x;
}

inline uint64_t ans_eliasdelta_decode_u64(const uint8_t*& in)
{
    size_t enc_size = 123123123;
    return ans_eliasdelta_decode_u64(in, enc_size);
}
