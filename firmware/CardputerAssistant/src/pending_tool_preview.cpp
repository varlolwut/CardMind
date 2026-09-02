#include "pending_tool_preview.h"

#include "text_utils.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace cardputer {
namespace {

constexpr std::size_t kChangedLineLeadingContextBytes = 48;

class CanonicalArgumentsReader {
public:
    explicit CanonicalArgumentsReader(const std::string& value)
        : value_(value), position_(0) {}

    int available() const
    {
        return position_ < value_.size() ? 1 : 0;
    }

    int read()
    {
        return available()
            ? static_cast<unsigned char>(value_[position_++]) : -1;
    }

    int peek() const
    {
        return available()
            ? static_cast<unsigned char>(value_[position_]) : -1;
    }

    std::size_t position() const
    {
        return position_;
    }

    bool seek(std::size_t position)
    {
        if (position > value_.size()) return false;
        position_ = position;
        return true;
    }

private:
    const std::string& value_;
    std::size_t position_;
};

bool isContinuationByte(char value)
{
    return (static_cast<std::uint8_t>(value) & 0xC0U) == 0x80U;
}

std::size_t utf8BoundaryAtOrBefore(const std::string& value,
                                   std::size_t offset)
{
    std::size_t result = std::min(offset, value.size());
    while (result > 0 && result < value.size() &&
           isContinuationByte(value[result])) {
        --result;
    }
    return result;
}

std::size_t firstDifferentByte(const std::string& current,
                               const std::string& proposed)
{
    const std::size_t sharedBytes = std::min(current.size(), proposed.size());
    std::size_t offset = 0;
    while (offset < sharedBytes && current[offset] == proposed[offset]) {
        ++offset;
    }
    const std::string& boundarySource = offset < current.size()
        ? current : proposed;
    return utf8BoundaryAtOrBefore(boundarySource, offset);
}

std::size_t lineStartAtOrBefore(const std::string& value,
                                std::size_t offset)
{
    std::size_t result = std::min(offset, value.size());
    while (result > 0 && value[result - 1] != '\n') {
        --result;
    }
    return result;
}

bool appendBody(std::string& body, const std::string& value)
{
    if (value.size() > kMaximumPendingToolPreviewBodyBytes -
                           std::min(body.size(),
                                    kMaximumPendingToolPreviewBodyBytes)) {
        return false;
    }
    body += value;
    return true;
}

std::string hexadecimalEscape(std::uint8_t value)
{
    char escaped[5] = {};
    std::snprintf(escaped, sizeof(escaped), "\\x%02X", value);
    return escaped;
}

struct RenderedSide {
    std::size_t consumedBytes;
    bool clipped;
};

RenderedSide appendSide(std::string& body,
                        const std::string& source,
                        std::size_t start,
                        std::size_t lineStart,
                        char prefix)
{
    if (start >= source.size()) {
        const std::string emptyLine = std::string(1, prefix) + "<empty>\n";
        return {start, !appendBody(body, emptyLine)};
    }

    std::size_t cursor = start;
    bool clipped = false;
    for (std::size_t lineIndex = 0;
         lineIndex < kMaximumPendingToolPreviewLinesPerSide &&
         cursor < source.size();
         ++lineIndex) {
        const std::size_t newline = source.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string::npos
            ? source.size() : newline + 1;
        const std::size_t visibleEnd = newline == std::string::npos
            ? lineEnd : newline;
        const std::size_t whitespaceEnd = visibleEnd > cursor &&
                source[visibleEnd - 1] == '\r'
            ? visibleEnd - 1 : visibleEnd;
        std::size_t firstNonSpace = cursor;
        while (firstNonSpace < whitespaceEnd &&
               (source[firstNonSpace] == ' ' ||
                source[firstNonSpace] == '\t')) {
            ++firstNonSpace;
        }
        std::size_t lastNonSpace = whitespaceEnd;
        while (lastNonSpace > firstNonSpace &&
               (source[lastNonSpace - 1] == ' ' ||
                source[lastNonSpace - 1] == '\t')) {
            --lastNonSpace;
        }

        std::string rendered(1, prefix);
        if (lineIndex == 0 && start > lineStart) {
            rendered += "...";
        }
        std::size_t sourceCursor = cursor;
        while (sourceCursor < lineEnd) {
            const std::uint8_t byte = static_cast<std::uint8_t>(
                source[sourceCursor]);
            std::string visible;
            std::size_t sourceBytes = 1;
            if (byte == '\n') {
                visible = "\\n";
            } else if (byte == '\r') {
                visible = "\\r";
            } else if (byte == '\t') {
                visible = "\\t";
            } else if (byte == ' ' &&
                       (sourceCursor < firstNonSpace ||
                        sourceCursor >= lastNonSpace)) {
                visible = "\\s";
            } else if (byte < 0x20U || byte == 0x7FU) {
                visible = hexadecimalEscape(byte);
            } else {
                std::size_t next = sourceCursor + 1;
                while (next < lineEnd && isContinuationByte(source[next])) {
                    ++next;
                }
                sourceBytes = next - sourceCursor;
                visible.assign(source, sourceCursor, sourceBytes);
            }
            if (visible.size() > kMaximumPendingToolPreviewLineBytes -
                                     std::min(rendered.size(),
                                              kMaximumPendingToolPreviewLineBytes) ||
                rendered.size() + visible.size() + 3 >
                    kMaximumPendingToolPreviewLineBytes) {
                if (rendered.size() + 3 <=
                    kMaximumPendingToolPreviewLineBytes) {
                    rendered += "...";
                }
                clipped = true;
                cursor = sourceCursor;
                break;
            }
            rendered += visible;
            sourceCursor += sourceBytes;
        }
        rendered += '\n';
        if (!appendBody(body, rendered)) {
            return {cursor, true};
        }
        if (clipped) {
            return {cursor, true};
        }
        cursor = lineEnd;
    }
    return {cursor, cursor < source.size()};
}

std::string omittedLine(const char* label, std::size_t bytes)
{
    return "... " + std::to_string(bytes) + " " + label +
        " bytes not shown\n";
}

}  // namespace

json_reader::JsonStringValueResult readCanonicalStringArgument(
    const std::string& arguments,
    const char* field,
    std::size_t maximumBytes)
{
    constexpr std::size_t kMaximumCanonicalToolArgumentFieldNameBytes =
        sizeof("max_inline_output_bytes") - 1;
    CanonicalArgumentsReader reader(arguments);
    return json_reader::readObjectStringField(
        reader, field, kMaximumCanonicalToolArgumentFieldNameBytes,
        maximumBytes);
}

PendingToolPreviewBodyResult buildPendingFileReplacementPreview(
    const std::string& currentPrefix,
    std::uint32_t currentBytes,
    bool currentComplete,
    const std::string& proposed)
{
    if (currentPrefix.size() > kMaximumPendingFilePreviewSourceBytes ||
        proposed.size() > kMaximumPendingFilePreviewSourceBytes ||
        currentPrefix.size() > currentBytes ||
        currentComplete != (currentPrefix.size() == currentBytes) ||
        !isValidUtf8(currentPrefix) || !isValidUtf8(proposed)) {
        return {false, "", false,
                "File preview input is outside the current bounded UTF-8 limits"};
    }

    std::string body = "--- current (" + std::to_string(currentBytes) +
        " bytes)\n+++ proposed (" + std::to_string(proposed.size()) +
        " bytes)\n";
    if (currentComplete && currentPrefix == proposed) {
        if (!appendBody(body, "@@ no changes @@\n")) {
            return {false, "", false, "File preview header exceeds its byte limit"};
        }
        return {true, std::move(body), false, ""};
    }

    const std::size_t difference = firstDifferentByte(currentPrefix, proposed);
    if (!appendBody(body, "@@ first difference at byte " +
                          std::to_string(difference) + " @@\n")) {
        return {false, "", false, "File preview header exceeds its byte limit"};
    }
    const std::string& contextSource = difference <= currentPrefix.size()
        ? currentPrefix : proposed;
    const std::size_t lineStart = lineStartAtOrBefore(contextSource, difference);
    const std::size_t desiredStart = difference > kChangedLineLeadingContextBytes
        ? difference - kChangedLineLeadingContextBytes : 0;
    const std::size_t windowStart = utf8BoundaryAtOrBefore(
        contextSource, std::max(lineStart, desiredStart));
    const bool leadingContextOmitted = windowStart > lineStart;

    const RenderedSide current = appendSide(
        body, currentPrefix, std::min(windowStart, currentPrefix.size()),
        lineStart, '-');
    const RenderedSide next = appendSide(
        body, proposed, std::min(windowStart, proposed.size()),
        lineStart, '+');
    const std::size_t currentOmitted = currentBytes -
        std::min<std::size_t>(current.consumedBytes, currentBytes);
    const std::size_t proposedOmitted = proposed.size() -
        std::min(next.consumedBytes, proposed.size());
    const bool truncated = leadingContextOmitted || !currentComplete ||
        current.clipped || next.clipped || currentOmitted != 0 ||
        proposedOmitted != 0;
    if (currentOmitted != 0 &&
        !appendBody(body, omittedLine("current", currentOmitted))) {
        return {false, "", false, "File preview omission marker exceeds its byte limit"};
    }
    if (proposedOmitted != 0 &&
        !appendBody(body, omittedLine("proposed", proposedOmitted))) {
        return {false, "", false, "File preview omission marker exceeds its byte limit"};
    }
    if (body.size() > kMaximumPendingToolPreviewBodyBytes ||
        !isValidUtf8(body)) {
        return {false, "", false,
                "File preview output exceeds its byte or UTF-8 limit"};
    }
    return {true, std::move(body), truncated, ""};
}

PendingToolPreviewBodyResult buildPendingSshCommandPreview(
    std::string command)
{
    if (command.empty() || command.size() > 1024 || !isValidUtf8(command)) {
        return {false, "", false,
                "SSH preview requires exact UTF-8 command text of at most 1024 bytes"};
    }
    return {true, std::move(command), false, ""};
}

PendingToolPreviewBodyResult buildPendingPythonSourcePreview(
    const String& name,
    std::uint32_t sourceBytes,
    const std::string& sha256,
    std::string sourcePrefix,
    bool sourceComplete)
{
    if (name.length() == 0 || sourcePrefix.size() > sourceBytes ||
        sourceComplete != (sourcePrefix.size() == sourceBytes) ||
        sha256.size() != 64 || !isValidUtf8(sourcePrefix)) {
        return {false, "", false,
                "Python preview input is outside the exact bounded limits"};
    }
    std::string body =
        "Privileged one-run MicroPython code; no sandbox.\n"
        "Output is stored durably in the originating chat.\n"
        "Watchdog and next-boot selection are recovery aids, not adversarial containment.\n"
        "Open Files and inspect the complete source before approval.\n"
        "Path: " + std::string(name.c_str()) + "\nBytes: " +
        std::to_string(sourceBytes) + "\nSHA-256: " + sha256 + "\n\n";
    const std::size_t available = kMaximumPendingToolPreviewBodyBytes -
        std::min(body.size(), kMaximumPendingToolPreviewBodyBytes);
    bool truncated = !sourceComplete || sourcePrefix.size() > available;
    if (sourcePrefix.size() > available) {
        sourcePrefix.resize(utf8BoundaryAtOrBefore(sourcePrefix, available));
    }
    body += sourcePrefix;
    if (body.size() > kMaximumPendingToolPreviewBodyBytes ||
        !isValidUtf8(body)) {
        return {false, "", false,
                "Python preview output exceeds its byte or UTF-8 limit"};
    }
    return {true, std::move(body), truncated, ""};
}

}  // namespace cardputer
