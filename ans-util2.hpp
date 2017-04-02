#pragma once

namespace constants {
const uint32_t MAX_SIGMA = 256;
const uint32_t L = (1u << 23);
const uint32_t OUTPUT_BASE = 256;
const uint8_t OUTPUT_BASE_LOG2 = 8;
}

using freq_table = std::array<uint64_t, constants::MAX_SIGMA>;

constexpr size_t clog2(size_t n) { return ((n < 2) ? 0 : 1 + clog2(n / 2)); }

struct norm_freq {
    uint32_t org;
    uint32_t norm;
    uint8_t sym;
};

void ans_normalize_freqs(freq_table& freqs, std::vector<uint16_t>& nfreqs,
    uint32_t frame_size, bool require_all_encodeable = true)
{
    // (1) compute the counts
    // if (require_all_encodeable) {
    //     for (size_t i = 0; i < nfreqs.size(); i++) {
    //         if (freqs[i] == 0)
    //             freqs[i] = 1;
    //     }
    // }
    uint64_t num_syms = 0;
    for (size_t i = 0; i < nfreqs.size(); i++) {
        num_syms += freqs[i];
    }

    // // freqs[0] == 10%
    // freqs[0] = (num_syms - freqs[0]) / 10;
    // num_syms = 0;
    // for (size_t i = 0; i < nfreqs.size(); i++) {
    //     num_syms += freqs[i];
    // }

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

template <class t_model>
inline uint32_t ans_byte_encode(
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

template <class t_model>
inline uint32_t ans_byte_fake_encode(
    const t_model& model, uint32_t state, uint8_t sym, size_t& bytes_emitted)
{

    uint32_t freq = model.nfreqs[sym];
    uint32_t base = model.base[sym];
    // (1) normalize
    uint32_t sym_range_upper_bound = model.sym_upper_bound[sym];
    while (state >= sym_range_upper_bound) {
        bytes_emitted++;
        state = state >> constants::OUTPUT_BASE_LOG2;
    }

    // (2) transform state
    uint32_t next = ((state / freq) * model.frame_size) + (state % freq) + base;
    return next;
}

inline void ans_byte_encode_flush(uint32_t final_state, uint8_t*& out8)
{
    out8 -= sizeof(uint32_t);
    auto out32 = reinterpret_cast<uint32_t*>(out8);
    *out32 = final_state;
}

inline uint32_t ans_byte_decode_init(const uint8_t*& in8, size_t& encoding_size)
{
    auto in = reinterpret_cast<const uint32_t*>(in8);
    uint32_t initial_state = *in;
    in8 += sizeof(uint32_t);
    encoding_size -= sizeof(uint32_t);
    return initial_state;
}

template <uint32_t t_frame_size> struct ans_byte_encode_model {
    uint32_t max_sym_encodeable;
    std::vector<uint16_t> nfreqs;
    std::vector<uint16_t> base;
    std::vector<uint8_t> csum2sym;
    std::vector<uint32_t> sym_upper_bound;
    static const uint32_t frame_size = t_frame_size;
    ans_byte_encode_model() {}
    ans_byte_encode_model(ans_byte_encode_model&& other)
    {
        max_sym_encodeable = std::move(other.max_sym_encodeable);
        nfreqs = std::move(other.nfreqs);
        base = std::move(other.base);
        csum2sym = std::move(other.csum2sym);
        sym_upper_bound = std::move(other.sym_upper_bound);
    }
    ans_byte_encode_model& operator=(ans_byte_encode_model&& other)
    {
        max_sym_encodeable = std::move(other.max_sym_encodeable);
        nfreqs = std::move(other.nfreqs);
        base = std::move(other.base);
        csum2sym = std::move(other.csum2sym);
        sym_upper_bound = std::move(other.sym_upper_bound);
        return *this;
    }
    ans_byte_encode_model(freq_table& freqs, bool reserve_zero)
    {
        // (1) determine max symbol and allocate tables
        max_sym_encodeable = 0;
        for (size_t i = 0; i < constants::MAX_SIGMA; i++) {
            if (freqs[i] != 0)
                max_sym_encodeable = i;
        }
        nfreqs.resize(max_sym_encodeable + 1);
        sym_upper_bound.resize(max_sym_encodeable + 1);
        base.resize(max_sym_encodeable + 1);
        csum2sym.resize(t_frame_size);
        // (2) normalize frequencies
        ans_normalize_freqs(freqs, nfreqs, t_frame_size, reserve_zero);
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

struct dec_table_entry {
    uint16_t freq;
    uint16_t base_offset;
    uint8_t sym;
    uint8_t vb_sym;
    uint8_t finish;
};

template <uint32_t t_frame_size> struct ans_byte_decode_model {
    uint8_t max_sym_encodeable;
    static const uint32_t frame_size = t_frame_size;
    static const uint8_t frame_size_log2 = clog2(frame_size);
    static const uint32_t frame_size_mask = frame_size - 1;
    uint32_t freq[constants::MAX_SIGMA];
    uint32_t base[constants::MAX_SIGMA];
    uint32_t framesize[constants::MAX_SIGMA];
    dec_table_entry table[t_frame_size];
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

inline void vbyte_freq_count(uint32_t x, freq_table& f)
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

inline void vbyte_encode_u32(uint8_t*& out, uint32_t x)
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

inline uint32_t vbyte_encode_reverse(uint8_t*& out, uint32_t x)
{
    out--;
    if (x < (1U << 7)) {
        *out = static_cast<uint8_t>(x & 127);
        return 1;
    } else if (x < (1U << 14)) {
        *out-- = ans_extract7bitsmaskless<1>(x) & 127;
        *out = ans_extract7bits<0>(x) | 128;
        return 2;
    } else if (x < (1U << 21)) {
        *out-- = ans_extract7bitsmaskless<2>(x) & 127;
        *out-- = ans_extract7bits<1>(x) | 128;
        *out = ans_extract7bits<0>(x) | 128;
        return 3;
    } else if (x < (1U << 28)) {
        *out-- = ans_extract7bitsmaskless<3>(x) & 127;
        *out-- = ans_extract7bits<2>(x) | 128;
        *out-- = ans_extract7bits<1>(x) | 128;
        *out = ans_extract7bits<0>(x) | 128;
        return 4;
    } else {
        *out-- = ans_extract7bitsmaskless<4>(x) & 127;
        *out-- = ans_extract7bits<3>(x) | 128;
        *out-- = ans_extract7bits<2>(x) | 128;
        *out-- = ans_extract7bits<1>(x) | 128;
        *out = ans_extract7bits<0>(x) | 128;
        return 5;
    }
}

inline uint32_t vbyte_size(uint32_t x)
{
    if (x < (1U << 7)) {
        return 1;
    } else if (x < (1U << 14)) {
        return 2;
    } else if (x < (1U << 21)) {
        return 3;
    } else if (x < (1U << 28)) {
        return 4;
    } else {
        return 5;
    }
    return 6;
}

inline uint32_t vbyte_decode_u32(const uint8_t*& input)
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

inline uint32_t vbyte_decode_u32(const uint8_t*& input, size_t& enc_size)
{
    uint32_t x = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t c = *input++;
        enc_size--;
        x += ((c & 127) << shift);
        if (!(c & 128)) {
            return x;
        }
        shift += 7;
    }
    return x;
}