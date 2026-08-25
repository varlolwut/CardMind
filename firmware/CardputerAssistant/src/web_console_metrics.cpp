#include "web_console_metrics.h"

#include <esp_heap_caps.h>

namespace cardputer {
namespace {

struct WebRequestMetrics {
    const char* endpoint;
    std::uint32_t startedAt;
    std::uint32_t sdReadMs;
    std::uint32_t sdWriteMs;
    std::size_t responseBytes;
    bool active;
};

WebRequestMetrics requestMetrics = {"", 0, 0, 0, 0, false};
bool diagnosticsEnabled = false;

void addDuration(std::uint32_t& total, std::uint32_t durationMs)
{
    total += durationMs;
}

}  // namespace

void beginWebRequestMetrics(const char* endpoint)
{
    requestMetrics = {endpoint, millis(), 0, 0, 0, true};
}

void finishWebRequestMetrics()
{
    if (!requestMetrics.active) {
        return;
    }
    if (diagnosticsEnabled) {
        Serial.printf(
            "WEB_METRIC endpoint=%s duration_ms=%u response_bytes=%u "
            "sd_read_ms=%u sd_write_ms=%u heap=%u min_heap=%u largest_heap=%u\n",
            requestMetrics.endpoint,
            static_cast<unsigned int>(millis() - requestMetrics.startedAt),
            static_cast<unsigned int>(requestMetrics.responseBytes),
            static_cast<unsigned int>(requestMetrics.sdReadMs),
            static_cast<unsigned int>(requestMetrics.sdWriteMs),
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(ESP.getMinFreeHeap()),
            static_cast<unsigned int>(
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    }
    requestMetrics.active = false;
}

void addWebResponseMetrics(WebServer& server, std::size_t responseBytes)
{
    if (!requestMetrics.active) {
        return;
    }
    requestMetrics.responseBytes = responseBytes;
    server.sendHeader(
        "Server-Timing",
        "cardmind;dur=" + String(millis() - requestMetrics.startedAt));
    server.sendHeader("X-CardMind-Response-Bytes", String(responseBytes));
    server.sendHeader("X-CardMind-Free-Heap", String(ESP.getFreeHeap()));
}

void recordWebSdRead(std::uint32_t durationMs)
{
    if (requestMetrics.active) {
        addDuration(requestMetrics.sdReadMs, durationMs);
    }
}

void recordWebSdWrite(std::uint32_t durationMs)
{
    if (requestMetrics.active) {
        addDuration(requestMetrics.sdWriteMs, durationMs);
    }
}

void setWebDiagnosticsEnabled(bool enabled)
{
    diagnosticsEnabled = enabled;
}

bool webDiagnosticsEnabled()
{
    return diagnosticsEnabled;
}

}  // namespace cardputer
