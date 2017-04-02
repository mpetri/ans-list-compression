#pragma once

#include <memory>

#include "ans-util.hpp"
#include "util.hpp"

template <uint32_t t_frame_size = 4096> struct ans_vbyte_single {
private:
    ans_byte_encode_model<t_frame_size> model;
    ans_byte_decode_model<t_frame_size> dmodel;

public:
    bool required_increasing = false;
    std::string name()
    {
        return "ans_vbyte_single_" + std::to_string(t_frame_size);
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
        for (size_t j = 0; j < model.nfreqs.size(); j++) {
            vbyte_encode_u32(out8, model.nfreqs[j]);
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
        uint32_t base = 0;
        for (size_t j = 0; j < max; j++) {
            uint16_t cur_freq = vbyte_decode_u32(in8);
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

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
        // (1) vbyte encode list
        static std::vector<uint8_t> tmp_vbyte_buf;
        static std::vector<uint8_t> tmp_out_buf;
        if (tmp_vbyte_buf.size() < len * 8) {
            tmp_vbyte_buf.resize(len * 8);
            tmp_out_buf.resize(len * 8);
        }

        auto vb_ptr = tmp_vbyte_buf.data();
        for (size_t j = 0; j < len; j++) {
            uint32_t num = in[j];
            vbyte_encode_u32(vb_ptr, num);
        }

        size_t num_vb = (vb_ptr - tmp_vbyte_buf.data());
        uint32_t state = 0;
        auto tmp_out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
        auto tmp_out_start = tmp_out_ptr;
        auto encin = vb_ptr - 1;

        // we encode the input in reverse order as well
        for (size_t i = 0; i < num_vb; i++) {
            uint8_t sym = *encin--;
            state = ans_byte_encode(model, state, sym, tmp_out_ptr);
        }
        ans_byte_encode_flush(state, tmp_out_ptr);
        // as we encoded in reverse order, we have to write in into a tmp
        // buf and then output the written bytes
        size_t enc_size = (tmp_out_start - tmp_out_ptr);

        // write the output
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        vbyte_encode_u32(out8, num_vb - len);
        vbyte_encode_u32(out8, enc_size);
        memcpy(out8, tmp_out_ptr, enc_size);
        out8 += enc_size;

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
        size_t num_vb = vbyte_decode_u32(in8) + list_len;
        size_t enc_size = vbyte_decode_u32(in8);
        // (2) init encoding
        auto state = ans_byte_decode_init(in8, enc_size);
        uint32_t cur_decoded_num = 0;
        uint8_t shift = 0;
        for (size_t j = 0; j < num_vb; j++) {
            // where in the frame are we?
            uint32_t state_mod_fs = state & dmodel.frame_size_mask;
            const auto& entry = dmodel.table[state_mod_fs];

            // interleaved vb decoding
            cur_decoded_num += entry.vb_sym << shift;
            shift += 7;
            if (entry.finish) {
                *out++ = cur_decoded_num;
                cur_decoded_num = 0;
                shift = 0;
            }

            // update state and renormalize
            state = entry.freq * (state >> dmodel.frame_size_log2)
                + entry.base_offset;
            while (enc_size && state < constants::L) {
                uint8_t new_byte = *in8++;
                state = (state << constants::OUTPUT_BASE_LOG2)
                    | uint32_t(new_byte);
                enc_size--;
            }
        }
        return out;
    }
};