
namespace constants {
const uint32_t MAX_SIGMA_U16 = 1 << 16;
}

using u16_freq_table = std::array<uint64_t, constants::MAX_SIGMA_U16>;

struct u16_norm_freq {
    uint32_t org;
    uint32_t norm;
    uint16_t sym;
};

void ans_u16_normalize_freqs(u16_freq_table& freqs,
    std::vector<uint32_t>& nfreqs, uint32_t frame_size,
    bool require_all_encodeable = true)
{
    // (1) compute the counts
    if (require_all_encodeable) {
        for (size_t i = 0; i < nfreqs.size(); i++) {
            if (freqs[i] == 0)
                freqs[i] = 1;
        }
    }
    uint64_t num_syms = 0;
    for (size_t i = 0; i < nfreqs.size(); i++) {
        num_syms += freqs[i];
    }

    // (2) crude normalization
    uint32_t actual_freq_csum = 0;
    std::vector<u16_norm_freq> norm_freqs(nfreqs.size());
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
    auto cmp_pdiff_func = [num_syms, frame_size](
        const u16_norm_freq& a, const u16_norm_freq& b) {
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
    auto cmp_sym_func = [](const u16_norm_freq& a, const u16_norm_freq& b) {
        return a.sym < b.sym;
    };
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
inline uint32_t ans_u16_encode(
    const t_model& model, uint32_t state, uint16_t sym, uint8_t*& out8)
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
inline uint32_t ans_u16_fake_encode(
    const t_model& model, uint32_t state, uint16_t sym, size_t& bytes_emitted)
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

inline void ans_u16_encode_flush(uint32_t final_state, uint8_t*& out8)
{
    out8 -= sizeof(uint32_t);
    auto out32 = reinterpret_cast<uint32_t*>(out8);
    *out32 = final_state;
}

inline uint32_t ans_u16_decode_init(const uint8_t*& in8, size_t& encoding_size)
{
    auto in = reinterpret_cast<const uint32_t*>(in8);
    uint32_t initial_state = *in;
    in8 += sizeof(uint32_t);
    encoding_size -= sizeof(uint32_t);
    return initial_state;
}

template <uint32_t t_frame_size> struct ans_u16_encode_model {
    uint32_t max_sym_encodeable;
    std::vector<uint32_t> nfreqs;
    std::vector<uint32_t> base;
    std::vector<uint16_t> csum2sym;
    std::vector<uint32_t> sym_upper_bound;
    static const uint32_t frame_size = t_frame_size;
    ans_u16_encode_model() {}
    ans_u16_encode_model(ans_u16_encode_model&& other)
    {
        max_sym_encodeable = std::move(other.max_sym_encodeable);
        nfreqs = std::move(other.nfreqs);
        base = std::move(other.base);
        csum2sym = std::move(other.csum2sym);
        sym_upper_bound = std::move(other.sym_upper_bound);
    }
    ans_u16_encode_model& operator=(ans_u16_encode_model&& other)
    {
        max_sym_encodeable = std::move(other.max_sym_encodeable);
        nfreqs = std::move(other.nfreqs);
        base = std::move(other.base);
        csum2sym = std::move(other.csum2sym);
        sym_upper_bound = std::move(other.sym_upper_bound);
        return *this;
    }
    ans_u16_encode_model(u16_freq_table& freqs, bool reserve_zero)
    {
        // (1) determine max symbol and allocate tables
        max_sym_encodeable = 0;
        for (size_t i = 0; i < constants::MAX_SIGMA_U16; i++) {
            if (freqs[i] != 0)
                max_sym_encodeable = i;
        }
        nfreqs.resize(max_sym_encodeable + 1);
        sym_upper_bound.resize(max_sym_encodeable + 1);
        base.resize(max_sym_encodeable + 1);
        csum2sym.resize(t_frame_size);
        // (2) normalize frequencies
        ans_u16_normalize_freqs(freqs, nfreqs, t_frame_size, reserve_zero);
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

struct u16_dec_table_entry {
    uint32_t freq;
    uint32_t base_offset;
    uint16_t sym;
};

template <uint32_t t_frame_size> struct ans_u16_decode_model {
    uint32_t max_sym_encodeable;
    static const uint32_t frame_size = t_frame_size;
    static const uint8_t frame_size_log2 = clog2(frame_size);
    static const uint32_t frame_size_mask = frame_size - 1;
    u16_dec_table_entry table[t_frame_size];
};
