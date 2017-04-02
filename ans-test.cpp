#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this
// in one cpp file
#include "ans-util.hpp"
#include "catch.hpp"

#include <limits>

// TEST_CASE("magnitude", "[ans-util]")
// {
//     SECTION("special cases")
//     {
//         REQUIRE(ans_magnitude(1) == 0);
//         REQUIRE(ans_magnitude(2) == 1);
//         REQUIRE(ans_magnitude(3) == 2);
//         REQUIRE(ans_magnitude(4) == 2);
//     }
//     SECTION("exact powers of 2")
//     {
//         for (uint8_t i = 1; i < 31; i++) {
//             REQUIRE(ans_magnitude(1ULL << i) == i);
//         }
//     }
//     SECTION("exact powers of 2 + 1")
//     {
//         for (uint8_t i = 1; i < 31; i++) {
//             REQUIRE(ans_magnitude((1ULL << i) + 1) == i + 1);
//         }
//     }
//     SECTION("everything small")
//     {
//         uint32_t max = std::numeric_limits<uint32_t>::max();
//         uint32_t max_small = 32768;
//         for (uint32_t i = 2; i < max_small; i++) {
//             uint8_t correct_mag = ceil(log2(i));
//             REQUIRE(ans_magnitude(i) == correct_mag);
//         }
//     }
//     SECTION("random stuff")
//     {
//         for (size_t i = 0; i < 100000; i++) {
//             uint32_t num = rand();
//             uint8_t correct_mag = ceil(log2(num));
//             REQUIRE(ans_magnitude(num) == correct_mag);
//         }
//     }
// }

// TEST_CASE("max_val_in_mag", "[ans-util]")
// {
//     REQUIRE(ans_max_val_in_mag(0) == 1);
//     for (uint8_t mag = 1; mag <= 31; mag++) {
//         REQUIRE(ans_max_val_in_mag(mag) == 1U << mag);
//     }
// }

// TEST_CASE("min_val_in_mag", "[ans-util]")
// {
//     REQUIRE(ans_min_val_in_mag(0) == 1);
//     for (uint8_t mag = 2; mag <= 31; mag++) {
//         REQUIRE(ans_min_val_in_mag(mag) == ((1U << (mag - 1)) + 1));
//     }
// }

// TEST_CASE("ans_uniq_vals_in_mag", "[ans-util]")
// {
//     REQUIRE(ans_uniq_vals_in_mag(0) == 1);
//     REQUIRE(ans_uniq_vals_in_mag(1) == 1);
//     REQUIRE(ans_uniq_vals_in_mag(2) == 2);
//     REQUIRE(ans_uniq_vals_in_mag(3) == 4);
//     REQUIRE(ans_uniq_vals_in_mag(4) == 8);
//     REQUIRE(ans_uniq_vals_in_mag(5) == 16);
//     REQUIRE(ans_uniq_vals_in_mag(6) == 32);
//     REQUIRE(ans_uniq_vals_in_mag(7) == 64);
//     REQUIRE(ans_uniq_vals_in_mag(8) == 128);
//     REQUIRE(ans_uniq_vals_in_mag(9) == 256);
//     REQUIRE(ans_uniq_vals_in_mag(10) == 512);
//     REQUIRE(ans_uniq_vals_in_mag(11) == 1024);
//     REQUIRE(ans_uniq_vals_in_mag(12) == 2 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(13) == 4 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(14) == 8 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(15) == 16 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(16) == 32 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(17) == 64 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(18) == 128 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(19) == 256 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(20) == 512 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(21) == 1024 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(22) == 2 * 1024 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(23) == 4 * 1024 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(24) == 8 * 1024 * 1024);
//     REQUIRE(ans_uniq_vals_in_mag(25) == 16 * 1024 * 1024);
// }

// TEST_CASE("next_power_of_two", "[ans-util]")
// {
//     REQUIRE(next_power_of_two(0) == 1);
//     REQUIRE(next_power_of_two(1) == 2);
//     REQUIRE(next_power_of_two(2) == 4);
//     REQUIRE(next_power_of_two(3) == 4);
//     REQUIRE(next_power_of_two(4) == 8);
//     REQUIRE(next_power_of_two(5) == 8);
//     REQUIRE(next_power_of_two(15) == 16);
//     REQUIRE(next_power_of_two(19) == 32);
// }

// TEST_CASE("normalize_power_of_two", "[ans-util]")
// {
//     SECTION("random stuff")
//     {
//         std::vector<uint32_t> A{ 42, 23, 10, 3, 1 };
//         auto B = normalize_power_of_two(A);
//         uint64_t cur_cnt = 0;
//         for (size_t i = 0; i < B.size(); i++) {
//             cur_cnt += B[i] * ans_uniq_vals_in_mag(i);
//         }
//         REQUIRE(is_power_of_two(cur_cnt));
//     }
//     {
//         std::vector<uint32_t> A{ 5121, 23, 10, 3, 1 };
//         auto B = normalize_power_of_two(A);
//         uint64_t cur_cnt = 0;
//         for (size_t i = 0; i < B.size(); i++) {
//             cur_cnt += B[i] * ans_uniq_vals_in_mag(i);
//         }
//         REQUIRE(is_power_of_two(cur_cnt));
//     }
// }

TEST_CASE("write_unary", "[ans-util]")
{
    uint8_t buf[256] = { 0 };
    uint8_t in_byte_offset = 0;
    uint8_t* ptr = buf;
    write_unary(ptr, in_byte_offset, 1);
    REQUIRE((int)buf[0] == 2);
    REQUIRE(in_byte_offset == 2);
    write_unary(ptr, in_byte_offset, 3);
    REQUIRE((int)buf[0] == 34);
    REQUIRE(in_byte_offset == 6);
    write_unary(ptr, in_byte_offset, 5);
    REQUIRE((int)buf[0] == 34);
    REQUIRE((int)buf[1] == 8);
    REQUIRE(in_byte_offset == 4);
    write_unary(ptr, in_byte_offset, 6);
    REQUIRE((int)buf[0] == 34);
    REQUIRE((int)buf[1] == 8);
    REQUIRE((int)buf[2] == 4);
    REQUIRE(in_byte_offset == 3);
    write_unary(ptr, in_byte_offset, 1);
    REQUIRE((int)buf[0] == 34);
    REQUIRE((int)buf[1] == 8);
    REQUIRE((int)buf[2] == 20);
    REQUIRE(in_byte_offset == 5);
}

TEST_CASE("read_unary", "[ans-util]")
{
    uint8_t buf[256] = { 34, 8, 20 };
    uint8_t in_byte_offset = 0;
    const uint8_t* ptr = buf;
    uint64_t x = read_unary(ptr, in_byte_offset);
    REQUIRE(x == 1);
    REQUIRE(in_byte_offset == 2);
    x = read_unary(ptr, in_byte_offset);
    REQUIRE(x == 3);
    REQUIRE(in_byte_offset == 6);
    x = read_unary(ptr, in_byte_offset);
    REQUIRE(x == 5);
    REQUIRE(in_byte_offset == 4);
    x = read_unary(ptr, in_byte_offset);
    REQUIRE(x == 6);
    REQUIRE(in_byte_offset == 3);
    x = read_unary(ptr, in_byte_offset);
    REQUIRE(x == 1);
    REQUIRE(in_byte_offset == 5);
}

TEST_CASE("write_int8", "[ans-util]")
{
    uint8_t buf[256] = { 0 };
    uint8_t in_byte_offset = 0;
    uint8_t* ptr = buf;
    write_int8(ptr, in_byte_offset, 1, 1);
    REQUIRE((int)buf[0] == 1);
    REQUIRE(in_byte_offset == 1);

    write_int8(ptr, in_byte_offset, 3, 2);
    REQUIRE((int)buf[0] == 7);
    REQUIRE(in_byte_offset == 3);

    write_int8(ptr, in_byte_offset, 51232, 16);
    REQUIRE((int)buf[0] == 7);
    REQUIRE((int)buf[1] == 65);
    REQUIRE((int)buf[2] == 6);
    REQUIRE(in_byte_offset == 3);
    write_int8(ptr, in_byte_offset, 0x008888888800, 56);
    REQUIRE((int)buf[0] == 7);
    REQUIRE((int)buf[1] == 65);
    REQUIRE((int)buf[2] == 6);
    REQUIRE((int)buf[3] == 64);
    REQUIRE((int)buf[4] == 68);
    REQUIRE((int)buf[5] == 68);
    REQUIRE((int)buf[6] == 68);
    REQUIRE((int)buf[7] == 4);
    REQUIRE((int)buf[8] == 0);
    REQUIRE(in_byte_offset == 3);
}

TEST_CASE("read_int8", "[ans-util]")
{
    uint8_t buf[256] = { 7, 65, 6, 64, 68, 68, 68, 4, 0 };
    uint8_t in_byte_offset = 0;
    const uint8_t* ptr = buf;
    uint64_t x = read_int8(ptr, in_byte_offset, 1);
    REQUIRE(x == 1);
    REQUIRE(in_byte_offset == 1);
    x = read_int8(ptr, in_byte_offset, 2);
    REQUIRE(x == 3);
    REQUIRE(in_byte_offset == 3);
    x = read_int8(ptr, in_byte_offset, 16);
    REQUIRE(x == 51232);
    REQUIRE(in_byte_offset == 3);
    x = read_int8(ptr, in_byte_offset, 56);
    REQUIRE(x == 0x008888888800);
    REQUIRE(in_byte_offset == 3);
}

TEST_CASE("readwrite_int8", "[ans-util]")
{
    std::mt19937 gen(42);
    std::uniform_int_distribution<uint64_t> dis(
        0, std::numeric_limits<uint64_t>::max());
    uint8_t in_byte_offset = 0;
    std::vector<uint8_t> buf(10000000);
    uint8_t* ptr = buf.data();
    std::vector<uint64_t> nums(10000);
    for (size_t i = 0; i < 10000; i++) {
        uint64_t num = dis(gen);
        uint64_t len = log2(num) + 1;
        nums.push_back(num);
        write_int8(ptr, in_byte_offset, num, len);
    }
    in_byte_offset = 0;
    const uint8_t* rptr = buf.data();
    for (size_t i = 0; i < 10000; i++) {
        uint64_t num = nums[i];
        uint64_t len = log2(num) + 1;
        uint64_t x = read_int8(rptr, in_byte_offset, len);
        REQUIRE(x == num);
    }
}

TEST_CASE("elias_delta", "[ans-util]")
{
    // SECTION("custom stuff")
    // {
    //     uint8_t buf[256] = { 0 };
    //     uint8_t* ptr = buf;
    //     ans_eliasdelta_encode_u64(ptr, 1);
    //     ans_eliasdelta_encode_u64(ptr, 2);
    //     ans_eliasdelta_encode_u64(ptr, 16);
    //     ans_eliasdelta_encode_u64(ptr, 51232);
    //     ans_eliasdelta_encode_u64(ptr, 0x008888888800);
    //     const uint8_t* dptr = buf;
    //     uint64_t x = ans_eliasdelta_decode_u64(dptr);
    //     REQUIRE(x == 1);
    //     x = ans_eliasdelta_decode_u64(dptr);
    //     REQUIRE(x == 2);
    //     x = ans_eliasdelta_decode_u64(dptr);
    //     REQUIRE(x == 16);
    //     x = ans_eliasdelta_decode_u64(dptr);
    //     REQUIRE(x == 51232);
    //     x = ans_eliasdelta_decode_u64(dptr);
    //     REQUIRE(x == 0x008888888800);
    // }
    SECTION("custom stuff")
    {
        uint8_t buf[256] = { 0 };
        uint64_t num = 17537583593393853710ULL;
        uint8_t* ptr = buf;
        ans_eliasdelta_encode_u64(ptr, num);
        const uint8_t* dptr = buf;
        uint64_t x = ans_eliasdelta_decode_u64(dptr);
        REQUIRE(x == num);
    }
    SECTION("random stuff")
    {
        std::mt19937 gen(42);
        std::uniform_int_distribution<uint64_t> dis(
            0, std::numeric_limits<uint64_t>::max());

        for (size_t i = 0; i < 10000; i++) {
            uint8_t buf[256] = { 0 };
            uint8_t* ptr = buf;
            uint64_t num = dis(gen);
            ans_eliasdelta_encode_u64(ptr, num);
            const uint8_t* dptr = buf;
            uint64_t x = ans_eliasdelta_decode_u64(dptr);
            REQUIRE(x == num);
        }
    }
}
