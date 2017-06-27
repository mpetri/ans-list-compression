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
        ("col-name,c",po::value<std::string>()->required(), "name of the collection")
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

template <class T>
void print_block_info(
    const uint32_t* A, size_t n, const T& mags, size_t listnr, size_t offset)
{
    printf("L[%lu/%lu] = (", listnr, offset);
    for (size_t i = 0; i < n; i++) {
        printf("<%u,%d>", A[i], (int)mags[i]);
    }
    printf(")\n");
}

using mag_table = std::vector<uint64_t>;
using mag_matrix = std::vector<mag_table>;

struct block_stats {
    bool skipfirst;
    mag_matrix mm;
    mag_table mag_stats;
    std::string part;
    block_stats()
    {
        mag_stats.resize(constants::MAX_MAG + 1);
        mm.resize(constants::MAX_MAG + 1);
        for (size_t i = 0; i < mm.size(); i++) {
            mm[i].resize(constants::MAX_MAG + 1);
            mag_stats[i] = 0;
            for (size_t j = 0; j < mm[i].size(); j++)
                mm[i][j] = 0;
        }
    }
};

//  ofs << "part;block_max;mag;total;count;cumsum;skipfirst;numblocks\n";

std::ostream& operator<<(std::ostream& os, const block_stats& bs)
{
    for (size_t bmax = 0; bmax < bs.mm.size(); bmax++) {
        uint64_t block_with_mag = bs.mag_stats[bmax];
        uint64_t total = 0;
        for (size_t j = 0; j < bs.mm[bmax].size(); j++)
            total += bs.mm[bmax][j];
        uint64_t cumsum = 0;
        for (size_t mag = 0; mag < bs.mm[bmax].size(); mag++) {
            cumsum += bs.mm[bmax][mag];
            os << bs.part << ";" << bmax << ";" << mag << ";" << total << ";"
               << bs.mm[bmax][mag] << ";" << cumsum << ";" << bs.skipfirst
               << ";" << block_with_mag << std::endl;
        }
    }
    return os;
}

void determine_block_stats(const uint32_t* A, uint32_t n, block_stats& stats)
{
    const uint32_t bs = constants::block_size;
    static std::array<uint8_t, bs> block_mags;
    uint8_t max_mag = 0;
    for (size_t i = 0; i < n; i++) {
        block_mags[i] = ans_magnitude(A[i]);
        max_mag = std::max(max_mag, block_mags[i]);
    }
    for (size_t i = 0; i < n; i++) {
        stats.mm[max_mag][block_mags[i]]++;
    }
    stats.mag_stats[max_mag]++;
}

block_stats block_mag_stats(
    const list_data& ld, std::string part, bool skipfirst)
{
    const uint32_t bs = constants::block_size;

    block_stats bstats;
    bstats.part = part;
    bstats.skipfirst = skipfirst;

    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        const uint32_t* in = ld.list_ptrs[i];
        for (size_t j = 0; j < list_size; j += bs) {
            auto ptr = in + j;
            if (skipfirst && j == 0)
                determine_block_stats(ptr + 1, bs - 1, bstats);
            else
                determine_block_stats(ptr, bs, bstats);
        }
    }
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_prefix = cmdargs["input-prefix"].as<std::string>();
    auto col_name = cmdargs["col-name"].as<std::string>();

    auto inputs = read_all_input_ds2i(input_prefix, true);

    std::ofstream ofs(col_name + "_block_mag_stats.csv");

    ofs << "part;block_max;mag;total;count;cumsum;skipfirst;numblocks\n";

    auto bs_docs = block_mag_stats(inputs.docids, "docids", false);
    ofs << bs_docs;

    bs_docs = block_mag_stats(inputs.docids, "docids", true);
    ofs << bs_docs;

    auto bs_freqs = block_mag_stats(inputs.freqs, "freqs", false);
    ofs << bs_freqs;

    bs_freqs = block_mag_stats(inputs.freqs, "freqs", true);
    ofs << bs_freqs;

    return EXIT_SUCCESS;
}
