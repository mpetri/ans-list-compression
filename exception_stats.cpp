#include <iostream>
#include <vector>

#include "benchmark.hpp"
#include "cutil.hpp"
#include "methods.hpp"
#include "util.hpp"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
namespace fs = boost::filesystem;

po::variables_map parse_cmdargs(int argc, char const* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "produce help message")
        ("input-prefix,i",po::value<std::string>()->required(), "prefix for the input files (d2si)");
    // clang-format on
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            exit(EXIT_SUCCESS);
        }
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc;
        std::cerr << "Missing required option: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (po::error& e) {
        std::cout << desc;
        std::cerr << "Error parsing cmdargs: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    return vm;
}

using mag_table = std::array<uint64_t, constants::MAX_MAG + 1>;

void determine_block_stats(
    const uint32_t* A, uint32_t num_removed, mag_table& stats)
{
    const uint32_t bs = constants::block_size;
    std::array<uint32_t, bs> tmp_buf;
    std::copy(A, A + bs, std::begin(tmp_buf));
    std::sort(std::begin(tmp_buf), std::end(tmp_buf));

    uint32_t new_bs = bs - num_removed;
    uint8_t max_mag = 0;
    for (size_t i = 0; i < new_bs; i++) {
        max_mag = std::max(max_mag, ans_magnitude(tmp_buf[i]));
    }
    stats[max_mag]++;
}

void exception_stats(const list_data& ld, std::string part, uint32_t percent)
{
    const uint32_t bs = constants::block_size;

    mag_table block_stats{ 0 };

    uint32_t elems_removed = double(bs) * double(percent) / 100.0;
    size_t num_blocks = 0;
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        const uint32_t* in = ld.list_ptrs[i];

        for (size_t j = 0; j < list_size; j += bs) {
            auto ptr = in + j;
            determine_block_stats(ptr, elems_removed, block_stats);
            num_blocks++;
        }
    }

    for (size_t i = 0; block_stats.size(); i++) {
        std::cout << part << ";" << percent << ";" << num_blocks << ";" << i
                  << ";" << block_stats[i] << ";" << std::endl;
    }
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_prefix = cmdargs["input-prefix"].as<std::string>();

    auto inputs = read_all_input_ds2i(input_prefix, true);

    exception_stats(inputs.docids, "docids", 0);
    exception_stats(inputs.docids, "docids", 1);
    exception_stats(inputs.docids, "docids", 2);
    exception_stats(inputs.docids, "docids", 3);
    exception_stats(inputs.docids, "docids", 4);
    exception_stats(inputs.docids, "docids", 5);
    exception_stats(inputs.docids, "docids", 10);

    exception_stats(inputs.freqs, "freqs", 0);
    exception_stats(inputs.freqs, "freqs", 1);
    exception_stats(inputs.freqs, "freqs", 2);
    exception_stats(inputs.freqs, "freqs", 3);
    exception_stats(inputs.freqs, "freqs", 4);
    exception_stats(inputs.freqs, "freqs", 5);
    exception_stats(inputs.freqs, "freqs", 10);

    return EXIT_SUCCESS;
}
