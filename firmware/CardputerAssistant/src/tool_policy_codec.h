#pragma once

#include "tool_policy.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cardputer {

constexpr std::size_t kEncodedToolPolicyLength = 42;

enum class ToolPolicyCodecError : std::uint8_t {
    None = 0,
    InvalidPermission = 1,
    InvalidLength = 2,
    InvalidVersion = 3,
    InvalidCapabilityOrder = 4,
    InvalidPermissionCode = 5,
};

struct EncodedToolPolicy {
    std::array<char, kEncodedToolPolicyLength + 1> value;
};

struct ToolPolicyEncodeResult {
    EncodedToolPolicy encoded;
    ToolPolicyCodecError error;
};

struct ToolPermissionPolicyDecodeResult {
    ToolPermissionPolicy policy;
    ToolPolicyCodecError error;
};

struct ScopedToolPermissionPolicyDecodeResult {
    ScopedToolPermissionPolicy policy;
    ToolPolicyCodecError error;
};

ToolPermissionPolicy defaultGlobalToolPermissionPolicy() noexcept;
ScopedToolPermissionPolicy defaultNewChatToolPermissionPolicy() noexcept;
ScopedToolPermissionPolicy inheritedToolPermissionPolicy() noexcept;
ScopedToolPermissionPolicy migrateLegacyChatToolPermissionPolicy(
    bool sshToolsEnabled) noexcept;
ScopedToolPermissionPolicy setLegacySshToolsEnabled(
    const ScopedToolPermissionPolicy& policy,
    bool sshToolsEnabled) noexcept;
bool legacySshToolsEnabled(
    const ScopedToolPermissionPolicy& policy) noexcept;

ToolPolicyEncodeResult encodeToolPermissionPolicy(
    const ToolPermissionPolicy& policy) noexcept;
ToolPolicyEncodeResult encodeScopedToolPermissionPolicy(
    const ScopedToolPermissionPolicy& policy) noexcept;
ToolPermissionPolicyDecodeResult decodeToolPermissionPolicy(
    const char* text,
    std::size_t length) noexcept;
ScopedToolPermissionPolicyDecodeResult decodeScopedToolPermissionPolicy(
    const char* text,
    std::size_t length) noexcept;
const char* toolPolicyCodecErrorText(ToolPolicyCodecError error) noexcept;

}  // namespace cardputer
