#pragma once

#include <array>
#include <memory>

#include "util.hpp"

namespace constants {
const uint32_t PAYLOAD_BITS = 60;
const uint32_t NUM_SELECTORS = 16;
const uint32_t MAX_MAGNITUDE = 25;
const uint32_t WINDOW_SIZE = 65;
using sel_type = const std::array<uint8_t, constants::NUM_SELECTORS>;
sel_type S = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 19, 22, 25 };
}

using L_type = std::array<std::array<uint64_t, constants::MAX_MAGNITUDE>,
    constants::NUM_SELECTORS>;

#define MEDIAN(x, y, z)                                                        \
    (x <= y ? (z < x ? x : (z > y ? y : z)) : (z < y ? y : (z > x ? x : z)))

uint8_t magnitude(uint64_t x)
{
    if (x == 1)
        return 0;
    uint32_t res = 63 - __builtin_clzll(x);
    return res + 1;
}

uint8_t round_up(uint8_t cur_mag)
{
    uint8_t sel = constants::S.back();
    if (cur_mag < sel) {
        sel = cur_mag;
    }
    while (sel > 0 && constants::S[sel - 1] >= cur_mag) {
        sel--;
    }
    return sel;
}

uint8_t estimator(std::vector<uint8_t>& bin_mags, uint64_t cur_pos)
{
    uint64_t cur_mag = bin_mags[cur_pos];

    // (1) go left WINDOW_SIZE/2 till we would fill 64bit
    uint64_t left_mag = cur_mag;
    {
        auto steps = 1;
        while (steps != constants::WINDOW_SIZE / 2) {
            auto new_mag = left_mag;
            if (new_mag < bin_mags[cur_pos - steps]) {
                new_mag = bin_mags[cur_pos - steps];
            }
            if (new_mag * (steps + 1) > constants::PAYLOAD_BITS) {
                break;
            }
            left_mag = new_mag;
            steps++;
        }
        printf("%lu left_mag = %u [", cur_pos, left_mag);
        for (size_t i = 1; i <= constants::WINDOW_SIZE / 2; i++) {
            printf("%u,", bin_mags[cur_pos - i]);
        }
        printf("]\n");
    }

    // (2) go left and right WINDOW_SIZE/4 till we fill 64bit
    auto center_mag = cur_mag;
    {
        auto steps = 1;
        while (steps != constants::WINDOW_SIZE / 2) {
            auto new_mag = left_mag;
            if (new_mag < bin_mags[cur_pos - steps]) {
                new_mag = bin_mags[cur_pos - steps];
            }
            if (new_mag < bin_mags[cur_pos + steps]) {
                new_mag = bin_mags[cur_pos + steps];
            }
            if (new_mag * (2 * steps + 1) > constants::PAYLOAD_BITS) {
                break;
            }
            center_mag = new_mag;
            steps++;
        }
        printf("%lu center_mag = %u [", cur_pos, center_mag);
        for (size_t i = 1; i <= constants::WINDOW_SIZE; i++) {
            printf("%u,", bin_mags[cur_pos - constants::WINDOW_SIZE / 2 + i]);
        }
        printf("]\n");
    }

    // (3) go right WINDOW_SIZE/2 till we fill 64bit
    auto right_mag = cur_mag;
    {
        auto steps = 1;
        while (steps != constants::WINDOW_SIZE / 2) {
            auto new_mag = right_mag;
            if (new_mag < bin_mags[cur_pos + steps]) {
                new_mag = bin_mags[cur_pos + steps];
            }
            if (new_mag * (steps + 1) > constants::PAYLOAD_BITS) {
                break;
            }
            right_mag = new_mag;
            steps++;
        }
        printf("%lu right_mag = %u [", cur_pos, right_mag);
        for (size_t i = 1; i <= constants::WINDOW_SIZE / 2; i++) {
            printf("%u,", bin_mags[cur_pos + i]);
        }
        printf("]\n");
    }

    // (4) take the median of the 3 windows
    return MEDIAN(left_mag, center_mag, right_mag);
}

L_type compute_magnitude_estimates(const list_data& ld)
{
    L_type L = { { 0 } };
    static std::vector<uint8_t> bin_mags;
    for (size_t i = 0; i < ld.num_lists; i++) {
        const auto& cur_list = ld.list_ptrs[i];
        size_t list_len = ld.list_sizes[i];
        // (1) compute magnitude for list
        if (bin_mags.size() < list_len) {
            bin_mags.resize(list_len);
        }
        for (size_t k = 0; k < list_len; k++) {
            bin_mags[k] = magnitude(cur_list[k]);
        }
        // (2) with sliding window fill the L table
        auto last = list_len - constants::WINDOW_SIZE / 2;
        for (size_t k = constants::WINDOW_SIZE / 2; k < last; k++) {
            auto l_k = round_up(estimator(bin_mags, k));
            L[bin_mags[k]][l_k]++;
        }
    }
    return L;
}

struct frame_parameters {
    int a;
};

frame_parameters compute_frame_parameters(L_type& L)
{
    frame_parameters fp;

    return fp;
}

struct ans_simple {
public: // method params
    bool required_increasing = true;
    std::string name() { return "ans_simple"; }
private: // config stuff
public:
    void init(const list_data& ld, uint32_t* out, size_t& written_u32)
    {
        auto L = compute_magnitude_estimates(ld);

        auto frame_params = compute_frame_parameters(L);

        // (3) encode frame parameters
    }
    const uint32_t* dec_init(const uint32_t* in) { return in; }

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
    }
    uint32_t* decodeArray(const uint32_t* in, const size_t enc_u32,
        uint32_t* out, size_t list_len)
    {
        return out;
    }
};
