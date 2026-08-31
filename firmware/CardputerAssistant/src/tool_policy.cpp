#include "tool_policy.h"

#include <cstring>

namespace cardputer {
namespace {

struct EffectivePermission {
    ToolPermission permission;
    ToolPermissionSource source;
};

template <typename Enum>
bool enumValueIsValid(Enum value, Enum count) noexcept
{
    return static_cast<std::uint8_t>(value) <
        static_cast<std::uint8_t>(count);
}

EffectivePermission restrictPermission(EffectivePermission current,
                                       ToolPermission candidate,
                                       ToolPermissionSource source) noexcept
{
    return static_cast<std::uint8_t>(candidate) <
            static_cast<std::uint8_t>(current.permission)
        ? EffectivePermission{candidate, source}
        : current;
}

EffectivePermission restrictScopedPermission(
    EffectivePermission current,
    ScopedToolPermission candidate,
    ToolPermissionSource source) noexcept
{
    if (candidate == ScopedToolPermission::Inherit) {
        return current;
    }
    const ToolPermission permission = static_cast<ToolPermission>(
        static_cast<std::uint8_t>(candidate) - 1);
    return restrictPermission(current, permission, source);
}

ToolPolicyResolutionResult invalidResolution(
    ToolPolicyContractError error) noexcept
{
    ToolPolicyResolutionResult result = {};
    result.requiredGroups = 0;
    result.error = error;
    for (ResolvedToolPermission& permission : result.permissions) {
        permission = {
            ToolPermissionDecision::Deny,
            ToolPermissionSource::None,
        };
    }
    return result;
}

ToolPolicyContractError validateInputs(
    const ToolPermissionPolicy& builtIn,
    const ToolPermissionPolicy& global,
    const ScopedToolPermissionPolicy& project,
    const ScopedToolPermissionPolicy& chat,
    const ToolMessageIntent& intent) noexcept
{
    if (!enumValueIsValid(intent.mode, ToolMessageIntentMode::Count)) {
        return ToolPolicyContractError::InvalidIntentMode;
    }
    const bool groupsAreKnown =
        (intent.requiredGroups &
         static_cast<std::uint8_t>(~kAllToolCapabilityGroups)) == 0;
    const bool groupsMatchMode =
        intent.mode == ToolMessageIntentMode::Required
        ? intent.requiredGroups != 0
        : intent.requiredGroups == 0;
    if (!groupsAreKnown || !groupsMatchMode) {
        return ToolPolicyContractError::InvalidIntentGroups;
    }

    for (std::size_t index = 0; index < kToolCapabilityCount; ++index) {
        if (!enumValueIsValid(builtIn[index], ToolPermission::Count)) {
            return ToolPolicyContractError::InvalidBuiltInPermission;
        }
        if (!enumValueIsValid(global[index], ToolPermission::Count)) {
            return ToolPolicyContractError::InvalidGlobalPermission;
        }
        if (!enumValueIsValid(project[index], ScopedToolPermission::Count)) {
            return ToolPolicyContractError::InvalidProjectPermission;
        }
        if (!enumValueIsValid(chat[index], ScopedToolPermission::Count)) {
            return ToolPolicyContractError::InvalidChatPermission;
        }
    }
    return ToolPolicyContractError::None;
}

}  // namespace

EncodedToolMessageIntent encodeToolMessageIntent(
    const ToolMessageIntent& intent) noexcept
{
    EncodedToolMessageIntent result = {};
    if (!enumValueIsValid(intent.mode, ToolMessageIntentMode::Count)) {
        result.error = ToolMessageIntentCodecError::InvalidIntentMode;
        return result;
    }
    const bool groupsAreKnown =
        (intent.requiredGroups &
         static_cast<std::uint8_t>(~kAllToolCapabilityGroups)) == 0;
    const bool groupsMatchMode =
        intent.mode == ToolMessageIntentMode::Required
        ? intent.requiredGroups != 0
        : intent.requiredGroups == 0;
    if (!groupsAreKnown || !groupsMatchMode) {
        result.error = ToolMessageIntentCodecError::InvalidIntentGroups;
        return result;
    }

    const char* text = nullptr;
    if (intent.mode == ToolMessageIntentMode::Auto) {
        text = "auto";
        result.length = 4;
    } else if (intent.mode == ToolMessageIntentMode::NoTools) {
        text = "none";
        result.length = 4;
    } else {
        text = "required:";
        result.length = kMaximumEncodedToolMessageIntentLength;
    }
    std::memcpy(result.value.data(), text,
                intent.mode == ToolMessageIntentMode::Required
                    ? result.length - 1
                    : result.length);
    if (intent.mode == ToolMessageIntentMode::Required) {
        result.value[result.length - 1] = intent.requiredGroups < 10
            ? static_cast<char>('0' + intent.requiredGroups)
            : static_cast<char>('a' + intent.requiredGroups - 10);
    }
    result.value[result.length] = '\0';
    result.error = ToolMessageIntentCodecError::None;
    return result;
}

DecodedToolMessageIntent decodeToolMessageIntent(
    const char* text,
    std::size_t length) noexcept
{
    const DecodedToolMessageIntent invalidLength = {
        {ToolMessageIntentMode::Auto, 0},
        ToolMessageIntentCodecError::InvalidLength,
    };
    if (text == nullptr || (length != 4 &&
                            length != kMaximumEncodedToolMessageIntentLength)) {
        return invalidLength;
    }
    if (length == 4) {
        if (std::memcmp(text, "auto", length) == 0) {
            return {
                {ToolMessageIntentMode::Auto, 0},
                ToolMessageIntentCodecError::None,
            };
        }
        if (std::memcmp(text, "none", length) == 0) {
            return {
                {ToolMessageIntentMode::NoTools, 0},
                ToolMessageIntentCodecError::None,
            };
        }
        return {
            {ToolMessageIntentMode::Auto, 0},
            ToolMessageIntentCodecError::InvalidValue,
        };
    }
    if (std::memcmp(text, "required:", length - 1) != 0) {
        return {
            {ToolMessageIntentMode::Auto, 0},
            ToolMessageIntentCodecError::InvalidValue,
        };
    }
    const char encodedMask = text[length - 1];
    const std::uint8_t mask = encodedMask >= '1' && encodedMask <= '9'
        ? static_cast<std::uint8_t>(encodedMask - '0')
        : encodedMask >= 'a' && encodedMask <= 'f'
            ? static_cast<std::uint8_t>(encodedMask - 'a' + 10)
            : 0;
    if (mask == 0) {
        return {
            {ToolMessageIntentMode::Auto, 0},
            ToolMessageIntentCodecError::InvalidValue,
        };
    }
    return {
        {ToolMessageIntentMode::Required, mask},
        ToolMessageIntentCodecError::None,
    };
}

EncodedSshProfileId encodeSshProfileId(std::uint64_t profileId) noexcept
{
    EncodedSshProfileId result = {};
    if (profileId == 0) {
        result.error = SshProfileIdCodecError::InvalidValue;
        return result;
    }
    constexpr char kHexDigits[] = "0123456789abcdef";
    result.length = kEncodedSshProfileIdLength;
    for (std::size_t index = 0; index < result.length; ++index) {
        const std::size_t shift = (result.length - index - 1) * 4;
        result.value[index] = kHexDigits[(profileId >> shift) & 0x0F];
    }
    result.value[result.length] = '\0';
    result.error = SshProfileIdCodecError::None;
    return result;
}

DecodedSshProfileId decodeSshProfileId(
    const char* text,
    std::size_t length) noexcept
{
    if (text == nullptr || length != kEncodedSshProfileIdLength) {
        return {0, SshProfileIdCodecError::InvalidLength};
    }
    std::uint64_t profileId = 0;
    for (std::size_t index = 0; index < length; ++index) {
        const char value = text[index];
        const std::uint8_t nibble = value >= '0' && value <= '9'
            ? static_cast<std::uint8_t>(value - '0')
            : value >= 'a' && value <= 'f'
                ? static_cast<std::uint8_t>(value - 'a' + 10)
                : UINT8_MAX;
        if (nibble == UINT8_MAX) {
            return {0, SshProfileIdCodecError::InvalidValue};
        }
        profileId = (profileId << 4) | nibble;
    }
    return profileId == 0
        ? DecodedSshProfileId{0, SshProfileIdCodecError::InvalidValue}
        : DecodedSshProfileId{profileId, SshProfileIdCodecError::None};
}

bool isValidSshProfileCeiling(
    const char* text,
    std::size_t length) noexcept
{
    return length == 0 ||
        decodeSshProfileId(text, length).error == SshProfileIdCodecError::None;
}

bool sshProfileCeilingsAllowSelected(
    std::uint64_t selectedProfileId,
    const char* projectCeiling,
    std::size_t projectCeilingLength,
    const char* chatCeiling,
    std::size_t chatCeilingLength) noexcept
{
    if (selectedProfileId == 0) {
        return false;
    }
    const auto matches = [selectedProfileId](const char* text,
                                              std::size_t length) {
        if (length == 0) {
            return true;
        }
        const DecodedSshProfileId decoded = decodeSshProfileId(text, length);
        return decoded.error == SshProfileIdCodecError::None &&
            decoded.profileId == selectedProfileId;
    };
    return matches(projectCeiling, projectCeilingLength) &&
        matches(chatCeiling, chatCeilingLength);
}

ToolCapabilityGroupMaskResult toolCapabilityGroupMask(
    ToolCapability capability) noexcept
{
    switch (capability) {
        case ToolCapability::WebSearch:
        case ToolCapability::WebFetch:
            return {
                static_cast<std::uint8_t>(
                    1U << static_cast<std::uint8_t>(ToolCapabilityGroup::Web)),
                ToolPolicyContractError::None,
            };
        case ToolCapability::FilesRead:
        case ToolCapability::FilesWriteDelete:
            return {
                static_cast<std::uint8_t>(
                    1U << static_cast<std::uint8_t>(ToolCapabilityGroup::Files)),
                ToolPolicyContractError::None,
            };
        case ToolCapability::SshRead:
        case ToolCapability::SshMutate:
        case ToolCapability::SftpReadWrite:
            return {
                static_cast<std::uint8_t>(
                    1U << static_cast<std::uint8_t>(ToolCapabilityGroup::Ssh)),
                ToolPolicyContractError::None,
            };
        case ToolCapability::PythonWriteRun:
            return {
                static_cast<std::uint8_t>(
                    1U << static_cast<std::uint8_t>(ToolCapabilityGroup::Python)),
                ToolPolicyContractError::None,
            };
        case ToolCapability::Count:
            return {0, ToolPolicyContractError::InvalidCapability};
    }
    return {0, ToolPolicyContractError::InvalidCapability};
}

ToolPolicyResolutionResult resolveToolPolicy(
    const ToolPermissionPolicy& builtIn,
    const ToolPermissionPolicy& global,
    const ScopedToolPermissionPolicy& project,
    const ScopedToolPermissionPolicy& chat,
    const ToolMessageIntent& intent,
    const ToolAvailabilitySet& availability) noexcept
{
    const ToolPolicyContractError inputError = validateInputs(
        builtIn, global, project, chat, intent);
    if (inputError != ToolPolicyContractError::None) {
        return invalidResolution(inputError);
    }

    ToolPolicyResolutionResult result = {};
    result.requiredGroups = intent.requiredGroups;
    result.error = ToolPolicyContractError::None;
    for (std::size_t index = 0; index < kToolCapabilityCount; ++index) {
        EffectivePermission effective = {
            builtIn[index],
            builtIn[index] == ToolPermission::Allow
                ? ToolPermissionSource::None
                : ToolPermissionSource::BuiltIn,
        };
        effective = restrictPermission(
            effective, global[index], ToolPermissionSource::Global);
        effective = restrictScopedPermission(
            effective, project[index], ToolPermissionSource::Project);
        effective = restrictScopedPermission(
            effective, chat[index], ToolPermissionSource::Chat);

        ResolvedToolPermission resolved = {
            static_cast<ToolPermissionDecision>(effective.permission),
            effective.source,
        };
        if (intent.mode == ToolMessageIntentMode::NoTools &&
            resolved.decision != ToolPermissionDecision::Deny) {
            resolved = {
                ToolPermissionDecision::Deny,
                ToolPermissionSource::Message,
            };
        }
        if (resolved.decision != ToolPermissionDecision::Deny &&
            !availability[index]) {
            resolved = {
                ToolPermissionDecision::Unavailable,
                ToolPermissionSource::Availability,
            };
        }
        result.permissions[index] = resolved;
    }
    return result;
}

}  // namespace cardputer
