#include "adv_audio_power.h"

#include <M5Cardputer.h>

#include <array>
#include <cstdint>

namespace cardputer {
namespace {

constexpr std::uint8_t kEs8311Address = 0x18;
constexpr std::uint32_t kCodecI2cFrequency = 100000;
constexpr std::uint8_t kI2cWriteAttempts = 3;

struct CodecRegisterValue {
    std::uint8_t reg;
    std::uint8_t value;
};

constexpr std::array<CodecRegisterValue, 14> kCodecPowerDownSequence = {{
    {0x32, 0x00},
    {0x17, 0x00},
    {0x0E, 0xFF},
    {0x12, 0x02},
    {0x14, 0x00},
    {0x0D, 0xFA},
    {0x15, 0x00},
    {0x02, 0x10},
    {0x00, 0x00},
    {0x00, 0x1F},
    {0x01, 0x30},
    {0x01, 0x00},
    {0x45, 0x00},
    {0x0D, 0xFC},
}};

constexpr std::array<CodecRegisterValue, 1> kCodecPoweredDownRegisters = {{
    {0x0D, 0xFC},
}};

template <std::size_t Size>
bool writeCodecRegisters(const std::array<CodecRegisterValue, Size>& values)
{
    for (const CodecRegisterValue& item : values) {
        bool written = false;
        for (std::uint8_t attempt = 0; attempt < kI2cWriteAttempts; ++attempt) {
            written = M5.In_I2C.writeRegister8(
                kEs8311Address, item.reg, item.value, kCodecI2cFrequency);
            if (written) {
                break;
            }
            delay(1);
        }
        if (!written) {
            return false;
        }
    }
    return true;
}

}  // namespace

OperationResult verifyCardputerAdvAudioPoweredDown()
{
    for (const CodecRegisterValue& expected : kCodecPoweredDownRegisters) {
        const std::uint8_t actual = M5.In_I2C.readRegister8(
            kEs8311Address, expected.reg, kCodecI2cFrequency);
        if (actual != expected.value) {
            return {
                false,
                "ES8311 register 0x" + String(expected.reg, HEX) +
                    " did not enter the expected power-down state",
            };
        }
    }
    return {true, ""};
}

OperationResult initializeCardputerAdvAudioPowerControl()
{
    if (M5.getBoard() != m5::board_t::board_M5CardputerADV) {
        return {false, "ES8311 power control requires M5Stack Cardputer ADV"};
    }
    return {true, ""};
}

OperationResult powerDownCardputerAdvAudio()
{
    M5Cardputer.Speaker.end();
    if (!writeCodecRegisters(kCodecPowerDownSequence)) {
        return {false, "Failed to power down the Cardputer ADV ES8311 audio codec"};
    }
    return verifyCardputerAdvAudioPoweredDown();
}

}  // namespace cardputer
