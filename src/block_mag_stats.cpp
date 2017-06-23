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
using mag_matrix = std::array<mag_table, constants::MAX_MAG + 1>;

void determine_block_stats(const uint32_t* A, mag_matrix& stats)
{
    const uint32_t bs = constants::block_size;
    static std::array<uint8_t, bs> block_mags;
    uint8_t max_mag = 0;
    for (size_t i = 0; i < bs; i++) {
        block_mags[i] = ans_magnitude(A[i]);
        max_mag = std::max(max_mag, block_mags[i]);
    }
    for (size_t i = 0; i < bs; i++) {
        stats[max_mag][block_mags[i]]++;
    }
}

void block_mag_stats(const list_data& ld, std::string part)
{
    const uint32_t bs = constants::block_size;

    mag_matrix block_stats;
    for (size_t i = 0; i < block_stats.size(); i++)
        for (size_t j = 0; j < block_stats[i].size(); j++)
            block_stats[i][j] = 0;

    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        const uint32_t* in = ld.list_ptrs[i];
        for (size_t j = 0; j < list_size; j += bs) {
            auto ptr = in + j;
            determine_block_stats(ptr, block_stats);
        }
    }

    for (size_t i = 0; i < block_stats.size(); i++) {
        uint64_t total = 0;
        for (size_t j = 0; j < block_stats[i].size(); j++)
            total += block_stats[i][j];
        for (size_t j = 0; j < block_stats[i].size(); j++) {
            std::cout << part << ";" << i << ";" << j << ";" << total << ";"
                      << block_stats[i][j] << std::endl;
        }
    }
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_prefix = cmdargs["input-prefix"].as<std::string>();

    auto inputs = read_all_input_ds2i(input_prefix, true);

    std::cout << "part;block_max;mag;total;count\n";

    block_mag_stats(inputs.docids, "docids");

    block_mag_stats(inputs.freqs, "freqs");

    return EXIT_SUCCESS;
}
