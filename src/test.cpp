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

TEST_CASE("magnitude", "[ans-util]")
{
    SECTION("special cases")
    {
        REQUIRE(ans_magnitude(1) == 0);
        REQUIRE(ans_magnitude(2) == 1);
        REQUIRE(ans_magnitude(3) == 2);
        REQUIRE(ans_magnitude(4) == 2);
    }
    SECTION("exact powers of 2")
    {
        for (uint8_t i = 1; i < 31; i++) {
            REQUIRE(ans_magnitude(1ULL << i) == i);
        }
    }
    SECTION("exact powers of 2 + 1")
    {
        for (uint8_t i = 1; i < 31; i++) {
            REQUIRE(ans_magnitude((1ULL << i) + 1) == i + 1);
        }
    }
    SECTION("everything small")
    {
        uint32_t max = std::numeric_limits<uint32_t>::max();
        uint32_t max_small = 32768;
        for (uint32_t i = 2; i < max_small; i++) {
            uint8_t correct_mag = ceil(log2(i));
            REQUIRE(ans_magnitude(i) == correct_mag);
        }
    }
    SECTION("random stuff")
    {
        for (size_t i = 0; i < 100000; i++) {
            uint32_t num = rand();
            uint8_t correct_mag = ceil(log2(num));
            REQUIRE(ans_magnitude(num) == correct_mag);
        }
    }
}

TEST_CASE("max_val_in_mag", "[ans-util]")
{
    REQUIRE(ans_max_val_in_mag(0) == 1);
    for (uint8_t mag = 1; mag <= 31; mag++) {
        REQUIRE(ans_max_val_in_mag(mag) == 1U << mag);
    }
}

TEST_CASE("min_val_in_mag", "[ans-util]")
{
    REQUIRE(ans_min_val_in_mag(0) == 1);
    for (uint8_t mag = 2; mag <= 31; mag++) {
        REQUIRE(ans_min_val_in_mag(mag) == ((1U << (mag - 1)) + 1));
    }
}

TEST_CASE("ans_uniq_vals_in_mag", "[ans-util]")
{
    REQUIRE(ans_uniq_vals_in_mag(0) == 1);
    REQUIRE(ans_uniq_vals_in_mag(1) == 1);
    REQUIRE(ans_uniq_vals_in_mag(2) == 2);
    REQUIRE(ans_uniq_vals_in_mag(3) == 4);
    REQUIRE(ans_uniq_vals_in_mag(4) == 8);
    REQUIRE(ans_uniq_vals_in_mag(5) == 16);
    REQUIRE(ans_uniq_vals_in_mag(6) == 32);
    REQUIRE(ans_uniq_vals_in_mag(7) == 64);
    REQUIRE(ans_uniq_vals_in_mag(8) == 128);
    REQUIRE(ans_uniq_vals_in_mag(9) == 256);
    REQUIRE(ans_uniq_vals_in_mag(10) == 512);
    REQUIRE(ans_uniq_vals_in_mag(11) == 1024);
    REQUIRE(ans_uniq_vals_in_mag(12) == 2 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(13) == 4 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(14) == 8 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(15) == 16 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(16) == 32 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(17) == 64 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(18) == 128 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(19) == 256 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(20) == 512 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(21) == 1024 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(22) == 2 * 1024 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(23) == 4 * 1024 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(24) == 8 * 1024 * 1024);
    REQUIRE(ans_uniq_vals_in_mag(25) == 16 * 1024 * 1024);
}

TEST_CASE("next_power_of_two", "[ans-util]")
{
    REQUIRE(next_power_of_two(0) == 1);
    REQUIRE(next_power_of_two(1) == 2);
    REQUIRE(next_power_of_two(2) == 4);
    REQUIRE(next_power_of_two(3) == 4);
    REQUIRE(next_power_of_two(4) == 8);
    REQUIRE(next_power_of_two(5) == 8);
    REQUIRE(next_power_of_two(15) == 16);
    REQUIRE(next_power_of_two(19) == 32);
}

template <typename t_compressor>
void model_encode_and_decode(std::vector<uint32_t>& input)
{
    t_compressor comp;
    if (comp.required_increasing) {
        std::partial_sum(input.begin(), input.end(), input.begin());
        std::sort(input.begin(), input.end()); // ugly way to handle overflows
        auto new_end = std::unique(input.begin(), input.end());
        input.resize(std::distance(input.begin(), new_end));
    }

    auto out = reinterpret_cast<uint32_t*>(
        aligned_alloc(16, input.size() * 3 * sizeof(uint32_t)));
    auto ld = list_data(input);

    // store model to file
    size_t u32_written = input.size() * 2;
    comp.init(ld, nullptr, u32_written);

    // compress
    const uint32_t* in = input.data();
    u32_written = input.size() * 2;
    comp.encodeArray(in, input.size(), out, u32_written);
    REQUIRE(u32_written < input.size() * 2);

    // decompress
    std::vector<uint32_t> decompressed_data(input.size() + 1024);
    uint32_t* recovered = decompressed_data.data();
    auto new_out = comp.decodeArray(out, u32_written, recovered, input.size());
    size_t u32_processed = new_out - out;
    decompressed_data.resize(input.size());
    REQUIRE(decompressed_data == input);
    REQUIRE(u32_written == u32_processed);
    aligned_free(out);
}

template <typename t_compressor> void test_method_model_allone()
{
    SECTION("all ones")
    {
        SECTION("1000 elements")
        {
            std::vector<uint32_t> data(1000, 1);
            REQUIRE(data.size() == 1000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            std::vector<uint32_t> data(10000, 1);
            REQUIRE(data.size() == 10000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            std::vector<uint32_t> data(100000, 1);
            REQUIRE(data.size() == 100000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            std::vector<uint32_t> data(1000000, 1);
            REQUIRE(data.size() == 1000000);
            model_encode_and_decode<t_compressor>(data);
        }
    }
}

template <typename t_compressor> void test_method_model_randsmall()
{
    SECTION("uniform random small values")
    {
        std::uniform_int_distribution<uint32_t> d(1, 255);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            model_encode_and_decode<t_compressor>(data);
        }
    }
}

template <typename t_compressor> void test_method_model_randlarge()
{
    SECTION("uniform random large values")
    {
        std::uniform_int_distribution<uint32_t> d(1, 1 << 23);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            model_encode_and_decode<t_compressor>(data);
        }
    }
}

template <typename t_compressor> void test_method_model_geom()
{
    SECTION("geometric 0.001")
    {
        std::geometric_distribution<> d(0.1);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            model_encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("geometric 0.01")
    {
        std::geometric_distribution<> d(0.1);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            model_encode_and_decode<t_compressor>(data);
        }
    }
    SECTION("geometric 0.1")
    {
        std::geometric_distribution<> d(0.1);
        SECTION("1000 elements")
        {
            auto data = generate_random_data(d, 1000);
            REQUIRE(data.size() == 1000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("10000 elements")
        {
            auto data = generate_random_data(d, 10000);
            REQUIRE(data.size() == 10000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("100000 elements")
        {
            auto data = generate_random_data(d, 100000);
            REQUIRE(data.size() == 100000);
            model_encode_and_decode<t_compressor>(data);
        }
        SECTION("1000000 elements")
        {
            auto data = generate_random_data(d, 1000000);
            REQUIRE(data.size() == 1000000);
            model_encode_and_decode<t_compressor>(data);
        }
    }
}

TEST_CASE("encode_and_decode geom B=128", "[ans-packed]")
{
    test_method_model_geom<ans_packed<128> >();
}

TEST_CASE("encode_and_decode allone B=128", "[ans-packed]")
{
    test_method_model_allone<ans_packed<128> >();
}

TEST_CASE("encode_and_decode randsmall B=128", "[ans-packed]")
{
    test_method_model_randsmall<ans_packed<128> >();
}

TEST_CASE("encode_and_decode geom B=256", "[ans-packed]")
{
    test_method_model_geom<ans_packed<256> >();
}

TEST_CASE("encode_and_decode allone B=256", "[ans-packed]")
{
    test_method_model_allone<ans_packed<256> >();
}

TEST_CASE("encode_and_decode randsmall B=256", "[ans-packed]")
{
    test_method_model_randsmall<ans_packed<256> >();
}

TEST_CASE("encode_and_decode randlarge B=128", "[ans-packed]")
{
    test_method_model_randlarge<ans_packed<128> >();
}
