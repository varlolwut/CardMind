#include "web_console_transport.h"

#include "web_console_asset.h"

#include <algorithm>
#include <cstring>

namespace cardputer {
namespace {

constexpr std::size_t kJsonTransportBufferBytes = 1024;

class BufferedJsonWriter : public Print {
public:
    explicit BufferedJsonWriter(Client& client)
        : client_(client)
    {
    }

    std::size_t write(std::uint8_t value) override
    {
        return write(&value, 1);
    }

    std::size_t write(const std::uint8_t* data, std::size_t size) override
    {
        if (failed_ || data == nullptr || size == 0) {
            return 0;
        }
        std::size_t accepted = 0;
        while (accepted < size) {
            const std::size_t available = sizeof(buffer_) - buffered_;
            const std::size_t copied = std::min(available, size - accepted);
            std::memcpy(buffer_ + buffered_, data + accepted, copied);
            buffered_ += copied;
            accepted += copied;
            if (buffered_ == sizeof(buffer_) && !flushBuffer()) {
                return accepted;
            }
        }
        return accepted;
    }

    bool finish()
    {
        return !failed_ && flushBuffer();
    }

private:
    bool flushBuffer()
    {
        std::size_t written = 0;
        while (written < buffered_) {
            const std::size_t chunk = client_.write(buffer_ + written,
                                                    buffered_ - written);
            if (chunk == 0) {
                failed_ = true;
                return false;
            }
            written += chunk;
        }
        buffered_ = 0;
        return true;
    }

    Client& client_;
    std::uint8_t buffer_[kJsonTransportBufferBytes] = {};
    std::size_t buffered_ = 0;
    bool failed_ = false;
};

}  // namespace

void sendWebJson(WebServer& server, int statusCode, const JsonDocument& document)
{
    const std::size_t expectedBytes = measureJson(document);
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(expectedBytes);
    server.send(statusCode, "application/json; charset=utf-8", "");
    BufferedJsonWriter writer(server.client());
    const std::size_t serializedBytes = serializeJson(document, writer);
    if (serializedBytes != expectedBytes || !writer.finish()) {
        Serial.printf(
            "WEB_TRANSPORT result=failed expected_bytes=%u serialized_bytes=%u\n",
            static_cast<unsigned int>(expectedBytes),
            static_cast<unsigned int>(serializedBytes));
        server.client().stop();
    }
}

void sendWebJsonError(WebServer& server, int statusCode, const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    sendWebJson(server, statusCode, document);
}

void sendWebConsolePage(WebServer& server)
{
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Encoding", "gzip");
    server.setContentLength(kWebConsolePageGzipSize);
    server.send(200, "text/html; charset=utf-8", "");
    server.sendContent_P(reinterpret_cast<PGM_P>(kWebConsolePageGzip),
                         kWebConsolePageGzipSize);
}

void sendWebSse(WebServer& server, const char* type, const std::string& delta,
                const String& error)
{
    JsonDocument document;
    document["type"] = type;
    if (!delta.empty()) {
        document["delta"] = delta;
    }
    if (!error.isEmpty()) {
        document["error"] = error;
    }
    String output;
    output.reserve(delta.size() + error.length() + 48);
    serializeJson(document, output);
    server.sendContent("data:" + output + "\n\n");
}

}  // namespace cardputer
