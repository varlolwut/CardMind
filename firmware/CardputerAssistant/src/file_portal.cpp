#include "file_portal.h"

#include "file_workspace.h"
#include "storage.h"
#include "text_utils.h"
#include "ui.h"

#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstdint>
#include <cstdio>

namespace cardputer {
namespace {

WebServer server(80);
bool restartPending = false;
std::uint32_t restartAt = 0;

String htmlEscape(const String& value)
{
    String result;
    for (std::size_t index = 0; index < value.length(); ++index) {
        switch (value[index]) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += value[index]; break;
        }
    }
    return result;
}

String fileMimeType(const String& name)
{
    if (name.endsWith(".md") || name.endsWith(".txt") || name.endsWith(".csv")) {
        return "text/plain; charset=utf-8";
    }
    if (name.endsWith(".json")) {
        return "application/json; charset=utf-8";
    }
    if (name.endsWith(".html")) {
        return "text/html; charset=utf-8";
    }
    if (name.endsWith(".svg")) {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

String filesPage(const String& error)
{
    const WorkspaceFilesResult files = listWorkspaceFiles();
    String page =
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Cardputer Files</title><style>"
        "body{font:16px system-ui;background:#101522;color:#eef;max-width:620px;margin:32px auto;padding:16px}"
        "a{color:#67e8d2}li{padding:9px 0}.card{background:#1b2436;padding:20px;border-radius:14px}"
        "button{padding:11px 16px;border:0;border-radius:8px;background:#55d6be;color:#08131a;font-weight:700}"
        ".error{background:#642a35;padding:10px;border-radius:8px}.note{color:#aebbd1}</style></head><body>"
        "<div class='card'><h1>Cardputer files</h1>"
        "<p class='note'>Files created by the model in the isolated microSD workspace.</p>";
    if (!error.isEmpty()) {
        page += "<p class='error'>" + htmlEscape(error) + "</p>";
    }
    if (!files.success) {
        page += "<p class='error'>" + htmlEscape(files.error) + "</p>";
    } else if (files.files.empty()) {
        page += "<p>No files yet.</p>";
    } else {
        page += "<ul>";
        for (const auto& file : files.files) {
            page += "<li><a href='/download?name=" + file.name + "'>" +
                    htmlEscape(file.name) + "</a> — " + String(file.size) + " bytes</li>";
        }
        page += "</ul>";
    }
    page += "<form method='post' action='/restart'><button type='submit'>Restart assistant</button>"
            "</form></div></body></html>";
    return page;
}

void sendFilesPage()
{
    server.send(200, "text/html; charset=utf-8", filesPage(""));
}

void downloadFile()
{
    const String name = server.arg("name");
    if (!isValidWorkspaceFilename(name.c_str())) {
        server.send(400, "text/html; charset=utf-8", filesPage("Invalid filename"));
        return;
    }
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        server.send(404, "text/html; charset=utf-8", filesPage("File not found"));
        return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    const std::size_t sent = server.streamFile(file, fileMimeType(name));
    const std::size_t expected = file.size();
    file.close();
    if (sent != expected) {
        Serial.println("ERROR event=file_download result=incomplete");
    } else {
        Serial.println("INFO event=file_download result=ok");
    }
}

String macSuffix()
{
    const std::uint64_t mac = ESP.getEfuseMac();
    char suffix[5] = {};
    std::snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned int>(mac & 0xFFFF));
    return String(suffix);
}

}  // namespace

[[noreturn]] void runFilePortal()
{
    const OperationResult workspace = initializeFileWorkspace();
    if (!workspace.success) {
        showFatalError(workspace.error);
        Serial.println("FATAL event=file_portal_workspace result=failed");
        while (true) {
            delay(1000);
        }
    }
    String password;
    const OperationResult passwordResult = loadSetupAccessPointPassword(password);
    if (!passwordResult.success || password.isEmpty()) {
        showFatalError(passwordResult.success
                           ? String("Setup access-point password is missing")
                           : passwordResult.error);
        Serial.println("FATAL event=file_portal_password result=failed");
        while (true) {
            delay(1000);
        }
    }
    const String accessPointName = "Cardputer-Files-" + macSuffix();
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(accessPointName.c_str(), password.c_str())) {
        showFatalError("Failed to start protected files access point");
        Serial.println("FATAL event=file_portal_ap result=failed");
        while (true) {
            delay(1000);
        }
    }
    showFilesPortal(accessPointName, password);
    server.on("/", HTTP_GET, sendFilesPage);
    server.on("/download", HTTP_GET, downloadFile);
    server.on("/restart", HTTP_POST, []() {
        server.send(200, "text/html; charset=utf-8",
                    "<!doctype html><meta charset='utf-8'><h1>Restarting...</h1>");
        restartPending = true;
        restartAt = millis() + 1000;
    });
    server.onNotFound([]() {
        server.send(404, "text/plain; charset=utf-8", "Not found");
    });
    server.begin();
    Serial.println("FILE_PORTAL result=ready address=192.168.4.1");
    while (true) {
        server.handleClient();
        if (restartPending && static_cast<std::int32_t>(millis() - restartAt) >= 0) {
            ESP.restart();
        }
        delay(2);
    }
}

}  // namespace cardputer
