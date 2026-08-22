#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cardputer {

std::string removeLastUtf8CodePoint(const std::string& value);
std::string mapKeyToRussian(char key);
std::vector<std::string> wrapUtf8Text(const std::string& value, std::size_t maxCells);
bool extractSseData(const std::string& line, std::string& data);
bool isValidUtf8(const std::string& value);
std::string buildVersionedApiUrl(const std::string& baseUrl, const std::string& versionedPath);
std::string ellipsizeUtf8(const std::string& value, std::size_t maxCells);
std::string makeChatTitle(const std::string& prompt, std::size_t maxCells);
bool isValidChatId(const std::string& value);
bool isValidWorkspaceFilename(const std::string& value);
bool requestsWorkspaceAccess(const std::string& prompt);
bool requestsWebSearch(const std::string& prompt);
bool isWebSearchToolName(const std::string& name);
bool isWebFetchToolName(const std::string& name);

}  // namespace cardputer
