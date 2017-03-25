#pragma once

#include <memory>

#include "util.hpp"

namespace constants {
const uint32_t MAX_SIGMA = 256;
const uint32_t L = (1u << 23);
const uint32_t OUTPUT_BASE = 256;
const uint8_t OUTPUT_BASE_LOG2 = 8;
const uint8_t MAX_INT_THRES = 127;
const uint8_t EXCEPT_SYM = 0;
}

constexpr size_t clog2(size_t n) { return ((n < 2) ? 0 : 1 + clog2(n / 2)); }

template <uint32_t t_frame_size> struct ans_op4_model {
    std::vector<uint16_t> nfreqs;
    std::vector<uint16_t> base;
    std::vector<uint8_t> csum2sym;
    std::vector<uint32_t> sym_upper_bound;
    static const uint32_t frame_size = t_frame_size;
    ans_op4_model() {}
    ans_op4_model(freq_table& freqs)
    {
        // (1) determine max symbol and allocate tables
        uint8_t max_sym = 0;
        for (size_t i = 0; i < constants::MAX_SIGMA; i++) {
            if (freqs[i] != 0)
                max_sym = i;
        }
        nfreqs.resize(max_sym + 1);
        sym_upper_bound.resize(max_sym + 1);
        base.resize(max_sym + 1);
        csum2sym.resize(t_frame_size);
        // (2) normalize frequencies
        normalize_freqs(freqs, nfreqs, t_frame_size);
        // (3) fill the tables
        uint32_t cumsum = 0;
        for (size_t i = 0; i < nfreqs.size(); i++) {
            base[i] = cumsum;
            for (size_t j = 0; j < nfreqs[i]; j++) {
                csum2sym[cumsum + j] = i;
            }
            sym_upper_bound[i]
                = ((constants::L / t_frame_size) * constants::OUTPUT_BASE)
                * nfreqs[i];
            cumsum += nfreqs[i];
        }
    }
};

template <uint32_t i> inline uint8_t ans_extract7bits(const uint32_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
}

template <uint32_t i>
inline uint8_t ans_extract7bitsmaskless(const uint32_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)));
}

inline void ans_vbyte_encode_u32(uint8_t*& out, uint32_t x)
{
    if (x < (1U << 7)) {
        *out++ = static_cast<uint8_t>(x & 127);
    } else if (x < (1U << 14)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bitsmaskless<1>(x) & 127;
    } else if (x < (1U << 21)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bitsmaskless<2>(x) & 127;
    } else if (x < (1U << 28)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bitsmaskless<3>(x) & 127;
    } else {
        *out++ = ans_extract7bits<0>(x) | 128;
        *out++ = ans_extract7bits<1>(x) | 128;
        *out++ = ans_extract7bits<2>(x) | 128;
        *out++ = ans_extract7bits<3>(x) | 128;
        *out++ = ans_extract7bitsmaskless<4>(x) & 127;
    }
}

inline uint32_t ans_vbyte_decode_u32(const uint8_t*& input)
{
    uint32_t x = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t c = *input++;
        x += ((c & 127) << shift);
        if (!(c & 128)) {
            return x;
        }
        shift += 7;
    }
    return x;
}

template <class t_model>
inline uint32_t ans_op4_encode(
    const t_model& model, uint32_t state, uint32_t num, uint8_t*& out8)
{
    if (num > model.max_int_thres) {
        // write exception in reverse order
        uint32_t except = num - model.max_int_thres;
        ans_vbyte_encode_reverse_u32(except, out8);
        // we then ans encode the exception marker
        num = EXCEPT_SYM;
    }

    uint32_t freq = model.nfreqs[num];
    uint32_t base = model.base[num];
    // (1) normalize
    uint32_t sym_range_upper_bound = model.sym_upper_bound[num];
    while (state >= sym_range_upper_bound) {
        --out8;
        *out8 = (uint8_t)(state & 0xFF);
        state = state >> constants::OUTPUT_BASE_LOG2;
    }

    // (2) transform state
    uint32_t next = ((state / freq) * model.frame_size) + (state % freq) + base;
    return next;
}

inline void ans_encode_flush(uint32_t final_state, uint8_t*& out8)
{
    out8 -= sizeof(uint32_t);
    auto out32 = reinterpret_cast<uint32_t*>(out8);
    *out32 = final_state;
}

inline uint32_t ans_decode_init(const uint8_t*& in8, size_t& encoding_size)
{
    auto in = reinterpret_cast<const uint32_t*>(in8);
    uint32_t initial_state = *in;
    in8 += sizeof(uint32_t);
    encoding_size -= sizeof(uint32_t);
    return initial_state;
}

struct dec_table_entry {
    uint16_t freq;
    uint16_t base_offset;
    uint8_t sym;
    uint8_t vb_sym;
    uint8_t finish;
};

template <uint32_t t_frame_size> struct ans_decode_model {
    static const uint32_t frame_size = t_frame_size;
    static const uint8_t frame_size_log2 = clog2(frame_size);
    static const uint32_t frame_size_mask = frame_size - 1;
    dec_table_entry table[t_frame_size];
};

template <uint32_t t_block_size = 128, uint32_t t_frame_size = 4096>
struct ans_op4 {
private:
    const std::vector<std::pair<uint32_t, uint32_t> > thresholds
        = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 },
            { 6, 7 }, { 7, 8 }, { 8, 10 }, { 10, 11 }, { 11, 12 }, { 12, 15 },
            { 16, 32 }, { 32, 64 }, { 64, 96 }, { 96, 127 } };
    using ans_model_type = ans_vbyte_model<t_frame_size>;
    std::vector<ans_model_type> models;
    std::vector<ans_decode_model<t_frame_size> > decode_models;

private:
    uint8_t pick_model(uint32_t block_max)
    {
        for (size_t i = 0; i < thresholds.size(); i++) {
            size_t min = thresholds[i].first;
            size_t max = thresholds[i].second;
            if (min < block_max && block_max <= max) {
                return i;
            }
        }
        return thresholds.size() - 1;
    }

public:
    bool required_increasing = false;
    std::string name()
    {
        return "ans_vbyte_M" + std::to_string(t_frame_size) + "_B"
            + std::to_string(t_block_size);
    }
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count frequencies for each model
        std::vector<freq_table> freqs(thresholds.size());
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            size_t num_blocks = n / t_block_size;
            size_t left = n % t_block_size;
            std::array<uint8_t, t_block_size * 8> tmp_vbyte_buf;
            for (size_t j = 0; j < num_blocks; j++) {
                size_t block_offset = j * t_block_size;
                uint8_t block_max = 0;
                auto tmp_ptr = tmp_vbyte_buf.data();
                for (size_t k = 0; k < t_block_size; k++) {
                    uint32_t num = cur_list[block_offset + k];
                    ans_vbyte_encode_u32(tmp_ptr, num, block_max);
                }
                auto model_id = pick_model(block_max);
                // update the frequency counts for the block model
                size_t nvb = (tmp_ptr - tmp_vbyte_buf.data());
                for (size_t k = 0; k < nvb; k++) {
                    freqs[model_id][tmp_vbyte_buf[k]]++;
                }
            }
            if (left) {
                size_t block_offset = num_blocks * t_block_size;
                uint8_t block_max = 0;
                auto tmp_ptr = tmp_vbyte_buf.data();
                for (size_t k = 0; k < left; k++) {
                    uint32_t num = cur_list[block_offset + k];
                    ans_vbyte_encode_u32(tmp_ptr, num, block_max);
                }
                auto model_id = pick_model(block_max);
                // update the frequency counts for the block model
                size_t nvb = (tmp_ptr - tmp_vbyte_buf.data());
                for (size_t k = 0; k < nvb; k++) {
                    freqs[model_id][tmp_vbyte_buf[k]]++;
                }
            }
        }
        // (2) create models using the computed freq table
        for (size_t i = 0; i < thresholds.size(); i++) {
            models.emplace_back(ans_model_type(freqs[i]));
        }

        // (3) output the models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        uint8_t dummy = 0;
        for (size_t i = 0; i < thresholds.size(); i++) {
            ans_vbyte_encode_u32(out8, models[i].nfreqs.size(), dummy);
            for (size_t j = 0; j < models[i].nfreqs.size(); j++) {
                ans_vbyte_encode_u32(out8, models[i].nfreqs[j], dummy);
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
        decode_models.resize(thresholds.size());
        for (size_t i = 0; i < decode_models.size(); i++) {
            size_t max = ans_vbyte_decode_u32(in8);
            uint32_t base = 0;
            for (size_t j = 0; j < max; j++) {
                uint16_t cur_freq = ans_vbyte_decode_u32(in8);
                for (size_t k = 0; k < cur_freq; k++) {
                    decode_models[i].table[base + k].sym = j;
                    decode_models[i].table[base + k].vb_sym = j & 127;
                    decode_models[i].table[base + k].freq = cur_freq;
                    decode_models[i].table[base + k].base_offset = k;
                    decode_models[i].table[base + k].finish = (j < 128) ? 1 : 0;
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
        size_t left = len % t_block_size;

        // (1) vbyte encode list and determine block types
        static std::vector<uint8_t> tmp_vbyte_buf;
        if (tmp_vbyte_buf.size() < len * 8) {
            tmp_vbyte_buf.resize(len * 8);
        }
        static std::vector<uint32_t> vbyte_per_block;
        static std::vector<uint8_t> block_model;
        if (vbyte_per_block.size() < num_blocks + 1) {
            vbyte_per_block.resize(num_blocks + 1);
            block_model.resize(num_blocks + 2);
        }
        auto tmp_ptr = tmp_vbyte_buf.data();
        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_offset = j * t_block_size;
            auto block_start = tmp_ptr;
            uint8_t block_max = 0;
            for (size_t k = 0; k < t_block_size; k++) {
                uint32_t num = in[block_offset + k];
                ans_vbyte_encode_u32(tmp_ptr, num, block_max);
            }
            auto model_id = pick_model(block_max);
            size_t nvb = (tmp_ptr - block_start);
            vbyte_per_block[j] = nvb;
            block_model[j] = model_id;
        }
        if (left) {
            size_t block_offset = num_blocks * t_block_size;
            uint8_t block_max = 0;
            auto block_start = tmp_ptr;
            for (size_t k = 0; k < left; k++) {
                uint32_t num = in[block_offset + k];
                ans_vbyte_encode_u32(tmp_ptr, num, block_max);
            }
            auto model_id = pick_model(block_max);
            size_t nvb = (tmp_ptr - block_start);
            vbyte_per_block[num_blocks] = nvb;
            block_model[num_blocks] = model_id;
            num_blocks++;
        }

        // (2) encode block types
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        for (size_t i = 0; i < num_blocks; i += 2) {
            uint8_t packed_block_types
                = (block_model[i] << 4) + (block_model[i + 1]);
            *out8++ = packed_block_types;
        }

        // (3) encode the blocks
        auto vb_ptr = tmp_vbyte_buf.data();
        static std::array<uint8_t, t_block_size * 6> tmp_out_buf;
        for (size_t j = 0; j < num_blocks; j++) { // for each block
            size_t num_vb_syms = vbyte_per_block[j];
            if (block_model[j] == 0) { // special case all 1's
                vb_ptr += num_vb_syms;
                continue;
            }
            uint32_t state = 0;
            const auto& cur_model = models[block_model[j]];
            auto tmp_out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
            auto tmp_out_start = tmp_out_ptr;
            auto in = vb_ptr + num_vb_syms - 1;
            // we encode the input in reverse order as well
            for (size_t i = 0; i < num_vb_syms; i++) {
                uint8_t sym = *in--;
                state = ans_encode(cur_model, state, sym, tmp_out_ptr);
            }
            vb_ptr += num_vb_syms;
            ans_encode_flush(state, tmp_out_ptr);
            // as we encoded in reverse order, we have to write in into a tmp
            // buf and then output the written bytes
            size_t enc_size = (tmp_out_start - tmp_out_ptr);
            ans_vbyte_encode_u32(out8, enc_size);
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
        size_t num_blocks = list_len / t_block_size;
        size_t last_block_size = list_len % t_block_size;
        if (last_block_size) {
            num_blocks++;
        } else {
            last_block_size = t_block_size;
        }

        static std::vector<uint8_t> block_model;
        if (block_model.size() < (num_blocks + 2)) {
            block_model.resize(num_blocks + 2);
        }

        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (size_t i = 0; i < num_blocks; i += 2) {
            uint8_t packed_block_types = *in8++;
            block_model[i] = packed_block_types >> 4;
            block_model[i + 1] = packed_block_types & 15;
        }
        for (size_t i = 0; i < num_blocks; i++) {
            bool last_block = ((i + 1) == num_blocks);
            auto model_id = block_model[i];
            if (model_id == 0) { // uniform block
                if (last_block) {
                    for (size_t j = 0; j < last_block_size; j++)
                        *out++ = 1;
                } else {
                    for (size_t j = 0; j < t_block_size; j++)
                        *out++ = 1;
                }
                continue;
            }

            // (1) undo ans
            const auto& model = decode_models[model_id];
            size_t enc_size = ans_vbyte_decode_u32(in8);
            auto state = ans_decode_init(in8, enc_size);
            uint32_t cur_decoded_num = 0;
            uint8_t shift = 0;
            while (state != 0) {
                uint32_t state_mod_fs = state & model.frame_size_mask;
                const auto& entry = model.table[state_mod_fs];
                cur_decoded_num += entry.vb_sym << shift;
                shift += 7;
                if (entry.finish) {
                    *out++ = cur_decoded_num;
                    cur_decoded_num = 0;
                    shift = 0;
                }
                state = entry.freq * (state >> model.frame_size_log2)
                    + entry.base_offset;
                while (enc_size && state < constants::L) {
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