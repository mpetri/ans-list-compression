#pragma once

namespace constants {
const uint64_t ANS_START_STATE = 0;
const uint32_t OUTPUT_BASE = 256;
const uint8_t OUTPUT_BASE_LOG2 = 8;
const uint32_t MAX_SIGMA = 256;
const uint8_t MAX_MAG = 25;
const uint8_t NUM_MAGS = 16;
const std::array<uint8_t, NUM_MAGS> SEL2MAG{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12,
    14, 16, 19, 22, 25 };
const std::array<uint8_t, MAX_MAG + 1> MAG2SEL{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9,
    10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15 };
const uint64_t TOPFREQ = 1048576;
}