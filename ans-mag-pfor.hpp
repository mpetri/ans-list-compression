#pragma once

#include <memory>

#include "ans-util.hpp"
#include "util.hpp"

namespace constants {
const uint32_t MAX_MAG = 25;
const uint32_t EXCEPT_PERCENTAGE = 5;
const uint32_t EXCEPT_MARKER = 0;
const uint32_t NORM_FREQS_SUM = 2 * 4096;
const uint32_t MAGNITUDE_START = 0;
}

template <uint32_t i> inline uint8_t ans_extract7bits_u64(const uint64_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)) & ((1ULL << 7) - 1));
}

template <uint32_t i>
inline uint8_t ans_extract7bitsmaskless_u64(const uint64_t val)
{
    return static_cast<uint8_t>((val >> (7 * i)));
}

inline uint32_t vbyte_encode_reverse_u64(uint8_t*& out, uint64_t x)
{
    out--;
    if (x < (1ULL << 7)) {
        *out = static_cast<uint8_t>(x & 127);
        return 1;
    } else if (x < (1ULL << 14)) {
        *out-- = ans_extract7bitsmaskless_u64<1>(x) & 127;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 2;
    } else if (x < (1ULL << 21)) {
        *out-- = ans_extract7bitsmaskless_u64<2>(x) & 127;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 3;
    } else if (x < (1ULL << 28)) {
        *out-- = ans_extract7bitsmaskless_u64<3>(x) & 127;
        *out-- = ans_extract7bits_u64<2>(x) | 128;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 4;
    } else if (x < (1ULL << 35)) {
        *out-- = ans_extract7bitsmaskless_u64<4>(x) & 127;
        *out-- = ans_extract7bits_u64<3>(x) | 128;
        *out-- = ans_extract7bits_u64<2>(x) | 128;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 5;
    } else if (x < (1ULL << 42)) {
        *out-- = ans_extract7bitsmaskless_u64<5>(x) & 127;
        *out-- = ans_extract7bits_u64<4>(x) | 128;
        *out-- = ans_extract7bits_u64<3>(x) | 128;
        *out-- = ans_extract7bits_u64<2>(x) | 128;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 6;
    } else if (x < (1ULL << 49)) {
        *out-- = ans_extract7bitsmaskless_u64<6>(x) & 127;
        *out-- = ans_extract7bits_u64<5>(x) | 128;
        *out-- = ans_extract7bits_u64<4>(x) | 128;
        *out-- = ans_extract7bits_u64<3>(x) | 128;
        *out-- = ans_extract7bits_u64<2>(x) | 128;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 7;
    } else if (x < (1ULL << 56)) {
        *out-- = ans_extract7bitsmaskless_u64<7>(x) & 127;
        *out-- = ans_extract7bits_u64<6>(x) | 128;
        *out-- = ans_extract7bits_u64<5>(x) | 128;
        *out-- = ans_extract7bits_u64<4>(x) | 128;
        *out-- = ans_extract7bits_u64<3>(x) | 128;
        *out-- = ans_extract7bits_u64<2>(x) | 128;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 8;
    } else {
        *out-- = ans_extract7bitsmaskless_u64<8>(x) & 127;
        *out-- = ans_extract7bits_u64<7>(x) | 128;
        *out-- = ans_extract7bits_u64<6>(x) | 128;
        *out-- = ans_extract7bits_u64<5>(x) | 128;
        *out-- = ans_extract7bits_u64<4>(x) | 128;
        *out-- = ans_extract7bits_u64<3>(x) | 128;
        *out-- = ans_extract7bits_u64<2>(x) | 128;
        *out-- = ans_extract7bits_u64<1>(x) | 128;
        *out = ans_extract7bits_u64<0>(x) | 128;
        return 9;
    }
}

uint32_t max_val(uint8_t mag)
{
    if (mag == 0)
        return 1;
    return (1u << (mag));
}

uint32_t min_val(uint8_t mag)
{
    if (mag == 0)
        return 0;
    return (1u << (mag - 1)) + 1;
}

uint32_t magnitude(uint32_t x)
{
    uint64_t y = x;
    if (x < 2)
        return 0;
    // uint32_t res = 63 - __builtin_clzll(y);
    return ceil(log2(x));
}

template <class t_itr> void print_array(t_itr itr, size_t n, const char* name)
{
    fprintff(stderr, "%s: [", name);
    for (size_t i = 0; i < n; i++) {
        fprintff(stderr, "%u", *itr);
        if (i + 1 != n)
            fprintff(stderr, ",");
        ++itr;
    }
    fprintff(stderr, "]\n");
}

struct mag_norm_freq {
    uint32_t org;
    uint32_t norm;
    uint8_t mag;
};

void ans_normalize_mags(std::vector<uint32_t>& freqs,
    std::vector<uint16_t>& nfreqs, uint32_t frame_size)
{
    uint64_t num_syms = 0;
    for (size_t i = 0; i < nfreqs.size(); i++) {
        if (freqs[i] == 0)
            freqs[i] = 1;
        num_syms += freqs[i];
    }

    // (2) crude normalization
    uint32_t actual_freq_csum = 0;
    std::vector<mag_norm_freq> norm_freqs(nfreqs.size());
    for (size_t i = 0; i < nfreqs.size(); i++) {
        norm_freqs[i].mag = i;
        norm_freqs[i].org = freqs[i];
        norm_freqs[i].norm = (double(freqs[i]) / double(num_syms)) * frame_size;
        if (norm_freqs[i].norm == 0 && norm_freqs[i].org != 0)
            norm_freqs[i].norm = 1;
        actual_freq_csum += norm_freqs[i].norm;
    }

    // (3) fix things
    int32_t difference = int32_t(frame_size) - int32_t(actual_freq_csum);
    auto cmp_pdiff_func = [num_syms, frame_size](
        const mag_norm_freq& a, const mag_norm_freq& b) {
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
    auto cmp_mag_func = [](const mag_norm_freq& a, const mag_norm_freq& b) {
        return a.mag < b.mag;
    };
    std::sort(norm_freqs.begin(), norm_freqs.end(), cmp_mag_func);

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

struct ans_mag_model {
    uint32_t max_num_encodeable;
    uint32_t min_num_encodeable;
    std::vector<uint16_t> nfreqs;
    std::vector<uint16_t> nmags;
    std::vector<uint64_t> base;
    std::vector<uint64_t> sym_upper_bound;
    uint64_t frame_size;
    uint64_t normalization_lower_bound;
    ans_mag_model() {}
    ans_mag_model(ans_mag_model&& other)
    {
        frame_size = std::move(other.frame_size);
        normalization_lower_bound = std::move(other.normalization_lower_bound);
        max_num_encodeable = std::move(other.max_num_encodeable);
        min_num_encodeable = std::move(other.min_num_encodeable);
        nmags = std::move(other.nmags);
        nfreqs = std::move(other.nfreqs);
        base = std::move(other.base);
        sym_upper_bound = std::move(other.sym_upper_bound);
    }
    ans_mag_model& operator=(ans_mag_model&& other)
    {
        frame_size = std::move(other.frame_size);
        normalization_lower_bound = std::move(other.normalization_lower_bound);
        max_num_encodeable = std::move(other.max_num_encodeable);
        min_num_encodeable = std::move(other.min_num_encodeable);
        nmags = std::move(other.nmags);
        nfreqs = std::move(other.nfreqs);
        base = std::move(other.base);
        sym_upper_bound = std::move(other.sym_upper_bound);
        return *this;
    }

    ans_mag_model(std::vector<uint32_t>& freqs,
        const std::vector<std::pair<uint32_t, uint32_t> >& bands,
        uint32_t norm_freqs_sum)
    {
        // (1) determine max symbol and allocate tables
        max_num_encodeable = 0;
        min_num_encodeable = 0;
        bool first = true;
        for (size_t i = 0; i < freqs.size(); i++) {
            if (freqs[i] != 0) {
                if (bands[i].first == bands[i].second) {
                    max_num_encodeable = i;
                    if (first) {
                        min_num_encodeable = i;
                        first = false;
                    }
                } else {
                    max_num_encodeable = bands[i].second;
                    if (first) {
                        min_num_encodeable = bands[i].first;
                        first = false;
                    }
                }
            }
        }

        // (2) normalize the magnitude dists
        nfreqs.resize(freqs.size());
        print_array(freqs.begin(), freqs.size(), "F");
        for (size_t i = 0; i < freqs.size(); i++) {
            auto start = bands[i].first;
            auto stop = bands[i].second;
            auto num_syms_in_mag = stop - start + 1;
            freqs[i] /= num_syms_in_mag;
        }
        ans_normalize_mags(freqs, nfreqs, norm_freqs_sum);
        print_array(nfreqs.begin(), nfreqs.size(), "N");

        // allocate the tables
        sym_upper_bound.resize(max_num_encodeable + 1);
        base.resize(max_num_encodeable + 1);
        nfreqs.resize(max_num_encodeable + 1);

        // (3) fill the tables
        uint64_t cumsum = 0;
        frame_size = 0;
        for (size_t i = 0; i < nfreqs.size(); i++) {
            auto start = bands[i].first;
            auto stop = bands[i].second;
            fprintff(stderr, "mag %u min %u max %u\n", i, start, stop);
            for (size_t j = start; j <= stop; j++) {
                base[j] = cumsum;
                nfreqs[j] = nmags[i];
                frame_size += nfreqs[j];
                cumsum += nfreqs[j];
            }
        }

        // // give 0 a weight of 10%
        nfreqs[0] = norm_freqs_sum / (100 / constants::EXCEPT_PERCENTAGE);
        cumsum = nfreqs[0];
        frame_size = nfreqs[0];
        for (size_t i = 1; i < nfreqs.size(); i++) {
            base[i] = cumsum;
            frame_size += nfreqs[i];
            cumsum += nfreqs[i];
        }

        normalization_lower_bound = frame_size * constants::OUTPUT_BASE;
        for (size_t i = 0; i < nfreqs.size(); i++) {
            sym_upper_bound[i] = ((normalization_lower_bound / frame_size)
                                     * constants::OUTPUT_BASE)
                * nfreqs[i];
        }

        size_t ps = std::min(nfreqs.size(), 300ul);
        for (size_t i = 0; i < ps; i++) {
            fprintff(stderr, "SYM %u FREQ %u\n", i, nfreqs[i]);
        }
    }
};

inline uint64_t ans_mag_encode(
    const ans_mag_model& model, uint64_t state, uint32_t num, uint8_t*& out8)
{
    uint64_t freq = model.nfreqs[num];
    uint64_t base = model.base[num];
    // (1) normalize
    uint64_t sym_range_upper_bound = model.sym_upper_bound[num];
    while (state >= sym_range_upper_bound) {
        --out8;
        *out8 = (uint8_t)(state & 0xFF);
        state = state >> constants::OUTPUT_BASE_LOG2;
    }

    // (2) transform state
    uint64_t next = ((state / freq) * model.frame_size) + (state % freq) + base;
    return next;
}

inline uint64_t ans_mag_fake_encode(const ans_mag_model& model, uint64_t state,
    uint32_t num, size_t& bytes_written)
{
    uint64_t freq = model.nfreqs[num];
    uint64_t base = model.base[num];
    // (1) normalize
    uint64_t sym_range_upper_bound = model.sym_upper_bound[num];
    while (state >= sym_range_upper_bound) {
        bytes_written++;
        state = state >> constants::OUTPUT_BASE_LOG2;
    }

    // (2) transform state
    uint64_t next = ((state / freq) * model.frame_size) + (state % freq) + base;
    return next;
}

inline void ans_mag_encode_flush(uint64_t final_state, uint8_t*& out8)
{
    vbyte_encode_reverse_u64(out8, final_state);
}

template <uint32_t t_block_size = 128> struct ans_mag_pfor {
public:
    std::vector<std::pair<uint32_t, uint32_t> > bands;
    const uint32_t MAX_NUM_EXCEPT
        = t_block_size / (100 / constants::EXCEPT_PERCENTAGE) + 1;
    const uint32_t MAG_OFFSET
        = constants::MAGNITUDE_START - magnitude(constants::MAGNITUDE_START);

public:
    ans_mag_pfor()
    {
        // be precise for small numbers
        for (size_t i = 1; i < constants::MAGNITUDE_START; i++) {
            bands.emplace_back(i, i);
        }
        // magnitude scheme for large numbers
        size_t start_mag = magnitude(constants::MAGNITUDE_START);
        size_t stop_mag = magnitude(1u << constants::MAX_MAG);
        for (size_t i = start_mag; i <= stop_mag; i++) {
            auto b_max = max_val(i);
            auto b_min = min_val(i);
            bands.emplace_back(b_min, b_max);
        }
    }

private:
    std::vector<ans_mag_model> models;

private:
    size_t determine_encoding_size(
        uint8_t model_id, const uint32_t* in, size_t n)
    {
        const ans_mag_model& model = models[model_id];
        size_t num_bytes = 8; // for the final state
        uint32_t state = 0;
        for (size_t i = 0; i < n; i++) {
            uint32_t num = in[n - i - 1];
            if (num > model.max_num_encodeable) { // exception case!
                num_bytes += vbyte_size(num - (model.max_num_encodeable + 1));
                num = constants::EXCEPT_MARKER;
            }
            state = ans_mag_fake_encode(model, state, num, num_bytes);
        }
        return num_bytes;
    }
    uint8_t pick_model_opt(const uint32_t* in, size_t block_size)
    {
        // (2) for each model determine the actual size of the
        // encoding
        size_t best_encoding_bytes = std::numeric_limits<size_t>::max();
        size_t best_encoding_id = 0;
        for (size_t i = 0; i < models.size(); i++) {
            size_t bytes = determine_encoding_size(i, in, block_size);
            if (best_encoding_bytes > bytes) {
                best_encoding_bytes = bytes;
                best_encoding_id = i;
            }
        }
        return best_encoding_id;
    }

    uint8_t pick_model(const uint32_t* in, size_t n, uint32_t max_exceptions)
    {
        static std::array<uint32_t, t_block_size> tmp;
        auto beg = tmp.begin();
        auto end = beg + n;
        std::copy_n(in, n, tmp.begin());
        std::sort(beg, end);
        auto except_val_thres = *(end - max_exceptions - 1);
        for (size_t i = 0; i < bands.size(); i++) {
            if (except_val_thres <= bands[i].second) {
                return i;
            }
        }
        return bands.size() - 1;
    }

public:
    std::vector<uint64_t> except_dist;
    std::vector<uint64_t> except_num;
    std::array<uint64_t, 16> model_usage;
    bool required_increasing = false;
    std::string name()
    {
        return "ans_mag_pfor_B" + std::to_string(t_block_size);
    }
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count frequencies for each model
        std::vector<std::vector<uint32_t> > freqs(bands.size());
        for (size_t i = 0; i < bands.size(); i++) {
            freqs[i].resize(bands[i].second + 1);
        }
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            size_t num_blocks = n / t_block_size + 1;
            size_t last_block_size = n % t_block_size;
            if (last_block_size == 0) {
                num_blocks--;
                last_block_size = t_block_size;
            }
            for (size_t j = 0; j < num_blocks; j++) {
                size_t block_offset = j * t_block_size;
                auto b_in = cur_list + block_offset;
                size_t block_size = t_block_size;
                if (j + 1 == num_blocks)
                    block_size = last_block_size;
                auto model_id = pick_model(b_in, block_size, MAX_NUM_EXCEPT);
                for (size_t k = 0; k < block_size; k++) {
                    uint32_t num = cur_list[block_offset + k];
                    if (num > bands[model_id].second) {
                        num = constants::EXCEPT_MARKER;
                    }
                    if (num > constants::MAGNITUDE_START) {
                        num = MAG_OFFSET + magnitude(num);
                    }
                    freqs[model_id][num]++;
                }
            }
        }

        // (2) create models using the computed freq table
        for (size_t i = 0; i < bands.size(); i++) {
            bool empty = true;
            for (size_t j = 0; j < freqs[i].size(); j++) {
                if (freqs[i][j] != 0)
                    empty = false;
            }
            if (!empty)
                models.emplace_back(
                    ans_mag_model(freqs[i], bands, constants::NORM_FREQS_SUM));
        }

        // (3) output the models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        vbyte_encode_u32(out8, models.size());
        for (size_t i = 0; i < models.size(); i++) {
            vbyte_encode_u32(out8, models[i].nmags.size());
            for (size_t j = 0; j < models[i].nmags.size(); j++) {
                vbyte_encode_u32(out8, models[i].nmags[j]);
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
        // auto initin8 = reinterpret_cast<const uint8_t*>(in);
        // auto in8 = initin8;
        // dmodels.resize(thresholds.size());
        // for (size_t i = 0; i < dmodels.size(); i++) {
        //     size_t max = vbyte_decode_u32(in8);
        //     uint32_t base = 0;
        //     for (size_t j = 0; j < max; j++) {
        //         uint16_t cur_freq = vbyte_decode_u32(in8);
        //         for (size_t k = 0; k < cur_freq; k++) {
        //             dmodels[i].table[base + k].sym = j;
        //             dmodels[i].table[base + k].freq = cur_freq;
        //             dmodels[i].table[base + k].base_offset = k;
        //         }
        //         base += cur_freq;
        //     }
        // }
        // size_t pbytes = in8 - initin8;
        // if (pbytes % sizeof(uint32_t) != 0) {
        //     pbytes += sizeof(uint32_t) - (pbytes % (sizeof(uint32_t)));
        // }
        // size_t u32s = pbytes / sizeof(uint32_t);
        // return in + u32s;
        return in;
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
            auto model_id = pick_model_opt(in + block_offset, block_size);
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
        static std::array<uint8_t, t_block_size * 8> tmp_out_buf;
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            model_usage[model_id]++;
            const auto& cur_model = models[model_id];
            auto max_val_in_model = cur_model.max_num_encodeable;
            size_t block_offset = j * t_block_size;
            size_t block_size = t_block_size;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            // reverse encode the block using the selected ANS model
            uint64_t state = 0;
            auto out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
            auto out_start = out_ptr;
            auto exception_bytes = 0;
            auto num_except = 0;
            for (size_t k = 0; k < block_size; k++) {
                uint32_t num = in[block_offset + block_size - k - 1];
                if (num > max_val_in_model) { // exception case!
                    auto except_val = num - (max_val_in_model + 1);
                    if (except_dist.size() < except_val + 1) {
                        except_dist.resize(except_val * 2);
                    }
                    except_dist[except_val]++;
                    exception_bytes
                        += vbyte_encode_reverse(out_ptr, except_val);
                    num = constants::EXCEPT_MARKER;
                    num_except++;
                }
                state = ans_mag_encode(cur_model, state, num, out_ptr);
            }
            if (except_num.size() < num_except + 1) {
                except_num.resize(1 + num_except * 2);
            }
            except_num[num_except]++;
            ans_mag_encode_flush(state, out_ptr);

            // output the encoding
            size_t enc_size = (out_start - out_ptr);
            // fprintff(stderr, "model %u BPI = %lf\n", model_id,
            //     double(enc_size * 8) / double(block_size));
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
        // fprintff(stderr, "percent except = %lf\n",
        //     double(num_except) / double(num_ptr) * 100.0);
    }
    uint32_t* decodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t list_len)
    {
        // auto max_ans_value = thresholds.back().second;
        // size_t num_blocks = list_len / t_block_size;
        // size_t last_block_size = list_len % t_block_size;
        // if (last_block_size) {
        //     num_blocks++;
        // } else {
        //     last_block_size = t_block_size;
        // }

        // static std::vector<uint8_t> block_models;
        // if (block_models.size() < (num_blocks + 1)) {
        //     block_models.resize(num_blocks + 1);
        // }

        // // (1) decode block models
        // auto initin8 = reinterpret_cast<const uint8_t*>(in);
        // auto in8 = initin8;
        // for (size_t i = 0; i < num_blocks; i += 2) {
        //     uint8_t packed_block_types = *in8++;
        //     block_models[i] = packed_block_types >> 4;
        //     block_models[i + 1] = packed_block_types & 15;
        // }

        // // (3) perform actual decoding
        // for (size_t j = 0; j < num_blocks; j++) {
        //     auto model_id = block_models[j];
        //     size_t block_size = t_block_size;
        //     if (j + 1 == num_blocks)
        //         block_size = last_block_size;
        //     if (model_id == 0) { // uniform block
        //         for (size_t k = 0; k < block_size; k++) {
        //             *out++ = 1;
        //         }
        //         continue;
        //     }
        //     const auto& dmodel = dmodels[model_id];
        //     size_t enc_size = vbyte_decode_u32(in8);
        //     auto state = ans_byte_decode_init(in8, enc_size);
        //     for (size_t k = 0; k < block_size; k++) {
        //         // where in the frame are we?
        //         uint32_t state_mod_fs = state & dmodel.frame_size_mask;
        //         const auto& entry = dmodel.table[state_mod_fs];

        //         // update state and renormalize
        //         state = entry.freq * (state >> dmodel.frame_size_log2)
        //             + entry.base_offset;
        //         while (enc_size && state < constants::L) {
        //             uint8_t new_byte = *in8++;
        //             state = (state << constants::OUTPUT_BASE_LOG2)
        //                 | uint32_t(new_byte);
        //             enc_size--;
        //         }

        //         // decode the number and exception if necessary
        //         uint32_t num = entry.sym;
        //         if (num == EXCEPT_MARKER) {
        //             auto fixup = vbyte_decode_u32(in8);
        //             num = max_ans_value + fixup + 1;
        //         }
        //         *out++ = num;
        //     }
        // }

        return out;
    }
};
