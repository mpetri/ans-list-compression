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

void compute_overhead(const list_data& ld, std::string p)
{
    const uint64_t block_size = constants::block_size;

    std::vector<std::vector<uint32_t> > removed_blocks;
    std::vector<std::vector<uint32_t> > removed_blocks_prefixsum;

    // (1) determine new number of lists
    size_t lists_discarded = 0;
    size_t new_num_lists = 0;
    for (size_t i = 0; i < ld.num_lists; i++) {
        size_t list_size = ld.list_sizes[i];
        const uint32_t* in = ld.list_ptrs[i];
        if (list_size < block_size) {
            lists_discarded++;
        } else {
            new_num_lists++;
        }
    }

    // (2) do the filtering
    size_t p_discarded = 0;
    size_t new_num_postings = 0;
    for (size_t i = 0; i < ld.num_lists; i++) {
        auto cur_size = ld.list_sizes[i];
        // (1) skip small lists completely
        if (cur_size < block_size) {
            std::vector<uint32_t> lst;
            for (size_t j = 0; j < cur_size; j++) {
                lst.push_back(ld.list_ptrs[i][j]);
            }
            removed_blocks.push_back(lst);
            std::partial_sum(lst.begin(), lst.end(), lst.begin());
            removed_blocks_prefixsum.push_back(lst);
            p_discarded += cur_size;
            continue;
        }
        // (2) larger lists are multiples of block_size. we cut of the rest
        size_t num_blocks = cur_size / block_size;
        size_t left = cur_size % block_size;
        p_discarded += left;
        const auto& list = ld.list_ptrs[i];
        size_t new_list_len = num_blocks * block_size;
        new_num_postings += new_list_len;
        if (left) {
            std::vector<uint32_t> lst;
            for (size_t j = 0; j < left; j++)
                lst.push_back(ld.list_ptrs[i][new_list_len + j]);
            removed_blocks.push_back(lst);
            std::partial_sum(lst.begin(), lst.end(), lst.begin());
            removed_blocks_prefixsum.push_back(lst);
        }
    }

    // (3) compute compression ratios for interp and vbyte of removed blocks
    std::vector<uint32_t> tmp_buf(block_size * 10);
    uint32_t* tmp = tmp_buf.data();
    size_t vbyte_u32s = 0;
    size_t interp_u32s = 0;

    interpolative interp_coder;
    vbyte vbyte_coder;

    for (size_t i = 0; i < removed_blocks.size(); i++) {
        const auto& lst = removed_blocks[i];
        const auto& lstps = removed_blocks_prefixsum[i];
        auto in = lst.data();
        auto inps = lstps.data();
        size_t list_len = lst.size();

        size_t cur_u32s = tmp_buf.size();
        interp_coder.encodeArray(inps, list_len, tmp, cur_u32s);
        interp_u32s += cur_u32s;

        cur_u32s = tmp_buf.size();
        vbyte_coder.encodeArray(in, list_len, tmp, cur_u32s);
        vbyte_u32s += cur_u32s;
    }

    // (4) print stats to stderr
    double prcnt_l_disc = double(lists_discarded) / double(ld.num_lists) * 100;
    double prcnt_p_disc = double(p_discarded) / double(ld.num_postings) * 100;

    std::cout << p << " initial num_lists = " << ld.num_lists << std::endl;
    std::cout << p << " new num_lists = " << new_num_lists << std::endl;
    std::cout << p << " initial postings (Po) = " << ld.num_postings
              << std::endl;
    std::cout << p << " new num_postings (Pf) = " << new_num_postings
              << std::endl;
    std::cout << p << " postings removed = " << p_discarded << std::endl;
    std::cout << p << " lists removed = " << lists_discarded << std::endl;
    std::cout << p << " prcnt lists removed = " << prcnt_l_disc << std::endl;
    std::cout << p << " prcnt postings removed = " << prcnt_p_disc << std::endl;

    std::cout << p << " bits vbyte = " << vbyte_u32s * 32 << std::endl;
    std::cout << p << " bits interp = " << interp_u32s * 32 << std::endl;

    double bpi_vbyte = double(vbyte_u32s * 32) / double(new_num_postings);
    double bpi_interp = double(interp_u32s * 32) / double(new_num_postings);

    std::cout << p << " Overhead BPI vbyte (bits_vbyte/Pf) = " << bpi_vbyte
              << std::endl;
    std::cout << p << " Overhead BPI interp  (bits_interp/Pf) = " << bpi_interp
              << std::endl;
}

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto input_prefix = cmdargs["input-prefix"].as<std::string>();

    bool remove_nonfull = false;
    auto inputs = read_all_input_ds2i(input_prefix, remove_nonfull);

    compute_overhead(inputs.docids, "doc_ids");
    compute_overhead(inputs.freqs, "freqs");

    return EXIT_SUCCESS;
}