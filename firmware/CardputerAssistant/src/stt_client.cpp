#include "stt_client.h"

#include "storage.h"
#include "text_utils.h"
#include "tls_roots.h"
#include "voice_input.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

namespace cardputer {
namespace {

constexpr const char* kTranscriptionsPath = "/v1/audio/transcriptions";
constexpr const char* kDefaultModelsUrl = "https://api.groq.com/openai/v1/models";
constexpr std::uint16_t kHttpTimeoutMs = 60000;
constexpr int kMaximumAttempts = 3;
constexpr std::size_t kMaximumResponseBytes = 8192;
constexpr std::size_t kMaximumErrorLength = 140;

bool isTransientStatus(int status)
{
    return status <= 0 || status == 429 || status == 500 || status == 502 ||
           status == 503 || status == 529;
}

String transcriptionUrl(const Settings& settings)
{
    const std::string url = buildVersionedApiUrl(settings.sttBaseUrl.c_str(), kTranscriptionsPath);
    return String(url.c_str());
}

String modelsUrl(const Settings& settings)
{
    const std::string url = buildVersionedApiUrl(settings.sttBaseUrl.c_str(), "/v1/models");
    return String(url.c_str());
}

String boundedErrorMessage(const String& body)
{
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(document, body);
    String message;
    if (!jsonError) {
        const char* nested = document["error"]["message"].as<const char*>();
        const char* topLevel = document["message"].as<const char*>();
        if (nested != nullptr) {
            message = nested;
        } else if (topLevel != nullptr) {
            message = topLevel;
        }
    }
    if (message.isEmpty()) {
        message = "response did not contain a valid error message";
    }
    message.replace("\r", " ");
    message.replace("\n", " ");
    if (message.length() > kMaximumErrorLength) {
        message = message.substring(0, kMaximumErrorLength) + "...";
    }
    return message;
}

String multipartPrefix(const Settings& settings, const String& boundary)
{
    String prefix;
    prefix.reserve(420);
    prefix += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n";
    prefix += settings.sttModel + "\r\n";
    prefix += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n";
    prefix += "--" + boundary +
              "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\n"
              "Content-Type: audio/wav\r\n\r\n";
    return prefix;
}

struct HttpsEndpoint {
    String host;
    std::uint16_t port;
    String path;
};

struct UploadResult {
    int status;
    String body;
    String error;
};

OperationResult parseHttpsEndpoint(const String& url, HttpsEndpoint& endpoint)
{
    constexpr const char* scheme = "https://";
    if (!url.startsWith(scheme)) {
        return {false, "STT URL must start with https://"};
    }
    const String remainder = url.substring(strlen(scheme));
    const int slash = remainder.indexOf('/');
    const String authority = slash >= 0 ? remainder.substring(0, slash) : remainder;
    const String path = slash >= 0 ? remainder.substring(slash) : String("/");
    if (authority.isEmpty() || authority.indexOf('@') >= 0 || authority.indexOf('[') >= 0) {
        return {false, "STT URL contains an unsupported HTTPS authority"};
    }
    String host = authority;
    std::uint16_t port = 443;
    const int colon = authority.lastIndexOf(':');
    if (colon >= 0) {
        host = authority.substring(0, colon);
        const String portText = authority.substring(colon + 1);
        if (host.isEmpty() || portText.isEmpty()) {
            return {false, "STT URL contains an invalid host or port"};
        }
        for (std::size_t index = 0; index < portText.length(); ++index) {
            if (portText[index] < '0' || portText[index] > '9') {
                return {false, "STT URL port must contain only digits"};
            }
        }
        const long parsedPort = portText.toInt();
        if (parsedPort < 1 || parsedPort > 65535) {
            return {false, "STT URL port must be between 1 and 65535"};
        }
        port = static_cast<std::uint16_t>(parsedPort);
    }
    endpoint = {host, port, path};
    return {true, ""};
}

OperationResult writeFully(WiFiClientSecure& client,
                           const std::uint8_t* data,
                           std::size_t size,
                           const char* stage,
                           const CancelCallback& isCancelled)
{
    std::size_t offset = 0;
    std::uint32_t lastProgress = millis();
    while (offset < size) {
        if (isCancelled()) {
            return {false, "STT request canceled by user"};
        }
        if (client.available() > 0) {
            return {false, String("STT server responded during ") + stage + " at " + offset +
                               "/" + size + " bytes"};
        }
        if (!client.connected()) {
            return {false, String("STT connection closed during ") + stage + " at " + offset +
                               "/" + size + " bytes"};
        }
        const std::size_t written = client.write(data + offset, size - offset);
        if (written > 0) {
            offset += written;
            lastProgress = millis();
            continue;
        }
        if (millis() - lastProgress >= kHttpTimeoutMs) {
            return {false, String("STT upload timed out during ") + stage + " at " + offset +
                               "/" + size + " bytes"};
        }
        delay(2);
    }
    return {true, ""};
}

OperationResult writeString(WiFiClientSecure& client,
                            const String& value,
                            const char* stage,
                            const CancelCallback& isCancelled)
{
    return writeFully(client,
                      reinterpret_cast<const std::uint8_t*>(value.c_str()),
                      value.length(),
                      stage,
                      isCancelled);
}

OperationResult readHttpLine(WiFiClientSecure& client,
                             String& line,
                             const CancelCallback& isCancelled)
{
    line = "";
    const std::uint32_t startedAt = millis();
    while (millis() - startedAt < kHttpTimeoutMs) {
        if (isCancelled()) {
            return {false, "STT request canceled by user"};
        }
        while (client.available() > 0) {
            const int value = client.read();
            if (value < 0) {
                break;
            }
            if (value == '\n') {
                if (line.endsWith("\r")) {
                    line.remove(line.length() - 1);
                }
                return {true, ""};
            }
            if (line.length() >= 1024) {
                return {false, "STT HTTP header line exceeded 1024 bytes"};
            }
            line += static_cast<char>(value);
        }
        if (!client.connected() && client.available() == 0) {
            return {false, "STT connection closed while reading HTTP headers"};
        }
        delay(2);
    }
    return {false, "STT HTTP response header timed out"};
}

OperationResult readExactBody(WiFiClientSecure& client,
                              String& body,
                              std::size_t size,
                              const CancelCallback& isCancelled)
{
    if (body.length() + size > kMaximumResponseBytes) {
        return {false, "STT HTTP response exceeded 8192 bytes"};
    }
    std::array<std::uint8_t, 512> buffer = {};
    std::size_t remaining = size;
    std::uint32_t lastProgress = millis();
    while (remaining > 0) {
        if (isCancelled()) {
            return {false, "STT request canceled by user"};
        }
        const std::size_t available = static_cast<std::size_t>(std::max(client.available(), 0));
        if (available > 0) {
            const std::size_t requested = std::min({remaining, available, buffer.size()});
            const int readCount = client.read(buffer.data(), requested);
            if (readCount > 0) {
                body.concat(reinterpret_cast<const char*>(buffer.data()), readCount);
                remaining -= static_cast<std::size_t>(readCount);
                lastProgress = millis();
                continue;
            }
        }
        if ((!client.connected() && client.available() == 0) ||
            millis() - lastProgress >= kHttpTimeoutMs) {
            return {false, "STT connection ended before the complete HTTP body arrived"};
        }
        delay(2);
    }
    return {true, ""};
}

OperationResult readChunkedBody(WiFiClientSecure& client,
                                String& body,
                                const CancelCallback& isCancelled)
{
    while (true) {
        String sizeLine;
        const OperationResult lineResult = readHttpLine(client, sizeLine, isCancelled);
        if (!lineResult.success) {
            return lineResult;
        }
        const int extension = sizeLine.indexOf(';');
        if (extension >= 0) {
            sizeLine = sizeLine.substring(0, extension);
        }
        char* end = nullptr;
        const unsigned long chunkSize = strtoul(sizeLine.c_str(), &end, 16);
        if (end == sizeLine.c_str() || *end != '\0') {
            return {false, "STT response contains an invalid HTTP chunk size"};
        }
        if (chunkSize == 0) {
            do {
                const OperationResult trailerResult = readHttpLine(
                    client, sizeLine, isCancelled);
                if (!trailerResult.success) {
                    return trailerResult;
                }
            } while (!sizeLine.isEmpty());
            return {true, ""};
        }
        const OperationResult bodyResult = readExactBody(
            client, body, chunkSize, isCancelled);
        if (!bodyResult.success) {
            return bodyResult;
        }
        const OperationResult endingResult = readHttpLine(client, sizeLine, isCancelled);
        if (!endingResult.success || !sizeLine.isEmpty()) {
            return {false, "STT response chunk is missing its CRLF terminator"};
        }
    }
}

UploadResult readHttpResponse(WiFiClientSecure& client,
                              const CancelCallback& isCancelled)
{
    String line;
    OperationResult result = readHttpLine(client, line, isCancelled);
    if (!result.success) {
        return {-1, "", result.error};
    }
    const int firstSpace = line.indexOf(' ');
    const int secondSpace = firstSpace >= 0 ? line.indexOf(' ', firstSpace + 1) : -1;
    if (!line.startsWith("HTTP/") || firstSpace < 0) {
        return {-1, "", "STT server returned an invalid HTTP status line"};
    }
    const String statusText = secondSpace >= 0
        ? line.substring(firstSpace + 1, secondSpace)
        : line.substring(firstSpace + 1);
    const int status = statusText.toInt();
    if (status < 100 || status > 599) {
        return {-1, "", "STT server returned an invalid HTTP status code"};
    }

    int contentLength = -1;
    bool chunked = false;
    while (true) {
        result = readHttpLine(client, line, isCancelled);
        if (!result.success) {
            return {-1, "", result.error};
        }
        if (line.isEmpty()) {
            break;
        }
        String lower = line;
        lower.toLowerCase();
        if (lower.startsWith("content-length:")) {
            const String lengthText = line.substring(line.indexOf(':') + 1);
            contentLength = lengthText.toInt();
            if (contentLength < 0) {
                return {-1, "", "STT response contains an invalid Content-Length"};
            }
        } else if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") >= 0) {
            chunked = true;
        }
    }

    if (status >= 100 && status < 200) {
        return {status, "", ""};
    }

    String body;
    body.reserve(contentLength > 0
        ? std::min<std::size_t>(contentLength, kMaximumResponseBytes)
        : 512U);
    if (chunked) {
        result = readChunkedBody(client, body, isCancelled);
    } else if (contentLength >= 0) {
        result = readExactBody(
            client, body, static_cast<std::size_t>(contentLength), isCancelled);
    } else {
        std::array<std::uint8_t, 512> buffer = {};
        std::uint32_t lastProgress = millis();
        while (client.connected() || client.available() > 0) {
            if (isCancelled()) {
                return {-1, "", "STT request canceled by user"};
            }
            const int available = client.available();
            if (available <= 0) {
                if (millis() - lastProgress >= kHttpTimeoutMs) {
                    return {-1, "", "STT HTTP response body timed out"};
                }
                delay(2);
                continue;
            }
            const std::size_t requested = std::min<std::size_t>(
                static_cast<std::size_t>(available), buffer.size());
            const int readCount = client.read(buffer.data(), requested);
            if (readCount > 0) {
                if (body.length() + static_cast<std::size_t>(readCount) > kMaximumResponseBytes) {
                    return {-1, "", "STT HTTP response exceeded 8192 bytes"};
                }
                body.concat(reinterpret_cast<const char*>(buffer.data()), readCount);
                lastProgress = millis();
            } else {
                delay(2);
            }
        }
        result = {true, ""};
    }
    return result.success ? UploadResult{status, body, ""}
                          : UploadResult{-1, "", result.error};
}

UploadResult uploadVoiceFile(const Settings& settings,
                             File& file,
                             const CancelCallback& isCancelled)
{
    HttpsEndpoint endpoint;
    const OperationResult endpointResult = parseHttpsEndpoint(transcriptionUrl(settings), endpoint);
    if (!endpointResult.success) {
        return {-1, "", endpointResult.error};
    }
    const String boundary = "CardputerVoice" + String(esp_random(), HEX);
    const String prefix = multipartPrefix(settings, boundary);
    const String suffix = "\r\n--" + boundary + "--\r\n";
    const std::size_t contentLength = prefix.length() + file.size() + suffix.length();
    const String hostHeader = endpoint.port == 443
        ? endpoint.host
        : endpoint.host + ":" + String(endpoint.port);
    String headers;
    headers.reserve(420 + settings.sttApiKey.length());
    headers += "POST " + endpoint.path + " HTTP/1.1\r\n";
    headers += "Host: " + hostHeader + "\r\n";
    headers += "Authorization: Bearer " + settings.sttApiKey + "\r\n";
    headers += "User-Agent: CardMind/1.9\r\n";
    headers += "Accept: application/json\r\n";
    headers += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    headers += "Content-Length: " + String(contentLength) + "\r\n";
    headers += "Expect: 100-continue\r\n";
    headers += "Connection: close\r\n\r\n";

    WiFiClientSecure client;
    client.setCACert(kGtsRootR4);
    client.setHandshakeTimeout(20);
    client.setTimeout(kHttpTimeoutMs);
    if (!client.connect(endpoint.host.c_str(), endpoint.port, 20000)) {
        return {-1, "", "Failed to establish the verified STT TLS connection"};
    }
    OperationResult writeResult = writeString(
        client, headers, "request headers", isCancelled);
    if (writeResult.success) {
        const std::uint32_t acknowledgementDeadline = millis() + 1500U;
        while (client.available() == 0 && client.connected() &&
               static_cast<std::int32_t>(acknowledgementDeadline - millis()) > 0) {
            if (isCancelled()) {
                client.stop();
                return {-1, "", "STT request canceled by user"};
            }
            delay(2);
        }
        if (client.available() > 0) {
            const UploadResult acknowledgement = readHttpResponse(client, isCancelled);
            if (acknowledgement.status != 100) {
                client.stop();
                return acknowledgement;
            }
        } else if (!client.connected()) {
            client.stop();
            return {-1, "", "STT connection closed before the upload acknowledgement"};
        }
    }
    if (writeResult.success) {
        writeResult = writeString(client, prefix, "multipart metadata", isCancelled);
    }
    constexpr std::size_t uploadBufferSize = 1024;
    std::unique_ptr<std::uint8_t[]> buffer(new (std::nothrow) std::uint8_t[uploadBufferSize]);
    if (!buffer) {
        client.stop();
        return {-1, "", "Not enough heap for the 1024-byte STT upload buffer"};
    }
    std::size_t fileBytes = 0;
    while (writeResult.success && fileBytes < file.size()) {
        if (isCancelled()) {
            writeResult = {false, "STT request canceled by user"};
            break;
        }
        const std::size_t requested = std::min(uploadBufferSize, file.size() - fileBytes);
        const std::size_t readCount = file.read(buffer.get(), requested);
        if (readCount != requested) {
            writeResult = {false, String("microSD read ended at ") + fileBytes + "/" + file.size() +
                                      " WAV bytes"};
            break;
        }
        writeResult = writeFully(
            client, buffer.get(), readCount, "WAV upload", isCancelled);
        if (!writeResult.success) {
            writeResult.error += String(" after ") + fileBytes + "/" + file.size() +
                                 " WAV bytes";
        }
        fileBytes += readCount;
    }
    if (writeResult.success) {
        writeResult = writeString(
            client, suffix, "multipart terminator", isCancelled);
    }
    if (!writeResult.success) {
        if (client.available() > 0) {
            const UploadResult earlyResponse = readHttpResponse(client, isCancelled);
            client.stop();
            return earlyResponse;
        }
        client.stop();
        return {-1, "", writeResult.error};
    }
    const UploadResult response = readHttpResponse(client, isCancelled);
    client.stop();
    return response;
}

}  // namespace

TranscriptionResult transcribeVoiceRecording(const Settings& settings,
                                              const CancelCallback& isCancelled)
{
    if (!voiceSettingsAreComplete(settings)) {
        return {false, {}, "Voice STT is not configured; use Fn+4 > Web setup"};
    }
    String lastError = "Voice transcription did not run";
    for (int attempt = 1; attempt <= kMaximumAttempts; ++attempt) {
        if (isCancelled()) {
            return {false, {}, "STT request canceled by user"};
        }
        File file = SD.open(voiceRecordingPath(), FILE_READ);
        if (!file) {
            return {false, {}, "Failed to open temporary voice.wav from microSD"};
        }
        const UploadResult upload = uploadVoiceFile(settings, file, isCancelled);
        file.close();

        if (upload.status == HTTP_CODE_OK) {
            JsonDocument document;
            const DeserializationError jsonError = deserializeJson(document, upload.body);
            if (jsonError) {
                return {false, {}, String("Invalid STT JSON response: ") + jsonError.c_str()};
            }
            const char* text = document["text"].as<const char*>();
            if (text == nullptr || text[0] == '\0') {
                return {false, {}, "STT response is missing a non-empty text field"};
            }
            const std::string transcription(text);
            if (!isValidUtf8(transcription)) {
                return {false, {}, "STT response text is not valid UTF-8"};
            }
            return {true, transcription, ""};
        }

        lastError = upload.status > 0
            ? "STT HTTP " + String(upload.status) + ": " + boundedErrorMessage(upload.body)
            : upload.error;
        const bool retry = isTransientStatus(upload.status) && attempt < kMaximumAttempts;
        if (!retry) {
            return {false, {}, lastError};
        }
        Serial.printf("WARN event=stt_retry attempt=%d status=%d\n", attempt, upload.status);
        const std::uint32_t retryAt = millis() +
            static_cast<std::uint32_t>(1000U << (attempt - 1)) + (esp_random() % 350U);
        while (static_cast<std::int32_t>(retryAt - millis()) > 0) {
            if (isCancelled()) {
                return {false, {}, "STT request canceled by user"};
            }
            delay(10);
        }
    }
    return {false, {}, lastError};
}

OperationResult probeDefaultSttTls()
{
    WiFiClientSecure client;
    client.setCACert(kGtsRootR4);
    client.setHandshakeTimeout(20);
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(20000);
    if (!http.begin(client, kDefaultModelsUrl)) {
        return {false, "Failed to initialize the Groq TLS probe"};
    }
    const int status = http.GET();
    http.end();
    if (status == HTTP_CODE_UNAUTHORIZED || status == HTTP_CODE_FORBIDDEN) {
        return {true, ""};
    }
    if (status <= 0) {
        return {false, "Groq TLS probe failed: " + HTTPClient::errorToString(status)};
    }
    return {false, "Groq TLS probe expected HTTP 401/403 without a key, received " + String(status)};
}

OperationResult validateSttCredentials(const Settings& settings)
{
    if (!voiceSettingsAreComplete(settings)) {
        return {false, "Voice STT is not configured; use Fn+4 > Web setup"};
    }
    String lastError = "STT credential validation did not run";
    for (int attempt = 1; attempt <= kMaximumAttempts; ++attempt) {
        WiFiClientSecure client;
        client.setCACert(kGtsRootR4);
        client.setHandshakeTimeout(20);
        HTTPClient http;
        http.setReuse(false);
        http.setTimeout(20000);
        if (!http.begin(client, modelsUrl(settings))) {
            return {false, "Failed to initialize STT credential validation"};
        }
        http.addHeader("Authorization", "Bearer " + settings.sttApiKey);
        http.addHeader("Accept", "application/json");
        const int status = http.GET();
        if (status == HTTP_CODE_OK) {
            http.end();
            return {true, ""};
        }
        const String body = status > 0 ? http.getString() : String();
        http.end();
        lastError = status > 0
            ? "STT auth HTTP " + String(status) + ": " + boundedErrorMessage(body)
            : "STT auth request failed: " + HTTPClient::errorToString(status);
        const bool retry = isTransientStatus(status) && attempt < kMaximumAttempts;
        if (!retry) {
            return {false, lastError};
        }
        Serial.printf("WARN event=stt_auth_retry attempt=%d status=%d\n", attempt, status);
        delay(static_cast<std::uint32_t>(1000U << (attempt - 1)) + (esp_random() % 350U));
    }
    return {false, lastError};
}

}  // namespace cardputer
