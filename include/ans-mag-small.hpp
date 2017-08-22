#pragma once

#include <cmath>

#include "ans-constants.hpp"
#include "ans-mag.hpp"
#include "ans-util.hpp"

struct dec_base {
    uint64_t value;
    uint64_t mag;
};

struct ans_mag_model_small {
public:
    uint64_t M; // frame size
    mag_table norm_mags = { { 0 } };
    uint8_t log2_M = 0;
    uint64_t mask_M = 0;
    uint64_t norm_lower_bound = 0;
    uint64_t total_max_val = 0;
    std::vector<uint64_t> nfreq;
    std::vector<uint64_t> base;
    std::vector<uint64_t> SUB;
    std::vector<dec_base> dbase;

public:
    ans_mag_model_small(const uint8_t*& in8)
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
    ans_mag_model_small(const mag_table& mags, uint32_t maxv)
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
        nfreq.resize(norm_mags.size());
        base.resize(norm_mags.size());
        SUB.resize(norm_mags.size());

        // (2) fill the tables
        uint64_t cumsum = 0;
        size_t non_zero_mags = 0;
        for (size_t i = 0; i < norm_mags.size(); i++) {
            nfreq[i] = norm_mags[i];
            if (norm_mags[i] == 0)
                continue;
            non_zero_mags++;
            auto min_val = ans_min_val_in_mag(i);
            auto max_val = ans_max_val_in_mag(i, total_max_val);
            base[i] = cumsum;
            cumsum += (max_val - min_val + 1) * nfreq[i];
        }
        M = cumsum;
        norm_lower_bound = constants::OUTPUT_BASE * M;
        for (size_t i = 0; i <= norm_mags.size(); i++) {
            SUB[i]
                = ((norm_lower_bound / M) * constants::OUTPUT_BASE) * nfreq[i];
        }
        mask_M = M - 1;
        log2_M = log2(M);
        fprintf(stderr, "M = %lu (small)\n", M);
        if (cumsum != M) {
            fprintf(stderr, "cumsum %lu != M %lu\n", cumsum, M);
        }

        // create decode model
        size_t j = 0;
        dbase.resize(non_zero_mags);
        for (size_t i = 0; i < norm_mags.size(); i++) {
            if (nfreq[i] != 0) {
                dbase[j].value = base[i];
                dbase[j].mag = i;
                j++;
            }
        }
    }
    uint64_t encode(uint64_t state, uint32_t num, uint8_t*& out8) const
    {
        // (0) lookup quantities
        uint8_t mag = ans_magnitude(num);
        uint64_t min_val = ans_min_val_in_mag(mag);
        uint64_t vals_before = num - min_val;
        uint64_t f = nfreq[mag];
        uint64_t b = base[mag] + (f * vals_before);
        uint64_t sub = SUB[mag];
        // (1) normalize
        while (state >= sub) {
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

    uint64_t encode_u64(const uint32_t* in, size_t n) const
    {
        uint64_t state = 0;
        for (size_t i = 0; i < n; i++) {
            auto num = in[i];
            uint8_t mag = ans_magnitude(num);
            uint64_t min_val = ans_min_val_in_mag(mag);
            uint64_t vals_before = num - min_val;
            uint64_t f = nfreq[mag];
            uint64_t b = base[mag] + (f * vals_before);
            uint64_t r = state % f;
            uint64_t j = (state - r) / f;
            state = j * M + r + b;
        }
        return state;
    }

    uint8_t find_mag(uint64_t state_mod_M) const
    {
        for (size_t i = 1; i < dbase.size(); i++) {
            if (dbase[i].value > state_mod_M)
                return i - 1;
        }
        return dbase.size() - 1;
    }

    void decode_u64(uint64_t state, uint32_t*& out) const
    {
        static std::vector<uint32_t> stack(constants::MAXSTACKSIZE);
        size_t num_decoded = 0;
        while (state > 0) {
            uint64_t r = 1ULL + ((state - 1ULL) & mask_M);
            uint64_t j = (state - r) >> log2_M;
            uint64_t state_mod_M = r - 1;
            uint8_t boff = find_mag(state_mod_M);
            uint8_t state_mag = dbase[boff].mag;
            uint32_t f = nfreq[state_mag];
            uint64_t mag_offset = (state_mod_M - dbase[boff].value);
            uint64_t offset = mag_offset % f;
            uint64_t num_offset = mag_offset / f;
            uint32_t num = ans_min_val_in_mag(state_mag) + num_offset;
            stack[num_decoded++] = num;
            state = f * j + offset;
        }
        for (size_t j = 0; j < num_decoded; j++) {
            *out++ = stack[num_decoded - j - 1];
        }
    }

    uint32_t decode(
        uint64_t& state, const uint8_t*& in8, size_t& enc_size) const
    {
        uint64_t state_mod_M = state & mask_M;
        uint8_t boff = find_mag(state_mod_M);
        uint8_t state_mag = dbase[boff].mag;
        uint32_t f = nfreq[state_mag];
        uint64_t mag_offset = (state_mod_M - dbase[boff].value);
        uint64_t offset = mag_offset % f;
        uint64_t num_offset = mag_offset / f;
        uint32_t num = ans_min_val_in_mag(state_mag) + num_offset;
        state = f * (state >> log2_M) + offset;
        while (enc_size && state < norm_lower_bound) {
            uint8_t new_byte = *in8++;
            state = (state << constants::OUTPUT_BASE_LOG2) | uint64_t(new_byte);
            enc_size--;
        }
        return num;
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
