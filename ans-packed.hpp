#pragma once

#include <array>
#include <memory>

#include "ans-util.hpp"
#include "util.hpp"

namespace constants {
const uint64_t ANS_START_STATE = 0;
const uint32_t OUTPUT_BASE = 256;
const uint8_t OUTPUT_BASE_LOG2 = 8;
const uint8_t MAX_MAG = 25;
const uint8_t NUM_MAGS = 16;
const std::array<uint8_t, NUM_MAGS> SEL2MAG{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12,
    14, 16, 19, 22, 25 };
const std::array<uint8_t, MAX_MAG + 1> MAG2SEL{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9,
    10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15 };
}

using mag_table = std::array<uint64_t, constants::MAX_MAG + 1>;

struct dec_table_entry {
    uint32_t freq;
    uint64_t offset;
    uint32_t sym;
};

size_t bits_selector = 0;
size_t bits_encsize = 0;
size_t bits_enc = 0;
size_t bits_flush = 0;
size_t bits_flushopt = 0;
size_t bits_align = 0;

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

public:
    ans_mag_model(const uint8_t*& in8)
    {
        // (1) read the normalized magnitudes
        for (size_t i = 0; i < norm_mags.size(); i++) {
            norm_mags[i] = ans_vbyte_decode_u64(in8);
        }

        // (2) init the model
        init_model();
    }
    ans_mag_model(ans_mag_model&&) = default;
    ans_mag_model(const mag_table& mags)
    {
        // (0) if all is 0 do nothing
        if (std::all_of(mags.cbegin(), mags.cend(),
                [](uint64_t i) { return i == 0; })) {
            return;
        }

        // (1) normalize such that the normalized freqs sum to a power of 2
        norm_mags = normalize_power_of_two_alistair(mags);

        // (2) init the model params
        init_model();
    }

    void init_model()
    {
        // (2) figure out max mag and max value
        uint64_t max_num_representable = 0;
        for (size_t i = 0; i < norm_mags.size(); i++) {
            if (norm_mags[i] != 0) {
                max_num_representable = ans_max_val_in_mag(i);
            }
        }

        // (3) allocate space
        normalized_freqs.resize(max_num_representable + 1);
        base.resize(max_num_representable + 1);
        sym_upper_bound.resize(max_num_representable + 1);

        // (4) fill the tables
        uint64_t cumsum = 0;
        for (size_t i = 0; i < norm_mags.size(); i++) {
            if (norm_mags[i] == 0)
                continue;
            auto min_val = ans_min_val_in_mag(i);
            auto max_val = ans_max_val_in_mag(i);
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
            bits_enc += 8;
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
        bits_flush += (vb_bytes * 8);
        bits_flushopt += log2(final_state);
        out8 -= vb_bytes;
        auto tmp = out8;
        ans_vbyte_encode_u64(tmp, final_state);
    }
    void write(uint8_t*& out8) const
    {
        for (size_t i = 0; i < norm_mags.size(); i++) {
            ans_vbyte_encode_u64(out8, norm_mags[i]);
        }
    }
};

template <uint32_t t_bs = 8> struct ans_packed {
private:
    std::vector<ans_mag_model> models;
    uint8_t pick_model(const uint32_t* in, size_t n)
    {
        uint8_t max_mag = 0;
        for (size_t i = 0; i < n; i++) {
            max_mag = std::max(max_mag, ans_magnitude(in[i]));
        }
        return constants::MAG2SEL[max_mag];
    }

public:
    bool required_increasing = false;
    std::string name() { return "ans_packed_B" + std::to_string(t_bs); }
    const uint32_t bs = t_bs;

public:
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count frequencies for each model
        std::vector<mag_table> mags(constants::NUM_MAGS);
        for (auto& mt : mags)
            mt.fill(0);
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            size_t last_block_size = n % t_bs;
            size_t num_blocks = n / t_bs + (last_block_size != 0);
            last_block_size = last_block_size == 0 ? t_bs : last_block_size;

            // (1a) for each block
            for (size_t j = 0; j < num_blocks; j++) {
                size_t block_offset = j * t_bs;
                size_t block_size = t_bs;
                if (j + 1 == num_blocks)
                    block_size = last_block_size;

                auto model_id = pick_model(cur_list + block_offset, block_size);
                for (size_t k = 0; k < block_size; k++) {
                    uint32_t num = cur_list[block_offset + k];
                    uint8_t mag = ans_magnitude(num);
                    mags[model_id][mag]++;
                }
            }
        }

        // (2) create the models
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models.emplace_back(ans_mag_model(mags[i]));
        }

        // (4) write out models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models[i].write(out8);
        }

        // (4) align to u32 boundary
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        }
        nvalue = wb / sizeof(uint32_t);
    }

    const uint32_t* dec_init(const uint32_t* in)
    {
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models.emplace_back(ans_mag_model(in8));
        }
        size_t pbytes = in8 - initin8;
        if (pbytes % sizeof(uint32_t) != 0) {
            pbytes += sizeof(uint32_t) - (pbytes % (sizeof(uint32_t)));
        }
        size_t u32s = pbytes / sizeof(uint32_t);
        return in + u32s;
        return in;
    }

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
        size_t left = len % t_bs;
        size_t num_blocks = len / t_bs + (left != 0);
        size_t last_block_size = left == 0 ? t_bs : left;

        // (1) determine block models
        static std::vector<uint8_t> block_models;
        if (block_models.size() < num_blocks + 1) {
            block_models.resize(num_blocks + 1);
        }
        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_offset = j * t_bs;
            size_t block_size = t_bs;
            if (j + 1 == num_blocks)
                block_size = last_block_size;
            auto model_id = pick_model(in + block_offset, block_size);
            block_models[j] = model_id;
        }
        // (2) encode block types
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        for (size_t i = 0; i < num_blocks; i += 2) {
            uint8_t packed_block_types
                = (block_models[i] << 4) + (block_models[i + 1]);
            *out8++ = packed_block_types;
            bits_selector += 8;
        }

        // (3) perform actual encoding
        static int list_id = 0;
        list_id++;
        static std::array<uint8_t, t_bs * 8> tmp_out_buf;
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            size_t block_offset = j * t_bs;
            size_t block_size = t_bs;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            if (model_id == 0) { // all 1s. continue
                continue;
            }

            // reverse encode the block using the selected ANS model
            const auto& cur_model = models[model_id];
            uint64_t state = constants::ANS_START_STATE;
            auto out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
            auto out_start = out_ptr;
            for (size_t k = 0; k < block_size; k++) {
                uint32_t num = in[block_offset + block_size - k - 1];
                state = cur_model.encode(state, num, out_ptr);
            }
            cur_model.flush(state, out_ptr);

            // output the encoding
            size_t enc_size = (out_start - out_ptr);
            auto before_encsize = out8;
            ans_vbyte_encode_u64(out8, enc_size);
            bits_encsize += (out8 - before_encsize) * 8;
            memcpy(out8, out_ptr, enc_size);

            out8 += enc_size;
        }
        // (4) align to u32 boundary
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            auto before_wb = wb;
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
            bits_align += (wb - before_wb) * 8;
        }
        nvalue = wb / sizeof(uint32_t);
    }
    uint32_t* decodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t list_len)
    {
        size_t left = list_len % t_bs;
        size_t num_blocks = list_len / t_bs + (left != 0);
        size_t last_block_size = left == 0 ? t_bs : left;

        static std::vector<uint8_t> block_models;
        if (block_models.size() < (num_blocks + 1)) {
            block_models.resize(num_blocks + 1);
        }

        // (1) decode block models
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (size_t i = 0; i < num_blocks; i += 2) {
            uint8_t packed_block_types = *in8++;
            block_models[i] = packed_block_types >> 4;
            block_models[i + 1] = packed_block_types & 15;
        }

        int list_id = 0;
        list_id++;

        // (2) perform actual decoding
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            size_t block_size = t_bs;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            if (model_id == 0) { // uniform block
                for (size_t k = 0; k < block_size; k++) {
                    *out++ = 1;
                }
                continue;
            }
            const auto& model = models[model_id];
            size_t enc_size = ans_vbyte_decode_u64(in8);
            uint64_t state = model.init_decoder(in8, enc_size);
            for (size_t k = 0; k < block_size; k++) {
                *out++ = model.decode(state, in8, enc_size);
            }
        }
        return out;
    }
};
