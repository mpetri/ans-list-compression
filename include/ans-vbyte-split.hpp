#pragma once

#include <cmath>
#include <memory>

#include "ans-byte.hpp"
#include "ans-constants.hpp"
#include "ans-util.hpp"
#include "util.hpp"

inline void ans_vbyte_freq_count(uint32_t x, freq_table& first, freq_table& rem)
{
    if (x < (1U << 7)) {
        first[x & 127]++;
    } else if (x < (1U << 14)) {
        first[ans_extract7bits<0>(x) | 128]++;
        rem[ans_extract7bitsmaskless<1>(x) & 127]++;
    } else if (x < (1U << 21)) {
        first[ans_extract7bits<0>(x) | 128]++;
        rem[ans_extract7bits<1>(x) | 128]++;
        rem[ans_extract7bitsmaskless<2>(x) & 127]++;
    } else if (x < (1U << 28)) {
        first[ans_extract7bits<0>(x) | 128]++;
        rem[ans_extract7bits<1>(x) | 128]++;
        rem[ans_extract7bits<2>(x) | 128]++;
        rem[ans_extract7bitsmaskless<3>(x) & 127]++;
    } else {
        first[ans_extract7bits<0>(x) | 128]++;
        rem[ans_extract7bits<1>(x) | 128]++;
        rem[ans_extract7bits<2>(x) | 128]++;
        rem[ans_extract7bits<3>(x) | 128]++;
        rem[ans_extract7bitsmaskless<4>(x) & 127]++;
    }
}

template <uint32_t t_frame_size = 4096> struct ans_vbyte_split {
private:
    ans_byte_model<t_frame_size> model_first;
    ans_byte_model<t_frame_size> model_rem;

private:
    template <class t_model>
    void encode(const t_model& m, uint8_t*& out8, uint8_t* buf, size_t n)
    {
        static std::vector<uint8_t> tmp_buf;
        if (tmp_buf.size() < n * 8) {
            tmp_buf.resize(n * 8);
        }
        auto state = constants::ANS_START_STATE;
        auto tmp_out_ptr = tmp_buf.data() + tmp_buf.size() - 1;
        auto tmp_out_start = tmp_out_ptr;
        auto encin = buf + n - 1;
        // we encode the input in reverse order as well
        for (size_t i = 0; i < n; i++) {
            uint8_t sym = *encin--;
            state = m.encode(state, sym, tmp_out_ptr);
        }
        m.flush(state, tmp_out_ptr);
        // as we encoded in reverse order, we have to write in into a tmp
        // buf and then output the written bytes
        size_t enc_size = (tmp_out_start - tmp_out_ptr);

        // write the output
        ans_vbyte_encode_u64(out8, enc_size);
        memcpy(out8, tmp_out_ptr, enc_size);
        out8 += enc_size;
    }

    template <class t_model>
    void decode(const t_model& m, const uint8_t*& in8, uint8_t* buf, size_t n)
    {
        size_t enc_size = ans_vbyte_decode_u64(in8);
        uint32_t state = m.init_decoder(in8, enc_size);
        for (size_t k = 0; k < n; k++) {
            *buf++ = m.decode(state, in8, enc_size);
        }
    }

public:
    bool required_increasing = false;
    std::string name()
    {
        return "ans_vbyte_split_" + std::to_string(t_frame_size);
    }
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count vbyte info
        freq_table freqs_first{ 0 };
        freq_table freqs_rem{ 0 };
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            for (size_t j = 0; j < n; j++) {
                ans_vbyte_freq_count(cur_list[j], freqs_first, freqs_rem);
            }
        }

        // (2) init model and move
        model_first = std::move(ans_byte_model<t_frame_size>(freqs_first));
        model_rem = std::move(ans_byte_model<t_frame_size>(freqs_rem));

        // (3) write out models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        model_first.write(out8);
        model_rem.write(out8);

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
        model_first = ans_byte_model<t_frame_size>(in8);
        model_rem = ans_byte_model<t_frame_size>(in8);
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
        static std::vector<uint8_t> tmp_vbyte_first_buf;
        static std::vector<uint8_t> tmp_vbyte_rem_buf;
        if (tmp_vbyte_first_buf.size() < len * 8) {
            tmp_vbyte_first_buf.resize(len * 8);
            tmp_vbyte_rem_buf.resize(len * 8);
        }

        auto vb_ptr_first = tmp_vbyte_first_buf.data();
        auto vb_ptr_rem = tmp_vbyte_rem_buf.data();
        for (size_t j = 0; j < len; j++) {
            uint32_t num = in[j];
            ans_vbyte_encode_split_u64(vb_ptr_first, vb_ptr_rem, num);
        }
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        size_t num_vb_first = (vb_ptr_first - tmp_vbyte_first_buf.data());
        size_t num_vb_rem = (vb_ptr_rem - tmp_vbyte_rem_buf.data());

        // (1) write num vbytes we will encode
        ans_vbyte_encode_u64(out8, num_vb_rem);

        // (2) encode the first bytes
        encode(model_first, out8, tmp_vbyte_first_buf.data(), num_vb_first);

        // (3) encode the second bytes
        encode(model_rem, out8, tmp_vbyte_rem_buf.data(), num_vb_rem);

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
        static std::vector<uint8_t> first_buf;
        static std::vector<uint8_t> rem_buf;
        if (first_buf.size() < list_len) {
            first_buf.resize(list_len);
            rem_buf.resize(list_len);
        }

        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;

        // (1) read the parameters
        size_t num_vb_rem = ans_vbyte_decode_u64(in8);
        size_t num_vb = list_len;

        // (2) decode the two streams
        decode(model_first, in8, first_buf.data(), num_vb);
        decode(model_rem, in8, rem_buf.data(), num_vb_rem);

        // (3) stitch them back together to create the output
        size_t first_offset = 0;
        size_t rem_offset = 0;
        for (size_t i = 0; i < list_len; i++) {
            auto cur_first = first_buf[first_offset++];
            if (cur_first < 128) {
                *out++ = uint32_t(cur_first);
            } else {
                uint32_t num = cur_first & 127;
                auto cur_rem = rem_buf[rem_offset++];
                uint8_t shift = 7;
                while (cur_rem > 127) {
                    num = num + ((cur_rem & 127) << shift);
                    cur_rem = rem_buf[rem_offset++];
                    shift += 7;
                }
                num = num + (cur_rem << shift);
                *out++ = num;
            }
        }

        return out;
    }
};
