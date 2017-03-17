#pragma once

namespace bits {
constexpr static uint32_t all_set{ 0xFFFFFFFFULL };

static const uint32_t lo_set[33] = { 0x00000000ULL, 0x00000001ULL,
    0x00000003ULL, 0x00000007ULL, 0x0000000FULL, 0x0000001FULL, 0x0000003FULL,
    0x0000007FULL, 0x000000FFULL, 0x000001FFULL, 0x000003FFULL, 0x000007FFULL,
    0x00000FFFULL, 0x00001FFFULL, 0x00003FFFULL, 0x00007FFFULL, 0x0000FFFFULL,
    0x0001FFFFULL, 0x0003FFFFULL, 0x0007FFFFULL, 0x000FFFFFULL, 0x001FFFFFULL,
    0x003FFFFFULL, 0x007FFFFFULL, 0x00FFFFFFULL, 0x01FFFFFFULL, 0x03FFFFFFULL,
    0x07FFFFFFULL, 0x0FFFFFFFULL, 0x1FFFFFFFULL, 0x3FFFFFFFULL, 0x7FFFFFFFULL,
    0xFFFFFFFFULL };

inline uint32_t hi(uint64_t x)
{
    if (x == 0)
        return 0;
    uint32_t res = 63 - __builtin_clzll(x);
    return res;
}

inline uint32_t read_int(uint32_t*& word, uint8_t& offset, const uint8_t len)
{
    uint32_t w1 = (*word) >> offset;
    if ((offset = (offset + len)) >= 32) { // if offset+len > 32
        if (offset == 32) {
            offset &= 31;
            ++word;
            return w1;
        } else {
            offset &= 31;
            return w1 | (((*(++word)) & lo_set[offset]) << (len - offset));
        }
    } else {
        return w1 & lo_set[len];
    }
}

inline void write_int(
    uint32_t*& word, uint32_t x, uint8_t& offset, const uint8_t len)
{
    x &= lo_set[len & 63];
    if (offset + len < 32) {
        *word &= ((all_set << (offset + len))
            | lo_set[offset]); // mask 1..10..01..1
        *word |= (x << offset);
        offset += len;
    } else {
        *word &= ((lo_set[offset])); // mask 0....01..1
        *word |= (x << offset);
        if ((offset = (offset + len)) > 32) { // offset+len >= 32
            offset &= 31;
            *(++word) &= (~lo_set[offset]); // mask 1...10..0
            *word |= (x >> (len - offset));
        } else {
            offset = 0;
            ++word;
        }
    }
}
}

struct bit_stream {
    uint32_t* of;
    uint32_t buf[3];
    uint32_t* first_ptr;
    uint32_t* cur_ptr;
    uint32_t* last_ptr;
    uint8_t in_word_offset;
    uint64_t bytes_written;
    uint64_t bytes_consumed;
    bool write_mode;
    bit_stream(uint32_t* f, bool write_stuff)
        : of(f)
        , write_mode(write_stuff)
    {
        first_ptr = &buf[0];
        cur_ptr = first_ptr;
        last_ptr = &buf[1];
        in_word_offset = 0;
        bytes_written = 0;
        bytes_consumed = 0;
        if (!write_mode) { // fill the buffer with the first word
            *first_ptr = *of++;
            bytes_consumed += sizeof(uint32_t);
        }
    };
    ~bit_stream() { flush(); }
    void put_int(uint32_t val, size_t bits)
    {
        if (bits == 0)
            return;
        bits::write_int(cur_ptr, val, in_word_offset, bits);
        if (cur_ptr == last_ptr) {
            // buffer full. write to file
            *of++ = *first_ptr;
            bytes_written += sizeof(uint32_t);
            cur_ptr = first_ptr;
            *first_ptr = *last_ptr;
        }
    }
    uint64_t get_int(size_t bits)
    {
        if (bits == 0)
            return 0;
        if (cur_ptr == last_ptr + 1) {
            cur_ptr = first_ptr;
            *first_ptr = *of++;
            bytes_consumed += sizeof(uint32_t);
        }
        if (cur_ptr == last_ptr || (size_t(in_word_offset) + bits > 32)) {
            *last_ptr = *of++;
            bytes_consumed += sizeof(uint32_t);
        }
        uint32_t x = bits::read_int(cur_ptr, in_word_offset, bits);
        if (cur_ptr == last_ptr && in_word_offset != 0) {
            cur_ptr = first_ptr;
            *first_ptr = *last_ptr;
        }
        return x;
    }

    size_t u32_read() { return bytes_consumed / sizeof(uint32_t); }

    size_t flush()
    {
        // write the leftovers if needed
        if (write_mode && in_word_offset != 0) {
            *of++ = *first_ptr;
            bytes_written += sizeof(uint32_t);
            in_word_offset = 0;
        }
        return bytes_written;
    }
};
