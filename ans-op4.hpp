#pragma once

#include <memory>

#include "ans-util.hpp"
#include "util.hpp"

template <uint32_t t_block_size = 128, uint32_t t_frame_size = 4096>
struct ans_op4 {
private:
    // clang-format off
    const std::vector<std::pair<uint32_t, uint32_t> > thresholds
        = { { 0, 1 },   //  1
            { 1, 2 },   //  2
            { 2, 3 },   //  3
            { 3, 4 },   //  4
            { 4, 5 },   //  5
            { 5, 6 },   //  6
            { 6, 8 },   //  7
            { 8, 10 },  //  8
            { 10, 11 }, //  9
            { 11, 12 }, // 10
            { 12, 15 }, // 11
            { 15, 32 }, // 12
            { 32, 64 }, // 13
            { 64, 96 }, // 14
            { 96, 127}, // 15
            {  0, 255}  // 16 -> catch all
           };
    // clang-format on
    std::vector<ans_byte_encode_model<t_frame_size> > models;
    std::vector<ans_byte_decode_model<t_frame_size> > dmodels;

private:
    const uint8_t EXCEPT_MARKER = 0;
    uint8_t pick_model(uint32_t block_max, bool except)
    {
        for (size_t i = 0; i < thresholds.size(); i++) {
            size_t min = thresholds[i].first;
            size_t max = thresholds[i].second;
            if (min < block_max && block_max <= max) {
                if (except && i == 0)
                    return 1;
                return i;
            }
        }
        return thresholds.size() - 1;
    }
    uint8_t pick_model_opt(const uint32_t* in, size_t block_size)
    {
        // (1) check if uniform
        uint32_t max_val = 0;
        for (size_t i = 0; i < block_size; i++) {
            max_val = std::max(max_val, in[i]);
        }
        if (max_val == 1)
            return 0;

        // (2) for each model determine the actual size of the
        // encoding
        size_t best_encoding_bytes = std::numeric_limits<size_t>::max();
        size_t best_encoding_id = 0;
        for (size_t i = 1; i < models.size(); i++) {
            size_t bytes = determine_encoding_size(models[i], in, block_size);
            if (best_encoding_bytes > bytes) {
                best_encoding_bytes = bytes;
                best_encoding_id = i;
            }
        }
        return best_encoding_id;
    }

    template <class t_model>
    size_t determine_encoding_size(
        const t_model& m, const uint32_t* in, size_t n)
    {
        size_t num_bytes = 4; // for the final state
        uint32_t state = 0;
        for (size_t i = 0; i < n; i++) {
            uint32_t num = in[n - i - 1];
            if (num > m.max_sym_encodeable) { // exception case!
                num_bytes += vbyte_size(num - (m.max_sym_encodeable + 1));
                num = EXCEPT_MARKER;
            }
            state = ans_byte_fake_encode(m, state, num, num_bytes);
        }
        return num_bytes;
    }

public:
    bool required_increasing = false;
    std::string name()
    {
        return "ans_op4_M" + std::to_string(t_frame_size) + "_B"
            + std::to_string(t_block_size);
    }
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count frequencies for each model
        auto max_ans_value = thresholds.back().second;
        std::vector<freq_table> freqs(thresholds.size());
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            size_t num_blocks = n / t_block_size;
            size_t left = n % t_block_size;
            size_t last_block_size = t_block_size;
            if (left) {
                num_blocks++;
                last_block_size = left;
            }
            for (size_t j = 0; j < num_blocks; j++) {
                size_t block_offset = j * t_block_size;
                uint32_t block_max = 0;
                size_t block_size = t_block_size;
                if (j + 1 == num_blocks)
                    block_size = last_block_size;
                bool except = false;
                for (size_t k = 0; k < block_size; k++) {
                    uint32_t num = cur_list[block_offset + k];
                    if (num > max_ans_value) {
                        num = EXCEPT_MARKER;
                        except = true;
                    }
                    block_max = std::max(block_max, num);
                }
                auto model_id = pick_model(block_max, except);
                for (size_t k = 0; k < block_size; k++) {
                    uint32_t num = cur_list[block_offset + k];
                    if (num > max_ans_value) {
                        num = EXCEPT_MARKER;
                    }
                    freqs[model_id][num]++;
                }
            }
        }

        // (2) create models using the computed freq table
        for (size_t i = 0; i < thresholds.size(); i++) {
            models.emplace_back(
                ans_byte_encode_model<t_frame_size>(freqs[i], true));
        }

        // (3) output the models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        for (size_t i = 0; i < thresholds.size(); i++) {
            vbyte_encode_u32(out8, models[i].nfreqs.size());
            for (size_t j = 0; j < models[i].nfreqs.size(); j++) {
                vbyte_encode_u32(out8, models[i].nfreqs[j]);
            }
        }

        // align to u32 boundary
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        }
        nvalue = wb / sizeof(uint32_t);
    };

    const uint32_t* dec_init(const uint32_t* in)
    {
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        dmodels.resize(thresholds.size());
        for (size_t i = 0; i < dmodels.size(); i++) {
            size_t max = vbyte_decode_u32(in8);
            dmodels[i].max_sym_encodeable = max - 1;
            uint32_t base = 0;
            for (size_t j = 0; j < max; j++) {
                uint16_t cur_freq = vbyte_decode_u32(in8);
                for (size_t k = 0; k < cur_freq; k++) {
                    dmodels[i].table[base + k].sym = j;
                    dmodels[i].table[base + k].freq = cur_freq;
                    dmodels[i].table[base + k].base_offset = k;
                }
                base += cur_freq;
            }
        }
        size_t pbytes = in8 - initin8;
        if (pbytes % sizeof(uint32_t) != 0) {
            pbytes += sizeof(uint32_t) - (pbytes % (sizeof(uint32_t)));
        }
        size_t u32s = pbytes / sizeof(uint32_t);
        return in + u32s;
    }

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
        size_t num_blocks = len / t_block_size;
        size_t last_block_size = len % t_block_size;
        if (last_block_size) {
            num_blocks++;
        } else {
            last_block_size = t_block_size;
        }

        // (1) determine block models
        static std::vector<uint8_t> block_models;
        if (block_models.size() < num_blocks + 1) {
            block_models.resize(num_blocks + 1);
        }
        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_offset = j * t_block_size;
            size_t block_size = t_block_size;
            if (j + 1 == num_blocks)
                block_size = last_block_size;
            block_models[j] = pick_model_opt(in + block_offset, block_size);
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
        static std::array<uint8_t, t_block_size * 8> tmp_out_buf;
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            if (model_id == 0) {
                // uniform block. decode nothing
                continue;
            }
            const auto& cur_model = models[model_id];
            size_t block_offset = j * t_block_size;
            size_t block_size = t_block_size;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            // reverse encode the block using the selected ANS model
            uint32_t state = 0;
            auto out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
            auto out_start = out_ptr;
            auto exception_bytes = 0;
            for (size_t k = 0; k < block_size; k++) {
                uint32_t num = in[block_offset + block_size - k - 1];
                if (num > cur_model.max_sym_encodeable) { // exception case!
                    exception_bytes += vbyte_encode_reverse(
                        out_ptr, num - (cur_model.max_sym_encodeable + 1));
                    num = EXCEPT_MARKER;
                }
                state = ans_byte_encode(cur_model, state, num, out_ptr);
            }
            ans_byte_encode_flush(state, out_ptr);

            // output the encoding
            size_t enc_size = (out_start - out_ptr);
            vbyte_encode_u32(out8, enc_size - exception_bytes);
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
        size_t num_blocks = list_len / t_block_size;
        size_t last_block_size = list_len % t_block_size;
        if (last_block_size) {
            num_blocks++;
        } else {
            last_block_size = t_block_size;
        }

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

        // (3) perform actual decoding
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            size_t block_size = t_block_size;
            if (j + 1 == num_blocks)
                block_size = last_block_size;
            if (model_id == 0) { // uniform block
                for (size_t k = 0; k < block_size; k++) {
                    *out++ = 1;
                }
                continue;
            }
            const auto& dmodel = dmodels[model_id];
            size_t enc_size = vbyte_decode_u32(in8);
            auto state = ans_byte_decode_init(in8, enc_size);
            for (size_t k = 0; k < block_size; k++) {
                // where in the frame are we?
                uint32_t state_mod_fs = state & dmodel.frame_size_mask;
                const auto& entry = dmodel.table[state_mod_fs];

                // update state and renormalize
                state = entry.freq * (state >> dmodel.frame_size_log2)
                    + entry.base_offset;
                while (enc_size && state < constants::L) {
                    uint8_t new_byte = *in8++;
                    state = (state << constants::OUTPUT_BASE_LOG2)
                        | uint32_t(new_byte);
                    enc_size--;
                }

                // decode the number and exception if necessary
                uint32_t num = entry.sym;
                if (num == EXCEPT_MARKER) {
                    auto fixup = vbyte_decode_u32(in8);
                    num = dmodel.max_sym_encodeable + fixup + 1;
                }
                *out++ = num;
            }
        }

        return out;
    }
};
