#pragma once

#include <iostream>
#include <sstream>

#include "util.hpp"

#include "interp.hpp"
#include "op4.hpp"
#include "qmx.hpp"
#include "simple16.hpp"
#include "vbyte.hpp"

enum class enc_method : char {
    vbyte = 'v',
    interp = 'i',
    simple16 = 's',
    op4 = 'o',
    qmx = 'q',
    unknown = '?',
};

struct list_output {
    std::vector<uint32_t> postings;
    std::vector<uint32_t> list_lens;
    uint64_t num_postings;
    uint32_t num_lists;
    std::chrono::nanoseconds time_ns;
    enc_method comp_method;
    list_output(){};
    list_output(uint32_t nl, uint64_t np, enc_method m) {
        comp_method = m;
        num_lists = nl;
        num_postings = np;
        postings.resize(num_postings);
        list_lens.resize(nl);
    }
};

std::ostream& operator<<(std::ostream& os, enc_method c) {
    switch (c) {
    case enc_method::vbyte:
        os << "vbyte";
        break;
    case enc_method::interp:
        os << "interp";
        break;
    case enc_method::simple16:
        os << "simple16";
        break;
    case enc_method::op4:
        os << "op4";
        break;
    case enc_method::qmx:
        os << "qmx";
        break;
    default:
        os.setstate(std::ios_base::failbit);
    }
    return os;
}

std::string available_methods() {
    std::string opt = "[";
    std::ostringstream s;
    s << enc_method::vbyte << ",";
    s << enc_method::interp << ",";
    s << enc_method::simple16 << ",";
    s << enc_method::op4 << ",";
    s << enc_method::qmx;
    opt += s.str() + "]";
    return opt;
}

enc_method parse_method(std::string arg) {
    if (arg == "vbyte") {
        return enc_method::vbyte;
    }
    if (arg == "interp") {
        return enc_method::interp;
    }
    if (arg == "simple16") {
        return enc_method::simple16;
    }
    if (arg == "op4") {
        return enc_method::op4;
    }
    if (arg == "qmx") {
        return enc_method::qmx;
    }
    return enc_method::unknown;
}

template <typename t_func>
encoding_stats compress_lists(t_func codec_func, list_input& inputs, FILE* f) {
    encoding_stats estats;
    estats.bytes_written = 0;
    // allocate a lage output buffer
    std::vector<uint32_t> out_buf(inputs.num_postings * 1.5);
    uint32_t* initout = out_buf.data();
    size_t u32_written = 0;
    {
        timer t("encode lists");
        uint32_t* out = initout;
        const auto& lists = inputs.postings_lists;
        auto start = high_resolution_clock::now();
        // encode lens as u32 at the start
        for (size_t i = 0; i < lists.size(); i++) {
            *out++ = lists[i].size();
        }
        for (size_t i = 0; i < lists.size(); i++) {
            fprintf(stderr, "%lu encode start at offset %lu\n", i,
                    (out - initout));
            size_t list_size = lists[i].size();
            const uint32_t* in = lists[i].data();
            out += codec_func(in, list_size, out);
        }
        auto stop = high_resolution_clock::now();
        estats.time_ns = stop - start;
        u32_written = (out - initout);
    }
    estats.bytes_written += write_u32s(f, initout, u32_written);
    write_u32(f, inputs.postings_lists.size());
    write_u64(f, inputs.num_postings);
    return estats;
}

template <typename t_func>
list_output decompress_lists(t_func codec_func, FILE* f,
                             enc_method comp_method) {
    auto content = read_file_content_u32(f);
    uint32_t* in = content.data();
    size_t num_u32 = content.size();

    uint32_t num_lists = in[num_u32 - 3];
    uint64_t num_postings = *((uint64_t*)&in[num_u32 - 2]);
    fprintf(stderr, "num_lists = %u\n", num_lists);
    fprintf(stderr, "num_postings = %lu\n", num_postings);

    list_output decoded_lists(num_lists, num_postings, comp_method);

    uint32_t* out = decoded_lists.postings.data();
    uint32_t* initin = in;
    {
        timer t("decode lists");
        auto start = high_resolution_clock::now();
        // readid the list lens first
        for (size_t i = 0; i < num_lists; i++) {
            decoded_lists.list_lens[i] = *in++;
        }
        for (size_t i = 0; i < num_lists; i++) {
            fprintf(stderr, "%lu decode start at offset %lu\n", i,
                    (in - initin));
            in += codec_func(in, out, decoded_lists.list_lens[i]);
            out += decoded_lists.list_lens[i];
        }
        auto stop = high_resolution_clock::now();
        decoded_lists.time_ns = stop - start;
    }
    return decoded_lists;
}

void undo_gaps(list_input& inputs) {
    for (size_t i = 0; i < inputs.postings_lists.size(); i++) {
        size_t list_size = inputs.postings_lists[i].size();
        uint32_t* lst = inputs.postings_lists[i].data();
        for (size_t j = 1; j < list_size; j++)
            lst[i] += lst[i - 1];
    }
}

encoding_stats compress_lists(enc_method method, list_input& inputs, FILE* f) {

    // encode method
    char method_sym = static_cast<char>(method);
    write_byte(f, method_sym);
    encoding_stats es;
    switch (method) {
    case enc_method::vbyte:
        es = compress_lists(vbyte_encode, inputs, f);
        break;
    case enc_method::qmx:
        es = compress_lists(qmx_encode, inputs, f);
        break;
    case enc_method::interp:
        undo_gaps(inputs);
        es = compress_lists(interp_encode, inputs, f);
        break;
    case enc_method::simple16:
        es = compress_lists(simple16_encode, inputs, f);
        break;
    case enc_method::op4:
        es = compress_lists(op4_encode, inputs, f);
        break;
    case enc_method::unknown:
        quit("invalid compression method.");
    }
    es.bytes_written++; // for method selector
    return es;
}

list_output decompress_lists(FILE* f) {
    char method_sym = read_byte(f);
    enc_method method = static_cast<enc_method>(method_sym);

    switch (method) {
    case enc_method::qmx:
        return decompress_lists(qmx_decode, f, method);
    case enc_method::vbyte:
        return decompress_lists(vbyte_decode, f, method);
    case enc_method::interp:
        return decompress_lists(interp_decode, f, method);
    case enc_method::simple16:
        return decompress_lists(simple16_decode, f, method);
    case enc_method::op4:
        return decompress_lists(op4_decode, f, method);
    case enc_method::unknown:
        quit("invalid compression method.");
    }
    // we never go here
    return list_output();
}
