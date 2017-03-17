#include <iostream>
#include <vector>

#include "methods.hpp"
#include "util.hpp"

int main(int argc, char const* argv[]) {
    if (argc != 2) {
        printff("%s <compressed_file> > output_file\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::string input_filename = argv[1];
    auto in_file = fopen_or_fail(input_filename, "rb");

    auto decoded_lists = decompress_lists(in_file);

    printf("%u\n", decoded_lists.num_lists);
    auto pos_ptr = decoded_lists.postings.data();
    for (size_t i = 0; i < decoded_lists.num_lists; i++) {
        output_list_to_stdout(pos_ptr, decoded_lists.list_lens[i]);
        pos_ptr += decoded_lists.list_lens[i];
    }
    fclose_or_fail(in_file);

    double ints_per_nssec = double(decoded_lists.num_postings) /
                            double(decoded_lists.time_ns.count());
    double mints_per_sec = ints_per_nssec * 1000;
    std::cerr << "decodingspeed(" << decoded_lists.comp_method << ") "
              << mints_per_sec << " million ints per sec" << std::endl;

    return EXIT_SUCCESS;
}
