#pragma once

#include <cmath>
#include <memory>

#include "ans-byte.hpp"
#include "ans-constants.hpp"
#include "ans-util.hpp"
#include "util.hpp"

inline void ans_vbyte_freq_count(uint32_t x, freq_table& f)
{
    if (x < (1U << 7)) {
        f[x & 127]++;
    } else if (x < (1U << 14)) {
        f[ans_extract7bits<0>(x) | 128]++;
        f[ans_extract7bitsmaskless<1>(x) & 127]++;
    } else if (x < (1U << 21)) {
        f[ans_extract7bits<0>(x) | 128]++;
        f[ans_extract7bits<1>(x) | 128]++;
        f[ans_extract7bitsmaskless<2>(x) & 127]++;
    } else if (x < (1U << 28)) {
        f[ans_extract7bits<0>(x) | 128]++;
        f[ans_extract7bits<1>(x) | 128]++;
        f[ans_extract7bits<2>(x) | 128]++;
        f[ans_extract7bitsmaskless<3>(x) & 127]++;
    } else {
        f[ans_extract7bits<0>(x) | 128]++;
        f[ans_extract7bits<1>(x) | 128]++;
        f[ans_extract7bits<2>(x) | 128]++;
        f[ans_extract7bits<3>(x) | 128]++;
        f[ans_extract7bitsmaskless<4>(x) & 127]++;
    }
}

inline void ans_vbyte_encode_split_u64(
    uint8_t*& out_first, uint8_t*& out, uint64_t x)
{
    if (x < (1ULL << 7)) {
        *out_first++ = static_cast<uint8_t>(x & 127);
    } else if (x < (1ULL << 14)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bitsmaskless<1>(x) & 127;
    } else if (x < (1ULL << 21)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bitsmaskless<2>(x) & 127;
    } else if (x < (1ULL << 28)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bitsmaskless<3>(x) & 127;
    } else if (x < (1ULL << 35)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bitsmaskless<4>(x) & 127;
    } else if (x < (1ULL << 42)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bitsmaskless<5>(x) & 127;
    } else if (x < (1ULL << 49)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bits<5>(x) | 128;
        *out++ = ans_extract7bitsmaskless<6>(x) & 127;
    } else if (x < (1ULL << 56)) {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bits<5>(x) | 128;
        *out++ = ans_extract7bitsmaskless<6>(x) & 127;
    } else {
        *out_first++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bits<4>(x) | 128;
        *out++ = ans_extract7bits<5>(x) | 128;
        *out++ = ans_extract7bits<6>(x) | 128;
        *out++ = ans_extract7bitsmaskless<7>(x) & 127;
    }
}

template <uint32_t t_frame_size = 4096> struct ans_vbyte_single {
private:
    ans_byte_model<t_frame_size> model;

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
        return "ans_vbyte_single_" + std::to_string(t_frame_size);
    }
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count vbyte info
        freq_table freqs{ 0 };
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            for (size_t j = 0; j < n; j++) {
                ans_vbyte_freq_count(cur_list[j], freqs);
            }
        }

        // (2) init model and move
        model = std::move(ans_byte_model<t_frame_size>(freqs));

        // (3) write out models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        model.write(out8);

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
        model = ans_byte_model<t_frame_size>(in8);
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
        if (tmp_vbyte_buf.size() < len * 8) {
            tmp_vbyte_buf.resize(len * 8);
        }

        auto vb_ptr = tmp_vbyte_buf.data();
        for (size_t j = 0; j < len; j++) {
            uint32_t num = in[j];
            ans_vbyte_encode_u64(vb_ptr, num);
        }
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        size_t num_vb = (vb_ptr - tmp_vbyte_buf.data());

        // (1) write num vbytes we will encode
        ans_vbyte_encode_u64(out8, num_vb - len);

        // (2) encode the first bytes
        encode(model, out8, tmp_vbyte_buf.data(), num_vb);

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

        static std::vector<uint8_t> buf;
        if (buf.size() < list_len * 8) {
            buf.resize(list_len * 8);
        }

        // (1) read the parameters
        size_t num_vb_rem = ans_vbyte_decode_u64(in8);
        size_t num_vb = list_len + num_vb_rem;

        // (2) decode the two streams
        decode(model, in8, buf.data(), num_vb);

        // (3) stitch them back together to create the output
        const uint8_t* vb_ptr = buf.data();
        for (size_t i = 0; i < list_len; i++) {
            *out++ = ans_vbyte_decode_u64(vb_ptr);
        }
        return out;
    }
};
