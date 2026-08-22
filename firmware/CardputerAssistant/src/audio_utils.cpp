#include "audio_utils.h"

#include <stdexcept>

namespace cardputer {
namespace {

void writeLittleEndian16(std::array<std::uint8_t, 44>& header,
                         std::size_t offset,
                         std::uint16_t value)
{
    header[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    header[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void writeLittleEndian32(std::array<std::uint8_t, 44>& header,
                         std::size_t offset,
                         std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte) {
        header[offset + byte] = static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xFFU);
    }
}

}  // namespace

std::array<std::uint8_t, 44> buildPcmWavHeader(std::uint32_t sampleRate,
                                               std::uint32_t sampleCount)
{
    if (sampleRate == 0) {
        throw std::invalid_argument("sampleRate must be greater than zero");
    }
    if (sampleRate > UINT32_MAX / 2U) {
        throw std::overflow_error("sampleRate exceeds PCM byte-rate limits");
    }
    if (sampleCount > (UINT32_MAX - 36U) / 2U) {
        throw std::overflow_error("PCM sample count exceeds WAV size limits");
    }
    const std::uint32_t dataBytes = sampleCount * 2U;
    std::array<std::uint8_t, 44> header = {};
    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    writeLittleEndian32(header, 4, dataBytes + 36U);
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    writeLittleEndian32(header, 16, 16U);
    writeLittleEndian16(header, 20, 1U);
    writeLittleEndian16(header, 22, 1U);
    writeLittleEndian32(header, 24, sampleRate);
    writeLittleEndian32(header, 28, sampleRate * 2U);
    writeLittleEndian16(header, 32, 2U);
    writeLittleEndian16(header, 34, 16U);
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    writeLittleEndian32(header, 40, dataBytes);
    return header;
}

}  // namespace cardputer
