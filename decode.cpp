#include <iostream>
#include <vector>

#include "methods.hpp"
#include "util.hpp"

int main(int argc, char const* argv[])
{
    if (argc != 2) {
        printff("%s <compressed_file> > output_file\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::string input_filename = argv[1];
    std::string metadata_filename = input_filename + ".metadata";
    auto in_file = fopen_or_fail(input_filename, "rb");
    auto metadata_file = fopen_or_fail(metadata_filename, "r");

    const auto& results = decompress_lists(in_file, metadata_file);
    const auto& decoded_lists = results.first;
    const auto& decoding_stats = results.second;

    printf("%lu\n", decoded_lists.num_lists);
    for (size_t i = 0; i < decoded_lists.num_lists; i++) {
        auto list_ptr = decoded_lists.list_ptrs[i];
        output_list_to_stdout(list_ptr, decoded_lists.list_sizes[i]);
    }
    fclose_or_fail(in_file);

    double ints_per_nssec = double(decoded_lists.num_postings)
        / double(decoding_stats.time_ns.count());
    double mints_per_sec = ints_per_nssec * 1000;
    std::cerr << "decodingspeed(" << decoding_stats.method << ") "
              << mints_per_sec << " million ints per sec" << std::endl;

    return EXIT_SUCCESS;
}
