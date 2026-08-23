#include "web_search_client.h"

#include "storage.h"
#include "text_utils.h"
#include "tls_roots.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <array>

namespace cardputer {
namespace {

constexpr const char* kSearchPath = "/search";
constexpr const char* kContentsPath = "/contents";
constexpr const char* kWebResponsePath = "/assistant-web-response.tmp";
constexpr const char* kSearchCachePath = "/assistant/search-sources.json";
constexpr const char* kSearchCacheTemporaryPath = "/assistant/search-sources.tmp";
constexpr std::uint16_t kHttpTimeoutMs = 30000;
constexpr int kMaximumAttempts = 3;
constexpr std::size_t kMaximumResponseBytes = 24576;
constexpr std::size_t kMaximumSnippetBytes = 700;
constexpr std::size_t kMaximumErrorLength = 160;
constexpr std::size_t kMaximumQueryBytes = 400;
constexpr std::size_t kMaximumUrlBytes = 1200;

bool isTransientStatus(int status)
{
    return status <= 0 || status == 429 || status == 500 || status == 502 ||
           status == 503 || status == 504;
}

String endpointUrl(const Settings& settings, const String& path)
{
    String base = settings.webSearchBaseUrl;
    if (base.endsWith(kSearchPath)) {
        base.remove(base.length() - String(kSearchPath).length());
    } else if (base.endsWith(kContentsPath)) {
        base.remove(base.length() - String(kContentsPath).length());
    }
    return base + path;
}

String boundedError(const String& body)
{
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(document, body);
    String message;
    if (!jsonError) {
        const char* detailError = document["detail"]["error"].as<const char*>();
        const char* detail = document["detail"].as<const char*>();
        const char* errorMessage = document["error"]["message"].as<const char*>();
        const char* topLevelError = document["error"].as<const char*>();
        const char* topLevelMessage = document["message"].as<const char*>();
        if (detailError != nullptr) {
            message = detailError;
        } else if (detail != nullptr) {
            message = detail;
        } else if (errorMessage != nullptr) {
            message = errorMessage;
        } else if (topLevelError != nullptr) {
            message = topLevelError;
        } else if (topLevelMessage != nullptr) {
            message = topLevelMessage;
        }
    }
    if (message.isEmpty()) {
        message = "response did not contain a recognized error field";
    }
    message.replace("\r", " ");
    message.replace("\n", " ");
    if (message.length() > kMaximumErrorLength) {
        message = message.substring(0, kMaximumErrorLength) + "...";
    }
    return message;
}

std::string boundedUtf8(const char* value, std::size_t maximumBytes)
{
    std::string result(value == nullptr ? "" : value);
    if (result.size() <= maximumBytes) {
        return result;
    }
    result.resize(maximumBytes);
    while (!result.empty() && !isValidUtf8(result)) {
        result.pop_back();
    }
    return result;
}

ToolExecutionResult failure(const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    String output;
    serializeJson(document, output);
    return {false, output.c_str(), error};
}

OperationResult cacheSearchSources(const std::string& output)
{
    if (output.empty() || output.size() > 8192) {
        return {false, "Search source cache must contain 1 to 8192 bytes"};
    }
    if (SD.exists(kSearchCacheTemporaryPath) && !SD.remove(kSearchCacheTemporaryPath)) {
        return {false, "Failed to remove the previous temporary search cache"};
    }
    File file = SD.open(kSearchCacheTemporaryPath, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create the search source cache on microSD"};
    }
    const std::size_t written = file.write(
        reinterpret_cast<const std::uint8_t*>(output.data()), output.size());
    file.flush();
    file.close();
    if (written != output.size()) {
        SD.remove(kSearchCacheTemporaryPath);
        return {false, "Failed to write the complete search source cache"};
    }
    if (SD.exists(kSearchCachePath) && !SD.remove(kSearchCachePath)) {
        SD.remove(kSearchCacheTemporaryPath);
        return {false, "Failed to replace the previous search source cache"};
    }
    if (!SD.rename(kSearchCacheTemporaryPath, kSearchCachePath)) {
        return {false, "Failed to commit the search source cache"};
    }
    return {true, ""};
}

OperationResult requestJsonToMicroSd(const Settings& settings,
                                     const String& path,
                                     const String& payload,
                                     const String& operation,
                                     const CancelCallback& isCancelled)
{
    if (SD.exists(kWebResponsePath) && !SD.remove(kWebResponsePath)) {
        return {false, "Failed to remove the previous temporary web response from microSD"};
    }
    String lastError = operation + " request was not attempted";
    for (int attempt = 1; attempt <= kMaximumAttempts; ++attempt) {
        if (isCancelled()) {
            return {false, operation + " canceled by user"};
        }
        WiFiClientSecure client;
        client.setCACert(kGtsRootR4);
        client.setHandshakeTimeout(20);
        HTTPClient http;
        http.setReuse(false);
        http.setTimeout(kHttpTimeoutMs);
        if (!http.begin(client, endpointUrl(settings, path))) {
            return {false, "Failed to initialize HTTPS " + operation + " request"};
        }
        http.addHeader("x-api-key", settings.webSearchApiKey);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");
        const char* responseHeaderKeys[] = {"Content-Type"};
        http.collectHeaders(responseHeaderKeys, 1);
        const int status = http.POST(payload);
        if (status == HTTP_CODE_OK) {
            const int declaredSize = http.getSize();
            if (declaredSize > static_cast<int>(kMaximumResponseBytes)) {
                http.end();
                return {false, operation + " response exceeded 24576 bytes"};
            }
            String contentType = http.header("Content-Type");
            contentType.toLowerCase();
            if (!contentType.startsWith("application/json")) {
                http.end();
                return {false, operation + " returned HTTP 200 without application/json content"};
            }
            File responseFile = SD.open(kWebResponsePath, FILE_WRITE);
            if (!responseFile) {
                http.end();
                return {false, "Failed to create a temporary web response on microSD"};
            }
            NetworkClient* stream = http.getStreamPtr();
            std::array<std::uint8_t, 2048> buffer = {};
            std::size_t bytesWritten = 0;
            std::uint32_t lastDataAt = millis();
            OperationResult transfer = {true, ""};
            while (declaredSize < 0 || bytesWritten < static_cast<std::size_t>(declaredSize)) {
                if (isCancelled()) {
                    transfer = {false, operation + " canceled by user"};
                    break;
                }
                const int available = stream->available();
                if (available <= 0) {
                    if (!http.connected()) {
                        break;
                    }
                    if (millis() - lastDataAt >= kHttpTimeoutMs) {
                        transfer = {false, operation + " response body timed out"};
                        break;
                    }
                    delay(2);
                    continue;
                }
                const std::size_t requested = std::min<std::size_t>(
                    buffer.size(), static_cast<std::size_t>(available));
                const std::size_t received = stream->readBytes(buffer.data(), requested);
                if (received == 0 || bytesWritten + received > kMaximumResponseBytes ||
                    responseFile.write(buffer.data(), received) != received) {
                    transfer = {false, bytesWritten + received > kMaximumResponseBytes
                        ? operation + " response exceeded 24576 bytes"
                        : "Failed to stream the web response to microSD"};
                    break;
                }
                bytesWritten += received;
                lastDataAt = millis();
            }
            responseFile.close();
            http.end();
            if (!transfer.success) {
                SD.remove(kWebResponsePath);
                return transfer;
            }
            if (declaredSize >= 0 &&
                bytesWritten != static_cast<std::size_t>(declaredSize)) {
                SD.remove(kWebResponsePath);
                return {false, operation + " response ended before all bytes arrived"};
            }
            responseFile = SD.open(kWebResponsePath, FILE_READ);
            if (!responseFile) {
                SD.remove(kWebResponsePath);
                return {false, "Failed to reopen the temporary web response from microSD"};
            }
            const std::size_t responseBytes = responseFile.size();
            responseFile.close();
            if (responseBytes == 0 || responseBytes > kMaximumResponseBytes) {
                SD.remove(kWebResponsePath);
                return {false, responseBytes == 0
                    ? operation + " returned an empty response body"
                    : operation + " response exceeded 24576 bytes"};
            }
            return {true, ""};
        }
        const String responseBody = status > 0 ? http.getString() : String();
        lastError = status > 0
            ? operation + " HTTP " + String(status) + ": " + boundedError(responseBody)
            : operation + " transport failed: " + HTTPClient::errorToString(status);
        const bool retry = isTransientStatus(status) && attempt < kMaximumAttempts;
        http.end();
        if (!retry) {
            return {false, lastError};
        }
        Serial.printf("WARN event=web_retry operation=%s attempt=%d status=%d\n",
                      operation.c_str(), attempt, status);
        const std::uint32_t retryAt = millis() +
            static_cast<std::uint32_t>(1000U << (attempt - 1)) + (esp_random() % 350U);
        while (static_cast<std::int32_t>(retryAt - millis()) > 0) {
            if (isCancelled()) {
                return {false, operation + " canceled by user"};
            }
            delay(10);
        }
    }
    return {false, lastError};
}

ToolExecutionResult parseSearchResponse(Stream& body, const String& query)
{
    JsonDocument response;
    const DeserializationError jsonError = deserializeJson(response, body);
    if (jsonError) {
        return failure(String("Web search returned invalid JSON: ") + jsonError.c_str());
    }
    const JsonArrayConst results = response["results"].as<JsonArrayConst>();
    if (results.isNull()) {
        return failure("Web search response is missing the results array");
    }
    JsonDocument outputDocument;
    outputDocument["ok"] = true;
    outputDocument["query"] = query;
    JsonArray outputResults = outputDocument["results"].to<JsonArray>();
    std::size_t resultCount = 0;
    for (const JsonObjectConst result : results) {
        if (resultCount >= 5) {
            break;
        }
        const char* title = result["title"].as<const char*>();
        const char* url = result["url"].as<const char*>();
        const JsonArrayConst highlights = result["highlights"].as<JsonArrayConst>();
        const char* content = !highlights.isNull() && highlights.size() > 0
            ? highlights[0].as<const char*>()
            : result["text"].as<const char*>();
        if (content == nullptr) {
            content = result["summary"].as<const char*>();
        }
        if (title == nullptr || url == nullptr || title[0] == '\0' || url[0] == '\0') {
            return failure("Web search result is missing title or url");
        }
        const String resultUrl(url);
        if (!resultUrl.startsWith("https://") && !resultUrl.startsWith("http://")) {
            return failure("Web search result contains an invalid source URL");
        }
        JsonObject item = outputResults.add<JsonObject>();
        item["title"] = boundedUtf8(title, 300);
        item["url"] = url;
        item["content"] = boundedUtf8(content == nullptr ? "" : content, kMaximumSnippetBytes);
        ++resultCount;
    }
    String output;
    serializeJson(outputDocument, output);
    return {true, output.c_str(), ""};
}

ToolExecutionResult parseFetchResponse(Stream& body, const String& requestedUrl)
{
    JsonDocument response;
    const DeserializationError jsonError = deserializeJson(response, body);
    if (jsonError) {
        return failure(String("Web fetch returned invalid JSON: ") + jsonError.c_str());
    }
    const JsonArrayConst results = response["results"].as<JsonArrayConst>();
    if (results.isNull() || results.size() == 0) {
        return failure("Web fetch response is missing a non-empty results array");
    }
    const JsonObjectConst result = results[0].as<JsonObjectConst>();
    const char* title = result["title"].as<const char*>();
    const char* url = result["url"].as<const char*>();
    const char* content = result["text"].as<const char*>();
    if (content == nullptr) {
        const JsonArrayConst highlights = result["highlights"].as<JsonArrayConst>();
        content = !highlights.isNull() && highlights.size() > 0
            ? highlights[0].as<const char*>()
            : result["summary"].as<const char*>();
    }
    if (url == nullptr || url[0] == '\0' || content == nullptr || content[0] == '\0') {
        return failure("Web fetch result is missing url or extracted text");
    }
    JsonDocument outputDocument;
    outputDocument["ok"] = true;
    outputDocument["requested_url"] = requestedUrl;
    outputDocument["url"] = url;
    outputDocument["title"] = boundedUtf8(title, 300);
    outputDocument["content"] = boundedUtf8(content, 6000);
    String output;
    serializeJson(outputDocument, output);
    return {true, output.c_str(), ""};
}

}  // namespace

ToolExecutionResult executeWebSearchTool(const Settings& settings,
                                         const ToolCall& call,
                                         const CancelCallback& isCancelled)
{
    if (!isWebSearchToolName(call.name)) {
        return failure("Web search executor received unsupported tool '" +
                       String(call.name.c_str()) + "'");
    }
    if (!webSearchSettingsAreComplete(settings)) {
        return failure("Web search is not configured; open Fn+4 > Web setup");
    }
    JsonDocument arguments;
    const DeserializationError argumentsError = deserializeJson(arguments, call.arguments);
    if (argumentsError) {
        return failure(String("web_search arguments are invalid JSON: ") + argumentsError.c_str());
    }
    const char* queryValue = arguments["query"].as<const char*>();
    if (queryValue == nullptr || queryValue[0] == '\0') {
        return failure("web_search requires a non-empty query string");
    }
    const String query(queryValue);
    if (query.length() > kMaximumQueryBytes || !isValidUtf8(query.c_str())) {
        return failure("web_search query must be valid UTF-8 and at most 400 bytes");
    }

    JsonDocument requestDocument;
    requestDocument["query"] = query;
    requestDocument["type"] = "fast";
    requestDocument["numResults"] = 5;
    requestDocument["moderation"] = true;
    requestDocument["contents"]["highlights"]["maxCharacters"] = 700;
    String payload;
    serializeJson(requestDocument, payload);

    const OperationResult request = requestJsonToMicroSd(
        settings, kSearchPath, payload, "Web search", isCancelled);
    if (!request.success) {
        return failure(request.error);
    }
    File responseFile = SD.open(kWebResponsePath, FILE_READ);
    if (!responseFile) {
        SD.remove(kWebResponsePath);
        return failure("Failed to open the temporary web search response from microSD");
    }
    const ToolExecutionResult result = parseSearchResponse(responseFile, query);
    responseFile.close();
    if (!SD.remove(kWebResponsePath)) {
        return failure("Failed to remove the temporary web search response from microSD");
    }
    if (result.success) {
        const OperationResult cached = cacheSearchSources(result.output);
        if (!cached.success) {
            return failure(cached.error);
        }
    }
    return result;
}

ToolExecutionResult executeWebFetchTool(const Settings& settings,
                                        const ToolCall& call,
                                        const CancelCallback& isCancelled)
{
    if (!isWebFetchToolName(call.name)) {
        return failure("Web fetch executor received unsupported tool '" +
                       String(call.name.c_str()) + "'");
    }
    if (!webSearchSettingsAreComplete(settings)) {
        return failure("Web fetch is not configured; open Fn+4 > Web setup");
    }
    JsonDocument arguments;
    const DeserializationError argumentsError = deserializeJson(arguments, call.arguments);
    if (argumentsError) {
        return failure(String("web_fetch arguments are invalid JSON: ") + argumentsError.c_str());
    }
    const char* urlValue = arguments["url"].as<const char*>();
    if (urlValue == nullptr || urlValue[0] == '\0') {
        return failure("web_fetch requires a non-empty url string");
    }
    const String url(urlValue);
    if (!url.startsWith("https://") || url.length() > kMaximumUrlBytes ||
        !isValidUtf8(url.c_str())) {
        return failure("web_fetch url must be a valid HTTPS URL of at most 1200 bytes");
    }

    JsonDocument requestDocument;
    requestDocument["urls"].to<JsonArray>().add(url);
    requestDocument["text"]["maxCharacters"] = 6000;
    String payload;
    serializeJson(requestDocument, payload);
    const OperationResult request = requestJsonToMicroSd(
        settings, kContentsPath, payload, "Web fetch", isCancelled);
    if (!request.success) {
        return failure(request.error);
    }
    File responseFile = SD.open(kWebResponsePath, FILE_READ);
    if (!responseFile) {
        SD.remove(kWebResponsePath);
        return failure("Failed to open the temporary web fetch response from microSD");
    }
    const ToolExecutionResult result = parseFetchResponse(responseFile, url);
    responseFile.close();
    if (!SD.remove(kWebResponsePath)) {
        return failure("Failed to remove the temporary web fetch response from microSD");
    }
    return result;
}

WebSearchSourcesResult loadLatestWebSearchSources()
{
    File file = SD.open(kSearchCachePath, FILE_READ);
    if (!file) {
        return {false, "", {}, "No cached web search sources are available"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["ok"].is<bool>() || !document["ok"].as<bool>() ||
        !document["query"].is<const char*>() || !document["results"].is<JsonArray>()) {
        return {false, "", {}, "Cached web search sources contain invalid JSON"};
    }
    WebSearchSourcesResult result = {true, document["query"].as<const char*>(), {}, ""};
    for (const JsonObjectConst item : document["results"].as<JsonArrayConst>()) {
        if (!item["title"].is<const char*>() || !item["url"].is<const char*>() ||
            !item["content"].is<const char*>()) {
            return {false, "", {}, "Cached web search source is missing a typed field"};
        }
        result.sources.push_back({item["title"].as<const char*>(),
                                  item["url"].as<const char*>(),
                                  item["content"].as<const char*>()});
    }
    if (result.sources.empty()) {
        return {false, "", {}, "Cached web search did not contain any sources"};
    }
    return result;
}

}  // namespace cardputer
