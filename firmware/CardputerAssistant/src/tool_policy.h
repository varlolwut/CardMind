#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cardputer {

enum class ToolCapability : std::uint8_t {
    WebSearch = 0,
    WebFetch = 1,
    FilesRead = 2,
    FilesWriteDelete = 3,
    SshRead = 4,
    SshMutate = 5,
    SftpReadWrite = 6,
    PythonWriteRun = 7,
    Count = 8,
};

enum class ToolCapabilityGroup : std::uint8_t {
    Web = 0,
    Files = 1,
    Ssh = 2,
    Python = 3,
    Count = 4,
};

enum class ToolPermission : std::uint8_t {
    Off = 0,
    Ask = 1,
    Allow = 2,
    Count = 3,
};

enum class ScopedToolPermission : std::uint8_t {
    Inherit = 0,
    Off = 1,
    Ask = 2,
    Allow = 3,
    Count = 4,
};

enum class ToolMessageIntentMode : std::uint8_t {
    Auto = 0,
    NoTools = 1,
    Required = 2,
    Count = 3,
};

enum class ToolPermissionDecision : std::uint8_t {
    Deny = 0,
    Ask = 1,
    Allow = 2,
    Unavailable = 3,
};

enum class ToolPermissionSource : std::uint8_t {
    None = 0,
    BuiltIn = 1,
    Global = 2,
    Project = 3,
    Chat = 4,
    Message = 5,
    Availability = 6,
};

enum class ToolPolicyContractError : std::uint8_t {
    None = 0,
    InvalidCapability = 1,
    InvalidBuiltInPermission = 2,
    InvalidGlobalPermission = 3,
    InvalidProjectPermission = 4,
    InvalidChatPermission = 5,
    InvalidIntentMode = 6,
    InvalidIntentGroups = 7,
};

constexpr std::size_t kToolCapabilityCount =
    static_cast<std::size_t>(ToolCapability::Count);
constexpr std::size_t kToolCapabilityGroupCount =
    static_cast<std::size_t>(ToolCapabilityGroup::Count);
constexpr std::uint8_t kAllToolCapabilityGroups = 0x0F;
constexpr std::size_t kMaximumEncodedToolMessageIntentLength = 10;
constexpr std::size_t kEncodedSshProfileIdLength = 16;

using ToolPermissionPolicy = std::array<ToolPermission, kToolCapabilityCount>;
using ScopedToolPermissionPolicy =
    std::array<ScopedToolPermission, kToolCapabilityCount>;
using ToolAvailabilitySet = std::array<bool, kToolCapabilityCount>;

struct ToolMessageIntent {
    ToolMessageIntentMode mode;
    std::uint8_t requiredGroups;
};

enum class ToolMessageIntentCodecError : std::uint8_t {
    None = 0,
    InvalidIntentMode = 1,
    InvalidIntentGroups = 2,
    InvalidLength = 3,
    InvalidValue = 4,
};

struct EncodedToolMessageIntent {
    std::array<char, kMaximumEncodedToolMessageIntentLength + 1> value;
    std::size_t length;
    ToolMessageIntentCodecError error;
};

struct DecodedToolMessageIntent {
    ToolMessageIntent intent;
    ToolMessageIntentCodecError error;
};

enum class SshProfileIdCodecError : std::uint8_t {
    None = 0,
    InvalidLength = 1,
    InvalidValue = 2,
};

struct EncodedSshProfileId {
    std::array<char, kEncodedSshProfileIdLength + 1> value;
    std::size_t length;
    SshProfileIdCodecError error;
};

struct DecodedSshProfileId {
    std::uint64_t profileId;
    SshProfileIdCodecError error;
};

struct ResolvedToolPermission {
    ToolPermissionDecision decision;
    ToolPermissionSource source;
};

struct ToolCapabilityGroupMaskResult {
    std::uint8_t mask;
    ToolPolicyContractError error;
};

struct ToolPolicyResolutionResult {
    std::array<ResolvedToolPermission, kToolCapabilityCount> permissions;
    std::uint8_t requiredGroups;
    ToolPolicyContractError error;
};

ToolCapabilityGroupMaskResult toolCapabilityGroupMask(
    ToolCapability capability) noexcept;
EncodedToolMessageIntent encodeToolMessageIntent(
    const ToolMessageIntent& intent) noexcept;
DecodedToolMessageIntent decodeToolMessageIntent(
    const char* text,
    std::size_t length) noexcept;
EncodedSshProfileId encodeSshProfileId(std::uint64_t profileId) noexcept;
DecodedSshProfileId decodeSshProfileId(
    const char* text,
    std::size_t length) noexcept;
bool isValidSshProfileCeiling(
    const char* text,
    std::size_t length) noexcept;
bool sshProfileCeilingsAllowSelected(
    std::uint64_t selectedProfileId,
    const char* projectCeiling,
    std::size_t projectCeilingLength,
    const char* chatCeiling,
    std::size_t chatCeilingLength) noexcept;
ToolPolicyResolutionResult resolveToolPolicy(
    const ToolPermissionPolicy& builtIn,
    const ToolPermissionPolicy& global,
    const ScopedToolPermissionPolicy& project,
    const ScopedToolPermissionPolicy& chat,
    const ToolMessageIntent& intent,
    const ToolAvailabilitySet& availability) noexcept;

}  // namespace cardputer
