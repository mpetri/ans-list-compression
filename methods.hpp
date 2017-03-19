#pragma once

#include "FastPFor-master/headers/compositecodec.h"
#include "FastPFor-master/headers/optpfor.h"
#include "FastPFor-master/headers/simple16.h"
#include "FastPFor-master/headers/variablebyte.h"
#include "compress_qmx.h"
#include "interp.hpp"

struct interpolative {
    bool required_increasing = true;
    std::string name() { return "interpolative"; }
    void init(const list_data&){};

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& enc_u32)
    {
        // (1) write length later
        *out++ = in[len - 1];
        // (2) delegate to FastPFor code
        uint32_t bw
            = interpolative_internal::encode(out, in, len - 1, in[len - 1]);
        enc_u32 = (bw / sizeof(uint32_t)) + 1; // for list len and universe
    }
    const uint32_t* decodeArray(const uint32_t* in, const size_t /*enc_u32*/,
        uint32_t* out, size_t list_len)
    {
        uint32_t universe = *in++;
        size_t u32_read
            = interpolative_internal::decode(in, out, list_len - 1, universe);
        out[list_len - 1] = universe;
        return in + u32_read + 1;
    }
};

struct vbyte {
    bool required_increasing = false;
    std::string name() { return "vbyte"; }
    void init(const list_data&){};

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& enc_u32)
    {
        static FastPForLib::VariableByte vb;
        vb.encodeArray(in, len, out, enc_u32);
    }
    const uint32_t* decodeArray(const uint32_t* in, const size_t enc_u32,
        uint32_t* out, size_t list_len)
    {
        static FastPForLib::VariableByte vb;
        return vb.decodeArray(in, enc_u32, out, list_len);
    }
};

template <uint32_t t_block_size = 128> struct op4 {
    static_assert(
        t_block_size % 32 == 0, "op4 blocksize must be multiple of 32");
    bool required_increasing = false;
    std::string name() { return "op4"; }
    void init(const list_data&){};

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& enc_u32)
    {
        using op4_codec = FastPForLib::OPTPFor<t_block_size / 32>;
        using vb_codec = FastPForLib::VariableByte;
        static FastPForLib::CompositeCodec<op4_codec, vb_codec> op4c;
        op4c.encodeArray(in, len, out, enc_u32);
    }
    const uint32_t* decodeArray(const uint32_t* in, const size_t enc_u32,
        uint32_t* out, size_t list_len)
    {
        using op4_codec = FastPForLib::OPTPFor<t_block_size / 32>;
        using vb_codec = FastPForLib::VariableByte;
        static FastPForLib::CompositeCodec<op4_codec, vb_codec> op4c;
        return op4c.decodeArray(in, enc_u32, out, list_len);
    }
};

struct simple16 {
    bool required_increasing = false;
    std::string name() { return "simple16"; }
    void init(const list_data&){};

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& enc_u32)
    {
        using s16_codec = FastPForLib::Simple16<false>;
        static s16_codec s16;
        s16.encodeArray(in, len, out, enc_u32);
    }
    const uint32_t* decodeArray(const uint32_t* in, const size_t enc_u32,
        uint32_t* out, size_t list_len)
    {
        using s16_codec = FastPForLib::Simple16<false>;
        static s16_codec s16;
        return s16.decodeArray(in, enc_u32, out, list_len);
    }
};

struct qmx {
    bool required_increasing = false;
    std::string name() { return "qmx"; }
    void init(const list_data&){};

    void encodeArray(
        const uint32_t* in, const size_t len, uint32_t* out, size_t& enc_u32)
    {
        static compress_qmx qc;

        // align output ptr to 128 bit boundaries as required by compress_qmx
        size_t bytes_left = 10000000;
        out = align_ptr(16, 1, out, bytes_left);
        size_t align_u32 = (10000000 - bytes_left) / (sizeof(uint32_t));

        qc.encodeArray(in, len, out, &enc_u32);
        enc_u32 += align_u32;
    }
    const uint32_t* decodeArray(
        const uint32_t* in, size_t enc_u32, uint32_t* out, size_t list_len)
    {
        // align input ptr to 128 bit boundaries as required by compress_qmx
        size_t bytes_left = 10000000;
        in = align_ptr(16, 1, in, bytes_left);
        enc_u32 -= (10000000 - bytes_left) / (sizeof(uint32_t));

        bool isInputAligned = ((reinterpret_cast<size_t>(in) % 16) == 0);
        bool isOutputAligned = ((reinterpret_cast<size_t>(out) % 16) == 0);

        if (!isInputAligned || !isOutputAligned) {
            printf("qmx decode alignment error: %lu %lu\n",
                reinterpret_cast<size_t>(in), reinterpret_cast<size_t>(out));
        }

        static compress_qmx qc;
        return qc.decodeArray(in, enc_u32, out, list_len);
    }
};
