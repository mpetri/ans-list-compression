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
        ("remove-nonfull,r","remove nonfull blocks B=128")
        ("col-name,c",po::value<std::string>()->required(), "alias for the collection")
        ("input-prefix,i",po::value<std::string>()->required(), "prefix for the input files (d2si)")
        ("output-path,o",po::value<std::string>()->required(), "output path to store indexes");
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

int main(int argc, char const* argv[])
{
    auto cmdargs = parse_cmdargs(argc, argv);
    auto col_name = cmdargs["col-name"].as<std::string>();
    auto input_prefix = cmdargs["input-prefix"].as<std::string>();
    auto out_prefix = cmdargs["output-path"].as<std::string>();

    bool remove_nonfull = false;
    if (cmdargs.count("remove-nonfull")) {
        remove_nonfull = true;
    }

    auto inputs = read_all_input_ds2i(input_prefix, remove_nonfull);

    fprintff(stderr,
        "col;part;method;postings;lists;size_bits;encoding_time_ns;"
        "decoding_time_ns\n");

    // run<qmx>(inputs.docids, out_prefix, col_name, "docids");
    // run<qmx>(inputs.freqs, out_prefix, col_name, "freqs");
    // run<vbyte>(inputs.docids, out_prefix, col_name, "docids");
    // run<vbyte>(inputs.freqs, out_prefix, col_name, "freqs");
    // run<op4<128> >(inputs.docids, out_prefix, col_name, "docids");
    // run<op4<128> >(inputs.freqs, out_prefix, col_name, "freqs");
    // run<simple16>(inputs.docids, out_prefix, col_name, "docids");
    // run<simple16>(inputs.freqs, out_prefix, col_name, "freqs");
    // run<interpolative>(inputs.docids, out_prefix, col_name, "docids");
    // run<interpolative>(inputs.freqs, out_prefix, col_name, "freqs");

    // run<ans_simple>(inputs.docids, out_prefix, col_name, "docids");
    // run<ans_simple>(inputs.freqs, out_prefix, col_name, "freqs");
    run<ans_packed<128> >(inputs.docids, out_prefix, col_name, "docids");
    run<ans_packed<128> >(inputs.freqs, out_prefix, col_name, "freqs");
    // run<ans_packed<256> >(inputs.docids, out_prefix, col_name, "docids");
    // run<ans_packed<256> >(inputs.freqs, out_prefix, col_name, "freqs");
    // run<ans_vbyte_split<4096> >(inputs.docids, out_prefix, col_name,
    // "docids");
    // run<ans_vbyte_split<4096> >(inputs.freqs, out_prefix, col_name, "freqs");
    // run<ans_vbyte_single<4096> >(inputs.docids, out_prefix, col_name,
    // "docids");
    // run<ans_vbyte_single<4096> >(inputs.freqs, out_prefix, col_name,
    // "freqs");

    return EXIT_SUCCESS;
}
