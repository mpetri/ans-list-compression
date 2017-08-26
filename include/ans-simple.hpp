#pragma once

#include <array>
#include <memory>

#include "ans-mag-fast.hpp"
#include "ans-mag-small.hpp"
#include "ans-util.hpp"
#include "util.hpp"

namespace constants {
const uint64_t WINDOW = 65; /* should be odd */
const uint64_t PAYLOADBITS = 64;
}

struct enc_res {
    uint8_t model_id;
    uint64_t span;
    uint64_t word;
};

struct ans_simple {
private:
    std::vector<ans_mag_model_small> models_small;
    std::vector<ans_mag_model_fast> models_fast;

private:
    enc_res pick_model(const uint32_t* in, size_t n)
    {
        uint8_t best_model = 0;
        uint64_t best_span = 0;
        uint64_t best_word = 0;
        for (size_t i = 0; i < models_fast.size(); i++) {
            const auto& m = models_fast[i];
            auto span_and_word = m.try_encode_u64(in, n);
            if (span_and_word.first > best_span) {
                best_model = i;
                best_span = span_and_word.first;
                best_word = span_and_word.second;
            }
        }
        for (size_t i = 0; i < models_small.size(); i++) {
            const auto& m = models_small[i];
            auto span_and_word = m.try_encode_u64(in, n);
            if (span_and_word.first > best_span) {
                best_model = i + models_fast.size();
                best_span = span_and_word.first;
                best_word = span_and_word.second;
            }
        }
        return enc_res{ best_model, best_span, best_word };
    }

public:
    bool required_increasing = false;
    std::string name() { return "ans_simple"; }

    const uint32_t* load_binary(const uint32_t* in)
    {
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            if (i <= constants::MAX_FAST_SEL) {
                models_fast.emplace_back(ans_mag_model_fast(in8));
            } else {
                models_small.emplace_back(ans_mag_model_small(in8));
            }
        }
        size_t pbytes = in8 - initin8;
        if (pbytes % sizeof(uint32_t) != 0) {
            pbytes += sizeof(uint32_t) - (pbytes % (sizeof(uint32_t)));
        }
        size_t u32s = pbytes / sizeof(uint32_t);
        return in + u32s;
    }
    void store_binary(uint32_t* out, size_t& nvalue) const
    {
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        size_t num_fast_models = 0;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            if (i <= constants::MAX_FAST_SEL) {
                models_fast[i].write(out8);
                num_fast_models++;
            } else {
                models_small[i - num_fast_models].write(out8);
            }
        }

        // (4) align to u32 boundary
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        }
        nvalue = wb / sizeof(uint32_t);
    }
    void load_plain(std::ifstream& is)
    {
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            if (i <= constants::MAX_FAST_SEL) {
                models_fast.emplace_back(ans_mag_model_fast(is));
            } else {
                models_small.emplace_back(ans_mag_model_small(is));
            }
        }
    }
    void store_plain(std::ofstream& os) const
    {
        size_t num_fast_models = 0;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            if (i <= constants::MAX_FAST_SEL) {
                models_fast[i].write_plain(os);
                num_fast_models++;
            } else {
                models_small[i - num_fast_models].write_plain(os);
            }
        }
    }

public:
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count frequencies for each model
        std::vector<uint8_t> M(4096 * constants::WINDOW, 0);
        std::vector<mag_table> L(constants::NUM_MAGS);
        for (auto& mt : L)
            mt.fill(0);
        uint32_t max_val = 0;
        for (size_t i = 0; i < input.num_lists; i++) {
            const auto& cur_list = input.list_ptrs[i];
            size_t n = input.list_sizes[i];
            size_t pos = 0;

            /* (nearly) fill the window and get ready for steady-state
             * processing */
            for (pos = 0; pos < constants::WINDOW; pos++) {
                auto next = cur_list[pos];
                if (next > max_val) {
                    max_val = next;
                }
                M[pos] = ans_magnitude(next);
            }

            /* get a new "final" element in to the window */
            size_t off = 0;
            while (pos < n) {
                /* get a new "final" element in to the window */
                auto next = cur_list[pos];
                M[off + constants::WINDOW - 1] = ans_magnitude(next);
                if (next > max_val) {
                    max_val = next;
                }
                auto mid = constants::WINDOW / 2;
                auto mag = M[off + mid];
                uint8_t ocen = M[off + mid], olft = M[off + mid],
                        orgt = M[off + mid];
                /* try to the left first */
                for (uint64_t stp = 1; stp <= constants::WINDOW / 2; stp++) {
                    uint8_t nlft = olft;
                    if (nlft < M[off + mid - stp]) {
                        nlft = M[off + mid - stp];
                    }
                    if (nlft * (stp + 1) > constants::PAYLOADBITS) {
                        /* time to stop */
                        break;
                    }
                    olft = nlft;
                }
                /* try to the right next */
                for (uint64_t stp = 1; stp <= constants::WINDOW / 2; stp++) {
                    uint8_t nrgt = orgt;
                    if (nrgt < M[off + mid + stp]) {
                        nrgt = M[off + mid + stp];
                    }
                    if (nrgt * (stp + 1) > constants::PAYLOADBITS) {
                        /* time to stop */
                        break;
                    }
                    orgt = nrgt;
                }
                /* and then symmetric about the middle */
                for (uint64_t stp = 1; stp <= constants::WINDOW / 2; stp++) {
                    uint8_t ncen = ocen;
                    if (ncen < M[off + mid - stp]) {
                        ncen = M[off + mid - stp];
                    }
                    if (ncen < M[off + mid + stp]) {
                        ncen = M[off + mid + stp];
                    }
                    if (ncen * (2 * stp + 1) >= constants::PAYLOADBITS) {
                        /* end of the line */
                        break;
                    }
                    ocen = ncen;
                }
                /* select the median */
                auto med = ans_median(olft, ocen, orgt);

                auto big = constants::MAG2SEL[med];
                L[big][mag]++;

                /* and then copy down and move pos ahead*/
                if ((off + constants::WINDOW + 1) >= M.size()) {
                    for (uint64_t i = 1; i < constants::WINDOW; i++) {
                        M[i - 1] = M[off + i];
                    }
                    off = 0;
                } else {
                    off++;
                }
                pos++;
            }
        }
        fprintf(stderr, "gather stats done.\n");

        // (2) create the models
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            auto maxv = ans_max_val_in_mag(constants::SEL2MAG[i], max_val);
            // (2a) ensure we can encode everything up 2 max_mag
            uint8_t max_mag = 0;
            for (size_t j = 0; j < L[i].size(); j++) {
                if (L[i][j] != 0)
                    max_mag = j;
            }
            if (max_mag != 0) {
                for (size_t j = 0; j <= max_mag; j++) {
                    if (L[i][j] == 0)
                        L[i][j] = 1;
                }
            }
            if (i <= constants::MAX_FAST_SEL) {
                models_fast.emplace_back(L[i], maxv);
            } else {
                models_small.emplace_back(L[i], maxv);
            }
        }
        fprintf(stderr, "create models done.\n");

        // (4) write out models
        if (out != nullptr) {
            store_binary(out, nvalue);
        }
    }

    const uint32_t* dec_init(const uint32_t* in)
    {
        std::cerr << "dec_init() START" << std::endl;
        auto new_out = load_binary(in);
        std::cerr << "dec_init() STOP" << std::endl;
        return new_out;
    }

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
        // fprintf(stderr, "encodeArray START\n");
        static std::vector<uint8_t> model_ids;
        static std::vector<uint64_t> encoded_data;
        if (model_ids.size() < (len + 1)) {
            model_ids.resize(len + 1);
            encoded_data.resize(len + 1);
        }

        size_t words_written = 0;
        size_t pos = 0;
        while (pos < len) {
            size_t remaining = len - pos;
            auto enc_res = pick_model(in + pos, remaining);
            model_ids[words_written] = enc_res.model_id;
            encoded_data[words_written++] = enc_res.word;
            pos += enc_res.span;
        }

        // (3) write to output
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;

        // (3a) write selectors
        ans_vbyte_encode_u64(out8, words_written);
        for (size_t i = 0; i < words_written; i += 2) {
            uint8_t packed_selectors = (model_ids[i] << 4) + (model_ids[i + 1]);
            *out8++ = packed_selectors;
        }

        // (3a) write data
        auto out64 = reinterpret_cast<uint64_t*>(out8);
        for (size_t i = 0; i < words_written; i++) {
            *out64++ = encoded_data[i];
        }

        // (4) align to u32 boundary
        out8 = reinterpret_cast<uint8_t*>(out64);
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        }
        nvalue = wb / sizeof(uint32_t);
    }
    const uint32_t* decodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t list_len)
    {
        // fprintf(stderr, "decodeArray START\n");
        // (1) decode selectors
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        size_t num_sels = ans_vbyte_decode_u64(in8);
        static std::vector<uint8_t> selectors;
        if (selectors.size() < (num_sels + 1)) {
            selectors.resize(num_sels + 1);
        }
        for (size_t i = 0; i < num_sels; i += 2) {
            uint8_t packed_sels = *in8++;
            selectors[i] = packed_sels >> 4;
            selectors[i + 1] = packed_sels & 15;
        }

        // (2) decode content
        auto in64 = reinterpret_cast<const uint64_t*>(in8);
        for (size_t i = 0; i < num_sels; i++) {
            auto model_id = selectors[i];
            uint64_t state = *in64++;
            if (model_id <= constants::MAX_FAST_SEL) {
                const auto& model = models_fast[model_id];
                model.decode_u64(state, out);
            } else {
                const auto& model = models_small[model_id - models_fast.size()];
                model.decode_u64(state, out);
            }
        }

        // (3) decode content
        in8 = reinterpret_cast<const uint8_t*>(in64);
        size_t rb = in8 - initin8;
        if (rb % sizeof(uint32_t) != 0) {
            rb += sizeof(uint32_t) - (rb % (sizeof(uint32_t)));
        }
        return reinterpret_cast<const uint32_t*>(initin8 + wb);
    }
};
