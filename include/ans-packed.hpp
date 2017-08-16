#pragma once

#include <array>
#include <memory>

#include "ans-mag-small.hpp"
#include "ans-mag.hpp"
#include "ans-util.hpp"
#include "util.hpp"

template <uint32_t t_bs = 8> struct ans_packed {
private:
    std::vector<ans_mag_model_small> models;
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

        std::vector<uint32_t> max_vals(constants::NUM_MAGS, 0);
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
                    max_vals[model_id] = std::max(num, max_vals[model_id]);
                    mags[model_id][mag]++;
                }
            }
        }

        // (2) create the models
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models.emplace_back(ans_mag_model_small(mags[i], max_vals[i]));
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
        std::cerr << "dec_init() START" << std::endl;
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models.emplace_back(ans_mag_model_small(in8));
        }
        size_t pbytes = in8 - initin8;
        if (pbytes % sizeof(uint32_t) != 0) {
            pbytes += sizeof(uint32_t) - (pbytes % (sizeof(uint32_t)));
        }
        size_t u32s = pbytes / sizeof(uint32_t);
        std::cerr << "dec_init() STOP" << std::endl;
        return in + u32s;
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
            ans_vbyte_encode_u64(out8, enc_size);
            memcpy(out8, out_ptr, enc_size);

            out8 += enc_size;
        }
        // (4) align to u32 boundary
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
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
