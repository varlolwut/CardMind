#include "voice_input.h"

#include "adv_audio_power.h"
#include "audio_utils.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace cardputer {
namespace {

constexpr const char* kVoicePath = "/voice.wav";
constexpr int kSdClockPin = 40;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 14;
constexpr int kSdChipSelectPin = 12;
constexpr std::uint32_t kSampleRate = 16000;
constexpr std::size_t kChunkSamples = 1024;
constexpr std::uint32_t kMinimumSamples = kSampleRate / 2;
constexpr std::uint32_t kMaximumRecordingSeconds = 60;
constexpr std::uint32_t kMaximumSamples = kSampleRate * kMaximumRecordingSeconds;
constexpr std::size_t kWavHeaderSize = 44;
constexpr std::uint8_t kSdMountAttempts = 3;

std::uint16_t normalizedPeak(const std::array<std::int16_t, kChunkSamples>& samples)
{
    std::int32_t peak = 0;
    for (const std::int16_t sample : samples) {
        peak = std::max(peak, std::abs(static_cast<std::int32_t>(sample)));
    }
    return static_cast<std::uint16_t>(std::min<std::int32_t>(1000, peak * 1000 / 32767));
}

std::uint16_t normalizedMean(std::uint64_t absoluteTotal, std::uint32_t sampleCount)
{
    if (sampleCount == 0) {
        return 0;
    }
    const std::uint64_t scaled = absoluteTotal * 1000ULL /
        (static_cast<std::uint64_t>(sampleCount) * 32767ULL);
    return static_cast<std::uint16_t>(std::min<std::uint64_t>(1000ULL, scaled));
}

OperationResult deleteRecordingIfPresent()
{
    if (!SD.exists(kVoicePath)) {
        return {true, ""};
    }
    if (!SD.remove(kVoicePath)) {
        return {false, "Failed to remove temporary voice recording from microSD"};
    }
    return {true, ""};
}

}  // namespace

OperationResult initializeVoiceStorage()
{
    bool mountSucceeded = false;
    for (std::uint8_t attempt = 1; attempt <= kSdMountAttempts; ++attempt) {
        SPI.begin(kSdClockPin, kSdMisoPin, kSdMosiPin, kSdChipSelectPin);
        const bool mounted = SD.begin(kSdChipSelectPin, SPI, 25000000);
        mountSucceeded = mountSucceeded || mounted;
        if (mounted && SD.cardType() != CARD_NONE) {
            return deleteRecordingIfPresent();
        }
        Serial.printf("WARN event=sd_mount attempt=%u result=failed\n",
                      static_cast<unsigned int>(attempt));
        SD.end();
        SPI.end();
        if (attempt < kSdMountAttempts) {
            delay(static_cast<std::uint32_t>(attempt) * 250U);
        }
    }
    return mountSucceeded
        ? OperationResult{false, "No microSD card was detected after 3 attempts"}
        : OperationResult{false, "Failed to mount microSD after 3 attempts; reinsert or check the card"};
}

VoiceRecordingResult recordVoiceWhileButtonHeld(const VoiceProgressCallback& onProgress)
{
    if (!M5Cardputer.BtnA.isPressed()) {
        return {false, 0, 0, 0, "Hold the G0 microphone button while speaking"};
    }
    const OperationResult deleteResult = deleteRecordingIfPresent();
    if (!deleteResult.success) {
        return {false, 0, 0, 0, deleteResult.error};
    }
    File file = SD.open(kVoicePath, FILE_WRITE);
    if (!file) {
        return {false, 0, 0, 0, "Failed to create temporary /voice.wav on microSD"};
    }
    const std::array<std::uint8_t, kWavHeaderSize> emptyHeader = {};
    if (file.write(emptyHeader.data(), emptyHeader.size()) != emptyHeader.size()) {
        file.close();
        deleteRecordingIfPresent();
        return {false, 0, 0, 0, "Failed to reserve the WAV header on microSD"};
    }

    const OperationResult audioPowerResult = powerDownCardputerAdvAudio();
    if (!audioPowerResult.success) {
        file.close();
        deleteRecordingIfPresent();
        return {false, 0, 0, 0, audioPowerResult.error};
    }
    if (!M5Cardputer.Mic.begin()) {
        file.close();
        deleteRecordingIfPresent();
        return {false, 0, 0, 0, "Failed to start the Cardputer ADV microphone"};
    }

    std::array<std::int16_t, kChunkSamples> samples = {};
    std::uint32_t sampleCount = 0;
    std::uint64_t absoluteTotal = 0;
    std::uint16_t peakLevel = 0;
    String error;
    const std::uint32_t startedAt = millis();
    onProgress(0, 0);
    while (M5Cardputer.BtnA.isPressed() && sampleCount < kMaximumSamples) {
        const std::size_t remaining = kMaximumSamples - sampleCount;
        const std::size_t requested = std::min(kChunkSamples, remaining);
        if (!M5Cardputer.Mic.record(samples.data(), requested, kSampleRate)) {
            error = "Microphone recording queue rejected an audio chunk";
            break;
        }
        while (M5Cardputer.Mic.isRecording() != 0) {
            M5Cardputer.update();
            delay(1);
        }
        const std::size_t bytes = requested * sizeof(std::int16_t);
        if (file.write(reinterpret_cast<const std::uint8_t*>(samples.data()), bytes) != bytes) {
            error = "Failed to write a microphone audio chunk to microSD";
            break;
        }
        for (std::size_t index = 0; index < requested; ++index) {
            absoluteTotal += static_cast<std::uint64_t>(
                std::abs(static_cast<std::int32_t>(samples[index])));
        }
        peakLevel = std::max(peakLevel, normalizedPeak(samples));
        sampleCount += static_cast<std::uint32_t>(requested);
        onProgress(millis() - startedAt, normalizedPeak(samples));
    }
    M5Cardputer.Mic.end();
    const OperationResult shutdownResult = powerDownCardputerAdvAudio();
    if (!shutdownResult.success && error.isEmpty()) {
        error = shutdownResult.error;
    }

    if (!error.isEmpty()) {
        file.close();
        deleteRecordingIfPresent();
        return {false, sampleCount, peakLevel, normalizedMean(absoluteTotal, sampleCount), error};
    }
    if (sampleCount < kMinimumSamples) {
        file.close();
        deleteRecordingIfPresent();
        return {false, sampleCount, peakLevel, normalizedMean(absoluteTotal, sampleCount),
                "Voice recording is too short; hold G0 for at least half a second"};
    }
    const auto header = buildPcmWavHeader(kSampleRate, sampleCount);
    if (!file.seek(0) || file.write(header.data(), header.size()) != header.size()) {
        file.close();
        deleteRecordingIfPresent();
        return {false, sampleCount, peakLevel, normalizedMean(absoluteTotal, sampleCount),
                "Failed to finalize the temporary WAV header"};
    }
    file.flush();
    file.close();
    return {true, sampleCount, peakLevel, normalizedMean(absoluteTotal, sampleCount), ""};
}

VoiceRecordingResult probeMicrophone(std::uint32_t durationMs)
{
    if (durationMs == 0 || durationMs > 5000) {
        return {false, 0, 0, 0, "Microphone probe duration must be between 1 and 5000 ms"};
    }
    const OperationResult audioPowerResult = powerDownCardputerAdvAudio();
    if (!audioPowerResult.success) {
        return {false, 0, 0, 0, audioPowerResult.error};
    }
    if (!M5Cardputer.Mic.begin()) {
        return {false, 0, 0, 0, "Failed to start the Cardputer ADV microphone"};
    }

    std::array<std::int16_t, kChunkSamples> samples = {};
    const std::uint32_t targetSamples =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(kSampleRate) * durationMs / 1000U);
    std::uint32_t sampleCount = 0;
    std::uint64_t absoluteTotal = 0;
    std::uint16_t peakLevel = 0;
    while (sampleCount < targetSamples) {
        const std::size_t requested = std::min<std::size_t>(
            kChunkSamples, static_cast<std::size_t>(targetSamples - sampleCount));
        if (!M5Cardputer.Mic.record(samples.data(), requested, kSampleRate)) {
            M5Cardputer.Mic.end();
            const OperationResult shutdownResult = powerDownCardputerAdvAudio();
            const String error = shutdownResult.success
                ? String("Microphone probe queue rejected an audio chunk")
                : shutdownResult.error;
            return {false, sampleCount, peakLevel,
                    normalizedMean(absoluteTotal, sampleCount),
                    error};
        }
        while (M5Cardputer.Mic.isRecording() != 0) {
            M5Cardputer.update();
            delay(1);
        }
        for (std::size_t index = 0; index < requested; ++index) {
            absoluteTotal += static_cast<std::uint64_t>(
                std::abs(static_cast<std::int32_t>(samples[index])));
        }
        peakLevel = std::max(peakLevel, normalizedPeak(samples));
        sampleCount += static_cast<std::uint32_t>(requested);
    }
    M5Cardputer.Mic.end();
    const OperationResult shutdownResult = powerDownCardputerAdvAudio();
    if (!shutdownResult.success) {
        return {false, sampleCount, peakLevel,
                normalizedMean(absoluteTotal, sampleCount), shutdownResult.error};
    }
    return {true, sampleCount, peakLevel, normalizedMean(absoluteTotal, sampleCount), ""};
}

OperationResult removeVoiceRecording()
{
    return deleteRecordingIfPresent();
}

const char* voiceRecordingPath()
{
    return kVoicePath;
}

std::uint32_t maximumVoiceRecordingMs()
{
    return kMaximumRecordingSeconds * 1000U;
}

}  // namespace cardputer
