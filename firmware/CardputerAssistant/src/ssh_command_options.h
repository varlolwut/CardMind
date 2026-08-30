#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cardputer {

inline constexpr std::uint32_t kMinimumSshCommandTimeoutMs = 1000;
inline constexpr std::uint32_t kMaximumSshCommandTimeoutMs = 60000;
inline constexpr std::uint32_t kDefaultSshCommandTimeoutMs = 60000;
inline constexpr std::size_t kMinimumSshCommandInlineOutputBytes = 1;
inline constexpr std::size_t kMaximumSshCommandInlineOutputBytes = 16384;
inline constexpr std::size_t kDefaultSshCommandInlineOutputBytes = 16384;

enum class SshCommandTerminalState : std::uint8_t {
    None,
    UserCancelled,
    TimedOut,
};

constexpr bool isValidSshCommandTimeout(std::uint32_t timeoutMs)
{
    return timeoutMs >= kMinimumSshCommandTimeoutMs &&
        timeoutMs <= kMaximumSshCommandTimeoutMs;
}

constexpr bool isValidSshCommandInlineOutputLimit(std::size_t maximumBytes)
{
    return maximumBytes >= kMinimumSshCommandInlineOutputBytes &&
        maximumBytes <= kMaximumSshCommandInlineOutputBytes;
}

constexpr bool sshCommandDeadlineExpired(std::uint32_t startedAt,
                                         std::uint32_t currentTime,
                                         std::uint32_t timeoutMs)
{
    return static_cast<std::uint32_t>(currentTime - startedAt) >= timeoutMs;
}

constexpr SshCommandTerminalState observeSshCommandTerminalState(
    SshCommandTerminalState currentState,
    bool userCancellationRequested,
    std::uint32_t startedAt,
    std::uint32_t currentTime,
    std::uint32_t timeoutMs)
{
    if (currentState != SshCommandTerminalState::None) {
        return currentState;
    }
    if (userCancellationRequested) {
        return SshCommandTerminalState::UserCancelled;
    }
    return sshCommandDeadlineExpired(startedAt, currentTime, timeoutMs)
        ? SshCommandTerminalState::TimedOut
        : SshCommandTerminalState::None;
}

constexpr bool sshCommandOutputFits(std::size_t currentBytes,
                                    std::size_t incomingBytes,
                                    std::size_t maximumBytes)
{
    return currentBytes <= maximumBytes &&
        incomingBytes <= maximumBytes - currentBytes;
}

inline bool appendSshCommandOutput(std::string& output,
                                   const char* bytes,
                                   std::size_t incomingBytes,
                                   std::size_t maximumBytes)
{
    if (!sshCommandOutputFits(output.size(), incomingBytes, maximumBytes)) {
        std::string().swap(output);
        return false;
    }
    output.append(bytes, incomingBytes);
    return true;
}

}  // namespace cardputer
