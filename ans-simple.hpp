#pragma once

#include <memory>

#include "util.hpp"

struct ans_simple {
    std::string name() { return "ans_simple"; }

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
    }
    uint32_t* decodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& nvalue)
    {
    }
};
