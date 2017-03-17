#pragma once

#include <chrono>
#include <cstdarg>
#include <cstring>

using namespace std::chrono;

struct list_input {
    std::vector<std::vector<uint32_t>> postings_lists;
    std::chrono::nanoseconds time_ns;
    uint64_t num_postings = 0;
};

struct encoding_stats {
    uint64_t bytes_written;
    std::chrono::nanoseconds time_ns;
};

struct timer {
    high_resolution_clock::time_point start;
    std::string name;
    timer(const std::string& _n) : name(_n) {
        std::cerr << "START(" << name << ")" << std::endl;
        start = high_resolution_clock::now();
    }
    ~timer() {
        auto stop = high_resolution_clock::now();
        std::cerr << "STOP(" << name << ") - "
                  << duration_cast<milliseconds>(stop - start).count() / 1000.0f
                  << " sec" << std::endl;
    }
};

int printff(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stdout, format, args);
    va_end(args);
    fflush(stdout);
    return ret;
}

void quit(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);
    va_end(args);
    if (errno != 0) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    exit(EXIT_FAILURE);
}

inline uint32_t read_uint32_line() {
    const uint64_t BUFFER_SIZE = 1024 * 512u;
    static uint8_t buf[BUFFER_SIZE];
    static size_t pos = 0;
    static size_t left = 0;
    uint32_t x = 0;
refill_buf:
    if (pos == left) {
        left = fread(buf, 1, BUFFER_SIZE, stdin);
        pos = 0;
        if (left == 0) {
            quit("error reading number from line.");
        }
    }
    while (pos != left) {
        uint8_t digit = buf[pos++];
        if (digit == '\n') {
            return x;
        }
        if (digit < '0' || digit > '9') {
            quit("error reading number from line.");
        }
        x = (x * 10) + (digit - '0');
    }
    goto refill_buf;
}

std::vector<uint32_t> read_uint32_list() {
    uint32_t list_len = read_uint32_line();
    std::vector<uint32_t> list(list_len);
    for (uint32_t j = 0; j < list_len; j++) {
        list[j] = read_uint32_line();
    }
    return list;
}

list_input read_all_input_from_stdin() {
    list_input input;
    timer t("read input lists from stdin");
    std::vector<std::vector<uint32_t>> lists;
    uint32_t num_lists = read_uint32_line();
    for (uint32_t i = 0; i < num_lists; i++) {
        auto list = read_uint32_list();
        input.num_postings += list.size();
        input.postings_lists.push_back(list);
    }
    printff("num lists = %lu num ints = %lu\n", num_lists, input.num_postings);
    return input;
}

void output_list_to_stdout(uint32_t* list, uint32_t n) {
    printf("%u\n", n);
    for (uint32_t j = 0; j < n; j++) {
        printf("%u\n", list[j]);
    }
}

std::vector<uint8_t> read_file_content(FILE* f) {
    std::vector<uint8_t> content;
    auto cur = ftell(f);
    fseek(f, 0, SEEK_END);
    auto end = ftell(f);
    size_t file_size = (end - cur);
    content.resize(file_size);
    fseek(f, cur, SEEK_SET);
    size_t ret = fread(content.data(), 1, file_size, f);
    if (ret != file_size) {
        quit("reading file content failed: %d", ret);
    }
    return content;
}

std::vector<uint32_t> read_file_content_u32(FILE* f) {
    std::vector<uint32_t> content;
    auto cur = ftell(f);
    fseek(f, 0, SEEK_END);
    auto end = ftell(f);
    size_t file_size = (end - cur);
    size_t file_size_u32 = file_size / sizeof(uint32_t);
    if (file_size % sizeof(uint32_t) != 0) {
        quit("reading file content failed: file size % 32bit != 0");
    }
    content.resize(file_size_u32);
    fseek(f, cur, SEEK_SET);
    size_t ret = fread(content.data(), sizeof(uint32_t), file_size_u32, f);
    if (ret != file_size_u32) {
        quit("reading file content failed: %d", ret);
    }
    return content;
}

FILE* fopen_or_fail(std::string file_name, const char* mode) {
    FILE* out_file = fopen(file_name.c_str(), mode);
    if (!out_file) {
        quit("opening output file %s failed", file_name.c_str());
    }
    return out_file;
}

void fclose_or_fail(FILE* f) {
    int ret = fclose(f);
    if (ret != 0) {
        quit("closing file failed");
    }
}

uint64_t read_u64(FILE* f) {
    uint64_t x;
    int ret = fread(&x, sizeof(uint64_t), 1, f);
    if (ret != 1) {
        quit("read byte from file failed: %d != %d", ret, 1);
    }
    return x;
}

size_t write_u64(FILE* f, uint64_t x) {
    size_t ret = fwrite(&x, sizeof(uint64_t), 1u, f);
    if (ret != 1u) {
        quit("writing byte to file: %u != %u", ret, 1u);
    }
    return sizeof(uint64_t);
}

uint32_t read_u32(FILE* f) {
    uint32_t x;
    int ret = fread(&x, sizeof(uint32_t), 1, f);
    if (ret != 1) {
        quit("read byte from file failed: %d != %d", ret, 1);
    }
    return x;
}

size_t write_u32(FILE* f, uint32_t x) {
    size_t ret = fwrite(&x, sizeof(uint32_t), 1u, f);
    if (ret != 1u) {
        quit("writing byte to file: %u != %u", ret, 1u);
    }
    return sizeof(uint32_t);
}

size_t write_u32s(FILE* f, uint32_t* buf, size_t n) {
    size_t ret = fwrite(buf, sizeof(uint32_t), n, f);
    if (ret != n) {
        quit("writing byte to file: %u != %u", ret, n);
    }
    return n * sizeof(uint32_t);
}

size_t write_byte(FILE* f, uint8_t x) {
    size_t ret = fwrite(&x, 1, 1, f);
    if (ret != 1u) {
        quit("writing byte to file: %u != %u", ret, 1);
    }
    return 1;
}

uint8_t read_byte(FILE* f) {
    uint8_t x;
    int ret = fread(&x, 1, 1, f);
    if (ret != 1) {
        quit("read byte from file failed: %d != %d", ret, 1);
    }
    return x;
}
