#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cardputer {

struct SshTerminalText {
    std::string text;
    std::string escapeSequence;
    bool inEscapeSequence;
};

SshTerminalText appendSshTerminalBytes(SshTerminalText current,
                                       const std::uint8_t* data,
                                       std::size_t bytes,
                                       std::size_t maximumTextBytes);
std::vector<std::string> sshTerminalVisibleLines(const SshTerminalText& terminal,
                                                 std::size_t columns,
                                                 std::size_t rows);

}  // namespace cardputer
