#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include <string>

namespace cardputer {

void sendWebJson(WebServer& server, int statusCode, const JsonDocument& document);
void sendWebJsonError(WebServer& server, int statusCode, const String& error);
void sendWebConsolePage(WebServer& server);
void sendWebSse(WebServer& server, const char* type, const std::string& delta,
                const String& error);

}  // namespace cardputer
