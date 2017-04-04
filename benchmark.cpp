#include <iostream>
#include <vector>

#include "cutil.hpp"
#include "methods.hpp"
#include "util.hpp"

using encoding_stats = std::pair<std::chrono::nanoseconds, uint64_t>;

template <class t_compressor>
encoding_stats compress_lists(const list_data& ld, std::string output_prefix)
{
    t_compressor comp;
    std::string output_method_prefix = output_prefix + "." + comp.name();
    std::string output_data_filename = output_method_prefix + ".bin";
    std::string metadata_filename = output_method_prefix + ".metadata";
    std::cerr << "output_filename = " << output_data_filename << std::endl;
    std::cerr << "metadata_filename = " << metadata_filename << std::endl;
    std::chrono::nanoseconds encoding_time_ns;

    // make a local copy of the input data as we might have prefix sum
    list_data local_data = ld;
    if (comp.required_increasing) {
        prefix_sum_lists(local_data);
    }

    {
        auto out_file = fopen_or_fail(output_data_filename, "wb");
        // allocate a lage output buffer
        std::vector<uint32_t> out_buf(local_data.num_postings * 1.5);
        std::vector<uint64_t> list_starts(local_data.num_lists + 1);

        uint32_t* initout = out_buf.data();
        uint64_t total_u32_written = 0;
        {
            timer t("encode lists");
            auto start = std::chrono::high_resolution_clock::now();

            uint32_t* out = initout;

            {
                size_t encoded_u32 = 0;
                comp.init(ld, out, encoded_u32);
                out += encoded_u32;
                total_u32_written += encoded_u32;
            }

            for (size_t i = 0; i < local_data.num_lists; i++) {
                size_t list_size = local_data.list_sizes[i];
                const uint32_t* in = local_data.list_ptrs[i];
                list_starts[i] = (out - initout);
                size_t encoded_u32 = local_data.num_postings;
                comp.encodeArray(in, list_size, out, encoded_u32);
                out += encoded_u32;
                total_u32_written += encoded_u32;
            }
            list_starts[local_data.num_lists] = (out - initout);
            auto stop = std::chrono::high_resolution_clock::now();
            encoding_time_ns = stop - start;
        }
        write_u32s(out_file, initout, total_u32_written);
        fclose(out_file);

        {
            timer t("write meta data");
            auto meta_file = fopen_or_fail(metadata_filename, "w");
            write_metadata(meta_file, local_data, list_starts);
            fclose(meta_file);
        }
    }
    uint64_t size_bits = 0;
    {
        auto out_file = fopen_or_fail(output_data_filename, "r");
        fseek(out_file, 0L, SEEK_END);
        auto size_bytes = ftell(out_file);
        size_bits = size_bytes * 8;
    }
    return { encoding_time_ns, size_bits };
}

template <class t_compressor>
std::chrono::nanoseconds decompress_and_verify(
    const list_data& original, std::string prefix)
{
    t_compressor comp;
    std::string input_method_prefix = prefix + "." + comp.name();
    std::string input_data_filename = input_method_prefix + ".bin";
    std::string metadata_filename = input_method_prefix + ".metadata";
    std::cerr << "input_filename = " << input_data_filename << std::endl;
    std::cerr << "metadata_filename = " << metadata_filename << std::endl;
    std::chrono::nanoseconds decoding_time_ns;

    // (1) read metadata and allocate buffers
    list_data recovered;
    std::vector<uint64_t> list_starts;
    {
        timer t("read meta data");
        auto meta_file = fopen_or_fail(metadata_filename, "r");
        read_metadata(meta_file, recovered, list_starts);
        fclose(meta_file);
    }

    // (2) decompress
    {
        auto in_file = fopen_or_fail(input_data_filename, "rb");
        auto content = read_file_content_u32(in_file);
        fclose(in_file);
        const uint32_t* in = content.data();
        {
            timer t("decode lists");

            auto start = std::chrono::high_resolution_clock::now();
            {
                comp.dec_init(in);
            }

            for (size_t i = 0; i < recovered.num_lists; i++) {
                auto input_ptr = in + list_starts[i];
                size_t encoding_size_u32 = list_starts[i + 1] - list_starts[i];
                comp.decodeArray(input_ptr, encoding_size_u32,
                    recovered.list_ptrs[i], recovered.list_sizes[i]);
            }

            auto stop = std::chrono::high_resolution_clock::now();
            decoding_time_ns = stop - start;
        }
    }

    if (comp.required_increasing) {
        undo_prefix_sum_lists(recovered);
    }

    // (3) verify
    {
        timer t("verify encode/decode");
        REQUIRE_EQUAL(original.num_lists, recovered.num_lists, "num_lists");
        REQUIRE_EQUAL(
            original.num_postings, recovered.num_postings, "num_postings");
        for (size_t i = 0; i < original.num_lists; i++) {
            REQUIRE_EQUAL(
                original.list_sizes[i], recovered.list_sizes[i], "list_size");
            REQUIRE_EQUAL(original.list_ptrs[i], recovered.list_ptrs[i],
                recovered.list_sizes[i],
                "list_contents[" + std::to_string(i) + "]");
        }
    }
    return decoding_time_ns;
}

template <class t_compressor>
void run(const list_data& inputs, std::string output_prefix)
{
    auto estats = compress_lists<t_compressor>(inputs, output_prefix);
    auto dtime_ns = decompress_and_verify<t_compressor>(inputs, output_prefix);

    {
        t_compressor c;
        fprintff(stderr, "%s;%lu;%lu;%lu;%lu;%lu\n", c.name().c_str(),
            inputs.num_postings, inputs.num_lists, estats.second,
            estats.first.count(), dtime_ns.count());
    }
}

int main(int argc, char const* argv[])
{
    if (argc < 2) {
        fprintff(stderr, "%s <output_path> < input_file\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::string output_prefix = argv[1];
    uint32_t max_lists = 0;
    if (argc >= 3) {
        max_lists = std::atoi(argv[2]);
    }
    uint32_t skip = 0;
    if (argc >= 4) {
        skip = std::atoi(argv[3]);
    }

    auto inputs = read_all_input_from_stdin(max_lists, skip);

    fprintff(stderr,
        "method;postings;lists;size_bits;encoding_time_ns;decoding_time_ns\n");

    run<ans_vbyte_split<4096> >(inputs, output_prefix);
    run<ans_vbyte_single<4096> >(inputs, output_prefix);
    // run<ans_packed<128> >(inputs, output_prefix);
    // run<ans_packed<256> >(inputs, output_prefix);
    // run<qmx>(inputs, output_prefix);
    // run<vbyte>(inputs, output_prefix);
    // run<op4<128> >(inputs, output_prefix);
    // run<simple16>(inputs, output_prefix);
    // run<interpolative>(inputs, output_prefix);

    return EXIT_SUCCESS;
}
