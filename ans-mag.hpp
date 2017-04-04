#pragma once

#include "ans-constants.hpp"
#include "ans-util.hpp"

using mag_table = std::array<uint64_t, constants::MAX_MAG + 1>;

struct ans_mag_model {
public:
    uint64_t M; // frame size
    std::vector<uint32_t> normalized_freqs;
    std::vector<uint64_t> base;
    std::vector<uint64_t> sym_upper_bound;
    mag_table norm_mags;
    uint8_t log2_M;
    uint64_t mask_M;
    uint64_t norm_lower_bound;
    std::vector<uint32_t> csum2sym;
    uint64_t total_max_val;

public:
    ans_mag_model(const uint8_t*& in8)
    {
        total_max_val = ans_vbyte_decode_u64(in8);

        // (1) read the normalized magnitudes
        for (size_t i = 0; i < norm_mags.size(); i++) {
            norm_mags[i] = ans_vbyte_decode_u64(in8);
        }

        // (2) init the model
        init_model();
    }
    ans_mag_model(ans_mag_model&&) = default;
    ans_mag_model(const mag_table& mags, uint32_t maxv)
        : total_max_val(maxv)
    {
        // (0) if all is 0 do nothing
        if (std::all_of(mags.cbegin(), mags.cend(),
                [](uint64_t i) { return i == 0; })) {
            return;
        }

        // (1) normalize such that the normalized freqs sum to a power of 2
        norm_mags = normalize_power_of_two_alistair(mags, total_max_val);

        // (2) init the model params
        init_model();
    }

    void init_model()
    {
        // (1) allocate space
        normalized_freqs.resize(total_max_val + 1);
        base.resize(total_max_val + 1);
        sym_upper_bound.resize(total_max_val + 1);

        // (2) fill the tables
        uint64_t cumsum = 0;
        for (size_t i = 0; i < norm_mags.size(); i++) {
            if (norm_mags[i] == 0)
                continue;
            auto min_val = ans_min_val_in_mag(i);
            auto max_val = ans_max_val_in_mag(i, total_max_val);
            for (size_t j = min_val; j <= max_val; j++) {
                normalized_freqs[j] = norm_mags[i];
                base[j] = cumsum;
                cumsum += normalized_freqs[j];
            }
        }
        M = cumsum;
        norm_lower_bound = constants::OUTPUT_BASE * M;
        for (size_t j = 0; j < normalized_freqs.size(); j++) {
            sym_upper_bound[j]
                = ((norm_lower_bound / M) * constants::OUTPUT_BASE)
                * normalized_freqs[j];
        }
        mask_M = M - 1;
        log2_M = log2(M);
        // create csum table for decoding
        csum2sym.resize(M);
        cumsum = 0;
        for (size_t j = 0; j < normalized_freqs.size(); j++) {
            auto cur_freq = normalized_freqs[j];
            for (size_t i = 0; i < cur_freq; i++)
                csum2sym[cumsum + i] = j;
            cumsum += cur_freq;
        }
        if (cumsum != M) {
            fprintf(stderr, "cumsum %lu != M %lu\n", cumsum, M);
        }
    }
    uint64_t encode(uint64_t state, uint32_t num, uint8_t*& out8) const
    {
        uint64_t f = normalized_freqs[num];
        uint64_t b = base[num];
        // (1) normalize
        uint64_t SUB = sym_upper_bound[num];
        while (state >= SUB) {
            --out8;
            *out8 = (uint8_t)(state & 0xFF);
            state = state >> constants::OUTPUT_BASE_LOG2;
        }

        // (2) transform state
        uint64_t next = ((state / f) * M) + (state % f) + b;
        return next;
    }
    uint32_t decode(
        uint64_t& state, const uint8_t*& in8, size_t& enc_size) const
    {
        uint64_t state_mod_M = state & mask_M;
        uint32_t sym = csum2sym[state_mod_M];
        uint64_t f = normalized_freqs[sym];
        uint64_t b = base[sym];
        state = f * (state >> log2_M) + state_mod_M - b;
        while (enc_size && state < norm_lower_bound) {
            uint8_t new_byte = *in8++;
            state = (state << constants::OUTPUT_BASE_LOG2) | uint64_t(new_byte);
            enc_size--;
        }
        return sym;
    }
    uint64_t init_decoder(const uint8_t*& in8, size_t& enc_size) const
    {
        return ans_vbyte_decode_u64(in8, enc_size);
    }
    void flush(uint64_t final_state, uint8_t*& out8) const
    {
        auto vb_bytes = ans_vbyte_size(final_state);
        out8 -= vb_bytes;
        auto tmp = out8;
        ans_vbyte_encode_u64(tmp, final_state);
    }
    void write(uint8_t*& out8) const
    {
        ans_vbyte_encode_u64(out8, total_max_val);
        for (size_t i = 0; i < norm_mags.size(); i++) {
            ans_vbyte_encode_u64(out8, norm_mags[i]);
        }
    }
};
