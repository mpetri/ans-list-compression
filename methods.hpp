#pragma once

#include <cstdlib>
#include <iostream>
#include <memory>
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

const char* to_string(enc_method c)
{
    switch (c) {
    case enc_method::vbyte:
        return "vbyte";
        break;
    case enc_method::interp:
        return "interp";
        break;
    case enc_method::simple16:
        return "simple16";
        break;
    case enc_method::op4:
        return "op4";
        break;
    case enc_method::qmx:
        return "qmx";
        break;
    default:
        return "invalid";
    }
    return "invalid";
}

std::string available_methods()
{
    std::string opt = "[";
    std::ostringstream s;
    s << to_string(enc_method::vbyte) << ",";
    s << to_string(enc_method::interp) << ",";
    s << to_string(enc_method::simple16) << ",";
    s << to_string(enc_method::op4) << ",";
    s << to_string(enc_method::qmx);
    opt += s.str() + "]";
    return opt;
}

enc_method parse_method(std::string arg)
{
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
processing_stats compress_lists(t_func codec_func, list_data& ld, FILE* f)
{
    processing_stats stats;
    stats.bytes_written = 0;
    // allocate a lage output buffer
    std::vector<uint32_t> out_buf(ld.num_postings * 1.5);
    uint32_t* initout = out_buf.data();
    size_t u32_written = 0;
    {
        timer t("encode lists");
        uint32_t* out = initout;
        size_t align_size = ld.num_postings;
        out = align_ptr(16, 1, out, align_size);
        auto start = high_resolution_clock::now();
        for (size_t i = 0; i < ld.num_lists; i++) {
            size_t list_size = ld.list_sizes[i];
            const uint32_t* in = ld.list_ptrs[i];
            out += codec_func(in, list_size, out);
        }
        auto stop = high_resolution_clock::now();
        stats.time_ns = stop - start;
        u32_written = (out - initout);
    }
    stats.bytes_written += write_u32s(f, initout, u32_written);
    return stats;
}

template <typename t_func>
processing_stats decompress_lists(
    t_func codec_func, FILE* f, list_data& ld, enc_method comp_method)
{
    processing_stats stats;
    auto content = read_file_content_u32(f);
    uint32_t* in = content.data();
    size_t content_size = content.size() * sizeof(uint32_t);
    {
        timer t("decode lists");
        in = align_ptr(16, 1, in, content_size);
        auto start = high_resolution_clock::now();
        for (size_t i = 0; i < ld.num_lists; i++) {
            in += codec_func(in, ld.list_ptrs[i], ld.list_sizes[i]);
        }
        auto stop = high_resolution_clock::now();
        stats.time_ns = stop - start;
    }
    return stats;
}

void undo_gaps(list_data& ld)
{
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        uint32_t* lst = ld.list_ptrs[i];
        for (size_t j = 1; j < list_size; j++)
            lst[i] += lst[i - 1];
    }
}

processing_stats compress_lists(
    enc_method m, list_data& ld, FILE* f, FILE* meta)
{
    // (1) write the meta data
    fprintf(meta, "encoding = %s\n", to_string(m));
    fprintf(meta, "num_lists = %lu\n", ld.num_lists);
    fprintf(meta, "num_postings = %lu\n", ld.num_postings);
    for (size_t i = 0; i < ld.num_lists; i++) {
        fprintf(meta, "list len %lu = %u\n", i, ld.list_sizes[i]);
    }
    fclose(meta);

    // (2) encode data using method
    processing_stats es;
    switch (m) {
    case enc_method::vbyte:
        es = compress_lists(vbyte_encode, ld, f);
        break;
    case enc_method::qmx:
        es = compress_lists(qmx_encode, ld, f);
        break;
    case enc_method::interp:
        undo_gaps(ld);
        es = compress_lists(interp_encode, ld, f);
        break;
    case enc_method::simple16:
        es = compress_lists(simple16_encode, ld, f);
        break;
    case enc_method::op4:
        es = compress_lists(op4_encode, ld, f);
        break;
    case enc_method::unknown:
        quit("invalid compression method.");
    }
    es.method = to_string(m);
    return es;
}

std::pair<list_data, processing_stats> decompress_lists(FILE* f, FILE* meta)
{
    // (1) read the meta data
    char method_name[256] = { 0 };
    size_t num_lists = 0;
    size_t num_postings = 0;
    if (fscanf(meta, "encoding = %s\n", method_name) != 1)
        quit("can't parse encoding metadata");
    fprintf(stderr, "encoding = %s\n", method_name);
    if (fscanf(meta, "num_lists = %lu\n", &num_lists) != 1)
        quit("can't parse num_lists metadata\n");
    fprintf(stderr, "num_lists = %lu\n", num_lists);
    if (fscanf(meta, "num_postings = %lu\n", &num_postings) != 1)
        quit("can't parse num_postings metadata");
    fprintf(stderr, "num_postings = %lu\n", num_postings);
    list_data ld(num_lists);
    ld.num_postings = num_postings;
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_num = 0;
        uint32_t len = 0;
        if (fscanf(meta, "list len %lu = %u\n", &list_num, &len) != 2) {
            quit("can't parse list len metadata");
        }
        ld.list_sizes[list_num] = len;
        ld.list_ptrs[list_num]
            = (uint32_t*)aligned_alloc(16, len * sizeof(uint32_t) + 4096);
    }
    fclose(meta);

    enc_method method = parse_method(method_name);
    processing_stats ds;
    switch (method) {
    case enc_method::qmx:
        ds = decompress_lists(qmx_decode, f, ld, method);
        break;
    case enc_method::vbyte:
        ds = decompress_lists(vbyte_decode, f, ld, method);
    case enc_method::interp:
        ds = decompress_lists(interp_decode, f, ld, method);
    case enc_method::simple16:
        ds = decompress_lists(simple16_decode, f, ld, method);
    case enc_method::op4:
        ds = decompress_lists(op4_decode, f, ld, method);
    case enc_method::unknown:
        quit("invalid compression method.");
    }
    ds.method = to_string(method);
    return { std::move(ld), ds };
}
