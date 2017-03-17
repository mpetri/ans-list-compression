#include <iostream>
#include <vector>

#include "methods.hpp"
#include "util.hpp"

int main(int argc, char const* argv[]) {
    if (argc != 3) {
        printff("%s <method> <out_file> < input_file\n", argv[0]);
        printff("   where methods = %s\n", available_methods().c_str());
        return EXIT_FAILURE;
    }
    auto method = parse_method(argv[1]);
    std::string output_filename = argv[2];
    auto out_file = fopen_or_fail(output_filename, "wb");

    auto inputs = read_all_input_from_stdin();

    auto stats = compress_lists(method, inputs, out_file);

    double bpi = double(stats.bytes_written * 8) / double(inputs.num_postings);
    double ints_per_nssec =
        double(inputs.num_postings) / double(stats.time_ns.count());
    double mints_per_sec = ints_per_nssec * 1000;
    std::cout << "space(" << method << ") " << bpi << " bits per integer"
              << std::endl;
    std::cout << "encodingspeed(" << method << ") " << mints_per_sec
              << " million ints per sec" << std::endl;
    fclose_or_fail(out_file);
    return EXIT_SUCCESS;
}
