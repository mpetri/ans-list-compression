#pragma once

#include "ans-constants.hpp"
#include "ans-util.hpp"

using freq_table = std::array<uint64_t, constants::MAX_SIGMA>;

struct dec_table_entry {
    uint32_t freq;
    uint64_t offset;
    uint32_t sym;
};

template <uint32_t t_frame_size> struct ans_byte_model {
public:
    uint32_t M; // frame size
    std::vector<uint64_t> normalized_freqs;
    std::vector<uint64_t> base;
    std::vector<uint64_t> sym_upper_bound;
    uint8_t log2_M;
    uint64_t mask_M;
    uint64_t norm_lower_bound;
    std::vector<uint32_t> csum2sym;
    std::vector<dec_table_entry> dec_table;

public:
    ans_byte_model() = default;
    ans_byte_model(const uint8_t*& in8)
    {
        // (1) read the normalized magnitudes
        auto n = ans_vbyte_decode_u64(in8);
        normalized_freqs.resize(n);
        for (size_t i = 0; i < normalized_freqs.size(); i++) {
            normalized_freqs[i] = ans_vbyte_decode_u64(in8);
        }

        // (2) init the model
        init_model();
    }
    ans_byte_model(ans_byte_model&&) = default;
    ans_byte_model& operator=(ans_byte_model&&) = default;
    ans_byte_model& operator=(const ans_byte_model&) = default;
    ans_byte_model(const freq_table& freqs)
    {
        // (0) if all is 0 do nothing
        if (std::all_of(freqs.cbegin(), freqs.cend(),
                [](uint64_t i) { return i == 0; })) {
            return;
        }

        // (1) normalize such that the normalized freqs sum to a power of 2
        normalized_freqs
            = normalize_freqs_power_of_two_alistair(freqs, t_frame_size);

        // (2) init the model params
        init_model();
    }

    void init_model()
    {
        // (2) figure out max mag and max value
        uint16_t max_num_representable = 0;
        for (size_t i = 0; i < normalized_freqs.size(); i++) {
            if (normalized_freqs[i] != 0) {
                max_num_representable = i;
            }
        }

        // (3) allocate space
        normalized_freqs.resize(max_num_representable + 1);
        base.resize(max_num_representable + 1);
        sym_upper_bound.resize(max_num_representable + 1);

        // (4) fill the tables
        uint32_t cumsum = 0;
        for (size_t i = 0; i < normalized_freqs.size(); i++) {
            base[i] = cumsum;
            cumsum += normalized_freqs[i];
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
            fprintf(stderr, "cumsum %u != M %u\n", cumsum, M);
        }

        // init the decoder as well
        dec_table.resize(M);
        uint32_t base = 0;
        for (size_t j = 0; j < normalized_freqs.size(); j++) {
            uint16_t cur_freq = normalized_freqs[j];
            for (size_t k = 0; k < cur_freq; k++) {
                dec_table[base + k].sym = j;
                dec_table[base + k].freq = cur_freq;
                dec_table[base + k].offset = k;
            }
            base += cur_freq;
        }
    }
    uint32_t encode(uint32_t state, uint8_t sym, uint8_t*& out8) const
    {
        uint32_t f = normalized_freqs[sym];
        uint32_t b = base[sym];
        // (1) normalize
        uint32_t SUB = sym_upper_bound[sym];
        while (state >= SUB) {
            --out8;
            *out8 = (uint8_t)(state & 0xFF);
            state = state >> constants::OUTPUT_BASE_LOG2;
        }

        // (2) transform state
        uint64_t next = ((state / f) * M) + (state % f) + b;
        return next;
    }
    uint8_t decode(uint32_t& state, const uint8_t*& in8, size_t& enc_size) const
    {
        uint64_t state_mod_M = state & mask_M;
        const auto& entry = dec_table[state_mod_M];
        // update state and renormalize
        state = entry.freq * (state >> log2_M) + entry.offset;
        while (enc_size && state < norm_lower_bound) {
            uint8_t new_byte = *in8++;
            state = (state << constants::OUTPUT_BASE_LOG2) | uint32_t(new_byte);
            enc_size--;
        }
        return entry.sym;
    }
    uint32_t init_decoder(const uint8_t*& in8, size_t& enc_size) const
    {
        return ans_vbyte_decode_u64(in8, enc_size);
    }
    void flush(uint32_t final_state, uint8_t*& out8) const
    {
        auto vb_bytes = ans_vbyte_size(final_state);
        out8 -= vb_bytes;
        auto tmp = out8;
        ans_vbyte_encode_u64(tmp, final_state);
    }
    void write(uint8_t*& out8) const
    {
        ans_vbyte_encode_u64(out8, normalized_freqs.size());
        for (size_t i = 0; i < normalized_freqs.size(); i++) {
            ans_vbyte_encode_u64(out8, normalized_freqs[i]);
        }
    }
};
