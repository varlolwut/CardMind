#include "ssh_terminal.h"

#include "text_utils.h"

#include <algorithm>
#include <cctype>

namespace cardputer {
namespace {

void eraseCurrentLine(std::string& text)
{
    const std::size_t start = text.find_last_of('\n');
    text.erase(start == std::string::npos ? 0 : start + 1);
}

void applyControlSequence(SshTerminalText& terminal)
{
    const std::string& sequence = terminal.escapeSequence;
    if (sequence == "[2J" || sequence == "[3J") {
        terminal.text.clear();
    } else if (sequence == "[K" || sequence == "[0K" ||
               sequence == "[1K" || sequence == "[2K") {
        eraseCurrentLine(terminal.text);
    }
}

void trimTerminalText(std::string& text, std::size_t maximumTextBytes)
{
    if (text.size() <= maximumTextBytes) {
        return;
    }
    const std::size_t target = text.size() - maximumTextBytes;
    const std::size_t lineBoundary = text.find('\n', target);
    const std::size_t eraseBytes = lineBoundary == std::string::npos
        ? nextUtf8Boundary(text, target)
        : lineBoundary + 1;
    text.erase(0, eraseBytes);
}

}  // namespace

SshTerminalText appendSshTerminalBytes(SshTerminalText current,
                                       const std::uint8_t* data,
                                       std::size_t bytes,
                                       std::size_t maximumTextBytes)
{
    if (data == nullptr || maximumTextBytes < 256) {
        return current;
    }
    for (std::size_t index = 0; index < bytes; ++index) {
        const std::uint8_t byte = data[index];
        if (current.inEscapeSequence) {
            if (current.escapeSequence.size() < 32) {
                current.escapeSequence.push_back(static_cast<char>(byte));
            }
            if ((byte >= '@' && byte <= '~') && current.escapeSequence.size() > 1) {
                applyControlSequence(current);
                current.escapeSequence.clear();
                current.inEscapeSequence = false;
            }
            continue;
        }
        if (byte == 0x1B) {
            current.escapeSequence.clear();
            current.inEscapeSequence = true;
        } else if (byte == '\r') {
            eraseCurrentLine(current.text);
        } else if (byte == '\b' || byte == 0x7F) {
            current.text = removeLastUtf8CodePoint(current.text);
        } else if (byte == '\n' || byte == '\t' || byte >= 0x20) {
            current.text.push_back(static_cast<char>(byte));
        }
    }
    trimTerminalText(current.text, maximumTextBytes);
    return current;
}

std::vector<std::string> sshTerminalVisibleLines(const SshTerminalText& terminal,
                                                 std::size_t columns,
                                                 std::size_t rows)
{
    std::vector<std::string> lines = wrapUtf8Text(terminal.text, columns);
    if (lines.size() > rows) {
        lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(rows));
    }
    while (lines.size() < rows) {
        lines.insert(lines.begin(), "");
    }
    return lines;
}

}  // namespace cardputer
