#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <cstddef>
#include <cstdint>

namespace cardputer {

void beginWebRequestMetrics(const char* endpoint);
void finishWebRequestMetrics();
void addWebResponseMetrics(WebServer& server, std::size_t responseBytes);
void recordWebSdRead(std::uint32_t durationMs);
void recordWebSdWrite(std::uint32_t durationMs);
void setWebDiagnosticsEnabled(bool enabled);
bool webDiagnosticsEnabled();

}  // namespace cardputer
