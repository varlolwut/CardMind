#include "web_console_transport.h"

#include "web_console_asset.h"

namespace cardputer {

void sendWebJson(WebServer& server, int statusCode, const JsonDocument& document)
{
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(measureJson(document));
    server.send(statusCode, "application/json; charset=utf-8", "");
    serializeJson(document, server.client());
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
