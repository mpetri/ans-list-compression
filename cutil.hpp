#pragma once

#include "util.hpp"

template <typename t_a, typename t_b>
void REQUIRE_EQUAL(const t_a& a, const t_b& b, std::string name)
{
    if (a != b) {
        quit("ERROR. %s not equal.", name.c_str());
    }
}

void REQUIRE_EQUAL(
    const uint32_t* a, const uint32_t* b, size_t n, std::string name)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            quit("ERROR. %s not equal.", name.c_str());
        }
    }
}

void write_metadata(FILE* meta_file, const list_data& ld,
    const std::vector<uint64_t>& list_starts)
{
    fprintf(meta_file, "num_lists = %lu\n", ld.num_lists);
    fprintf(meta_file, "num_postings = %lu\n", ld.num_postings);
    for (size_t i = 0; i < ld.num_lists; i++) {
        fprintf(meta_file, "list len %lu = %u\n", i, ld.list_sizes[i]);
    }
    for (size_t i = 0; i < ld.num_lists + 1; i++) {
        fprintf(meta_file, "list start %lu = %lu\n", i, list_starts[i]);
    }
}

void read_metadata(
    FILE* meta_file, list_data& ld, std::vector<uint64_t>& list_starts)
{
    // (1) read the meta data
    size_t num_lists = 0;
    size_t num_postings = 0;
    if (fscanf(meta_file, "num_lists = %lu\n", &num_lists) != 1)
        quit("can't parse num_lists metadata");
    fprintf(stderr, "num_lists = %lu\n", num_lists);
    if (fscanf(meta_file, "num_postings = %lu\n", &num_postings) != 1)
        quit("can't parse num_postings metadata");
    fprintf(stderr, "num_postings = %lu\n", num_postings);
    ld = list_data(num_lists);
    ld.num_postings = num_postings;
    list_starts.resize(num_lists + 1);
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_num = 0;
        uint32_t len = 0;
        if (fscanf(meta_file, "list len %lu = %u\n", &list_num, &len) != 2) {
            quit("can't parse list len metadata");
        }
        ld.list_sizes[list_num] = len;
        ld.list_ptrs[list_num]
            = (uint32_t*)aligned_alloc(16, len * sizeof(uint32_t) + 4096);
    }
    for (size_t i = 0; i < ld.num_lists + 1; i++) {
        size_t list_num = 0;
        uint64_t offset = 0;
        if (fscanf(meta_file, "list start %lu = %lu\n", &list_num, &offset)
            != 2) {
            quit("can't parse list len metadata");
        }
        list_starts[list_num] = offset;
    }
}

void prefix_sum_lists(list_data& ld)
{
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        uint32_t* lst = ld.list_ptrs[i];
        for (size_t j = 1; j < list_size; j++)
            lst[j] += lst[j - 1];
    }
}

void undo_prefix_sum_lists(list_data& ld)
{
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        uint32_t* lst = ld.list_ptrs[i];
        size_t prev = 0;
        for (size_t j = 0; j < list_size; j++) {
            size_t cur = lst[j];
            lst[j] = cur - prev;
            prev = cur;
        }
    }
}