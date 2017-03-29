#pragma once

#include <memory>

#include "ans-util.hpp"
#include "bits.hpp"
#include "util.hpp"

template <class t_model>
inline uint32_t ans_byte_encode_variable(const t_model& model, uint32_t state,
    uint8_t sym, uint8_t*& out8, uint32_t frame_size, uint32_t base_offset)
{

    uint32_t freq = model.nfreqs[sym];
    uint32_t base = model.base[sym] - base_offset;
    // (1) normalize
    uint32_t SUB = constants::OUTPUT_BASE * constants::OUTPUT_BASE * freq;
    while (state >= SUB) {
        --out8;
        *out8 = (uint8_t)(state & 0xFF);
        state = state >> constants::OUTPUT_BASE_LOG2;
    }

    // (2) transform state
    uint32_t next = ((state / freq) * frame_size) + (state % freq) + base;
    return next;
}

template <uint32_t t_block_size = 128, uint32_t t_frame_size = 4096>
struct ans_vbyte_single_badapt {
private:
    ans_byte_encode_model<t_frame_size> model;
    ans_byte_decode_model<t_frame_size> dmodel;

public:
    bool required_increasing = false;
    std::string name()
    {
        return "ans_vbyte_single_badapt_" + std::to_string(t_frame_size);
    }
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        fprintff(stderr, "start_init()\n");
        // (1) count vbyte info
        freq_table freqs{ 0 };
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            for (size_t j = 0; j < n; j++) {
                vbyte_freq_count(cur_list[j], freqs);
            }
        }

        // (2) init model and move
        model = ans_byte_encode_model<t_frame_size>(freqs, false);

        // (3) write to output
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        vbyte_encode_u32(out8, model.nfreqs.size());
        // fprintff(stderr, "write %u model freqs\n", model.nfreqs.size());
        for (size_t j = 0; j < model.nfreqs.size(); j++) {
            vbyte_encode_u32(out8, model.nfreqs[j]);
            // fprintff(stderr, "write model freq %u\n", model.nfreqs[j]);
        }

        // (4) align to u32 boundary
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
        size_t max = vbyte_decode_u32(in8);
        // fprintff(stderr, "read %u model freqs\n", max);
        uint32_t base = 0;
        for (size_t j = 0; j < max; j++) {
            uint16_t cur_freq = vbyte_decode_u32(in8);
            // fprintff(stderr, "read model freq %u\n", cur_freq);
            dmodel.base[j] = base;
            dmodel.freq[j] = cur_freq;
            dmodel.framesize[j] = base + cur_freq;
            for (size_t k = 0; k < cur_freq; k++) {
                dmodel.table[base + k].sym = j;
                dmodel.table[base + k].vb_sym = j & 127;
                dmodel.table[base + k].freq = cur_freq;
                dmodel.table[base + k].base_offset = k;
                dmodel.table[base + k].finish = (j < 128) ? 1 : 0;
            }
            base += cur_freq;
        }
        size_t pbytes = in8 - initin8;
        if (pbytes % sizeof(uint32_t) != 0) {
            pbytes += sizeof(uint32_t) - (pbytes % (sizeof(uint32_t)));
        }
        size_t u32s = pbytes / sizeof(uint32_t);
        return in + u32s;
    }

    void encodeArray(const uint32_t* in, const size_t list_len, uint32_t* out,
        size_t& nvalue)
    {
        size_t num_blocks = list_len / t_block_size + 1;
        size_t last_block_size = list_len % t_block_size;
        if (last_block_size == 0) {
            num_blocks--;
            last_block_size = t_block_size;
        }
        // write the output
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        static std::array<uint8_t, t_block_size * 6> tmp_vb_buf;
        static std::array<uint8_t, t_block_size * 6> tmp_out_buf;
        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_size = t_block_size;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            // vbyte encode
            size_t block_offset = j * t_block_size;
            auto tmp_vb_ptr = tmp_vb_buf.data();
            for (size_t k = 0; k < block_size; k++) {
                uint32_t num = in[block_offset + k];
                vbyte_encode_u32(tmp_vb_ptr, num);
            }
            size_t num_vbytes = (tmp_vb_ptr - tmp_vb_buf.data());
            uint8_t max_val = 0;
            uint8_t min_val = 255;
            for (size_t k = 0; k < num_vbytes; k++) {
                max_val = std::max(tmp_vb_buf[k], max_val);
                min_val = std::min(tmp_vb_buf[k], min_val);
            }

            size_t frame_size = 0;
            for (size_t i = min_val; i <= max_val; i++)
                frame_size += model.nfreqs[i];
            size_t base_offset = model.base[min_val];

            auto tmp_out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
            auto tmp_out_start = tmp_out_ptr;
            auto vb_in = tmp_vb_ptr - 1;
            uint32_t state = 0;
            for (size_t i = 0; i < num_vbytes; i++) {
                uint8_t sym = *vb_in--;
                state = ans_byte_encode_variable(
                    model, state, sym, tmp_out_ptr, frame_size, base_offset);
            }
            size_t enc_size = (tmp_out_start - tmp_out_ptr);

            vbyte_encode_u32(out8, enc_size);
            *out8++ = min_val;
            *out8++ = max_val;
            vbyte_encode_u32(out8, state);
            memcpy(out8, tmp_out_ptr, enc_size);
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
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        // (1) read the parameters
        size_t num_blocks = list_len / t_block_size + 1;
        size_t last_block_size = list_len % t_block_size;
        if (last_block_size == 0) {
            num_blocks--;
            last_block_size = t_block_size;
        }

        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_size = t_block_size;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            size_t enc_size = vbyte_decode_u32(in8);
            size_t min_val = *in8++;
            size_t max_val = *in8++;

            // (1) compute framesize and fixed base offset
            uint32_t frame_size = 0;
            uint32_t base_offset_fix = dmodel.base[min_val];
            for (size_t i = min_val; i <= max_val; i++) {
                frame_size += dmodel.freq[i];
            }

            // (2) init encoding
            auto state = vbyte_decode_u32(in8);
            uint32_t cur_decoded_num = 0;
            uint8_t shift = 0;
            size_t varL = constants::OUTPUT_BASE * frame_size;
            for (size_t j = 0; j < block_size;) {
                // where in the frame are we?
                uint32_t state_mod_fs = state % frame_size;
                const auto& entry
                    = dmodel.table[state_mod_fs + base_offset_fix];

                // interleaved vb decoding
                cur_decoded_num += entry.vb_sym << shift;
                shift += 7;
                if (entry.finish) {
                    *out++ = cur_decoded_num;
                    cur_decoded_num = 0;
                    shift = 0;
                    j++;
                }

                // update state and renormalize
                state = entry.freq * (state / frame_size) + entry.base_offset;
                while (enc_size && state < varL) {
                    uint8_t new_byte = *in8++;
                    state = (state << constants::OUTPUT_BASE_LOG2)
                        | uint32_t(new_byte);
                    enc_size--;
                }
            }
        }
        return out;
    }
};
