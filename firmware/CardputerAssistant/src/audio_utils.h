#pragma once

#include <array>
#include <cstdint>

namespace cardputer {

std::array<std::uint8_t, 44> buildPcmWavHeader(std::uint32_t sampleRate,
                                               std::uint32_t sampleCount);

}  // namespace cardputer
