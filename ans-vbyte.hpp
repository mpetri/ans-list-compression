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

void normalize_freqs(
    freq_table& freqs, std::vector<uint32_t>& nfreqs, uint32_t frame_size)
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

template <uint32_t t_frame_size> struct ans_vbyte_model {
    std::vector<uint32_t> nfreqs;
    std::vector<uint32_t> base;
    std::vector<uint32_t> csum2sym;
    std::vector<uint32_t> sym_upper_bound;
    static const uint32_t frame_size = t_frame_size;
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

template <uint32_t i> uint8_t ans_extract7bits(const uint32_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
}

template <uint32_t i> uint8_t ans_extract7bitsmaskless(const uint32_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)));
}

void vbyte_encode_u32(uint8_t*& out, uint32_t x, uint8_t& max_elem)
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

template <class t_model>
uint32_t ans_encode(
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

void ans_encode_flush(uint32_t final_state, uint8_t*& out8)
{
    out8 -= sizeof(uint32_t);
    auto out32 = reinterpret_cast<uint32_t*>(out8);
    *out32 = final_state;
}

template <uint32_t t_block_size = 128, uint32_t t_frame_size = 4096>
struct ans_vbyte {
private:
    const std::vector<std::pair<uint32_t, uint32_t> > thresholds{ { 0, 1 },
        { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 },
        { 8, 16 }, { 16, 32 }, { 32, 64 }, { 64, 96 }, { 96, 128 },
        { 128, 160 }, { 160, 192 }, { 192, 256 } };
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
                    vbyte_encode_u32(tmp_ptr, num, block_max);
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
                    vbyte_encode_u32(tmp_ptr, num, block_max);
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
            size_t max = thresholds[i].second;
            for (size_t j = 0; j <= max; j++) {
                vbyte_encode_u32(out8, models[i].nfreqs[j], dummy);
            }
        }

        // align to u32 boundary
        size_t wb = out8 - initout8;
        wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        nvalue = wb / sizeof(uint32_t);
    };

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
            block_model.resize(num_blocks + 1);
        }

        auto tmp_ptr = tmp_vbyte_buf.data();
        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_offset = j * t_block_size;
            auto block_start = tmp_ptr;
            uint8_t block_max = 0;
            for (size_t k = 0; k < t_block_size; k++) {
                uint32_t num = in[block_offset + k];
                vbyte_encode_u32(tmp_ptr, num, block_max);
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
                vbyte_encode_u32(tmp_ptr, num, block_max);
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
        for (size_t j = 0; j < num_blocks; j++) { // for each block
            size_t num_vb_syms = vbyte_per_block[j];
            if (block_model[j] == 0) { // special case all 1's
                vb_ptr += num_vb_syms;
                continue;
            }
            uint32_t state = 0;
            const auto& cur_model = models[block_model[j]];
            static std::array<uint8_t, t_block_size * 2> tmp_out_buf;
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
            memcpy(out8, tmp_out_ptr, enc_size);
            out8 += enc_size;
        }

        // (4) align to u32 boundary
        size_t wb = out8 - initout8;
        wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        nvalue = wb / sizeof(uint32_t);
    }
    uint32_t* decodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t list_len)
    {
        return out;
    }
};
