#pragma once

#include <cmath>

#include "ans-constants.hpp"
#include "ans-mag.hpp"
#include "ans-util.hpp"

struct mag_enc_table_entry {
    uint32_t freq;
    uint64_t base;
    uint32_t SUB;
};

struct mag_dec_table_entry {
    uint32_t freq;
    uint64_t offset;
    uint32_t sym;
};

struct ans_mag_model_fast {
public:
    uint64_t M; // frame size
    std::vector<mag_enc_table_entry> enc_table;
    mag_table norm_mags = { { 0 } };
    uint8_t log2_M = 0;
    uint64_t mask_M = 0;
    uint64_t norm_lower_bound = 0;
    std::vector<mag_dec_table_entry> dec_table;
    uint64_t total_max_val = 0;

public:
    ans_mag_model_fast(const uint8_t*& in8)
    {
        total_max_val = ans_vbyte_decode_u64(in8);

        // (1) read the normalized magnitudes
        bool all_zero = true;
        for (size_t i = 0; i < norm_mags.size(); i++) {
            norm_mags[i] = ans_vbyte_decode_u64(in8);
            if (norm_mags[i] != 0)
                all_zero = false;
        }

        // (1a) empty model??
        if (all_zero)
            return;

        // (2) init the model
        init_model();
    }
    ans_mag_model_fast(const mag_table& mags, uint32_t maxv)
        : total_max_val(maxv)
    {
        // (0) if all is 0 do nothing
        if (std::all_of(mags.cbegin(), mags.cend(),
                [](uint64_t i) { return i == 0; })) {
            total_max_val = 0;
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
        enc_table.resize(total_max_val + 1);

        // (2) fill the tables
        uint64_t cumsum = 0;
        for (size_t i = 0; i < norm_mags.size(); i++) {
            if (norm_mags[i] == 0)
                continue;
            auto min_val = ans_min_val_in_mag(i);
            auto max_val = ans_max_val_in_mag(i, total_max_val);
            for (size_t j = min_val; j <= max_val; j++) {
                enc_table[j].freq = norm_mags[i];
                enc_table[j].base = cumsum;
                cumsum += enc_table[j].freq;
            }
        }
        M = cumsum;
        norm_lower_bound = constants::OUTPUT_BASE * M;
        for (size_t j = 1; j < enc_table.size(); j++) {
            enc_table[j].SUB = ((norm_lower_bound / M) * constants::OUTPUT_BASE)
                * enc_table[j].freq;
        }
        mask_M = M - 1;
        log2_M = log2(M);

        // create csum table for decoding
        dec_table.resize(M);
        size_t base = 0;
        for (size_t j = 1; j < enc_table.size(); j++) {
            auto cur_freq = enc_table[j].freq;
            for (size_t k = 0; k < cur_freq; k++) {
                dec_table[base + k].sym = j;
                dec_table[base + k].freq = cur_freq;
                dec_table[base + k].offset = k;
            }
            base += cur_freq;
        }
        fprintf(stderr, "M = %lu\n", M);
        if (cumsum != M) {
            fprintf(stderr, "cumsum %lu != M %lu\n", cumsum, M);
        }
    }
    uint64_t encode(uint64_t state, uint32_t num, uint8_t*& out8) const
    {
        const auto& entry = enc_table[num];
        uint64_t f = entry.freq;
        uint64_t b = entry.base;

        // (1) normalize
        while (state >= entry.SUB) {
            --out8;
            *out8 = (uint8_t)(state & 0xFF);
            state = state >> constants::OUTPUT_BASE_LOG2;
        }

        // (2) transform state
        auto state_divmod_f = lldiv(state, f);
        uint64_t state_div_f = state_divmod_f.quot;
        uint64_t state_mod_f = state_divmod_f.rem;
        uint64_t next = (state_div_f << log2_M) + state_mod_f + b;
        return next;
    }

    std::pair<uint64_t, uint64_t> try_encode_u64(
        const uint32_t* in, size_t n) const
    {
        uint64_t state = 0;
        uint64_t num_encoded = 0;
        for (size_t i = 0; i < n; i++) {
            auto num = in[i];
            if (num > total_max_val)
                break;
            const auto& entry = enc_table[num];
            if (entry.freq == 0)
                break;
            uint64_t f = entry.freq;
            uint64_t b = entry.base + 1;
            uint64_t r = state % f;
            uint64_t j = (state - r) / f;
            uint64_t new_state = 0;
            if (__builtin_umull_overflow(j, M, &new_state)) {
                break;
            }
            uint64_t tmp = r + b;
            uint64_t new_state_2 = 0;
            if (__builtin_uaddl_overflow(new_state, tmp, &new_state_2)) {
                break;
            }
            state = new_state_2;
            num_encoded++;
        }
        return { num_encoded, state };
    }

    uint64_t encode_u64(const uint32_t* in, size_t n) const
    {
        uint64_t state = 0;
        for (size_t i = 0; i < n; i++) {
            auto num = in[i];
            const auto& entry = enc_table[num];
            uint64_t f = entry.freq;
            uint64_t b = entry.base + 1;
            uint64_t r = state % f;
            uint64_t j = (state - r) / f;
            state = j * M + r + b;
        }
        return state;
    }

    void decode_u64(uint64_t state, uint32_t*& out) const
    {
        static std::vector<uint32_t> stack(constants::MAXSTACKSIZE);
        size_t num_decoded = 0;
        while (state > 0) {
            uint64_t r = 1ULL + ((state - 1ULL) & mask_M);
            uint64_t j = (state - r) >> log2_M;
            const auto& entry = dec_table[r - 1];
            uint64_t f = entry.freq;
            uint64_t b = entry.offset;
            stack[num_decoded++] = entry.sym;
            state = f * j + b;
        }
        for (size_t j = 0; j < num_decoded; j++) {
            *out++ = stack[num_decoded - j - 1];
        }
    }

    uint32_t decode(
        uint64_t& state, const uint8_t*& in8, size_t& enc_size) const
    {
        uint64_t state_mod_M = state & mask_M;
        const auto& entry = dec_table[state_mod_M];
        uint32_t sym = entry.sym;
        uint64_t f = entry.freq;
        state = f * (state >> log2_M) + entry.offset;
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
