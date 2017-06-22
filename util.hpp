#pragma once

#include <chrono>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <memory>
#include <type_traits>

#include "constants.hpp"

using namespace std::chrono;

inline void* align1(
    size_t __align, size_t __size, void*& __ptr, size_t& __space) noexcept
{
    const auto __intptr = reinterpret_cast<uintptr_t>(__ptr);
    const auto __aligned = (__intptr - 1u + __align) & -__align;
    const auto __diff = __aligned - __intptr;
    if ((__size + __diff) > __space)
        return nullptr;
    else {
        __space -= __diff;
        return __ptr = reinterpret_cast<void*>(__aligned);
    }
}

inline void* aligned_alloc(std::size_t alignment, std::size_t size)
{
    if (alignment < std::alignment_of<void*>::value) {
        alignment = std::alignment_of<void*>::value;
    }
    std::size_t n = size + alignment - 1;
    void* p1 = 0;
    void* p2 = std::malloc(n + sizeof p1);
    if (p2) {
        p1 = static_cast<char*>(p2) + sizeof p1;
        (void)align1(alignment, size, p1, n);
        *(static_cast<void**>(p1) - 1) = p2;
    }
    return p1;
}

inline void aligned_free(void* ptr)
{
    if (ptr) {
        void* p = *(static_cast<void**>(ptr) - 1);
        std::free(p);
    }
}

struct processing_stats {
    uint64_t bytes_written;
    std::chrono::nanoseconds time_ns;
    std::string method;
};

struct list_data {
    std::vector<uint32_t*> list_ptrs;
    std::vector<uint32_t> list_sizes;
    uint64_t num_postings = 0;
    uint64_t num_lists = 0;
    list_data(){};
    list_data(list_data&& ld)
    {
        num_postings = ld.num_postings;
        num_lists = ld.num_lists;
        list_ptrs = std::move(ld.list_ptrs);
        list_sizes = std::move(ld.list_sizes);
        ld.num_postings = 0;
        ld.num_lists = 0;
    }
    list_data(const list_data& ld)
    {
        num_postings = ld.num_postings;
        num_lists = ld.num_lists;
        list_ptrs.resize(ld.list_ptrs.size());
        list_sizes = ld.list_sizes;
        for (size_t i = 0; i < num_lists; i++) {
            list_ptrs[i] = (uint32_t*)aligned_alloc(
                16, list_sizes[i] * sizeof(uint32_t));
            for (size_t j = 0; j < list_sizes[i]; j++) {
                list_ptrs[i][j] = ld.list_ptrs[i][j];
            }
        }
    }
    list_data& operator=(list_data&& ld)
    {
        num_postings = ld.num_postings;
        num_lists = ld.num_lists;
        list_ptrs = std::move(ld.list_ptrs);
        list_sizes = std::move(ld.list_sizes);
        ld.num_postings = 0;
        ld.num_lists = 0;
        return *this;
    }
    list_data(uint32_t nl)
    {
        num_lists = nl;
        list_ptrs.resize(num_lists);
        list_sizes.resize(num_lists);
        for (size_t i = 0; i < nl; i++)
            list_ptrs[i] = nullptr;
    }
    ~list_data()
    {
        for (size_t i = 0; i < num_lists; i++)
            if (list_ptrs[i]) {
                aligned_free(list_ptrs[i]);
            }
    }
};

struct ds2i_data {
    uint32_t num_docs;
    list_data docids;
    list_data freqs;
};

struct timer {
    high_resolution_clock::time_point start;
    std::string name;
    timer(const std::string& _n)
        : name(_n)
    {
        std::cerr << "START(" << name << ")" << std::endl;
        start = high_resolution_clock::now();
    }
    ~timer()
    {
        auto stop = high_resolution_clock::now();
        std::cerr << "STOP(" << name << ") - "
                  << duration_cast<milliseconds>(stop - start).count() / 1000.0f
                  << " sec" << std::endl;
    }
};

int fprintff(FILE* f, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(f, format, args);
    va_end(args);
    fflush(f);
    return ret;
}

void quit(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);
    va_end(args);
    if (errno != 0) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    exit(EXIT_FAILURE);
}

void output_list_to_stdout(uint32_t* list, uint32_t n)
{
    printf("%u\n", n);
    for (uint32_t j = 0; j < n; j++) {
        printf("%u\n", list[j]);
    }
}

std::vector<uint8_t> read_file_content(FILE* f)
{
    std::vector<uint8_t> content;
    auto cur = ftell(f);
    fseek(f, 0, SEEK_END);
    auto end = ftell(f);
    size_t file_size = (end - cur);
    content.resize(file_size);
    fseek(f, cur, SEEK_SET);
    size_t ret = fread(content.data(), 1, file_size, f);
    if (ret != file_size) {
        quit("reading file content failed: %d", ret);
    }
    return content;
}

std::vector<uint32_t> read_file_content_u32(FILE* f)
{
    std::vector<uint32_t> content;
    auto cur = ftell(f);
    fseek(f, 0, SEEK_END);
    auto end = ftell(f);
    size_t file_size = (end - cur);
    size_t file_size_u32 = file_size / sizeof(uint32_t);
    if (file_size % sizeof(uint32_t) != 0) {
        quit("reading file content failed: file size % 32bit != 0");
    }
    content.resize(file_size_u32);
    fseek(f, cur, SEEK_SET);
    size_t ret = fread(content.data(), sizeof(uint32_t), file_size_u32, f);
    if (ret != file_size_u32) {
        quit("reading file content failed: %d", ret);
    }
    return content;
}

FILE* fopen_or_fail(std::string file_name, const char* mode)
{
    FILE* out_file = fopen(file_name.c_str(), mode);
    if (!out_file) {
        quit("opening output file %s failed", file_name.c_str());
    }
    return out_file;
}

void fclose_or_fail(FILE* f)
{
    int ret = fclose(f);
    if (ret != 0) {
        quit("closing file failed");
    }
}

uint64_t read_u64(FILE* f)
{
    uint64_t x;
    int ret = fread(&x, sizeof(uint64_t), 1, f);
    if (ret != 1) {
        quit("read byte from file failed: %d != %d", ret, 1);
    }
    return x;
}

size_t write_u64(FILE* f, uint64_t x)
{
    size_t ret = fwrite(&x, sizeof(uint64_t), 1u, f);
    if (ret != 1u) {
        quit("writing byte to file: %u != %u", ret, 1u);
    }
    return sizeof(uint64_t);
}

uint32_t read_u32(FILE* f)
{
    uint32_t x;
    int ret = fread(&x, sizeof(uint32_t), 1, f);
    if (feof(f)) {
        return 0;
    }
    if (ret != 1) {
        quit("read u32 from file failed: %d != %d", ret, 1);
    }
    return x;
}

void read_u32s(FILE* f, void* ptr, size_t n)
{
    size_t ret = fread(ptr, sizeof(uint32_t), n, f);
    if (ret != n) {
        quit("read u32s from file failed: %d != %d", ret, n);
    }
}

size_t write_u32(FILE* f, uint32_t x)
{
    size_t ret = fwrite(&x, sizeof(uint32_t), 1u, f);
    if (ret != 1u) {
        quit("writing byte to file: %u != %u", ret, 1u);
    }
    return sizeof(uint32_t);
}

size_t write_u32s(FILE* f, uint32_t* buf, size_t n)
{
    size_t ret = fwrite(buf, sizeof(uint32_t), n, f);
    if (ret != n) {
        quit("writing byte to file: %u != %u", ret, n);
    }
    return n * sizeof(uint32_t);
}

size_t write_byte(FILE* f, uint8_t x)
{
    size_t ret = fwrite(&x, 1, 1, f);
    if (ret != 1u) {
        quit("writing byte to file: %u != %u", ret, 1);
    }
    return 1;
}

uint8_t read_byte(FILE* f)
{
    uint8_t x;
    int ret = fread(&x, 1, 1, f);
    if (ret != 1) {
        quit("read byte from file failed: %d != %d", ret, 1);
    }
    return x;
}

inline uint32_t* align_ptr(
    size_t __align, size_t __size, uint32_t*& __ptr, size_t& __space) noexcept
{
    const auto __intptr = reinterpret_cast<uintptr_t>(__ptr);
    const auto __aligned = (__intptr - 1u + __align) & -__align;
    const auto __diff = __aligned - __intptr;
    if ((__size + __diff) > __space)
        return nullptr;
    else {
        __space -= __diff;
        return __ptr = reinterpret_cast<uint32_t*>(__aligned);
    }
}

inline const uint32_t* align_ptr(size_t __align, size_t __size,
    const uint32_t*& __ptr, size_t& __space) noexcept
{
    const auto __intptr = reinterpret_cast<uintptr_t>(__ptr);
    const auto __aligned = (__intptr - 1u + __align) & -__align;
    const auto __diff = __aligned - __intptr;
    if ((__size + __diff) > __space)
        return nullptr;
    else {
        __space -= __diff;
        return __ptr = reinterpret_cast<const uint32_t*>(__aligned);
    }
}

std::vector<uint32_t> read_uint32_list(FILE* f)
{
    uint32_t list_len = read_u32(f);
    if (list_len == 0)
        return std::vector<uint32_t>();
    std::vector<uint32_t> list(list_len);
    read_u32s(f, list.data(), list_len);
    for (uint32_t j = 0; j < list_len; j++) {
        list[j]++; // ensure there are no 0s
    }
    return list;
}

ds2i_data read_all_input_ds2i(std::string ds2i_prefix, bool remove_nonfull)
{
    const uint32_t block_size = constants::block_size;

    ds2i_data ds2i;
    timer t("read input lists from " + ds2i_prefix);

    std::string docs_file = ds2i_prefix + ".docs";
    auto df = fopen_or_fail(docs_file, "rb");
    {
        // (1) skip the numdocs list
        read_uint32_list(df);
        // (2) keep reading lists
        uint32_t max_doc_id = 0;
        while (!feof(df)) {
            const auto& list = read_uint32_list(df);
            size_t n = list.size();
            if (n == 0)
                break;

            // remove non-full parts as we read things
            if (remove_nonfull && n < block_size)
                break;
            if (remove_nonfull && n % block_size != 0) {
                n = n - (n % block_size);
            }

            max_doc_id = std::max(max_doc_id, list.back());
            ds2i.docids.list_sizes.push_back(n);
            uint32_t* ptr = (uint32_t*)aligned_alloc(16, n * sizeof(uint32_t));
            ds2i.docids.list_ptrs.push_back(ptr);
            auto in_ptr = list.data();
            memcpy(ptr, in_ptr, n * sizeof(uint32_t));
            ds2i.docids.num_lists++;
            ds2i.docids.num_postings += n;
        }
        ds2i.num_docs = max_doc_id - 1;
    }

    std::string freqs_file = ds2i_prefix + ".freqs";
    auto ff = fopen_or_fail(freqs_file, "rb");
    {
        while (!feof(ff)) {
            const auto& list = read_uint32_list(ff);
            size_t n = list.size();
            if (n == 0)
                break;
            ds2i.freqs.list_sizes.push_back(n);
            uint32_t* ptr = (uint32_t*)aligned_alloc(16, n * sizeof(uint32_t));
            ds2i.freqs.list_ptrs.push_back(ptr);
            auto in_ptr = list.data();
            memcpy(ptr, in_ptr, n * sizeof(uint32_t));
            ds2i.freqs.num_lists++;
            ds2i.freqs.num_postings += n;
        }
    }

    fprintf(stderr, "num_docs = %u\n", ds2i.num_docs);
    fprintf(stderr, "num_lists = %lu\n", ds2i.freqs.num_lists);
    fprintf(stderr, "num_postings = %lu\n", ds2i.freqs.num_postings);

    return ds2i;
}
