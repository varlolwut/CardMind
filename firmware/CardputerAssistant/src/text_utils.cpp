#include "text_utils.h"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace cardputer {
namespace {

struct DecodedCodePoint {
    std::string bytes;
    std::uint32_t value;
};

constexpr const char* kWorkspaceTextExtensions[] = {
    ".txt", ".md", ".json", ".jsonl", ".csv", ".html", ".svg", ".py",
    ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf", ".log", ".xml",
    ".css", ".js", ".mjs", ".cjs", ".ts", ".tsx", ".sh", ".bash", ".zsh",
    ".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", ".ino",
    ".env", ".properties", ".sql",
};

DecodedCodePoint decodeUtf8At(const std::string& value, std::size_t index)
{
    const auto first = static_cast<std::uint8_t>(value[index]);
    std::size_t length = 0;
    std::uint32_t codePoint = 0;
    if (first <= 0x7F) {
        length = 1;
        codePoint = first;
    } else if ((first & 0xE0) == 0xC0) {
        length = 2;
        codePoint = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        codePoint = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        codePoint = first & 0x07;
    } else {
        throw std::invalid_argument("Invalid UTF-8 leading byte");
    }
    if (index + length > value.size()) {
        throw std::invalid_argument("Truncated UTF-8 sequence");
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        const auto continuation = static_cast<std::uint8_t>(value[index + offset]);
        if ((continuation & 0xC0) != 0x80) {
            throw std::invalid_argument("Invalid UTF-8 continuation byte");
        }
        codePoint = (codePoint << 6) | (continuation & 0x3F);
    }
    if ((length == 2 && codePoint < 0x80) ||
        (length == 3 && codePoint < 0x800) ||
        (length == 4 && codePoint < 0x10000) ||
        codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        throw std::invalid_argument("Non-canonical UTF-8 sequence");
    }
    return {value.substr(index, length), codePoint};
}

std::vector<DecodedCodePoint> decodeUtf8(const std::string& value)
{
    std::vector<DecodedCodePoint> result;
    std::size_t index = 0;
    while (index < value.size()) {
        const DecodedCodePoint point = decodeUtf8At(value, index);
        result.push_back(point);
        index += point.bytes.size();
    }
    return result;
}

std::size_t displayCells(std::uint32_t codePoint)
{
    return codePoint <= 0x7F ? 1U : 2U;
}

std::string encodeUtf8CodePoint(std::uint32_t codePoint)
{
    if (codePoint <= 0x7F) {
        return std::string(1, static_cast<char>(codePoint));
    }
    if (codePoint <= 0x7FF) {
        return std::string({
            static_cast<char>(0xC0 | (codePoint >> 6)),
            static_cast<char>(0x80 | (codePoint & 0x3F)),
        });
    }
    if (codePoint <= 0xFFFF) {
        return std::string({
            static_cast<char>(0xE0 | (codePoint >> 12)),
            static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)),
            static_cast<char>(0x80 | (codePoint & 0x3F)),
        });
    }
    return std::string({
        static_cast<char>(0xF0 | (codePoint >> 18)),
        static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)),
        static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)),
        static_cast<char>(0x80 | (codePoint & 0x3F)),
    });
}

std::string lowercaseIntentText(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    std::size_t index = 0;
    while (index < value.size()) {
        const DecodedCodePoint point = decodeUtf8At(value, index);
        std::uint32_t lowered = point.value;
        if (lowered >= 'A' && lowered <= 'Z') {
            lowered += 'a' - 'A';
        } else if (lowered >= 0x0410 && lowered <= 0x042F) {
            lowered += 0x20;
        } else if (lowered == 0x0401) {
            lowered = 0x0451;
        }
        result += lowered == point.value ? point.bytes : encodeUtf8CodePoint(lowered);
        index += point.bytes.size();
    }
    return result;
}

bool endsWithAsciiIgnoreCase(const std::string& value, const char* suffix)
{
    const std::size_t suffixBytes = std::char_traits<char>::length(suffix);
    if (value.size() < suffixBytes) {
        return false;
    }
    const std::size_t first = value.size() - suffixBytes;
    for (std::size_t index = 0; index < suffixBytes; ++index) {
        const auto valueByte = static_cast<unsigned char>(value[first + index]);
        const auto suffixByte = static_cast<unsigned char>(suffix[index]);
        if (std::tolower(valueByte) != std::tolower(suffixByte)) {
            return false;
        }
    }
    return true;
}

bool containsWorkspaceTextExtensionReference(const std::string& value)
{
    for (const char* extension : kWorkspaceTextExtensions) {
        const std::size_t extensionBytes = std::char_traits<char>::length(extension);
        std::size_t position = value.find(extension);
        while (position != std::string::npos) {
            const std::size_t after = position + extensionBytes;
            if (after == value.size()) {
                return true;
            }
            const auto next = static_cast<unsigned char>(value[after]);
            if (!std::isalnum(next) && next != '_' && next != '-' && next != '.') {
                return true;
            }
            position = value.find(extension, position + 1);
        }
    }
    return false;
}

bool containsAny(const std::string& value, const std::vector<std::string>& fragments)
{
    for (const auto& fragment : fragments) {
        if (value.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string russianLower(char key)
{
    switch (key) {
        case '`': return "ё";
        case 'q': return "й";
        case 'w': return "ц";
        case 'e': return "у";
        case 'r': return "к";
        case 't': return "е";
        case 'y': return "н";
        case 'u': return "г";
        case 'i': return "ш";
        case 'o': return "щ";
        case 'p': return "з";
        case '[': return "х";
        case ']': return "ъ";
        case 'a': return "ф";
        case 's': return "ы";
        case 'd': return "в";
        case 'f': return "а";
        case 'g': return "п";
        case 'h': return "р";
        case 'j': return "о";
        case 'k': return "л";
        case 'l': return "д";
        case ';': return "ж";
        case '\'': return "э";
        case 'z': return "я";
        case 'x': return "ч";
        case 'c': return "с";
        case 'v': return "м";
        case 'b': return "и";
        case 'n': return "т";
        case 'm': return "ь";
        case ',': return "б";
        case '.': return "ю";
        default: return std::string(1, key);
    }
}

std::string uppercaseRussian(const std::string& lower)
{
    if (lower.size() != 2) {
        return lower;
    }
    const auto first = static_cast<std::uint8_t>(lower[0]);
    const auto second = static_cast<std::uint8_t>(lower[1]);
    if (first == 0xD0 && second >= 0xB0 && second <= 0xBF) {
        return std::string({static_cast<char>(0xD0), static_cast<char>(second - 0x20)});
    }
    if (first == 0xD1 && second >= 0x80 && second <= 0x8F) {
        return std::string({static_cast<char>(0xD0), static_cast<char>(second + 0x20)});
    }
    if (first == 0xD1 && second == 0x91) {
        return std::string({static_cast<char>(0xD0), static_cast<char>(0x81)});
    }
    return lower;
}

}  // namespace

std::string removeLastUtf8CodePoint(const std::string& value)
{
    if (value.empty()) {
        return value;
    }
    std::size_t index = value.size() - 1;
    while (index > 0 && (static_cast<std::uint8_t>(value[index]) & 0xC0) == 0x80) {
        --index;
    }
    const std::string prefix = value.substr(0, index);
    decodeUtf8(prefix);
    decodeUtf8(value.substr(index));
    return prefix;
}

std::size_t previousUtf8Boundary(const std::string& value, std::size_t index)
{
    if (index > value.size()) {
        throw std::out_of_range("UTF-8 cursor is outside the string");
    }
    decodeUtf8(value.substr(0, index));
    if (index == 0) {
        return 0;
    }
    std::size_t previous = index - 1;
    while (previous > 0 &&
           (static_cast<std::uint8_t>(value[previous]) & 0xC0U) == 0x80U) {
        --previous;
    }
    decodeUtf8(value.substr(previous, index - previous));
    return previous;
}

std::size_t nextUtf8Boundary(const std::string& value, std::size_t index)
{
    if (index > value.size()) {
        throw std::out_of_range("UTF-8 cursor is outside the string");
    }
    decodeUtf8(value.substr(0, index));
    if (index == value.size()) {
        return index;
    }
    const DecodedCodePoint point = decodeUtf8At(value, index);
    return index + point.bytes.size();
}

std::string insertUtf8At(const std::string& value,
                         std::size_t index,
                         const std::string& insertion)
{
    if (index > value.size()) {
        throw std::out_of_range("UTF-8 insertion index is outside the string");
    }
    decodeUtf8(value.substr(0, index));
    decodeUtf8(value.substr(index));
    decodeUtf8(insertion);
    return value.substr(0, index) + insertion + value.substr(index);
}

std::string eraseUtf8Before(const std::string& value, std::size_t index)
{
    const std::size_t previous = previousUtf8Boundary(value, index);
    return value.substr(0, previous) + value.substr(index);
}

std::string mapKeyToRussian(char key)
{
    const bool uppercase = key >= 'A' && key <= 'Z';
    const char normalized = uppercase ? static_cast<char>(key - 'A' + 'a') : key;
    const std::string lower = russianLower(normalized);
    return uppercase ? uppercaseRussian(lower) : lower;
}

std::vector<std::string> wrapUtf8Text(const std::string& value, std::size_t maxCells)
{
    if (maxCells == 0) {
        throw std::invalid_argument("maxCells must be greater than zero");
    }
    std::vector<std::string> lines(1, "");
    std::size_t cells = 0;
    std::string word;
    std::size_t wordCells = 0;
    bool pendingSpace = false;

    const auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        const std::size_t spaceCells = pendingSpace && cells > 0 ? 1U : 0U;
        if (cells > 0 && cells + spaceCells + wordCells > maxCells) {
            lines.push_back("");
            cells = 0;
        }
        if (pendingSpace && cells > 0) {
            lines.back() += ' ';
            ++cells;
        }
        std::size_t wordIndex = 0;
        while (wordIndex < word.size()) {
            const DecodedCodePoint wordPoint = decodeUtf8At(word, wordIndex);
            const std::size_t width = displayCells(wordPoint.value);
            if (cells > 0 && cells + width > maxCells) {
                lines.push_back("");
                cells = 0;
            }
            lines.back() += wordPoint.bytes;
            cells += width;
            wordIndex += wordPoint.bytes.size();
        }
        word.clear();
        wordCells = 0;
        pendingSpace = false;
    };

    std::size_t index = 0;
    while (index < value.size()) {
        const DecodedCodePoint point = decodeUtf8At(value, index);
        index += point.bytes.size();
        if (point.value == '\r') {
            continue;
        }
        if (point.value == '\n') {
            flushWord();
            lines.push_back("");
            cells = 0;
            pendingSpace = false;
            continue;
        }
        if (point.value == ' ' || point.value == '\t') {
            flushWord();
            pendingSpace = cells > 0;
            continue;
        }
        const std::size_t width = displayCells(point.value);
        if (!word.empty() && wordCells + width > maxCells) {
            flushWord();
        }
        word += point.bytes;
        wordCells += width;
    }
    flushWord();
    return lines;
}

bool extractSseData(const std::string& line, std::string& data)
{
    if (line.rfind("data:", 0) != 0) {
        return false;
    }
    std::size_t start = 5;
    if (start < line.size() && line[start] == ' ') {
        ++start;
    }
    data = line.substr(start);
    if (!data.empty() && data.back() == '\r') {
        data.pop_back();
    }
    return true;
}

bool isValidUtf8(const char* value, std::size_t size)
{
    if (value == nullptr) {
        return false;
    }
    std::size_t index = 0;
    while (index < size) {
        const auto first = static_cast<std::uint8_t>(value[index]);
        std::size_t length = 0;
        std::uint32_t codePoint = 0;
        if (first <= 0x7F) {
            length = 1;
            codePoint = first;
        } else if ((first & 0xE0) == 0xC0) {
            length = 2;
            codePoint = first & 0x1F;
        } else if ((first & 0xF0) == 0xE0) {
            length = 3;
            codePoint = first & 0x0F;
        } else if ((first & 0xF8) == 0xF0) {
            length = 4;
            codePoint = first & 0x07;
        } else {
            return false;
        }
        if (index + length > size) {
            return false;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation = static_cast<std::uint8_t>(value[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3F);
        }
        if ((length == 2 && codePoint < 0x80) ||
            (length == 3 && codePoint < 0x800) ||
            (length == 4 && codePoint < 0x10000) ||
            codePoint == 0 ||
            codePoint > 0x10FFFF ||
            (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            return false;
        }
        index += length;
    }
    return true;
}

bool isValidUtf8(const std::string& value)
{
    return isValidUtf8(value.data(), value.size());
}

std::string buildVersionedApiUrl(const std::string& baseUrl, const std::string& versionedPath)
{
    if (baseUrl.empty()) {
        throw std::invalid_argument("baseUrl must not be empty");
    }
    if (versionedPath.rfind("/v1/", 0) != 0) {
        throw std::invalid_argument("versionedPath must start with /v1/");
    }
    const bool baseAlreadyVersioned = baseUrl.size() >= 3 &&
        baseUrl.compare(baseUrl.size() - 3, 3, "/v1") == 0;
    return baseAlreadyVersioned ? baseUrl + versionedPath.substr(3) : baseUrl + versionedPath;
}

std::string ellipsizeUtf8(const std::string& value, std::size_t maxCells)
{
    if (maxCells < 4) {
        throw std::invalid_argument("maxCells must be at least four");
    }
    const auto points = decodeUtf8(value);
    std::size_t totalCells = 0;
    for (const auto& point : points) {
        totalCells += displayCells(point.value);
    }
    if (totalCells <= maxCells) {
        return value;
    }
    std::string result;
    std::size_t cells = 0;
    for (const auto& point : points) {
        const std::size_t width = displayCells(point.value);
        if (cells + width > maxCells - 3) {
            break;
        }
        result += point.bytes;
        cells += width;
    }
    return result + "...";
}

std::string makeChatTitle(const std::string& prompt, std::size_t maxCells)
{
    std::string normalized;
    std::size_t normalizedCells = 0;
    bool pendingSpace = false;
    std::size_t index = 0;
    while (index < prompt.size()) {
        const DecodedCodePoint point = decodeUtf8At(prompt, index);
        index += point.bytes.size();
        const bool whitespace = point.value == ' ' || point.value == '\t' ||
            point.value == '\r' || point.value == '\n';
        if (whitespace) {
            pendingSpace = !normalized.empty();
            continue;
        }
        if (pendingSpace) {
            normalized += ' ';
            ++normalizedCells;
            pendingSpace = false;
        }
        normalized += point.bytes;
        normalizedCells += displayCells(point.value);
        if (normalizedCells > maxCells) {
            break;
        }
    }
    return normalized.empty() ? "New chat" : ellipsizeUtf8(normalized, maxCells);
}

bool isValidChatId(const std::string& value)
{
    if (value.size() != 16) {
        return false;
    }
    for (const char character : value) {
        if (!std::isdigit(static_cast<unsigned char>(character)) &&
            (character < 'a' || character > 'f')) {
            return false;
        }
    }
    return true;
}

bool isValidWorkspaceFilename(const std::string& value)
{
    if (!isValidStorageRelativePath(value, 512)) {
        return false;
    }
    const std::size_t finalSeparator = value.rfind('/');
    const std::string finalSegment = finalSeparator == std::string::npos
        ? value : value.substr(finalSeparator + 1);
    return finalSegment.size() < 4 ||
        (finalSegment.compare(finalSegment.size() - 4, 4, ".tmp") != 0 &&
         finalSegment.compare(finalSegment.size() - 4, 4, ".bak") != 0);
}

bool isWorkspaceTextFile(const std::string& name)
{
    if (!isValidWorkspaceFilename(name)) {
        return false;
    }
    for (const char* extension : kWorkspaceTextExtensions) {
        if (endsWithAsciiIgnoreCase(name, extension)) {
            return true;
        }
    }
    return false;
}

bool isValidStorageRelativePath(const std::string& path, std::size_t maximumBytes)
{
    if (path.empty() || path.size() > maximumBytes || !isValidUtf8(path) ||
        path.front() == '/' || path.back() == '/' || path.find('\\') != std::string::npos) {
        return false;
    }
    std::size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const std::size_t separator = path.find('/', segmentStart);
        const std::size_t segmentEnd = separator == std::string::npos ? path.size() : separator;
        const std::string segment = path.substr(segmentStart, segmentEnd - segmentStart);
        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
        for (const unsigned char byte : segment) {
            if (byte < 0x20 || byte == 0x7f || byte == ':') {
                return false;
            }
        }
        if (separator == std::string::npos) {
            break;
        }
        segmentStart = separator + 1;
    }
    return true;
}

namespace {

std::size_t firstMessageWithinByteBudget(const std::vector<Message>& messages,
                                         std::size_t maximumBytes,
                                         std::size_t& retainedBytes)
{
    retainedBytes = 0;
    if (maximumBytes == 0) {
        return messages.size();
    }
    std::size_t firstRetained = messages.size();
    while (firstRetained > 0) {
        const Message& candidate = messages[firstRetained - 1];
        const std::size_t candidateBytes = candidate.role.length() +
            candidate.content.size() + 16;
        if (retainedBytes + candidateBytes > maximumBytes &&
            firstRetained < messages.size()) {
            break;
        }
        retainedBytes += candidateBytes;
        --firstRetained;
    }
    return firstRetained;
}

std::size_t firstUnsummarizedTailMessage(const ChatDocument& chat)
{
    const std::uint32_t tailCount = static_cast<std::uint32_t>(chat.messages.size());
    const std::uint32_t tailStart = chat.summary.messageCount > tailCount
        ? chat.summary.messageCount - tailCount : 0;
    return chat.summarizedMessageCount > tailStart
        ? std::min<std::uint32_t>(
              tailCount, chat.summarizedMessageCount - tailStart)
        : 0;
}

}  // namespace

ContextWindowResult fitMessagesToByteBudget(const std::vector<Message>& messages,
                                            std::size_t maximumBytes)
{
    ContextWindowResult result = {{}, 0, 0};
    const std::size_t firstRetained = firstMessageWithinByteBudget(
        messages, maximumBytes, result.retainedBytes);
    result.droppedMessages = static_cast<std::uint32_t>(firstRetained);
    result.retained.assign(messages.begin() + firstRetained, messages.end());
    return result;
}

ContextWindowResult fitOwnedMessagesToByteBudget(std::vector<Message> messages,
                                                 std::size_t maximumBytes)
{
    ContextWindowResult result = {{}, 0, 0};
    const std::size_t firstRetained = firstMessageWithinByteBudget(
        messages, maximumBytes, result.retainedBytes);
    result.droppedMessages = static_cast<std::uint32_t>(firstRetained);
    messages.erase(messages.begin(), messages.begin() + firstRetained);
    result.retained = std::move(messages);
    return result;
}

std::vector<Message> takeMessagesDroppedToByteBudget(
    std::vector<Message> messages, std::size_t maximumBytes)
{
    std::size_t retainedBytes = 0;
    const std::size_t firstRetained = firstMessageWithinByteBudget(
        messages, maximumBytes, retainedBytes);
    messages.erase(messages.begin() + firstRetained, messages.end());
    return messages;
}

bool requestsWorkspaceAccess(const std::string& prompt)
{
    const std::string value = lowercaseIntentText(prompt);
    if (value == "/file" || value.rfind("/file ", 0) == 0 ||
        value == "/files" || value.rfind("/files ", 0) == 0) {
        return true;
    }
    if (containsWorkspaceTextExtensionReference(value)) {
        return true;
    }
    const std::vector<std::string> actions = {
        "save", "write", "create", "read", "open", "list", "download", "export",
        "edit", "update", "show", "сохран", "запиш", "созда", "прочит", "откро",
        "перечисл", "покаж", "скача", "экспорт", "измен", "обнов",
    };
    const std::vector<std::string> objects = {
        " file", "files", "filename", "micro sd", "microsd", "sd card", "на sd",
        "с sd", "файл", "заметк", "документ", "script", "скрипт",
    };
    const std::vector<std::string> fileQueries = {
        "what files", "which files", "какие файл", "список файл", "что в файл",
    };
    const bool startsWithFile = value.rfind("file", 0) == 0;
    return containsAny(value, fileQueries) ||
        (containsAny(value, actions) && (startsWithFile || containsAny(value, objects)));
}

std::uint32_t resolveRequestOutputTokens(std::uint32_t projectTokens,
                                         std::uint32_t requestOverrideTokens)
{
    return requestOverrideTokens == 0 ? projectTokens : requestOverrideTokens;
}

ResolvedProjectRequestPolicy resolveProjectRequestPolicy(
    const Settings& settings,
    const ProjectDocument& project,
    const ChatDocument& chat,
    std::uint32_t requestOutputTokens)
{
    return {
        chat.model.length() != 0
            ? chat.model
            : (project.model.length() == 0 ? settings.model : project.model),
        project.contextByteBudget,
        resolveRequestOutputTokens(
            project.maximumOutputTokens, requestOutputTokens),
        project.automaticCompaction,
    };
}

bool shouldAutomaticallyCompactRequest(
    const ResolvedProjectRequestPolicy& policy,
    std::uint32_t droppedMessages)
{
    return policy.automaticCompaction && droppedMessages > 0;
}

ContextUsage resolveContextUsage(const ChatDocument& chat,
                                 std::size_t maximumBytes)
{
    const std::uint32_t totalMessages = chat.summary.messageCount;
    const std::uint32_t summarizedMessages = std::min(
        chat.summarizedMessageCount, totalMessages);
    const std::size_t boundedTailMessages = std::min<std::size_t>(
        totalMessages, chat.messages.size());
    const std::uint32_t tailMessages = static_cast<std::uint32_t>(
        boundedTailMessages);
    const std::uint32_t tailStart = totalMessages - tailMessages;
    const std::uint32_t firstUnsummarized = std::max(
        summarizedMessages, tailStart);
    const std::size_t vectorTailStart = chat.messages.size() - tailMessages;
    const std::size_t firstUnsummarizedInVector = vectorTailStart +
        static_cast<std::size_t>(firstUnsummarized - tailStart);
    std::size_t retainedBytes = 0;
    std::size_t firstRetained = chat.messages.size();
    if (maximumBytes > 0) {
        while (firstRetained > firstUnsummarizedInVector) {
            const Message& candidate = chat.messages[firstRetained - 1];
            const std::size_t candidateBytes = candidate.role.length() +
                candidate.content.size() + 16;
            if (retainedBytes + candidateBytes > maximumBytes &&
                firstRetained < chat.messages.size()) {
                break;
            }
            retainedBytes += candidateBytes;
            --firstRetained;
        }
    }
    const std::uint32_t retainedMessages = static_cast<std::uint32_t>(
        chat.messages.size() - firstRetained);
    const std::uint32_t droppedBeforeTail = tailStart > summarizedMessages
        ? tailStart - summarizedMessages : 0;
    const std::uint32_t droppedInTail = static_cast<std::uint32_t>(
        firstRetained - firstUnsummarizedInVector);
    return {
        retainedBytes,
        retainedMessages,
        droppedBeforeTail + droppedInTail,
        summarizedMessages,
        totalMessages,
    };
}

RetryRequestResult prepareRetryRequest(const std::vector<Message>& messages,
                                       std::size_t maximumBytes)
{
    if (messages.empty() || messages.back().role != "user" ||
        messages.back().content.empty()) {
        return {
            false, "", {},
            "The chat does not end with a failed user request"};
    }
    const ContextWindowResult fitted = fitMessagesToByteBudget(messages, maximumBytes);
    if (fitted.retained.empty() || fitted.retained.back().role != "user" ||
        fitted.retained.back().content != messages.back().content) {
        return {
            false, "", {},
            "The failed user request does not fit the context budget"};
    }
    return {true, messages.back().content, fitted.retained, ""};
}

std::vector<Message> unsummarizedChatTail(const ChatDocument& chat)
{
    if (chat.messages.empty()) {
        return {};
    }
    return std::vector<Message>(
        chat.messages.begin() + firstUnsummarizedTailMessage(chat),
        chat.messages.end());
}

std::vector<Message> takeUnsummarizedChatTail(ChatDocument chat)
{
    if (chat.messages.empty()) {
        return {};
    }
    chat.messages.erase(
        chat.messages.begin(),
        chat.messages.begin() + firstUnsummarizedTailMessage(chat));
    return std::move(chat.messages);
}

ContextSummaryPromptResult buildContextSummaryPrompt(
    const std::string& previousSummary,
    const std::vector<Message>& messages,
    std::size_t maximumBytes)
{
    constexpr const char* kRequest =
        "Create a compact factual conversation summary. Preserve decisions, names, "
        "constraints, unresolved questions and file or command references. Do not add "
        "facts. Return only the summary.\n\n";
    constexpr const char* kPreviousPrefix = "Previous summary:\n";
    constexpr const char* kNewMessagesPrefix = "\n\nNewly omitted messages:\n";
    const std::size_t previousBytes = previousSummary.empty()
        ? 0
        : std::char_traits<char>::length(kPreviousPrefix) + previousSummary.size() +
              std::char_traits<char>::length(kNewMessagesPrefix);
    std::size_t plannedBytes = std::char_traits<char>::length(kRequest) + previousBytes;
    if (plannedBytes > maximumBytes) {
        return {false, "", 0,
                "Existing context summary exceeds the safe compaction request budget"};
    }
    std::uint32_t includedMessages = 0;
    for (const Message& message : messages) {
        const std::size_t prefixBytes = message.role == "user" ? 5 : 4;
        const std::size_t messageBytes = prefixBytes + message.content.size() + 1;
        if (messageBytes > maximumBytes - plannedBytes) {
            break;
        }
        plannedBytes += messageBytes;
        ++includedMessages;
    }
    if (includedMessages == 0) {
        return {false, "", 0,
                "Messages selected for summary exceed the safe compaction request budget"};
    }
    std::string prompt;
    prompt.reserve(plannedBytes);
    prompt += kRequest;
    if (!previousSummary.empty()) {
        prompt += kPreviousPrefix;
        prompt += previousSummary;
        prompt += kNewMessagesPrefix;
    }
    for (std::uint32_t index = 0; index < includedMessages; ++index) {
        const Message& message = messages[index];
        prompt += message.role == "user" ? "You: " : "AI: ";
        prompt += message.content;
        prompt += '\n';
    }
    return {true, std::move(prompt), includedMessages, ""};
}

bool requestsWorkspaceWrite(const std::string& prompt)
{
    const std::string value = lowercaseIntentText(prompt);
    const std::vector<std::string> actions = {
        "save", "write", "create", "export", "edit", "update",
        "сохран", "запиш", "созда", "экспорт", "измен", "обнов",
    };
    const std::vector<std::string> objects = {
        " file", "files", "filename", "micro sd", "microsd", "sd card", "на sd",
        "файл", "заметк", "документ", "script", "скрипт",
    };
    return containsAny(value, actions) && containsAny(value, objects);
}

bool requestsWebSearch(const std::string& prompt)
{
    const std::string value = lowercaseIntentText(prompt);
    const std::vector<std::string> explicitSearch = {
        "/web", "/search", "search the web", "search web", "web search", "look up online",
        "find online", "on the internet", "in the internet", "browse the web",
        "найди в интернете", "найди в сети", "поищи в интернете", "поищи в сети",
        "поиск в интернете", "в интернете", "в сети",
    };
    const std::vector<std::string> currentInformation = {
        "latest", "current", "today", "recent", "news", "release date", "when will",
        "up-to-date", "актуальн", "сегодня", "сейчас", "последн", "свеж", "новост",
        "дата выхода", "когда выйдет", "когда появится", "уже вышел", "уже появилась",
    };
    return containsAny(value, explicitSearch) || containsAny(value, currentInformation);
}

bool isWebSearchToolName(const std::string& name)
{
    std::string normalized;
    for (const char character : name) {
        if (character == '_' || character == '-') {
            continue;
        }
        normalized += character >= 'A' && character <= 'Z'
            ? static_cast<char>(character - 'A' + 'a')
            : character;
    }
    return normalized == "websearch";
}

bool isWebFetchToolName(const std::string& name)
{
    std::string normalized;
    for (const char character : name) {
        if (character == '_' || character == '-') {
            continue;
        }
        normalized += character >= 'A' && character <= 'Z'
            ? static_cast<char>(character - 'A' + 'a')
            : character;
    }
    return normalized == "webfetch";
}

}  // namespace cardputer
