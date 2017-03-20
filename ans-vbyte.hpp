#pragma once

#include <memory>

#include "util.hpp"

namespace constants {
const uint32_t MAX_SIGMA = 256;
const uint32_t L = (1u << 23);
const uint32_t OUTPUT_BASE = 256;
const uint8_t OUTPUT_BASE_LOG2 = 8;
}

using freq_table = std::array<uint64_t, constants::MAX_SIGMA>;

struct norm_freq {
    uint32_t org;
    uint32_t norm;
    uint8_t sym;
};

constexpr size_t clog2(size_t n) { return ((n < 2) ? 0 : 1 + clog2(n / 2)); }

void normalize_freqs(
    freq_table& freqs, std::vector<uint16_t>& nfreqs, uint32_t frame_size)
{
    // (1) compute the counts
    if (freqs[0] != 0) {
        quit("we assume there are no 0's in the encoding");
    }
    freqs[0] = 1;
    uint64_t num_syms = 0;
    for (size_t i = 0; i < nfreqs.size(); i++) {
        num_syms += freqs[i];
    }

    // (2) crude normalization
    uint32_t actual_freq_csum = 0;
    std::vector<norm_freq> norm_freqs(nfreqs.size());
    for (size_t i = 0; i < nfreqs.size(); i++) {
        norm_freqs[i].sym = i;
        norm_freqs[i].org = freqs[i];
        norm_freqs[i].norm = (double(freqs[i]) / double(num_syms)) * frame_size;
        if (norm_freqs[i].norm == 0 && norm_freqs[i].org != 0)
            norm_freqs[i].norm = 1;
        actual_freq_csum += norm_freqs[i].norm;
    }

    // (3) fix things
    int32_t difference = int32_t(frame_size) - int32_t(actual_freq_csum);
    auto cmp_pdiff_func
        = [num_syms, frame_size](const norm_freq& a, const norm_freq& b) {
              double org_prob_a = double(a.org) / double(num_syms);
              double org_prob_b = double(b.org) / double(num_syms);
              double norm_prob_a = double(a.norm) / double(frame_size);
              if (a.norm == 1)
                  norm_prob_a = 0;
              double norm_prob_b = double(b.norm) / double(frame_size);
              if (b.norm == 1)
                  norm_prob_b = 0;
              return (norm_prob_b - org_prob_b) > (norm_prob_a - org_prob_a);
          };
    while (difference != 0) {
        std::sort(norm_freqs.begin(), norm_freqs.end(), cmp_pdiff_func);
        for (size_t i = 0; i < norm_freqs.size(); i++) {
            if (difference > 0) {
                norm_freqs[i].norm++;
                difference--;
                break;
            } else {
                if (norm_freqs[i].norm != 1) {
                    norm_freqs[i].norm--;
                    difference++;
                    break;
                }
            }
        }
    }

    // (4) put things back in order
    auto cmp_sym_func
        = [](const norm_freq& a, const norm_freq& b) { return a.sym < b.sym; };
    std::sort(norm_freqs.begin(), norm_freqs.end(), cmp_sym_func);

    // (5) check everything is ok
    actual_freq_csum = 0;
    for (size_t i = 0; i < nfreqs.size(); i++)
        actual_freq_csum += norm_freqs[i].norm;
    if (actual_freq_csum != frame_size) {
        quit("normalizing to framesize failed %u -> %u", frame_size,
            actual_freq_csum);
    }

    // (6) return actual normalized freqs
    for (size_t i = 0; i < norm_freqs.size(); i++) {
        nfreqs[i] = norm_freqs[i].norm;
    }
}

struct sym_data {
    uint16_t freq;
    uint16_t base;
};

template <uint32_t t_frame_size> struct ans_vbyte_model {
    std::vector<sym_data> nfreqs_base;
    std::vector<uint16_t> nfreqs;
    std::vector<uint16_t> base;
    std::vector<uint8_t> csum2sym;
    std::vector<uint32_t> sym_upper_bound;
    static const uint32_t frame_size = t_frame_size;
    static const uint8_t frame_size_log2 = clog2(frame_size);
    static const uint32_t frame_size_mask = frame_size - 1;
    ans_vbyte_model() {}
    ans_vbyte_model(freq_table& freqs)
    {
        // (1) determine max symbol and allocate tables
        uint8_t max_sym = 0;
        for (size_t i = 0; i < constants::MAX_SIGMA; i++) {
            if (freqs[i] != 0)
                max_sym = i;
        }
        nfreqs.resize(max_sym + 1);
        sym_upper_bound.resize(max_sym + 1);
        nfreqs_base.resize(max_sym + 1);
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
            nfreqs_base[i].freq = nfreqs[i];
            nfreqs_base[i].base = base[i];
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

inline void ans_vbyte_encode_u32(uint8_t*& out, uint32_t x, uint8_t& max_elem)
{
    if (x < (1U << 7)) {
        *out++ = static_cast<uint8_t>(x & 127);
        max_elem = std::max(max_elem, *(out - 1));
    } else if (x < (1U << 14)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bitsmaskless<1>(x) & 127;
    } else if (x < (1U << 21)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bits<1>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bitsmaskless<2>(x) & 127;
    } else if (x < (1U << 28)) {
        *out++ = ans_extract7bits<0>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bits<1>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bits<2>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bitsmaskless<3>(x) & 127;
    } else {
        *out++ = ans_extract7bits<0>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bits<1>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bits<2>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bits<3>(x) | 128;
        max_elem = std::max(max_elem, *(out - 1));
        *out++ = ans_extract7bitsmaskless<4>(x) & 127;
    }
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
inline uint32_t ans_encode(
    const t_model& model, uint32_t state, uint8_t sym, uint8_t*& out8)
{
    uint32_t freq = model.nfreqs[sym];
    uint32_t base = model.base[sym];
    // (1) normalize
    uint32_t sym_range_upper_bound = model.sym_upper_bound[sym];
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

template <class t_model>
inline uint8_t ans_decode_sym(const t_model& model, uint32_t current_state)
{
    uint32_t cumsum_in_framesize = current_state & model.frame_size_mask;
    return model.csum2sym[cumsum_in_framesize];
}

template <class t_model>
inline uint32_t ans_decode_advance(const t_model& model, uint32_t state,
    uint8_t sym, const uint8_t*& input, size_t& encoding_size)
{
    uint32_t freq = model.nfreqs_base[sym].freq;
    uint32_t base = model.nfreqs_base[sym].base;
    state = freq * (state >> model.frame_size_log2)
        + (state & model.frame_size_mask) - base;
    while (encoding_size && state < constants::L) {
        uint8_t new_byte = *input;
        state = (state << constants::OUTPUT_BASE_LOG2) | uint32_t(new_byte);
        ++input;
        encoding_size--;
    }
    return state;
}

template <uint32_t t_block_size = 128, uint32_t t_frame_size = 4096>
struct ans_vbyte {
private:
    const std::vector<std::pair<uint32_t, uint32_t> > thresholds
        = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 },
            { 6, 7 }, { 7, 8 }, { 8, 16 }, { 16, 32 }, { 32, 64 }, { 64, 96 },
            { 96, 128 }, { 128, 160 }, { 160, 192 }, { 192, 255 } };
    using ans_model_type = ans_vbyte_model<t_frame_size>;
    std::vector<ans_model_type> models;

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
        models.resize(thresholds.size());
        for (size_t i = 0; i < models.size(); i++) {
            size_t max = ans_vbyte_decode_u32(in8);
            models[i].nfreqs.resize(max);
            models[i].nfreqs_base.resize(max);
            models[i].base.resize(max);
            models[i].sym_upper_bound.resize(max);
            models[i].csum2sym.resize(t_frame_size);
            uint32_t cumsum = 0;
            for (size_t j = 0; j < max; j++) {
                models[i].nfreqs[j] = ans_vbyte_decode_u32(in8);
                models[i].base[j] = cumsum;
                for (size_t k = 0; k < models[i].nfreqs[j]; k++) {
                    models[i].csum2sym[cumsum + k] = j;
                }
                cumsum += models[i].nfreqs[j];
                models[i].sym_upper_bound[i]
                    = ((constants::L / t_frame_size) * constants::OUTPUT_BASE)
                    * models[i].nfreqs[i];
                models[i].nfreqs_base[j].freq = models[i].nfreqs[j];
                models[i].nfreqs_base[j].base = models[i].base[j];
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

        static std::array<uint8_t, t_block_size * 5> tmp_vb_buf;
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
            const auto& model = models[model_id];
            size_t enc_size = ans_vbyte_decode_u32(in8);
            auto state = ans_decode_init(in8, enc_size);
            size_t num_decoded = 0;
            while (state != 0) {
                uint8_t sym = ans_decode_sym(model, state);
                state = ans_decode_advance(model, state, sym, in8, enc_size);
                tmp_vb_buf[num_decoded++] = sym;
            }
            // (2) undo vbyte and output
            const uint8_t* vb_start = tmp_vb_buf.data();
            auto vb_end = vb_start + num_decoded;
            while (vb_start != vb_end) {
                *out++ = ans_vbyte_decode_u32(vb_start);
            }
        }
        return out;
    }
};
