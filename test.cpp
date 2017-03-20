#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this
// in one cpp file
#include "catch.hpp"

#include "methods.hpp"

#include <random>

template <class t_dist>
std::vector<uint32_t> generate_random_data(t_dist& d, size_t num_elems)
{
    std::mt19937 gen(42);
    std::vector<uint32_t> data(num_elems);
    for (size_t i = 0; i < num_elems; i++) {
        data[i] = d(gen) + 1; // we don't compress 0
    }
    return data;
}

template <typename t_compressor>
void encode_and_decode(std::vector<uint32_t>& input)
{
    t_compressor comp;
    if (comp.required_increasing) {
        std::partial_sum(input.begin(), input.end(), input.begin());
        std::sort(input.begin(), input.end()); // ugly way to handle overflows
        auto new_end = std::unique(input.begin(), input.end());
        input.resize(std::distance(input.begin(), new_end));
    }

    auto out = reinterpret_cast<uint32_t*>(
        aligned_alloc(16, input.size() * 2 * sizeof(uint32_t)));
    const uint32_t* in = input.data();

    // compress
    size_t u32_written = input.size() * 2;

    comp.encodeArray(in, input.size(), out, u32_written);
    REQUIRE(u32_written < input.size() * 2);

    // decompress
    std::vector<uint32_t> decompressed_data(input.size() + 1024);
    uint32_t* recovered = decompressed_data.data();
    auto new_out = comp.decodeArray(out, u32_written, recovered, input.size());
    size_t u32_processed = new_out - out;
    decompressed_data.resize(input.size());
    REQUIRE(u32_written == u32_processed);
    REQUIRE(decompressed_data == input);
    aligned_free(out);
}

template <typename t_compressor> void test_method()
{
    SECTION("geometric 0.001")
    {
        std::geometric_distribution<> d(0.1);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("geometric 0.01")
    {
        std::geometric_distribution<> d(0.1);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("geometric 0.1")
    {
        std::geometric_distribution<> d(0.1);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("all ones")
    {
        SECTION("1000 elements")
        {
            std::vector<uint32_t> data(1000, 1);
            REQUIRE(data.size() == 1000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            std::vector<uint32_t> data(10000, 1);
            REQUIRE(data.size() == 10000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            std::vector<uint32_t> data(100000, 1);
            REQUIRE(data.size() == 100000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            std::vector<uint32_t> data(1000000, 1);
            REQUIRE(data.size() == 1000000);
            encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("uniform random small values")
    {
        std::uniform_int_distribution<uint32_t> d(1, 255);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("uniform random large values")
    {
        std::uniform_int_distribution<uint32_t> d(1, 1 << 27);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            encode_and_decode<t_compressor>(data);
        }
    }
}

TEST_CASE("interpolative coding and decoding", "[interp]")
{
    test_method<interpolative>();
}

TEST_CASE("vbyte coding and decoding", "[vbyte]") { test_method<vbyte>(); }

TEST_CASE("OptPForDelta coding and decoding", "[op4]")
{
    test_method<op4<128> >();
}

TEST_CASE("Simple16 coding and decoding", "[simple16]")
{
    test_method<simple16>();
}

TEST_CASE("QMX coding and decoding", "[qmx]") { test_method<qmx>(); }