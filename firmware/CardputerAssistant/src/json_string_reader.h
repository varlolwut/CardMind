#pragma once

#include "app_types.h"
#include "text_utils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <utility>

namespace cardputer {
namespace json_reader {

struct JsonStringLengthResult {
    bool success;
    std::size_t bytes;
    String error;
};

struct JsonStringValueResult {
    bool success;
    std::string value;
    String error;
};

inline bool isWhitespace(int value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

template <typename Reader>
void skipWhitespace(Reader& reader)
{
    while (reader.available() && isWhitespace(reader.peek())) {
        reader.read();
    }
}

inline int hexadecimalValue(int value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline std::size_t utf8BytesForCodePoint(std::uint32_t codePoint)
{
    if (codePoint == 0 || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return 0;
    }
    if (codePoint <= 0x7FU) return 1;
    if (codePoint <= 0x7FFU) return 2;
    if (codePoint <= 0xFFFFU) return 3;
    return 4;
}

struct Utf8ValidationState {
    std::uint8_t remaining;
    std::uint8_t minimumNext;
    std::uint8_t maximumNext;
};

inline bool consumeUtf8Byte(Utf8ValidationState& state, int value)
{
    const std::uint8_t byte = static_cast<std::uint8_t>(value);
    if (state.remaining != 0) {
        if (byte < state.minimumNext || byte > state.maximumNext) return false;
        --state.remaining;
        state.minimumNext = 0x80U;
        state.maximumNext = 0xBFU;
        return true;
    }
    if (byte <= 0x7FU) return true;
    if (byte >= 0xC2U && byte <= 0xDFU) {
        state = {1, 0x80U, 0xBFU};
        return true;
    }
    if (byte >= 0xE0U && byte <= 0xEFU) {
        state = {2,
                 static_cast<std::uint8_t>(byte == 0xE0U ? 0xA0U : 0x80U),
                 static_cast<std::uint8_t>(byte == 0xEDU ? 0x9FU : 0xBFU)};
        return true;
    }
    if (byte >= 0xF0U && byte <= 0xF4U) {
        state = {3,
                 static_cast<std::uint8_t>(byte == 0xF0U ? 0x90U : 0x80U),
                 static_cast<std::uint8_t>(byte == 0xF4U ? 0x8FU : 0xBFU)};
        return true;
    }
    return false;
}

inline OperationResult appendCodePoint(std::string& output,
                                       std::uint32_t codePoint,
                                       std::size_t maximumBytes)
{
    const std::size_t encodedBytes = utf8BytesForCodePoint(codePoint);
    if (encodedBytes == 0) {
        return {false, "JSON string contains an unsupported Unicode code point"};
    }
    if (encodedBytes > maximumBytes - std::min(maximumBytes, output.size())) {
        return {false, "JSON string field exceeds its byte limit"};
    }
    char encoded[4] = {};
    if (encodedBytes == 1) {
        encoded[0] = static_cast<char>(codePoint);
    } else if (encodedBytes == 2) {
        encoded[0] = static_cast<char>(0xC0U | (codePoint >> 6));
        encoded[1] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else if (encodedBytes == 3) {
        encoded[0] = static_cast<char>(0xE0U | (codePoint >> 12));
        encoded[1] = static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU));
        encoded[2] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else {
        encoded[0] = static_cast<char>(0xF0U | (codePoint >> 18));
        encoded[1] = static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU));
        encoded[2] = static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU));
        encoded[3] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    }
    output.append(encoded, encodedBytes);
    return {true, ""};
}

template <typename Reader>
OperationResult readCodeUnit(Reader& reader, std::uint16_t& output)
{
    output = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        if (!reader.available()) {
            return {false, "JSON Unicode escape ended before four digits"};
        }
        const int digit = hexadecimalValue(reader.read());
        if (digit < 0) {
            return {false, "JSON Unicode escape contains a non-hexadecimal digit"};
        }
        output = static_cast<std::uint16_t>((output << 4) | digit);
    }
    return {true, ""};
}

template <typename Reader>
OperationResult readEscapedCodePoint(Reader& reader, std::uint32_t& codePoint)
{
    std::uint16_t first = 0;
    OperationResult result = readCodeUnit(reader, first);
    if (!result.success) return result;
    codePoint = first;
    if (first >= 0xD800U && first <= 0xDBFFU) {
        if (!reader.available() || reader.read() != '\\' ||
            !reader.available() || reader.read() != 'u') {
            return {false, "JSON high surrogate is missing its low surrogate"};
        }
        std::uint16_t second = 0;
        result = readCodeUnit(reader, second);
        if (!result.success) return result;
        if (second < 0xDC00U || second > 0xDFFFU) {
            return {false, "JSON high surrogate is followed by an invalid low surrogate"};
        }
        codePoint = 0x10000U +
            ((static_cast<std::uint32_t>(first) - 0xD800U) << 10) +
            (static_cast<std::uint32_t>(second) - 0xDC00U);
    }
    return {true, ""};
}

template <typename Reader>
JsonStringLengthResult measureDecodedString(Reader& reader,
                                            std::size_t maximumBytes)
{
    if (!reader.available() || reader.read() != '"') {
        return {false, 0, "Expected a JSON string"};
    }
    std::size_t bytes = 0;
    Utf8ValidationState utf8 = {0, 0x80U, 0xBFU};
    while (reader.available()) {
        const int value = reader.read();
        if (value == '"') {
            return utf8.remaining == 0
                ? JsonStringLengthResult{true, bytes, ""}
                : JsonStringLengthResult{false, 0,
                                         "JSON string contains invalid UTF-8"};
        }
        if (value >= 0 && value < 0x20) {
            return {false, 0, "JSON string contains an unescaped control character"};
        }
        std::size_t decodedBytes = 1;
        if (value == '\\') {
            if (utf8.remaining != 0) {
                return {false, 0, "JSON string contains invalid UTF-8"};
            }
            if (!reader.available()) {
                return {false, 0, "JSON string ends after an escape marker"};
            }
            const int escaped = reader.read();
            if (escaped == 'u') {
                std::uint32_t codePoint = 0;
                const OperationResult parsed = readEscapedCodePoint(reader, codePoint);
                if (!parsed.success) return {false, 0, parsed.error};
                decodedBytes = utf8BytesForCodePoint(codePoint);
                if (decodedBytes == 0) {
                    return {false, 0,
                            "JSON string contains an unsupported Unicode code point"};
                }
            } else if (escaped != '"' && escaped != '\\' && escaped != '/' &&
                       escaped != 'b' && escaped != 'f' && escaped != 'n' &&
                       escaped != 'r' && escaped != 't') {
                return {false, 0, "JSON string contains an invalid escape sequence"};
            }
        } else if (!consumeUtf8Byte(utf8, value)) {
            return {false, 0, "JSON string contains invalid UTF-8"};
        }
        if (decodedBytes > maximumBytes - std::min(maximumBytes, bytes)) {
            return {false, 0, "JSON string field exceeds its byte limit"};
        }
        bytes += decodedBytes;
    }
    return {false, 0, "JSON string ended before its closing quote"};
}

template <typename Reader>
OperationResult readDecodedStringMatches(Reader& reader,
                                         const char* expected,
                                         std::size_t maximumBytes,
                                         bool& matches)
{
    if (!reader.available() || reader.read() != '"') {
        return {false, "Expected a JSON string"};
    }
    std::size_t expectedBytes = 0;
    while (expected[expectedBytes] != '\0') ++expectedBytes;
    std::size_t decodedBytes = 0;
    Utf8ValidationState utf8 = {0, 0x80U, 0xBFU};
    matches = true;
    while (reader.available()) {
        const int value = reader.read();
        if (value == '"') {
            if (utf8.remaining != 0) {
                return {false, "JSON string contains invalid UTF-8"};
            }
            matches = matches && decodedBytes == expectedBytes;
            return {true, ""};
        }
        if (value >= 0 && value < 0x20) {
            return {false, "JSON string contains an unescaped control character"};
        }
        char decoded[4] = {};
        std::size_t decodedCount = 1;
        decoded[0] = static_cast<char>(value);
        if (value == '\\') {
            if (utf8.remaining != 0) {
                return {false, "JSON string contains invalid UTF-8"};
            }
            if (!reader.available()) {
                return {false, "JSON string ends after an escape marker"};
            }
            const int escaped = reader.read();
            switch (escaped) {
                case '"': decoded[0] = '"'; break;
                case '\\': decoded[0] = '\\'; break;
                case '/': decoded[0] = '/'; break;
                case 'b': decoded[0] = '\b'; break;
                case 'f': decoded[0] = '\f'; break;
                case 'n': decoded[0] = '\n'; break;
                case 'r': decoded[0] = '\r'; break;
                case 't': decoded[0] = '\t'; break;
                case 'u': {
                    std::uint32_t codePoint = 0;
                    const OperationResult parsed = readEscapedCodePoint(reader, codePoint);
                    if (!parsed.success) return parsed;
                    decodedCount = utf8BytesForCodePoint(codePoint);
                    if (decodedCount == 0) {
                        return {false,
                                "JSON string contains an unsupported Unicode code point"};
                    }
                    if (decodedCount == 1) {
                        decoded[0] = static_cast<char>(codePoint);
                    } else if (decodedCount == 2) {
                        decoded[0] = static_cast<char>(0xC0U | (codePoint >> 6));
                        decoded[1] = static_cast<char>(0x80U | (codePoint & 0x3FU));
                    } else if (decodedCount == 3) {
                        decoded[0] = static_cast<char>(0xE0U | (codePoint >> 12));
                        decoded[1] = static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU));
                        decoded[2] = static_cast<char>(0x80U | (codePoint & 0x3FU));
                    } else {
                        decoded[0] = static_cast<char>(0xF0U | (codePoint >> 18));
                        decoded[1] = static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU));
                        decoded[2] = static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU));
                        decoded[3] = static_cast<char>(0x80U | (codePoint & 0x3FU));
                    }
                    break;
                }
                default:
                    return {false, "JSON string contains an invalid escape sequence"};
            }
        } else if (!consumeUtf8Byte(utf8, value)) {
            return {false, "JSON string contains invalid UTF-8"};
        }
        if (decodedCount > maximumBytes - std::min(maximumBytes, decodedBytes)) {
            return {false, "JSON string field exceeds its byte limit"};
        }
        for (std::size_t index = 0; index < decodedCount; ++index) {
            if (decodedBytes + index >= expectedBytes ||
                decoded[index] != expected[decodedBytes + index]) {
                matches = false;
            }
        }
        decodedBytes += decodedCount;
    }
    return {false, "JSON string ended before its closing quote"};
}

template <typename Reader>
OperationResult readDecodedString(Reader& reader,
                                  std::string& output,
                                  std::size_t maximumBytes)
{
    if (!reader.available() || reader.read() != '"') {
        return {false, "Expected a JSON string"};
    }
    while (reader.available()) {
        const int value = reader.read();
        if (value == '"') {
            return isValidUtf8(output)
                ? OperationResult{true, ""}
                : OperationResult{false, "JSON string contains invalid UTF-8"};
        }
        if (value >= 0 && value < 0x20) {
            return {false, "JSON string contains an unescaped control character"};
        }
        if (value != '\\') {
            if (output.size() >= maximumBytes) {
                return {false, "JSON string field exceeds its byte limit"};
            }
            output.push_back(static_cast<char>(value));
            continue;
        }
        if (!reader.available()) return {false, "JSON string ends after an escape marker"};
        const int escaped = reader.read();
        char decoded = 0;
        switch (escaped) {
            case '"': decoded = '"'; break;
            case '\\': decoded = '\\'; break;
            case '/': decoded = '/'; break;
            case 'b': decoded = '\b'; break;
            case 'f': decoded = '\f'; break;
            case 'n': decoded = '\n'; break;
            case 'r': decoded = '\r'; break;
            case 't': decoded = '\t'; break;
            case 'u': {
                std::uint32_t codePoint = 0;
                const OperationResult parsed = readEscapedCodePoint(reader, codePoint);
                if (!parsed.success) return parsed;
                const OperationResult appended = appendCodePoint(
                    output, codePoint, maximumBytes);
                if (!appended.success) return appended;
                continue;
            }
            default:
                return {false, "JSON string contains an invalid escape sequence"};
        }
        if (output.size() >= maximumBytes) {
            return {false, "JSON string field exceeds its byte limit"};
        }
        output.push_back(decoded);
    }
    return {false, "JSON string ended before its closing quote"};
}

template <typename Reader>
OperationResult skipString(Reader& reader)
{
    if (!reader.available() || reader.read() != '"') {
        return {false, "Expected a JSON string while skipping a value"};
    }
    bool escaped = false;
    while (reader.available()) {
        const int value = reader.read();
        if (escaped) {
            escaped = false;
        } else if (value == '\\') {
            escaped = true;
        } else if (value == '"') {
            return {true, ""};
        } else if (value >= 0 && value < 0x20) {
            return {false, "JSON string contains an unescaped control character"};
        }
    }
    return {false, "JSON string ended before its closing quote"};
}

template <typename Reader>
OperationResult skipValue(Reader& reader)
{
    constexpr std::size_t kMaximumNestingDepth = 16;
    skipWhitespace(reader);
    if (!reader.available()) return {false, "JSON value is missing"};
    if (reader.peek() == '"') return skipString(reader);
    if (reader.peek() != '{' && reader.peek() != '[') {
        bool consumed = false;
        while (reader.available()) {
            const int value = reader.peek();
            if (value == ',' || value == '}' || value == ']' || isWhitespace(value)) break;
            reader.read();
            consumed = true;
        }
        return consumed ? OperationResult{true, ""}
                        : OperationResult{false, "JSON primitive value is missing"};
    }
    char closing[kMaximumNestingDepth] = {};
    std::size_t depth = 0;
    while (reader.available()) {
        const int value = reader.read();
        if (value == '"') {
            if (!reader.seek(reader.position() - 1)) {
                return {false, "Failed to rewind while skipping a JSON string"};
            }
            const OperationResult skipped = skipString(reader);
            if (!skipped.success) return skipped;
            continue;
        }
        if (value == '{' || value == '[') {
            if (depth >= kMaximumNestingDepth) {
                return {false, "JSON value exceeds the supported nesting depth"};
            }
            closing[depth++] = value == '{' ? '}' : ']';
        } else if (value == '}' || value == ']') {
            if (depth == 0 || closing[depth - 1] != value) {
                return {false, "JSON value contains mismatched brackets"};
            }
            --depth;
            if (depth == 0) return {true, ""};
        }
    }
    return {false, "JSON container ended before its closing bracket"};
}

template <typename Reader>
JsonStringLengthResult measureObjectStringField(Reader& reader,
                                                const char* field,
                                                std::size_t maximumFieldNameBytes,
                                                std::size_t maximumValueBytes)
{
    skipWhitespace(reader);
    if (!reader.available() || reader.read() != '{') {
        return {false, 0, "JSON document must contain a top-level object"};
    }
    bool found = false;
    bool closedObject = false;
    std::size_t selectedBytes = 0;
    while (reader.available()) {
        skipWhitespace(reader);
        if (reader.peek() == '}') {
            reader.read();
            closedObject = true;
            break;
        }
        const std::size_t keyPosition = reader.position();
        const JsonStringLengthResult measuredKey = measureDecodedString(
            reader, maximumFieldNameBytes);
        if (!measuredKey.success) {
            return {false, 0, "Failed to read JSON field name: " + measuredKey.error};
        }
        if (!reader.seek(keyPosition)) {
            return {false, 0, "Failed to rewind JSON field name"};
        }
        bool selectedField = false;
        OperationResult parsed = readDecodedStringMatches(
            reader, field, maximumFieldNameBytes, selectedField);
        if (!parsed.success) {
            return {false, 0, "Failed to read JSON field name: " + parsed.error};
        }
        skipWhitespace(reader);
        if (!reader.available() || reader.read() != ':') {
            return {false, 0, "JSON field is missing its colon"};
        }
        skipWhitespace(reader);
        if (selectedField) {
            if (found) {
                return {false, 0, "JSON document contains duplicate field: " +
                                           String(field)};
            }
            const JsonStringLengthResult measured = measureDecodedString(
                reader, maximumValueBytes);
            if (!measured.success) {
                return {false, 0, "Failed to measure JSON field " + String(field) +
                                           ": " + measured.error};
            }
            selectedBytes = measured.bytes;
            found = true;
        } else {
            parsed = skipValue(reader);
            if (!parsed.success) {
                return {false, 0, "Failed to skip a JSON field: " + parsed.error};
            }
        }
        skipWhitespace(reader);
        if (!reader.available()) {
            return {false, 0, "JSON object ended before its closing brace"};
        }
        const int delimiter = reader.read();
        if (delimiter == '}') {
            closedObject = true;
            break;
        }
        if (delimiter != ',') {
            return {false, 0, "JSON field is followed by an invalid delimiter"};
        }
    }
    skipWhitespace(reader);
    if (!closedObject) return {false, 0, "JSON object ended before its closing brace"};
    if (reader.available()) return {false, 0, "JSON document contains trailing data"};
    return found ? JsonStringLengthResult{true, selectedBytes, ""}
                 : JsonStringLengthResult{false, 0,
                                          "JSON string field is missing: " + String(field)};
}

template <typename Reader>
JsonStringValueResult readObjectStringField(Reader& reader,
                                            const char* field,
                                            std::size_t maximumFieldNameBytes,
                                            std::size_t maximumValueBytes)
{
    skipWhitespace(reader);
    if (!reader.available() || reader.read() != '{') {
        return {false, {}, "JSON document must contain a top-level object"};
    }
    bool found = false;
    bool closedObject = false;
    std::string selected;
    while (reader.available()) {
        skipWhitespace(reader);
        if (reader.peek() == '}') {
            reader.read();
            closedObject = true;
            break;
        }
        std::string key;
        OperationResult parsed = readDecodedString(reader, key, maximumFieldNameBytes);
        if (!parsed.success) {
            return {false, {}, "Failed to read JSON field name: " + parsed.error};
        }
        skipWhitespace(reader);
        if (!reader.available() || reader.read() != ':') {
            return {false, {}, "JSON field is missing its colon: " + String(key.c_str())};
        }
        skipWhitespace(reader);
        if (key == field) {
            if (found) {
                return {false, {}, "JSON document contains duplicate field: " +
                                           String(field)};
            }
            const std::size_t valuePosition = reader.position();
            const JsonStringLengthResult measured = measureDecodedString(
                reader, maximumValueBytes);
            if (!measured.success) {
                return {false, {}, "Failed to measure JSON field " + String(field) +
                                           ": " + measured.error};
            }
            if (!reader.seek(valuePosition)) {
                return {false, {}, "Failed to rewind JSON field before decoding: " +
                                           String(field)};
            }
            try {
                selected.reserve(measured.bytes);
            } catch (const std::bad_alloc&) {
                return {false, {}, "Failed to allocate decoded JSON field " +
                                           String(field)};
            }
            parsed = readDecodedString(reader, selected, maximumValueBytes);
            if (parsed.success && selected.size() != measured.bytes) {
                return {false, {}, "Decoded JSON field size changed between passes: " +
                                           String(field)};
            }
            found = parsed.success;
        } else {
            parsed = skipValue(reader);
        }
        if (!parsed.success) {
            return {false, {}, "Failed to read JSON field " + String(key.c_str()) +
                                       ": " + parsed.error};
        }
        skipWhitespace(reader);
        if (!reader.available()) {
            return {false, {}, "JSON object ended before its closing brace"};
        }
        const int delimiter = reader.read();
        if (delimiter == '}') {
            closedObject = true;
            break;
        }
        if (delimiter != ',') {
            return {false, {}, "JSON field is followed by an invalid delimiter"};
        }
    }
    skipWhitespace(reader);
    if (!closedObject) return {false, {}, "JSON object ended before its closing brace"};
    if (reader.available()) return {false, {}, "JSON document contains trailing data"};
    return found ? JsonStringValueResult{true, std::move(selected), ""}
                 : JsonStringValueResult{false, {},
                                         "JSON string field is missing: " + String(field)};
}

}  // namespace json_reader
}  // namespace cardputer
