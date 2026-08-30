#include "tool_policy_codec.h"

namespace cardputer {
namespace {

struct PersistedCapability {
    ToolCapability capability;
    char first;
    char second;
};

constexpr std::array<PersistedCapability, kToolCapabilityCount>
    kPersistedCapabilities = {{
        {ToolCapability::WebSearch, 'w', 's'},
        {ToolCapability::WebFetch, 'w', 'f'},
        {ToolCapability::FilesRead, 'f', 'r'},
        {ToolCapability::FilesWriteDelete, 'f', 'w'},
        {ToolCapability::SshRead, 's', 'r'},
        {ToolCapability::SshMutate, 's', 'm'},
        {ToolCapability::SftpReadWrite, 's', 'f'},
        {ToolCapability::PythonWriteRun, 'p', 'y'},
    }};

std::size_t capabilityIndex(ToolCapability capability) noexcept
{
    return static_cast<std::size_t>(capability);
}

char encodePermission(ToolPermission permission) noexcept
{
    switch (permission) {
        case ToolPermission::Off:
            return 'o';
        case ToolPermission::Ask:
            return 'q';
        case ToolPermission::Allow:
            return 'a';
        case ToolPermission::Count:
            return '\0';
    }
    return '\0';
}

char encodeScopedPermission(ScopedToolPermission permission) noexcept
{
    switch (permission) {
        case ScopedToolPermission::Inherit:
            return 'i';
        case ScopedToolPermission::Off:
            return 'o';
        case ScopedToolPermission::Ask:
            return 'q';
        case ScopedToolPermission::Allow:
            return 'a';
        case ScopedToolPermission::Count:
            return '\0';
    }
    return '\0';
}

ToolPolicyCodecError validateTextPrefix(const char* text,
                                        std::size_t length) noexcept
{
    if (text == nullptr || length != kEncodedToolPolicyLength) {
        return ToolPolicyCodecError::InvalidLength;
    }
    if (text[0] != 'v' || text[1] != '1') {
        return ToolPolicyCodecError::InvalidVersion;
    }
    return ToolPolicyCodecError::None;
}

ToolPolicyCodecError validateCapabilityText(
    const char* text,
    std::size_t cursor,
    const PersistedCapability& capability) noexcept
{
    if (text[cursor] != ';' || text[cursor + 3] != '=') {
        return ToolPolicyCodecError::InvalidCapabilityOrder;
    }
    if (text[cursor + 1] != capability.first ||
        text[cursor + 2] != capability.second) {
        return ToolPolicyCodecError::InvalidCapabilityOrder;
    }
    return ToolPolicyCodecError::None;
}

ToolPermissionPolicyDecodeResult invalidToolPolicyDecode(
    ToolPolicyCodecError error) noexcept
{
    return {defaultGlobalToolPermissionPolicy(), error};
}

ScopedToolPermissionPolicyDecodeResult invalidScopedToolPolicyDecode(
    ToolPolicyCodecError error) noexcept
{
    return {inheritedToolPermissionPolicy(), error};
}

}  // namespace

ToolPermissionPolicy defaultGlobalToolPermissionPolicy() noexcept
{
    return {
        ToolPermission::Allow,
        ToolPermission::Allow,
        ToolPermission::Allow,
        ToolPermission::Allow,
        ToolPermission::Allow,
        ToolPermission::Allow,
        ToolPermission::Allow,
        ToolPermission::Off,
    };
}

ScopedToolPermissionPolicy defaultNewChatToolPermissionPolicy() noexcept
{
    return {
        ScopedToolPermission::Allow,
        ScopedToolPermission::Allow,
        ScopedToolPermission::Ask,
        ScopedToolPermission::Ask,
        ScopedToolPermission::Off,
        ScopedToolPermission::Off,
        ScopedToolPermission::Off,
        ScopedToolPermission::Off,
    };
}

ScopedToolPermissionPolicy inheritedToolPermissionPolicy() noexcept
{
    ScopedToolPermissionPolicy policy = {};
    policy.fill(ScopedToolPermission::Inherit);
    return policy;
}

ScopedToolPermissionPolicy migrateLegacyChatToolPermissionPolicy(
    bool sshToolsEnabled) noexcept
{
    return setLegacySshToolsEnabled(
        defaultNewChatToolPermissionPolicy(), sshToolsEnabled);
}

ScopedToolPermissionPolicy setLegacySshToolsEnabled(
    const ScopedToolPermissionPolicy& policy,
    bool sshToolsEnabled) noexcept
{
    ScopedToolPermissionPolicy updated = policy;
    const ScopedToolPermission permission = sshToolsEnabled
        ? ScopedToolPermission::Allow
        : ScopedToolPermission::Off;
    updated[capabilityIndex(ToolCapability::SshRead)] = permission;
    updated[capabilityIndex(ToolCapability::SshMutate)] = permission;
    return updated;
}

bool legacySshToolsEnabled(
    const ScopedToolPermissionPolicy& policy) noexcept
{
    return policy[capabilityIndex(ToolCapability::SshRead)] ==
            ScopedToolPermission::Allow &&
        policy[capabilityIndex(ToolCapability::SshMutate)] ==
            ScopedToolPermission::Allow;
}

ToolPolicyEncodeResult encodeToolPermissionPolicy(
    const ToolPermissionPolicy& policy) noexcept
{
    ToolPolicyEncodeResult result = {};
    result.error = ToolPolicyCodecError::None;
    result.encoded.value[0] = 'v';
    result.encoded.value[1] = '1';
    std::size_t cursor = 2;
    for (const PersistedCapability& capability : kPersistedCapabilities) {
        const char permission = encodePermission(
            policy[capabilityIndex(capability.capability)]);
        if (permission == '\0') {
            return {{}, ToolPolicyCodecError::InvalidPermission};
        }
        result.encoded.value[cursor] = ';';
        result.encoded.value[cursor + 1] = capability.first;
        result.encoded.value[cursor + 2] = capability.second;
        result.encoded.value[cursor + 3] = '=';
        result.encoded.value[cursor + 4] = permission;
        cursor += 5;
    }
    result.encoded.value[cursor] = '\0';
    return result;
}

ToolPolicyEncodeResult encodeScopedToolPermissionPolicy(
    const ScopedToolPermissionPolicy& policy) noexcept
{
    ToolPolicyEncodeResult result = {};
    result.error = ToolPolicyCodecError::None;
    result.encoded.value[0] = 'v';
    result.encoded.value[1] = '1';
    std::size_t cursor = 2;
    for (const PersistedCapability& capability : kPersistedCapabilities) {
        const char permission = encodeScopedPermission(
            policy[capabilityIndex(capability.capability)]);
        if (permission == '\0') {
            return {{}, ToolPolicyCodecError::InvalidPermission};
        }
        result.encoded.value[cursor] = ';';
        result.encoded.value[cursor + 1] = capability.first;
        result.encoded.value[cursor + 2] = capability.second;
        result.encoded.value[cursor + 3] = '=';
        result.encoded.value[cursor + 4] = permission;
        cursor += 5;
    }
    result.encoded.value[cursor] = '\0';
    return result;
}

ToolPermissionPolicyDecodeResult decodeToolPermissionPolicy(
    const char* text,
    std::size_t length) noexcept
{
    ToolPolicyCodecError error = validateTextPrefix(text, length);
    if (error != ToolPolicyCodecError::None) {
        return invalidToolPolicyDecode(error);
    }
    ToolPermissionPolicyDecodeResult result = {
        defaultGlobalToolPermissionPolicy(),
        ToolPolicyCodecError::None,
    };
    std::size_t cursor = 2;
    for (const PersistedCapability& capability : kPersistedCapabilities) {
        error = validateCapabilityText(text, cursor, capability);
        if (error != ToolPolicyCodecError::None) {
            return invalidToolPolicyDecode(error);
        }
        ToolPermission permission = ToolPermission::Off;
        switch (text[cursor + 4]) {
            case 'o':
                permission = ToolPermission::Off;
                break;
            case 'q':
                permission = ToolPermission::Ask;
                break;
            case 'a':
                permission = ToolPermission::Allow;
                break;
            default:
                return invalidToolPolicyDecode(
                    ToolPolicyCodecError::InvalidPermissionCode);
        }
        result.policy[capabilityIndex(capability.capability)] = permission;
        cursor += 5;
    }
    return result;
}

ScopedToolPermissionPolicyDecodeResult decodeScopedToolPermissionPolicy(
    const char* text,
    std::size_t length) noexcept
{
    ToolPolicyCodecError error = validateTextPrefix(text, length);
    if (error != ToolPolicyCodecError::None) {
        return invalidScopedToolPolicyDecode(error);
    }
    ScopedToolPermissionPolicyDecodeResult result = {
        inheritedToolPermissionPolicy(),
        ToolPolicyCodecError::None,
    };
    std::size_t cursor = 2;
    for (const PersistedCapability& capability : kPersistedCapabilities) {
        error = validateCapabilityText(text, cursor, capability);
        if (error != ToolPolicyCodecError::None) {
            return invalidScopedToolPolicyDecode(error);
        }
        ScopedToolPermission permission = ScopedToolPermission::Inherit;
        switch (text[cursor + 4]) {
            case 'i':
                permission = ScopedToolPermission::Inherit;
                break;
            case 'o':
                permission = ScopedToolPermission::Off;
                break;
            case 'q':
                permission = ScopedToolPermission::Ask;
                break;
            case 'a':
                permission = ScopedToolPermission::Allow;
                break;
            default:
                return invalidScopedToolPolicyDecode(
                    ToolPolicyCodecError::InvalidPermissionCode);
        }
        result.policy[capabilityIndex(capability.capability)] = permission;
        cursor += 5;
    }
    return result;
}

const char* toolPolicyCodecErrorText(ToolPolicyCodecError error) noexcept
{
    switch (error) {
        case ToolPolicyCodecError::None:
            return "none";
        case ToolPolicyCodecError::InvalidPermission:
            return "policy contains an invalid permission value";
        case ToolPolicyCodecError::InvalidLength:
            return "policy has an invalid length";
        case ToolPolicyCodecError::InvalidVersion:
            return "policy has an unsupported version";
        case ToolPolicyCodecError::InvalidCapabilityOrder:
            return "policy capability names or order are invalid";
        case ToolPolicyCodecError::InvalidPermissionCode:
            return "policy contains an invalid permission code";
    }
    return "policy codec returned an unknown error";
}

}  // namespace cardputer
