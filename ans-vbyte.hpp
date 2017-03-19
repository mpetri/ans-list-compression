#pragma once

#include <memory>

#include "util.hpp"

struct ans_vbyte {
    bool required_increasing = false;
    std::string name() { return "ans_vbyte"; }
    void init(const list_data&){

    };

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
    }
    uint32_t* decodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
    }
};
