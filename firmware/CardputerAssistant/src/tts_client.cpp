#include "tts_client.h"

#include "storage.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace cardputer {
namespace {

constexpr const char* kTemporaryPcmPath = "/tts.pcm";
constexpr const char* kDefaultTtsBaseUrl = "https://api.elevenlabs.io";
constexpr const char* kGtsRootR1 = R"CERT(-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----)CERT";
constexpr std::uint32_t kConnectTimeoutMs = 15000;
constexpr std::uint16_t kHttpTimeoutMs = 60000;
constexpr std::size_t kMaximumSpeechBytes = 5000;
constexpr std::size_t kMaximumPcmBytes = 16U * 1024U * 1024U;
constexpr std::uint32_t kPcmSampleRate = 16000;
constexpr std::size_t kPlaybackSamples = 1024;
constexpr std::size_t kPlaybackBuffers = 3;
constexpr int kMaximumAttempts = 3;
std::array<std::array<std::int16_t, kPlaybackSamples>, kPlaybackBuffers> playbackBuffers;

class BoundedFileStream final : public Stream {
public:
    BoundedFileStream(File& file, std::size_t maximumBytes)
        : file_(file), maximumBytes_(maximumBytes)
    {
    }

    std::size_t write(std::uint8_t byte) override
    {
        return write(&byte, 1);
    }

    std::size_t write(const std::uint8_t* buffer, std::size_t size) override
    {
        if (writtenBytes_ + size > maximumBytes_) {
            overflowed_ = true;
            return 0;
        }
        const std::size_t written = file_.write(buffer, size);
        writtenBytes_ += written;
        return written;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override { file_.flush(); }
    bool overflowed() const { return overflowed_; }

private:
    File& file_;
    std::size_t maximumBytes_;
    std::size_t writtenBytes_ = 0;
    bool overflowed_ = false;
};

bool isTransientStatus(int status)
{
    return status <= 0 || status == 429 || status == 500 || status == 502 ||
           status == 503 || status == 529;
}

String endpointUrl(const Settings& settings, const String& path)
{
    const std::string url = buildVersionedApiUrl(settings.ttsBaseUrl.c_str(), path.c_str());
    return String(url.c_str());
}

String boundedApiError(const String& body)
{
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(document, body);
    String message;
    if (!jsonError) {
        const char* detail = document["detail"]["message"].as<const char*>();
        const char* nested = document["error"]["message"].as<const char*>();
        if (detail != nullptr) {
            message = detail;
        } else if (nested != nullptr) {
            message = nested;
        }
    }
    if (message.isEmpty()) {
        message = "response did not contain a valid error message";
    }
    message.replace("\r", " ");
    message.replace("\n", " ");
    if (message.length() > 140) {
        message = message.substring(0, 140) + "...";
    }
    return message;
}

OperationResult removeTemporaryPcm()
{
    if (!SD.exists(kTemporaryPcmPath)) {
        return {true, ""};
    }
    return SD.remove(kTemporaryPcmPath)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to remove temporary TTS audio from microSD"};
}

OperationResult playPcmFile(std::uint8_t volume)
{
    File file = SD.open(kTemporaryPcmPath, FILE_READ);
    if (!file) {
        return {false, "Failed to open temporary TTS audio from microSD"};
    }
    if (file.size() == 0 || file.size() % sizeof(std::int16_t) != 0) {
        file.close();
        return {false, "TTS returned empty or misaligned 16-bit PCM audio"};
    }
    M5Cardputer.Mic.end();
    if (!M5Cardputer.Speaker.begin()) {
        file.close();
        return {false, "Failed to start the Cardputer ADV speaker"};
    }
    M5Cardputer.Speaker.setVolume(volume);
    std::size_t bufferIndex = 0;
    while (file.available()) {
        auto& buffer = playbackBuffers[bufferIndex];
        const std::size_t bytes = file.read(
            reinterpret_cast<std::uint8_t*>(buffer.data()), buffer.size() * sizeof(std::int16_t));
        if (bytes == 0 || bytes % sizeof(std::int16_t) != 0) {
            file.close();
            M5Cardputer.Speaker.end();
            return {false, "Failed to read aligned PCM samples from microSD"};
        }
        if (!M5Cardputer.Speaker.playRaw(buffer.data(), bytes / sizeof(std::int16_t),
                                          kPcmSampleRate, false, 1, 0)) {
            file.close();
            M5Cardputer.Speaker.end();
            return {false, "Cardputer speaker rejected a TTS PCM chunk"};
        }
        bufferIndex = (bufferIndex + 1) % playbackBuffers.size();
    }
    file.close();
    while (M5Cardputer.Speaker.isPlaying()) {
        M5Cardputer.update();
        delay(1);
    }
    M5Cardputer.Speaker.end();
    return {true, ""};
}

OperationResult downloadSpeech(const Settings& settings, const std::string& text)
{
    const String voicePath = "/v1/text-to-speech/" + settings.ttsVoice + "/stream";
    const String url = endpointUrl(settings, voicePath) + "?output_format=pcm_16000";
    JsonDocument document;
    document["text"] = text;
    document["model_id"] = settings.ttsModel;
    document["voice_settings"]["stability"] = 0.5;
    document["voice_settings"]["similarity_boost"] = 0.75;
    String payload;
    serializeJson(document, payload);

    String lastError = "TTS request did not run";
    for (int attempt = 1; attempt <= kMaximumAttempts; ++attempt) {
        const OperationResult cleanup = removeTemporaryPcm();
        if (!cleanup.success) {
            return cleanup;
        }
        WiFiClientSecure client;
        client.setCACert(kGtsRootR1);
        client.setHandshakeTimeout(15);
        HTTPClient http;
        http.setConnectTimeout(kConnectTimeoutMs);
        http.setTimeout(kHttpTimeoutMs);
        if (!http.begin(client, url)) {
            return {false, "Failed to initialize verified ElevenLabs TTS HTTPS request"};
        }
        http.addHeader("xi-api-key", settings.ttsApiKey);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "audio/pcm");
        const int status = http.POST(payload);
        if (status == 200) {
            const int declaredSize = http.getSize();
            if (declaredSize > static_cast<int>(kMaximumPcmBytes)) {
                http.end();
                return {false, "TTS PCM response exceeds the 16 MiB microSD safety limit"};
            }
            File file = SD.open(kTemporaryPcmPath, FILE_WRITE);
            if (!file) {
                http.end();
                return {false, "Failed to create temporary TTS PCM file on microSD"};
            }
            BoundedFileStream boundedFile(file, kMaximumPcmBytes);
            const int written = http.writeToStream(&boundedFile);
            file.flush();
            file.close();
            http.end();
            if (boundedFile.overflowed()) {
                removeTemporaryPcm();
                return {false, "TTS PCM response exceeded the 16 MiB microSD safety limit"};
            }
            if (written <= 0) {
                removeTemporaryPcm();
                return {false, "Failed to stream TTS PCM response to microSD: " +
                               HTTPClient::errorToString(written)};
            }
            if (written > static_cast<int>(kMaximumPcmBytes)) {
                removeTemporaryPcm();
                return {false, "TTS PCM response exceeded the 16 MiB microSD safety limit"};
            }
            return {true, ""};
        }
        const String body = status > 0 ? http.getString() : String();
        lastError = status > 0
            ? "TTS HTTP " + String(status) + ": " + boundedApiError(body)
            : "TTS request failed: " + HTTPClient::errorToString(status);
        http.end();
        if (!isTransientStatus(status) || attempt == kMaximumAttempts) {
            return {false, lastError};
        }
        Serial.printf("WARN event=tts_request attempt=%d status=%d retry=yes\n", attempt, status);
        delay(250U * static_cast<std::uint32_t>(attempt));
    }
    return {false, lastError};
}

OperationResult requestUserEndpoint(const String& apiKey, bool requireAuthentication)
{
    WiFiClientSecure client;
    client.setCACert(kGtsRootR1);
    client.setHandshakeTimeout(15);
    HTTPClient http;
    http.setConnectTimeout(kConnectTimeoutMs);
    http.setTimeout(kHttpTimeoutMs);
    const String url = String(kDefaultTtsBaseUrl) + "/v1/user";
    if (!http.begin(client, url)) {
        return {false, "Failed to initialize verified ElevenLabs TLS probe"};
    }
    if (!apiKey.isEmpty()) {
        http.addHeader("xi-api-key", apiKey);
    }
    const int status = http.GET();
    const String body = status > 0 ? http.getString() : String();
    http.end();
    if (requireAuthentication && status == 200) {
        return {true, ""};
    }
    if (!requireAuthentication && (status == 401 || status == 403)) {
        return {true, ""};
    }
    return status > 0
        ? OperationResult{false, "ElevenLabs HTTP " + String(status) + ": " + boundedApiError(body)}
        : OperationResult{false, "ElevenLabs request failed: " + HTTPClient::errorToString(status)};
}

}  // namespace

OperationResult synthesizeAndPlaySpeech(const Settings& settings, const std::string& text)
{
    if (!ttsSettingsAreComplete(settings)) {
        return {false, "TTS is not configured; use Fn+4 > Web setup"};
    }
    if (text.empty()) {
        return {false, "There is no assistant text to speak"};
    }
    if (text.size() > kMaximumSpeechBytes) {
        return {false, "Assistant response exceeds the 5000-byte TTS limit; ask for a shorter reply"};
    }
    if (!isValidUtf8(text)) {
        return {false, "Assistant response contains invalid UTF-8 and cannot be spoken"};
    }
    const OperationResult download = downloadSpeech(settings, text);
    if (!download.success) {
        return download;
    }
    const OperationResult playback = playPcmFile(settings.ttsVolume);
    const OperationResult cleanup = removeTemporaryPcm();
    if (!playback.success) {
        return playback;
    }
    return cleanup;
}

OperationResult validateTtsCredentials(const Settings& settings)
{
    if (!ttsSettingsAreComplete(settings)) {
        return {false, "TTS is not configured; use Fn+4 > Web setup"};
    }
    return requestUserEndpoint(settings.ttsApiKey, true);
}

OperationResult probeDefaultTtsTls()
{
    return requestUserEndpoint("", false);
}

OperationResult playTtsHardwareTest(std::uint8_t volume)
{
    static std::array<std::int16_t, 1600> samples;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = (index / 20) % 2 == 0 ? 3500 : -3500;
    }
    M5Cardputer.Mic.end();
    if (!M5Cardputer.Speaker.begin()) {
        return {false, "Failed to start the Cardputer ADV speaker"};
    }
    M5Cardputer.Speaker.setVolume(volume);
    if (!M5Cardputer.Speaker.playRaw(samples.data(), samples.size(), kPcmSampleRate, false, 1, 0)) {
        M5Cardputer.Speaker.end();
        return {false, "Cardputer speaker rejected the hardware test PCM"};
    }
    while (M5Cardputer.Speaker.isPlaying()) {
        delay(1);
    }
    M5Cardputer.Speaker.end();
    return {true, ""};
}

}  // namespace cardputer
