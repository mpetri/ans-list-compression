#pragma once

#include <array>
#include <memory>

#include "ans-mag.hpp"
#include "ans-util.hpp"
#include "util.hpp"

namespace constants {
const uint64_t WINDOW = 65; /* should be odd */
const uint64_t PAYLOADBITS = 64;
}

struct ans_simple {
private:
    std::vector<ans_mag_model> models;
    uint8_t pick_model(const uint32_t* in, size_t n)
    {
        uint8_t max_mag = 0;
        for (size_t i = 0; i < n; i++) {
            max_mag = std::max(max_mag, ans_magnitude(in[i]));
        }
        return constants::MAG2SEL[max_mag];
    }

public:
    bool required_increasing = false;
    std::string name() { return "ans_simple" }
public:
    void init(const list_data& input, uint32_t* out, size_t& nvalue)
    {
        // (1) count frequencies for each model
        std::vector<mag_table> mags(constants::NUM_MAGS);
        for (auto& mt : mags)
            mt.fill(0);

        std::array<uint32_t, constants::WINDOW> W = { 1 };
        std::array<uint8_t, constants::WINDOW> M = { 0 };
        std::vector<mag_table> L(constants::NUM_MAGS);
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
                W[pos] = next;
                M[pos] = ans_magnitude(next);
            }

            /* get a new "final" element in to the window */
            while (pos < n) {
                /* get a new "final" element in to the window */
                auto next = cur_list[pos];
                W[constants::WINDOW - 1] = next;
                M[constants::WINDOW - 1] = ans_magnitude(next);
                if (next > max_val) {
                    max_val = next;
                }
                auto mid = constants::WINDOW / 2;
                auto mag = M[mid];
                auto big = constants::MAG2SEL[mag];
                auto ocen = olft = orgt = M[mid];
                /* try to the left first */
                for (uint64_t stp = 1; stp <= constants::WINDOW / 2; stp++) {
                    uint8_t nlft = olft;
                    if (nlft < M[mid - stp]) {
                        nlft = M[mid - stp];
                    }
                    if (nlft * (stp + 1) > constants::PAYLOADBITS) {
                        /* time to stop */
                        break;
                    }
                    A[0] = nlft;
                }
                /* try to the right next */
                for (uint64_t stp = 1; stp <= constants::WINDOW / 2; stp++) {
                    uint8_t nrgt = orgt;
                    if (nrgt < M[mid + stp]) {
                        nrgt = M[mid + stp];
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
                    if (ncen < M[mid - stp]) {
                        ncen = M[mid - stp];
                    }
                    if (ncen < M[mid + stp]) {
                        ncen = M[mid + stp];
                    }
                    if (ncen * (2 * stp + 1) >= constants::PAYLOADBITS) {
                        /* end of the line */
                        break;
                    }
                    ocen = ncen;
                }
                /* select the median */
                auto med = ans_median(olft, ocen, orgt);

                big = constants::MAG2SEL[med];
                L[big][mag]++;
                /* and then copy down and move pos ahead*/
                for (uint64_t i = 1; i < constants::WINDOW; i++) {
                    W[i - 1] = W[i];
                    M[i - 1] = M[i];
                }
                pos++;
            }
        }

        // (2) create the models
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models.emplace_back(ans_mag_model(mags[i], max_val));
        }

        // (4) write out models
        auto initout8 = reinterpret_cast<uint8_t*>(out);
        auto out8 = initout8;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models[i].write(out8);
        }

        // (4) align to u32 boundary
        size_t wb = out8 - initout8;
        if (wb % sizeof(uint32_t) != 0) {
            wb += sizeof(uint32_t) - (wb % (sizeof(uint32_t)));
        }
        nvalue = wb / sizeof(uint32_t);
    }

    const uint32_t* dec_init(const uint32_t* in)
    {
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (uint8_t i = 0; i < constants::NUM_MAGS; i++) {
            models.emplace_back(ans_mag_model(in8));
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
        size_t left = len % t_bs;
        size_t num_blocks = len / t_bs + (left != 0);
        size_t last_block_size = left == 0 ? t_bs : left;

        // (1) determine block models
        static std::vector<uint8_t> block_models;
        if (block_models.size() < num_blocks + 1) {
            block_models.resize(num_blocks + 1);
        }
        for (size_t j = 0; j < num_blocks; j++) {
            size_t block_offset = j * t_bs;
            size_t block_size = t_bs;
            if (j + 1 == num_blocks)
                block_size = last_block_size;
            auto model_id = pick_model(in + block_offset, block_size);
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
        static int list_id = 0;
        list_id++;
        static std::array<uint8_t, t_bs * 8> tmp_out_buf;
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            size_t block_offset = j * t_bs;
            size_t block_size = t_bs;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            if (model_id == 0) { // all 1s. continue
                continue;
            }

            // reverse encode the block using the selected ANS model
            const auto& cur_model = models[model_id];
            uint64_t state = constants::ANS_START_STATE;
            auto out_ptr = tmp_out_buf.data() + tmp_out_buf.size() - 1;
            auto out_start = out_ptr;
            for (size_t k = 0; k < block_size; k++) {
                uint32_t num = in[block_offset + block_size - k - 1];
                state = cur_model.encode(state, num, out_ptr);
            }
            cur_model.flush(state, out_ptr);

            // output the encoding
            size_t enc_size = (out_start - out_ptr);
            ans_vbyte_encode_u64(out8, enc_size);
            memcpy(out8, out_ptr, enc_size);

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
        size_t left = list_len % t_bs;
        size_t num_blocks = list_len / t_bs + (left != 0);
        size_t last_block_size = left == 0 ? t_bs : left;

        static std::vector<uint8_t> block_models;
        if (block_models.size() < (num_blocks + 1)) {
            block_models.resize(num_blocks + 1);
        }

        // (1) decode block models
        auto initin8 = reinterpret_cast<const uint8_t*>(in);
        auto in8 = initin8;
        for (size_t i = 0; i < num_blocks; i += 2) {
            uint8_t packed_block_types = *in8++;
            block_models[i] = packed_block_types >> 4;
            block_models[i + 1] = packed_block_types & 15;
        }

        int list_id = 0;
        list_id++;

        // (2) perform actual decoding
        for (size_t j = 0; j < num_blocks; j++) {
            auto model_id = block_models[j];
            size_t block_size = t_bs;
            if (j + 1 == num_blocks)
                block_size = last_block_size;

            if (model_id == 0) { // uniform block
                for (size_t k = 0; k < block_size; k++) {
                    *out++ = 1;
                }
                continue;
            }
            const auto& model = models[model_id];
            size_t enc_size = ans_vbyte_decode_u64(in8);
            uint64_t state = model.init_decoder(in8, enc_size);
            for (size_t k = 0; k < block_size; k++) {
                *out++ = model.decode(state, in8, enc_size);
            }
        }
        return out;
    }
};
