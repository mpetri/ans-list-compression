#include <iostream>
#include <vector>

#include "util.hpp"

int main(int argc, char const* argv[])
{
    if (argc != 2) {
        printff("%s <block_size> < input_file > output_file\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t block_size = std::atoi(argv[1]);

    auto inputs = read_all_input_from_stdin();

    // (1) determine new number of lists
    size_t lists_discarded = 0;
    size_t new_num_lists = 0;
    for (size_t i = 0; i < inputs.num_lists; i++) {
        auto cur_size = inputs.list_sizes[i];
        if (cur_size < block_size) {
            lists_discarded++;
        } else {
            new_num_lists++;
        }
    }

    // (2) do the filtering
    printf("%lu\n", new_num_lists);
    size_t postings_discarded = 0;
    size_t new_num_postings = 0;
    for (size_t i = 0; i < inputs.num_lists; i++) {
        auto cur_size = inputs.list_sizes[i];
        // (1) skip small lists completely
        if (cur_size < block_size) {
            postings_discarded += cur_size;
            continue;
        }
        // (2) larger lists are multiples of block_size. we cut of the rest
        size_t num_blocks = cur_size / block_size;
        size_t left = cur_size % block_size;
        postings_discarded += left;
        const auto& list = inputs.list_ptrs[i];
        size_t new_list_len = num_blocks * block_size;
        new_num_postings += new_list_len;
        printf("%lu\n", new_list_len);
        for (size_t j = 0; j < num_blocks; j++) {
            auto offset = j * block_size;
            for (size_t k = 0; k < block_size; k++) {
                printf("%u\n", list[offset + k]);
            }
        }
    }

    // (3) print stats to stderr
    double percent_lists_discarded
        = double(lists_discarded) / double(inputs.num_lists) * 100;
    double percent_postings_discarded
        = double(postings_discarded) / double(inputs.num_postings) * 100;

    fprintf(stderr, "initial num_lists = %lu\n", inputs.num_lists);
    fprintf(stderr, "new num_lists = %lu\n", new_num_lists);
    fprintf(stderr, "initial num_postings = %lu\n", inputs.num_postings);
    fprintf(stderr, "new num_postings = %lu\n", new_num_postings);
    fprintf(stderr, "postings discarded = %lu\n", postings_discarded);
    fprintf(stderr, "lists discarded = %lu\n", lists_discarded);
    fprintf(stderr, "percentage of lists discarded = %lf\n",
        percent_lists_discarded);
    fprintf(stderr, "percentage of postings discarded = %lf\n",
        percent_postings_discarded);

    return EXIT_SUCCESS;
}
